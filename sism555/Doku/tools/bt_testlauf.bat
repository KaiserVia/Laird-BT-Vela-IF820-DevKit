@echo off
rem Automatischer BT-Testlauf (siehe bt_testlauf_Anleitung.md). Weitere Optionen werden durchgereicht,
rem z. B.: bt_testlauf.bat --profil kurz
cd /d "%~dp0"
python bt_testlauf.py %*
echo.
echo Fertig (Rueckgabewert %ERRORLEVEL%). Zusammenfassung: Testreports\...\testlauf.md
pause
