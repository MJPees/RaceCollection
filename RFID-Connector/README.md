# RFID-Connector

**Firmware-Version: 2.0.0**

Der RFID-Connector ermöglicht eine Zeitnahme für z.B. Carrera Hybrid oder Dr!ft von Sturmkind usw. mit <a href="https://www.smartrace.de/">SmartRace</a> oder <a href="https://carrera-hybrid-racing-club.de/">CH-Racing-Club</a>.

**Anbindung je nach Anwendung:**
- **SmartRace:** WLAN + WebSocket (analoger Sensormodus) - unterstützt 8 gleichzeitige RFID-Tags
- **CH Racing Club:** BLE (Bluetooth Low Energy) - unterstützt 30 gleichzeitige RFID-Tags<br>

## Funktionen
- **RFID-Mapping:** Bis zu 30 RFID-Tag-zu-Controller-Mappings möglich. IDs werden automatisch erkannt und können individuell zugeordnet werden. SmartRace unterstützt 8 gleichzeitige Tags, CH Racing Club 30 gleichzeitige Tags.

- **Dense Mode:** Optimierter Lesemodus für dichte Umgebungen mit vielen RFID-Tags (aktiviert/deaktiviert per Konfiguration).

- **Power Level Einstellung:** RFID-Empfangsleistung von 10 dBm bis 26 dBm einstellbar für optimale Reichweite.

- **Minimale Rundenzeit:** Konfigurierbare minimale Zeit zwischen Runden (Standard: 3000 ms) verhindert doppelte Erfassungen.

- **Team-Rennen:** Mehrere RFID-IDs können eine RFID-ID zugeordnet werden (z.B. für Team-Rennen).

- **Statusanzeigen:** Optionale LEDs für RFID-Aktivität, BLE-Verbindung und WebSocket-Status.

- **Reset-Taster:** Temporäres Zurücksetzen der RFID-Speicherung (bis Reboot) über optional anschließbaren Taster.<br><br>


<a href="../script-flasher/README.md">Flash-Anleitungen</a>

## Hardware-Anforderungen

Als RFID-Leser wird ein **R200 der Firma Inveton** verwendet. Dieser kann z.B. über AliExpress bezogen werden und liegt inklusive einer 1dBi Antenne mit Versand aktuell bei ca. 50 Euro. 

**Unterstützte ESP32-Varianten:**
- ESP32-DEV (WROOM-32, getestet)
- ESP32-WROOM-32U (mit externem Antennenanschluss)
- ESP32-C3 (experimentell, Code vorbereitet aber auskommentiert)

**Kommunikation:**
- R200 ↔ ESP32 (UART): 115200 Baud
- USB/Serial (nach außen): 19200 Baud
- BLE: Nordic UART Service

Passende RFID-Aufkleber können bei aliexpress.com bezogen werden.<br>

## Beispielhardware/Bezugsquellen:

AliExpress: (Bitte die richtige Auswahl treffen! Meist ist nur ein Aufklebersatz für unter 10 Euro als default selektiert!)<br>
Mir ist aktuell keine Bezugsquelle aus Deutschland bekannt. Der Chip selber arbeitet mit in der EU zulässigen RFID-Frequenzen (wurde in der Software konfiguriert).<br>
<img src="../images/Invelion_R200_1dbi.png"/>
<br><br>
ESP32-WROOM-32U mit externem Antennenanschluss:<br>
https://amzn.eu/d/12kL505
<br><br>
kleine RFID-Tags (Carrera Hybrid):<br>
https://de.aliexpress.com/item/1005003501876260.html (2515-Wet inlay)
<br>Anmerkung:<br>
Über die configuration.html oder per CMD_WRITE_RFID Befehl ein Tag beschrieben werden.
<br><br>

## Konfiguration

Der RFID-Connector kann auf drei Arten konfiguriert werden:

### 1. USB/Serial-Konfiguration
- Öffne [configuration.html](configuration.html) im Browser (Chrome/Edge)
- Verbinde per USB (19200 Baud)
- Vollständige Konfiguration ohne WiFi-Verbindung möglich
- Ideal für initiales Setup oder Fehlerdiagnose

### 2. BLE-Konfiguration
- Öffne [configuration.html](configuration.html) im Browser (Chrome/Edge)
- Verbinde per Bluetooth Low Energy
- BLE-Name: "RFID-Connector"

### 3. CH Racing Club
- Verbinde per Bluetooth Low Energy
- Am verbundenen Gerät können die Einstellungen vorgenommen werden

### Verfügbare Kommandos (USB/BLE)

Die folgenden Befehle können über USB/Serial oder BLE gesendet werden:

**System & Info:**
- `CMD_GET_VERSION` - Firmware-Version abrufen
- `CMD_GET_MAC` - BLE-MAC-Adresse abrufen
- `CMD_GET_CONFIG` - Komplette Konfiguration abrufen
- `CMD_SAVE_SETTINGS` - Einstellungen im Flash speichern
- `CMD_REBOOT` - ESP32 neu starten

**RFID-Einstellungen:**
- `CMD_GET_POWER` / `CMD_SET_POWER:<10-26>` - Power Level (dBm)
- `CMD_GET_DENSE_MODE` / `CMD_SET_DENSE_MODE:<0|1>` - Dense Mode ein/aus
- `CMD_GET_MIN_LAP_TIME` / `CMD_SET_MIN_LAP_TIME:<ms>` - Minimale Rundenzeit

**WiFi & WebSocket:**
- `CMD_GET_WIFI` / `CMD_SET_WIFI:<ssid>,<password>` - WiFi-Konfiguration
- `CMD_GET_WEBSOCKET_SERVER` / `CMD_SET_WEBSOCKET_SERVER:<url>` - WebSocket-URL

**RFID-Mappings:**
- `CMD_GET_MAPPINGS` - Alle Mappings anzeigen
- `CMD_SET_MAPPING:<tag_id>,<controller_id>` - Mapping setzen
- `CMD_REMOVE_MAPPING:<tag_id>` - Mapping entfernen
- `CMD_CLEAR_MAPPINGS` - Alle Mappings löschen
- `CMD_RESET_RFID_STORAGE` - RFID-Speicher zurücksetzen

**RFID-Tag Programmierung:**
- `CMD_WRITE_RFID:<1-255>` - RFID-Tag mit neuer ID beschreiben

<br>

## Aufbau/Verdrahtung ohne Platine (ESP32-DEV)

**R200 ↔ ESP32 Verbindung (115200 Baud):**
- R200 5V ↔ ESP32 5V
- R200 TX ↔ ESP32 GPIO 17 (RX)
- R200 RX ↔ ESP32 GPIO 16 (TX)
- R200 GND ↔ ESP32 GND

**Optionale Status-LEDs (Kathode über Vorwiderstand):**
- RFID LED: Pin 32 (Anode an 3,3V, Kathode über Vorwiderstand an Pin 32)
- BLE LED: Pin 33 (Anode an 3,3V, Kathode über Vorwiderstand an Pin 33)
- WebSocket LED: Pin 25 (Anode an 3,3V, Kathode über Vorwiderstand an Pin 25)

**Optionaler Reset-Taster:**
- Taster zwischen GND und Pin 23

> **Hinweis:** Bei Verwendung von ESP32-C3 ändern sich die Pin-Zuordnungen (siehe Code).

## Verwendung Adapter-Platine (Plug & Play)

Platinen können auf Anfrage zum Selbstkostenpreis bezogen werden.

<img src="../images/RFID-Connector_Platine_vorne.jpg" width=300px/>
<img src="../images/RFID-Connector_Platine_hinten.jpg" width=300px/>
<img src="../KiCad/RFID-Connector/RFID-Connector_Front.jpg" width=300px/><img src="../KiCad/RFID-Connector/RFID-Connector_Back.jpg" width=300px/>

## Technische Details

- **Firmware-Version:** 2.0.0
- **RFID-Modul:** Inveton R200 (ESP32 ↔ R200: 115200 Baud, Europa-Frequenz konfiguriert)
- **Max. RFID-Tags:** 8 (SmartRace) / 30 (CH Racing Club)
- **Max. Mappings:** 30 Tag-zu-Controller-Zuordnungen
- **Standard Power Level:** 26 dBm (einstellbar 10-26 dBm)
- **Standard Min. Lap Time:** 3000 ms
- **BLE Service:** Nordic UART
- **USB/Serial:** 19200 Baud, 8N1 (PC ↔ ESP32)
- **Preferences-Namespace:** "rfid_connector"

## Bilder


## Montage der 1dbi Antenne als Brücke über Start/Ziel:

<img src="../images/Start_Ziel_Antenne.jpg"/>

## RFID-Aufkleber unter Carrera Hybrid Fahrzeugen
<img src="../images/Sensoren_Auto.jpg" height=200px/>

## Darstellung in SmartRace
<img src="../images/SmartRace.png"/>
