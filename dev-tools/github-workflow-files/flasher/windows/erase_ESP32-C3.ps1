##########################################################################
# Sollte Windows die Ausführung des Skriptes verhindern,
# so muss temporär die Skriptausführungsrichtlinie geändert werden!
# Powershell als Administrator öffnen und mit den folgenden
# Befehlen die Berechtigung setzen und wieder entziehen:
# Set-ExecutionPolicy Unrestricted
# Set-ExecutionPolicy Restricted
##########################################################################
Write-Host "##############################################"
Write-Host "#                                            #"
Write-Host "#              Erase ESP32                 #"
Write-Host "#                                            #"
Write-Host "##############################################`n"
$selectedPortNumber = Read-Host "COM-Port (Nummer) zum Flashen"

Write-Host "Ausgewaehlter COM-Port: COM$comPortToUse"
Write-Host "`nStarte Flashing-Vorgang..."

$command = "./esptool.exe --chip esp32c3 erase_flash
Write-Host "`nAusgefuehrter Befehl: $command"

Invoke-Expression $command

Write-Host ""
Write-Host "###################################################"
Write-Host "#                                                 #"
WRITE-Host "# Druecke Return/Enter, um das Skript zu beenden. #"
Write-Host "#                                                 #"
Write-Host "###################################################"
Read-Host "Warte..."