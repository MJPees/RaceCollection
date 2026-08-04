# StartingLightsDisplay

Eine optisch ansprechende Startampel, einfach zu bauen, basierend auf einem **ESP32-C6** mit kleinem Display (ST7789), SD-Karte und RGB-LED. Inklusive Werbebannern im Idle-Modus. Die Anzeige kann um 180° gedreht (gespiegelt) werden, sodass das Gerät mit USB-Anschluss links oder rechts montiert werden kann.

<img src="../images/StartingLightsDisplay_1.jpg" height=150/> <img src="../images/StartingLightsDisplay_2.jpg" height=150/> <img src="../images/StartingLightsDisplay_3.jpg" height=150/>

## Firmware flashen

- **Web-Flasher** (empfohlen): [https://mjpees.github.io/RaceCollection/](https://mjpees.github.io/RaceCollection/) – direkt aus dem Browser (Chrome/Edge), keine Installation nötig.
- **Skript-Flasher**: [Flash-Anleitungen](../script-flasher/README.md) für Windows, Linux und macOS.

## SD-Karte vorbereiten

Den Inhalt des Ordners [`sdcard/`](./sdcard/) auf eine FAT32-formatierte SD-Karte kopieren (Ampelbilder, Statusbilder sowie der Ordner `sponsor/`). Eigene Werbebanner als PNG im Ordner `sponsor/` ablegen – sie werden im Idle-Modus nach 30 Sekunden im 3-Sekunden-Takt durchgewechselt.

## Konfiguration & Steuerung über BLE

Die Konfiguration erfolgt über **BLE** (Gerätename `Starting-Light`, Nordic-UART-Schema). Der USB-Anschluss liefert nur Log-Ausgaben – Kommandos werden ausschließlich über BLE entgegengenommen.

Am einfachsten geht die Konfiguration über den **[CH-Racing-Club](https://carrera-hybrid-racing-club.de)**: Dort das Starting-Light per BLE verbinden und WLAN, WebSocket-Server sowie die Display-Ausrichtung direkt einstellen. Alternativ lassen sich die folgenden Kommandos mit jedem BLE-UART-Client senden.

| | UUID |
|---|---|
| Service | `6E400001-B5A3-F393-E0A9-E50E24DCCA9F` |
| RX (Write) | `6E400002-B5A3-F393-E0A9-E50E24DCCA9F` |
| TX (Read/Notify) | `6E400003-B5A3-F393-E0A9-E50E24DCCA9F` |

### Kommandos

| Kommando | Beschreibung |
|---|---|
| `CMD_GET_CONFIG` | Aktuelle Konfiguration abfragen |
| `CMD_GET_WIFI` | WLAN-SSID und -Passwort abfragen |
| `CMD_SET_WIFI:<ssid>,<password>` | WLAN-Zugangsdaten setzen |
| `CMD_GET_WEBSOCKET_SERVER` | WebSocket-Server-URL abfragen |
| `CMD_SET_WEBSOCKET_SERVER:<url>` | WebSocket-Server-URL setzen |
| `CMD_SAVE_SETTINGS` | Einstellungen dauerhaft speichern |
| `CMD_REBOOT` | Gerät neu starten |
| `CMD_GET_MAC` | BLE-MAC-Adresse abfragen |
| `CMD_GET_VERSION` | Firmware-Version abfragen |
| `CMD_STATUS_SET:<status>` | Status setzen: `idle`, `prepare_for_start`, `starting`, `running`, `suspended`, `ended` |
| `CMD_COUNTDOWN_SET:<pattern>` | Ampellichter setzen, 7-stelliges Muster (z. B. `1111100`) |
| `CMD_SET_ROTATION:<0\|1>` | Display um 180° drehen (`1` = USB-Anschluss rechts) |
| `CMD_SET_IDLE` | Status auf `idle` setzen und Idle-Bild anzeigen |
| `CMD_YELLOW_FLAG` | Gelbe Flagge blinken (im Status `suspended`) |
| `CMD_RED_FLAG` | Rote Flagge blinken (im Status `suspended`) |
| `CMD_FINISH_RACE` | Zieldurchfahrt anzeigen |

## WLAN & WebSocket (optional)

Sind WLAN und ein WebSocket-Server konfiguriert (`CMD_SET_WIFI`, `CMD_SET_WEBSOCKET_SERVER`), verbindet sich das Display mit der Rennmanagement-Software und reagiert auf deren Renn-Events (Startsequenz, virtuelles Safety-Car, Rennende, Reset). Bricht die Verbindung ab, versucht die Firmware automatisch alle 10 Sekunden einen Reconnect.

## Bedienung am Gerät

Die **BOOT-Taste** schaltet die Display-Helligkeit zyklisch in 10-%-Schritten durch (100 % → zurück auf 10 %).
