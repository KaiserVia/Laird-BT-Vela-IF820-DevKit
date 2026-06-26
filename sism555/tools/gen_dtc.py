#!/usr/bin/env python3
# Generiert dtc_text.h aus Doku/Diagnose-Codes.csv  -  NICHT die .h von Hand aendern.
# Tabelle: Code -> {Typ (F/W/I), Kurztext}. Wird in sio.c per #include eingebunden.
import csv, os, sys
HERE=os.path.dirname(os.path.abspath(__file__)); ROOT=os.path.dirname(HERE)
CSV=os.path.join(ROOT,'Doku','Diagnose-Codes.csv'); OUT=os.path.join(ROOT,'dtc_text.h')
rows=[]; seen=set()
with open(CSV,encoding='utf-8') as f:
    for r in csv.reader(f,delimiter=';'):
        if not r or r[0].startswith('#') or r[0].strip()=='Code': continue
        try: code=int(r[0])
        except: continue
        typ=(r[1].strip() or 'F')[0]; txt=r[3].strip()
        if code in seen: print("WARN Dublette:",code,file=sys.stderr); continue
        seen.add(code); rows.append((code,typ,txt))
rows.sort()
def esc(s): return s.replace('\\','\\\\').replace('"','\\"')
L=['/* dtc_text.h - GENERIERT aus Doku/Diagnose-Codes.csv (tools/gen_dtc.py).',
   '   NICHT von Hand aendern. Einbinden NUR in sio.c. */',
   '#ifndef DTC_TEXT_H_','#define DTC_TEXT_H_','',
   'typedef struct { unsigned long code; char typ; const char *txt; } dtc_entry_t;','',
   'static const dtc_entry_t dtc_table[] = {']
for code,typ,txt in rows:
    L.append('  {%5d,\'%s\',"%s"},'%(code,typ,esc(txt)))
L+=['};','#define DTC_TABLE_LEN (sizeof(dtc_table)/sizeof(dtc_table[0]))','','#endif /* DTC_TEXT_H_ */','']
open(OUT,'wb').write(('\r\n'.join(L)).encode('latin-1','replace'))
print("dtc_text.h: %d Eintraege geschrieben"%len(rows))
