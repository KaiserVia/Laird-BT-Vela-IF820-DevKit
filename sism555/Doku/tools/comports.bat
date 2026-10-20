@echo off
rem comports.bat - zeigt, welcher COM-Port womit verbunden ist (nur lesend). Doppelklick genuegt.
rem Die Zuordnung zu einem Board: Bluetooth-Ports tragen den Namen VIASIS_<Seriennummer>; die USB-Ports der Mainboards
rem nennen keine Seriennummer (dort hilft: python ble_mainboard_test.py --mode info --rs232-port COMxx).
powershell -NoProfile -ExecutionPolicy Bypass -Command "Invoke-Expression ((Get-Content -LiteralPath '%~f0' -Raw) -split ('###P'+'S###'))[1]"
echo.
pause
exit /b
###PS###
$bt = @{}
Get-PnpDevice -Class Bluetooth -ErrorAction SilentlyContinue | ForEach-Object {
  if ($_.InstanceId -match 'DEV_([0-9A-F]{12})') { $bt[$Matches[1]] = $_.FriendlyName }
}
$rows = foreach ($p in (Get-CimInstance Win32_PnPEntity | Where-Object { $_.Name -match '\(COM(\d+)\)' })) {
  $n = [int]([regex]::Match($p.Name, '\(COM(\d+)\)').Groups[1].Value)
  $id = $p.DeviceID
  if ($id -match 'LOCALMFG') { $art = 'Bluetooth ausgehend'; $wer = 'ungenutzt' }
  elseif ($id -match '&([0-9A-F]{12})_C00000000') {
    $a = $Matches[1]; $art = 'Bluetooth Classic (SPP)'
    $wer = if ($bt.ContainsKey($a)) { "$($bt[$a])  [$a]" } else { "unbekannt  [$a]" }
  }
  elseif ($id -match 'VID_1FC9&PID_80F7') { $art = 'USB'; $wer = 'Mainboard (LPC1766), Seriennummer siehe --mode info' }
  elseif ($id -match 'VID_067B&PID_23A3') { $art = 'USB'; $wer = 'RS232-Adapter (Prolific), RS232 eines Boards' }
  else { $art = 'sonstige'; $wer = $p.Name }
  [pscustomobject]@{ Nr = $n; Port = "COM$n"; Art = $art; Verbunden_mit = $wer }
}
$rows | Sort-Object Nr | Select-Object Port, Art, Verbunden_mit | Format-Table -AutoSize
