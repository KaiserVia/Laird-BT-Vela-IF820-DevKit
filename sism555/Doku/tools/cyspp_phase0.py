#!/usr/bin/env python3
"""cyspp_phase0.py - Phase 0 der CYSPP-Zustandsmaschine: die Fragen aus Abschnitt 6 von
../Konzept_CYSPP_Zustandsmaschine.md automatisch am Geraet klaeren.

Firmware-Gegenstueck (ab 5.129): Testmenue -> 8 (BT-Diagnose) -> 7 (Modulbefehl senden), bt_module_cmd() in btio.c.
Das Skript steuert das IF820 ueber RS232 (COM3) mit EZ-Serial-Befehlen und prueft vom PC aus per BLE (Bleak) und
Classic (SPP-COM-Port), ob das Modul wirbt, verbindbar ist, welche Ereignisse es im Command-Mode meldet und ob der
Datenkanal transparent wird. Alle Aenderungen am Modul nur im RAM (kein /SCFG); am Ende /RBT, dann gilt wieder der
Flash-Stand (Autostart), danach ein BLE-Smoke mit PIN als Gegenprobe.

  python cyspp_phase0.py                       # alle Fragen, Board B (Standardwerte)
  python cyspp_phase0.py --fragen 1,2,3
  python cyspp_phase0.py --stromzyklus         # vorher Relais-Stromzyklus (sauberer Ausgangszustand)

Infrastruktur aus ble_mainboard_test.py: Rs232Sniffer/SnifferChannel/navigate (RS232), TraceLogger (Report-Ordner
unter Testreports/), PowerRelay (Relais), ByteStream.
"""
import argparse
import asyncio
import json
import re
import sys
import time
from datetime import datetime
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import ble_mainboard_test as bmt  # noqa: E402

from bleak import BleakClient, BleakScanner  # noqa: E402
import serial  # noqa: E402

log = bmt.log

LINE_RE = re.compile(r"< \[\+(\d+)ms\] (.*)$")          # Modulausgabe im Menue "  < [+123ms] @E,..."
PIN_RE = re.compile(r"\[CYSPP\] (LOW|HIGH)")
RESP_RE = re.compile(r"@R,[0-9A-Fa-f]{4},([^,]+),([0-9A-Fa-f]{4})(.*)")
FIELD_RE = re.compile(r",([A-Z])=([^,]*)")
EVENT_RE = re.compile(r"@E,[0-9A-Fa-f]{4},([^,]+)(.*)")

# Vorlagen, falls das Get keine verwertbare Antwort liefert (Werte wie init_bluetooth()/btio.h)
CYSPPSP_TEMPLATE = ".CYSPPSP,E=02,G=00,C=0131,L=00000000,R=00000000,M=00000000,P=01,S=00,F=02"
SAP_TEMPLATE = "SAP,T=04,C=07,H=0030,D=001E,L=0140,O=0000,F=01"
SBTP_TEMPLATE = "SBTP,T=7D00,D=02,C=01,F=01,S=02,V=0000,N=0000"

UP_TEXT = "PH0UP"       # PC -> Modul (transparent: kommt roh in rbuf1 an; sonst als @E,...,W-Ereignis)
DOWN_TEXT = "PH0DOWN"   # Modul -> PC (nur im transparenten Modus als Notify/SPP-Daten)


# ---------------------------------------------------------------------------
# RS232: Menuepunkt "Modulbefehl senden"
# ---------------------------------------------------------------------------

class ModuleConsole:
    def __init__(self, sniffer):
        self.sniffer = sniffer
        self.capture = bmt.rs232_capture_start()

    def mark(self) -> int:
        return len(self.capture)

    def lines(self, since: int) -> list[str]:
        return [text for _, text in self.capture[since:]]

    def module_lines(self, since: int) -> list[str]:
        return [m.group(2) for text in self.lines(since) if (m := LINE_RE.search(text))]

    def pin_changes(self, since: int) -> list[str]:
        return [m.group(1) for text in self.lines(since) if (m := PIN_RE.search(text))]

    def pin_now(self) -> str | None:
        changes = self.pin_changes(0)
        return changes[-1] if changes else None

    async def wait_line(self, pattern: str, since: int, timeout: float) -> str | None:
        rx = re.compile(pattern)
        deadline = time.monotonic() + timeout
        seen = since
        while True:
            for text in self.module_lines(seen):
                if rx.search(text):
                    return text
            if time.monotonic() >= deadline:
                return None
            await asyncio.sleep(0.1)

    async def wait_pin(self, level: str, since: int, timeout: float) -> bool:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if level in self.pin_changes(since):
                return True
            await asyncio.sleep(0.1)
        return level in self.pin_changes(since)

    async def write(self, data: bytes) -> None:
        await asyncio.to_thread(self.sniffer.write, data)

    async def enter(self) -> bool:
        ok = await bmt.rs232_navigate([(None, b"H"), ("Hauptmen", b"3"), ("Testmen", b"8"), ("BT-Diagnose", b"7")])
        if not ok:
            return False
        since = self.mark() - 20
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            if any("Modulbefehl senden" in t for t in self.lines(max(0, since))):
                await asyncio.sleep(0.8)   # erster Pin-Abruf (Ausgangspegel) abwarten
                return True
            await asyncio.sleep(0.1)
        log("Menuepunkt 'Modulbefehl senden' nicht gefunden - Firmware ab 5.129 noetig.")
        return False

    async def leave(self) -> None:
        await self.write(b"\r")
        await asyncio.sleep(1.0)

    async def cmd(self, text: str, timeout: float = 2.0) -> dict:
        """Befehl senden, auf die @R-Antwort warten; liefert Ergebnis und Felder."""
        since = self.mark()
        await self.write(text.encode("ascii") + b"\r")
        name = text.split(",")[0]
        line = await self.wait_line(r"@R,[0-9A-Fa-f]{4}," + re.escape(name) + r",", since, timeout)
        result = {"cmd": text, "response": line, "ok": False, "fields": {}}
        if line and (m := RESP_RE.search(line)):
            result["result"] = m.group(2)
            result["ok"] = m.group(2) == "0000"
            result["fields"] = dict(FIELD_RE.findall(m.group(3)))
        log(f"  {text} -> {line or 'KEINE Antwort'}")
        bmt.trace_event("phase0_cmd", **result)
        return result

    async def raw(self, text: str) -> None:
        await self.write(b'"' + text.encode("ascii") + b"\r")


def build_set(set_name: str, fields: dict, changes: dict, template: str) -> str:
    """Set-Befehl aus den Feldern der Get-Antwort, nur die genannten Felder geaendert."""
    if not fields:
        fields = dict(FIELD_RE.findall("," + template.split(",", 1)[1]))
    merged = {**fields, **changes}
    return set_name + "".join(f",{k}={v}" for k, v in merged.items())


def events(lines: list[str]) -> list[str]:
    return [m.group(1) for line in lines if (m := EVENT_RE.search(line))]


# ---------------------------------------------------------------------------
# PC-Seite: BLE und Classic
# ---------------------------------------------------------------------------

async def ble_visible(address: str, secs: float) -> dict:
    found = await BleakScanner.discover(timeout=secs, return_adv=True)
    hit = found.get(address.upper())
    result = {"visible": hit is not None, "rssi": getattr(hit[1], "rssi", None) if hit else None,
              "name": hit[0].name if hit else None, "scan_s": secs}
    detail = f" ({result['name']}, RSSI {result['rssi']})" if hit else ""
    log(f"  BLE-Scan {secs:.0f} s: {'SICHTBAR' if hit else 'nicht sichtbar'}{detail}")
    bmt.trace_event("phase0_scan", address=address, **result)
    return result


class BleLink:
    def __init__(self, address: str):
        self.address = address
        self.client = None
        self.char = None
        self.rx = bmt.ByteStream("ble")
        self.disconnected_at = None

    async def connect(self, timeout: float) -> bool:
        def on_disconnect(_c):
            self.disconnected_at = time.monotonic()
        self.client = BleakClient(self.address, disconnected_callback=on_disconnect)
        try:
            await self.client.connect(timeout=timeout)
        except Exception as exc:  # noqa: BLE001 - jede Verbindungsstoerung ist hier ein Messergebnis
            log(f"  BLE-Verbindung fehlgeschlagen: {exc!r}")
            bmt.trace_event("phase0_ble_connect_failed", error=repr(exc))
            return False
        self.char = bmt.find_char_by_suffix(self.client, bmt.CYSPP_DATA_SUFFIX)
        log(f"  BLE verbunden ({self.address}).")
        return True

    async def subscribe(self) -> bool:
        if self.char is None:
            log("  CYSPP-Characteristic fehlt.")
            return False
        try:
            await self.client.start_notify(self.char, lambda _s, d: self.rx.feed(bytes(d)))
        except Exception as exc:  # noqa: BLE001
            log(f"  Subscribe fehlgeschlagen: {exc!r}")
            return False
        log("  Subscribe ok.")
        return True

    async def write(self, data: bytes) -> bool:
        try:
            await self.client.write_gatt_char(self.char, data, response=False)
            return True
        except Exception as exc:  # noqa: BLE001
            log(f"  BLE-Write fehlgeschlagen: {exc!r}")
            return False

    @property
    def connected(self) -> bool:
        return bool(self.client and self.client.is_connected)

    async def disconnect(self) -> None:
        if self.connected:
            try:
                await self.client.disconnect()
            except Exception as exc:  # noqa: BLE001
                log(f"  BLE-Trennen: {exc!r}")


class ClassicLink:
    def __init__(self, port: str):
        self.port = port
        self.ser = None
        self.rx = bytearray()
        self.loop_task = None

    async def connect(self, timeout: float) -> bool:
        started = time.monotonic()
        try:
            self.ser = await asyncio.wait_for(
                asyncio.to_thread(serial.Serial, self.port, 115200, timeout=0.2), timeout)
        except (asyncio.TimeoutError, serial.SerialException, OSError) as exc:
            log(f"  Classic {self.port}: keine Verbindung nach {time.monotonic() - started:.1f} s ({exc!r})")
            bmt.trace_event("phase0_classic_failed", port=self.port, error=repr(exc))
            return False
        log(f"  Classic {self.port} offen nach {time.monotonic() - started:.1f} s.")
        self.loop_task = asyncio.create_task(self._reader())
        return True

    async def _reader(self) -> None:
        while self.ser and self.ser.is_open:
            try:
                data = await asyncio.to_thread(self.ser.read, 256)
            except (serial.SerialException, OSError, TypeError, AttributeError):
                return
            if data:
                self.rx.extend(data)
                bmt.trace_event("phase0_classic_rx", **bmt.payload_summary(data))

    async def write(self, data: bytes) -> None:
        await asyncio.to_thread(self.ser.write, data)

    async def disconnect(self) -> None:
        if self.ser:
            ser, self.ser = self.ser, None
            await asyncio.to_thread(ser.close)
        if self.loop_task:
            await asyncio.sleep(0.3)
            self.loop_task.cancel()


async def wait_bytes(buf, needle: bytes, timeout: float) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if needle in bytes(buf):
            return True
        await asyncio.sleep(0.1)
    return needle in bytes(buf)


# ---------------------------------------------------------------------------
# Die Fragen aus Abschnitt 6
# ---------------------------------------------------------------------------

class Phase0:
    def __init__(self, args, con: ModuleConsole):
        self.args = args
        self.con = con
        self.base = {}          # Get-Antworten (Felder) vor den Aenderungen

    async def read_base(self) -> None:
        log("Ausgangszustand des Moduls lesen ...")
        await self.con.cmd("/PING")
        for get in (".CYSPPGP", "GAP", "GBTP"):
            r = await self.con.cmd(get)
            self.base[get] = r["fields"] if r["ok"] else {}

    async def set_cyspp(self, e: str) -> dict:
        return await self.con.cmd(build_set(".CYSPPSP", self.base.get(".CYSPPGP"), {"E": e}, CYSPPSP_TEMPLATE))

    async def set_adv_flags(self, f: str) -> dict:
        return await self.con.cmd(build_set("SAP", self.base.get("GAP"), {"F": f}, SAP_TEMPLATE))

    async def set_classic(self, c: str, d: str | None = None) -> dict:
        changes = {"C": c}
        if d is not None:
            changes["D"] = d
        return await self.con.cmd(build_set("SBTP", self.base.get("GBTP"), changes, SBTP_TEMPLATE))

    async def transparency(self, link, label: str) -> dict:
        """PC -> Modul (roh in rbuf1 oder als W-Ereignis?) und Modul -> PC (kommt DOWN_TEXT an?)."""
        since = self.con.mark()
        await link.write((UP_TEXT + "\r\n").encode())
        await asyncio.sleep(1.5)
        lines = self.con.module_lines(since)
        up_raw = any(UP_TEXT in line and not line.startswith("@") for line in lines)
        up_event = any(line.startswith("@E") and ",W," in line for line in lines)
        await self.con.raw(DOWN_TEXT)
        buf = link.rx.buf if isinstance(link, BleLink) else link.rx
        down = await wait_bytes(buf, DOWN_TEXT.encode(), 3.0)
        result = {"pc_to_module_raw": up_raw, "pc_to_module_as_gatt_event": up_event, "module_to_pc": down,
                  "transparent": up_raw and down}
        log(f"  {label} transparent: {'JA' if result['transparent'] else 'NEIN'} "
            f"(PC->Modul roh={up_raw}, als W-Ereignis={up_event}, Modul->PC={down})")
        return result

    # --- Frage 1 -----------------------------------------------------------
    async def q1(self) -> dict:
        log("Frage 1: E=1 + F=00 nur im RAM - wirbt das Modul noch? Classic verbindbar?")
        r = {"cyspp_e1": (await self.set_cyspp("01"))["ok"], "sap_f00": (await self.set_adv_flags("00"))["ok"]}
        await asyncio.sleep(1.0)
        r["scan_nach_e1_f00"] = await ble_visible(self.args.address, self.args.scan)
        r["ax"] = (await self.con.cmd("/AX"))["ok"]
        await asyncio.sleep(1.0)
        r["scan_nach_ax"] = await ble_visible(self.args.address, self.args.scan)
        r["classic"] = await self.classic_probe("Frage 1 (E=1, kein .CYSPPSTART)")
        r["antwort"] = (f"Werben nach E=1/F=00 {'noch AN' if r['scan_nach_e1_f00']['visible'] else 'AUS'}, "
                        f"nach /AX {'noch AN' if r['scan_nach_ax']['visible'] else 'AUS'}; "
                        f"Classic {'verbindbar' if r['classic']['connected'] else 'nicht verbindbar'}")
        return r

    async def classic_probe(self, label: str) -> dict:
        if self.args.no_classic:
            return {"connected": None, "skipped": True}
        since = self.con.mark()
        link = ClassicLink(self.args.classic_port)
        r = {"connected": await link.connect(self.args.classic_timeout)}
        if r["connected"]:
            r["pin_low"] = await self.con.wait_pin("LOW", since, 4.0)
            r["transparenz"] = await self.transparency(link, f"Classic ({label})")
            await link.disconnect()
            await asyncio.sleep(2.0)
        r["events"] = events(self.con.module_lines(since))
        r["module_lines"] = self.con.module_lines(since)[:40]
        log(f"  Classic-Ereignisse: {r['events']}")
        return r

    # --- Frage 2 + 3 -------------------------------------------------------
    async def q2(self) -> dict:
        log("Frage 2: nach .CYSPPSTART - Werben, Verbinden, Abo; Ereignisse vorher, Pin LOW, transparent?")
        r = {"cysppstart": (await self.con.cmd(".CYSPPSTART"))["ok"]}
        await asyncio.sleep(1.0)
        r["scan"] = await ble_visible(self.args.address, self.args.scan)
        since = self.con.mark()
        link = BleLink(self.args.address)
        r["connected"] = await link.connect(self.args.connect_timeout)
        if not r["connected"]:
            return r
        await asyncio.sleep(2.0)
        r["events_vor_abo"] = events(self.con.module_lines(since))
        r["pin_low_vor_abo"] = "LOW" in self.con.pin_changes(since)
        since_sub = self.con.mark()
        r["subscribed"] = await link.subscribe()
        r["pin_low_nach_abo"] = await self.con.wait_pin("LOW", since_sub, 4.0)
        r["events_nach_abo"] = events(self.con.module_lines(since_sub))
        r["transparenz"] = await self.transparency(link, "BLE")
        r["module_lines"] = self.con.module_lines(since)[:40]
        self.link = link
        r["antwort"] = (f"vor Abo Ereignisse {r['events_vor_abo']}, Pin vor Abo {'LOW' if r['pin_low_vor_abo'] else 'HIGH'}; "
                        f"nach Abo Pin {'LOW' if r['pin_low_nach_abo'] else 'HIGH'}, Ereignisse {r['events_nach_abo']}, "
                        f"transparent {r['transparenz']['transparent']}")
        return r

    async def q3(self) -> dict:
        log("Frage 3: nach dem Trennen - still bis zum naechsten .CYSPPSTART? DIS mit Grund?")
        link = getattr(self, "link", None)
        since = self.con.mark()
        if link is None or not link.connected:
            link = BleLink(self.args.address)
            await self.con.cmd(".CYSPPSTART")
            if not await link.connect(self.args.connect_timeout) or not await link.subscribe():
                return {"fehler": "keine Verbindung fuer Frage 3"}
            await asyncio.sleep(2.0)
            since = self.con.mark()
        await link.disconnect()
        r = {"pin_high": await self.con.wait_pin("HIGH", since, 5.0)}
        dis = await self.con.wait_line(r"@E,[0-9A-Fa-f]{4},DIS,", since, 5.0)
        r["dis"] = dis
        r["dis_grund"] = (m.group(1) if dis and (m := re.search(r",R=([0-9A-Fa-f]+)", dis)) else None)
        r["events"] = events(self.con.module_lines(since))
        await asyncio.sleep(1.0)
        r["scan_nach_trennen"] = await ble_visible(self.args.address, self.args.scan)
        r["module_lines"] = self.con.module_lines(since)[:40]
        r["antwort"] = (f"nach Trennen {'wirbt WEITER' if r['scan_nach_trennen']['visible'] else 'still'}; "
                        f"DIS-Grund {r['dis_grund']}, Ereignisse {r['events']}")
        return r

    # --- Frage 4 -----------------------------------------------------------
    async def q4(self) -> dict:
        log("Frage 4: Classic mit SBTP C=0/1 zur Laufzeit; transparent mit E=1? BTCON vorher?")
        if self.args.no_classic:
            return {"skipped": True}
        r = {"sbtp_c0": (await self.set_classic("00"))["ok"]}
        r["classic_c0"] = await self.classic_probe("SBTP C=0")
        r["sbtp_c1"] = (await self.set_classic("01"))["ok"]
        r["classic_c1_ohne_start"] = await self.classic_probe("SBTP C=1, ohne .CYSPPSTART")
        r["cysppstart"] = (await self.con.cmd(".CYSPPSTART"))["ok"]
        r["classic_c1_mit_start"] = await self.classic_probe("SBTP C=1, nach .CYSPPSTART")
        r["antwort"] = (f"C=0: {'verbindbar' if r['classic_c0']['connected'] else 'nicht verbindbar'}; "
                        f"C=1 ohne Start: {'verbindbar' if r['classic_c1_ohne_start']['connected'] else 'nicht verbindbar'}"
                        f" (Ereignisse {r['classic_c1_ohne_start']['events']}); "
                        f"C=1 mit Start: transparent "
                        f"{(r['classic_c1_mit_start'].get('transparenz') or {}).get('transparent')}")
        return r

    # --- Frage 5 -----------------------------------------------------------
    async def q5(self) -> dict:
        log("Frage 5: /UCP zwischen C und Abo - kommt CU vor dem Oeffnen des Datenkanals?")
        await self.con.cmd(".CYSPPSTART")
        since = self.con.mark()
        link = BleLink(self.args.address)
        r = {"connected": await link.connect(self.args.connect_timeout)}
        if not r["connected"]:
            return r
        c_line = await self.con.wait_line(r"@E,[0-9A-Fa-f]{4},C,", since, 5.0)
        r["c_event"] = c_line
        handle = re.search(r",C=([0-9A-Fa-f]+)", c_line or "")
        interval = re.search(r",I=([0-9A-Fa-f]+)", c_line or "")
        if not handle:
            r["fehler"] = "kein C-Ereignis mit Handle"
            await link.disconnect()
            return r
        want = "6" if (not interval or int(interval.group(1), 16) > 6) else "C"   # anderes Intervall als das aktuelle
        since_ucp = self.con.mark()
        r["ucp"] = await self.con.cmd(f"/UCP,C={handle.group(1)},I={want},L=0,O=190", timeout=3.0)
        cu = await self.con.wait_line(r"@E,[0-9A-Fa-f]{4},CU,", since_ucp, 6.0)
        r["cu_event"] = cu
        r["cu_vor_abo"] = cu is not None
        since_sub = self.con.mark()
        r["subscribed"] = await link.subscribe()
        r["pin_low_nach_abo"] = await self.con.wait_pin("LOW", since_sub, 4.0)
        r["transparenz"] = await self.transparency(link, "BLE nach /UCP")
        r["module_lines"] = self.con.module_lines(since)[:40]
        await link.disconnect()
        await asyncio.sleep(2.0)
        r["antwort"] = (f"/UCP {'ok' if r['ucp']['ok'] else 'abgelehnt'}; CU vor Abo: {cu or 'nein'}; "
                        f"danach transparent {r['transparenz']['transparent']}")
        return r

    # --- Frage 6 -----------------------------------------------------------
    async def q6(self) -> dict:
        log("Frage 6: /AX + SBTP C=0 in BEREIT - sofort unsichtbar? Laufende Verbindung?")
        r = {"cysppstart": (await self.con.cmd(".CYSPPSTART"))["ok"], "sbtp_c1": (await self.set_classic("01"))["ok"]}
        await asyncio.sleep(1.0)
        r["scan_bereit"] = await ble_visible(self.args.address, self.args.scan)
        r["ax"] = (await self.con.cmd("/AX"))["ok"]
        r["sbtp_c0"] = (await self.set_classic("00"))["ok"]
        await asyncio.sleep(0.5)
        r["scan_gesperrt"] = await ble_visible(self.args.address, self.args.scan)
        r["classic_gesperrt"] = await self.classic_probe("gesperrt (SBTP C=0)")
        # laufende Verbindung (verbunden, Datenkanal noch zu) und dann /AX
        await self.set_classic("01")
        await self.con.cmd(".CYSPPSTART")
        link = BleLink(self.args.address)
        r["connected"] = await link.connect(self.args.connect_timeout)
        if r["connected"]:
            await asyncio.sleep(1.5)
            since = self.con.mark()
            r["ax_verbunden"] = (await self.con.cmd("/AX"))["ok"]
            await asyncio.sleep(5.0)
            r["noch_verbunden_nach_ax"] = link.connected
            r["events_nach_ax"] = events(self.con.module_lines(since))
            if link.connected:
                since_sub = self.con.mark()
                r["subscribed"] = await link.subscribe()
                r["pin_low_nach_abo"] = await self.con.wait_pin("LOW", since_sub, 4.0)
                r["transparenz"] = await self.transparency(link, "BLE nach /AX")
            await link.disconnect()
            await asyncio.sleep(2.0)
        r["antwort"] = (f"nach /AX+C=0 BLE {'noch sichtbar' if r['scan_gesperrt']['visible'] else 'unsichtbar'}, "
                        f"Classic {'verbindbar' if r['classic_gesperrt'].get('connected') else 'nicht verbindbar'}; "
                        f"laufende Verbindung nach /AX {'bleibt' if r.get('noch_verbunden_nach_ax') else 'getrennt/keine'}"
                        f", danach transparent {(r.get('transparenz') or {}).get('transparent')}")
        return r

    # --- Rueckweg ----------------------------------------------------------
    async def restore(self) -> dict:
        log("Rueckweg: /RBT (Modul startet mit Flash-Stand = Autostart neu) ...")
        since = self.con.mark()
        await self.con.write(b"/RBT\r")
        boot = await self.con.wait_line(r"@E,[0-9A-Fa-f]{4},BOOT,", since, 8.0)
        r = {"boot_event": boot, "events": events(self.con.module_lines(since))}
        log(f"  BOOT-Ereignis: {boot or 'nicht gesehen'}")
        await asyncio.sleep(2.0)
        r["cyspp_nach_rbt"] = (await self.con.cmd(".CYSPPGP"))["fields"]
        return r


async def ble_smoke(args) -> dict:
    """Gegenprobe nach dem Rueckweg: verbinden, PIN-Prompt, PIN, Hauptmenue."""
    log("Gegenprobe: BLE-Smoke mit PIN ...")
    link = BleLink(args.address)
    r = {"connected": await link.connect(args.connect_timeout)}
    if r["connected"] and await link.subscribe():
        r["pin_prompt"] = await link.rx.wait_for_marker(bmt.MARK_PIN_PROMPT, 10.0)
        if r["pin_prompt"]:
            await link.write(args.pin.encode())
            r["hauptmenue"] = await link.rx.wait_for_marker(bmt.MARK_HAUPTMENU, 8.0)
        await link.disconnect()
    r["passed"] = bool(r.get("hauptmenue"))
    log(f"  Smoke {'bestanden' if r['passed'] else 'GESCHEITERT'}.")
    return r


def write_summary(result: dict) -> None:
    folder = bmt.RUN_REPORT_DIR
    (folder / "phase0.json").write_text(json.dumps(result, indent=2, ensure_ascii=False, default=str), encoding="utf-8")
    lines = [f"# CYSPP Phase 0 - {result['started']}", "",
             f"Board {result['args']['address']}, RS232 {result['args']['rs232_port']}, "
             f"Classic {result['args']['classic_port']}", "", "| Frage | Ergebnis |", "|---|---|"]
    for key, value in result["fragen"].items():
        lines.append(f"| {key} | {value.get('antwort') or value.get('fehler') or '-'} |")
    rest = result.get("rueckweg", {})
    lines += ["", f"Rueckweg: BOOT {'gesehen' if rest.get('boot_event') else 'NICHT gesehen'}, "
              f".CYSPPGP danach {rest.get('cyspp_nach_rbt')}",
              f"Gegenprobe BLE-Smoke: {'bestanden' if result.get('smoke', {}).get('passed') else 'GESCHEITERT'}"]
    (folder / "phase0.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    log(f"Bericht: {folder / 'phase0.md'}")


async def run(args) -> int:
    if args.stromzyklus:
        relay = bmt.PowerRelay(args.relais_port, args.relais)
        log(f"Stromzyklus {args.stromlos:.0f} s (Relais {args.relais} an {args.relais_port}) ...")
        relay.cycle(args.stromlos)
        relay.close()
    bmt.TRACE = bmt.TraceLogger(args)
    if not bmt.setup_rs232_sniffer(args) or bmt.SNIFFER is None:
        log("RS232 nicht verfuegbar - Abbruch.")
        return 2
    if args.stromzyklus:
        log(f"Hochlauf abwarten ({args.boot_wait:.0f} s) ...")
        await asyncio.sleep(args.boot_wait)
    con = ModuleConsole(bmt.SNIFFER)
    result = {"started": datetime.now().isoformat(timespec="seconds"), "args": vars(args), "fragen": {}}
    if not await con.enter():
        bmt.SNIFFER.stop()
        return 2
    p0 = Phase0(args, con)
    try:
        await p0.read_base()
        result["base"] = p0.base
        wanted = [int(x) for x in args.fragen.split(",")]
        steps = {1: p0.q1, 2: p0.q2, 3: p0.q3, 4: p0.q4, 5: p0.q5, 6: p0.q6}
        for number in wanted:
            try:
                result["fragen"][f"Frage {number}"] = await steps[number]()
            except Exception as exc:  # noqa: BLE001 - naechste Frage trotzdem versuchen
                log(f"Frage {number} abgebrochen: {exc!r}")
                result["fragen"][f"Frage {number}"] = {"fehler": repr(exc)}
            bmt.trace_event("phase0_frage", nummer=number, ergebnis=result["fragen"][f"Frage {number}"])
    finally:
        result["rueckweg"] = await p0.restore()
        await con.leave()
    await asyncio.sleep(3.0)
    result["smoke"] = await ble_smoke(args)
    result["rs232_tail"] = bmt.SNIFFER.tail(30)
    write_summary(result)
    bmt.SNIFFER.stop()
    for key, value in result["fragen"].items():
        print(f"{key}: {value.get('antwort') or value.get('fehler')}")
    return 0 if result["smoke"]["passed"] else 1


def main() -> int:
    p = argparse.ArgumentParser(description="Phase 0 der CYSPP-Zustandsmaschine (Konzept Abschnitt 6).")
    p.add_argument("--fragen", default="1,2,3,4,5,6", help="Fragen aus Abschnitt 6, komma-getrennt")
    p.add_argument("--address", default="FF:4F:7F:9E:EE:BA", help="BLE-Adresse (Default Board B 26SC9983)")
    p.add_argument("--pin", default="269983")
    p.add_argument("--classic-port", default="COM16")
    p.add_argument("--no-classic", action="store_true", help="Classic-Proben auslassen")
    p.add_argument("--rs232-port", default="COM3")
    p.add_argument("--rs232-baud", type=int, default=115200)
    p.add_argument("--scan", type=float, default=8.0, help="Dauer je BLE-Sichtbarkeits-Scan (s)")
    p.add_argument("--connect-timeout", type=float, default=20.0)
    p.add_argument("--classic-timeout", type=float, default=25.0)
    p.add_argument("--stromzyklus", action="store_true", help="vorher Stromzyklus ueber das Relais")
    p.add_argument("--relais-port", default="COM7")
    p.add_argument("--relais", type=int, default=2)
    p.add_argument("--stromlos", type=float, default=10.0)
    p.add_argument("--boot-wait", type=float, default=40.0)
    args = p.parse_args()
    args.mode = "cyspp-phase0"          # fuer TraceLogger/Report-Ordner
    args.transport = "ble"
    args.no_rs232 = False
    try:
        return asyncio.run(run(args))
    finally:
        if bmt.TRACE is not None:
            bmt.TRACE.close()


if __name__ == "__main__":
    sys.exit(main())
