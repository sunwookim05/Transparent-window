$ErrorActionPreference = 'Stop'
Push-Location (Split-Path $PSScriptRoot -Parent)
try {
    & gcc tests/trayPopup.c src/Installer.c src/Updater.c src/Settings.c src/Transparency.c src/Tracker.c src/thread.c src/System.c src/Scanner.c src/console.c src/algorithm.c -Iinc -o build/trayPopup.test.exe -luser32 -lgdi32 -lshell32 -lcomctl32 -lpsapi -lwinhttp -lshlwapi -ldwmapi -Wall -Wextra
    if ($LASTEXITCODE -ne 0) { throw 'Tray popup test build failed.' }
    & ./build/trayPopup.test.exe
    if ($LASTEXITCODE -ne 0) { throw 'Tray popup interaction checks failed.' }
} finally {
    Pop-Location
}
