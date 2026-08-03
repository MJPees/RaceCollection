# Web-Flasher

Firmware für alle RaceCollection-Geräte direkt aus dem Browser flashen – ohne esptool-Installation. Basiert auf [ESP Web Tools](https://esphome.github.io/esp-web-tools/) und der Web-Serial-API.

## Voraussetzungen

- **Browser**: Google Chrome oder Microsoft Edge (Desktop). Firefox und Safari unterstützen kein Web Serial.
- **Sicherer Kontext**: Die Seite muss über `https://` oder `localhost` aufgerufen werden – ein Öffnen der `index.html` direkt aus dem Dateisystem (`file://`) funktioniert nicht.
- **Internetzugang**: Die ESP-Web-Tools-Bibliothek wird von unpkg.com geladen.

## Nutzung

### Lokal

```bash
cd web-flasher
python3 -m http.server 8000
```

Dann im Browser <http://localhost:8000> öffnen, Gerät per USB verbinden und auf **Connect** klicken.

### Über GitHub Pages

Der Workflow [deploy-web-flasher.yml](../.github/workflows/deploy-web-flasher.yml) veröffentlicht diesen Ordner bei jedem Push auf `main` (mit Änderungen unter `web-flasher/`) automatisch per GitHub Pages. Der Flasher ist dann für alle erreichbar unter:

**<https://mjpees.github.io/RaceCollection/>**

Voraussetzung (einmalig, durch den Repo-Owner): In den Repo-Settings unter **Settings → Pages** die Source auf **„GitHub Actions"** stellen, falls der Workflow Pages nicht selbst aktivieren konnte.

## Gerät wird nicht erkannt?

- Anderen USB-Anschluss/Kabel probieren (Datenkabel, kein reines Ladekabel).
- **BOOT**-Taste gedrückt halten, kurz **RESET** drücken, dann erneut verbinden (Download-Modus).
- Ggf. Treiber für den USB-Serial-Chip (CP210x/CH34x) installieren.

## Aufbau

```
web-flasher/
├── index.html          # Flasher-Seite mit einem Install-Button pro Firmware-Variante
├── manifests/          # ESP-Web-Tools-Manifeste (Chip-Familie, Binaries, Flash-Offsets)
├── firmware/           # Firmware-Binaries, extrahiert aus script-flasher/*.zip
└── update-firmware.sh  # Binaries nach neuem Build aus den ZIPs neu extrahieren
```

Die Flash-Offsets in den Manifesten entsprechen den Skript-Flashern: Bootloader bei `0x0` (ESP32-C3/S3/C6) bzw. `0x1000` (klassischer ESP32), Partitionstabelle `0x8000`, boot_app0 `0xE000`, Anwendung `0x10000`.

## Firmware aktualisieren

Auf `main` passiert das automatisch: Jeder Build-Workflow (`.github/workflows/build-*.yml`) kopiert die frisch kompilierten Binaries im Schritt **„Update web-flasher firmware"** nach `firmware/`, committet sie und stößt anschließend den Pages-Deploy an.

Manuell (z. B. auf einem Feature-Branch oder zum lokalen Testen) lassen sich die Binaries aus den `script-flasher`-ZIPs übernehmen:

```bash
./web-flasher/update-firmware.sh
```

Kommt eine neue Firmware-Variante hinzu: Manifest in `manifests/` anlegen, Button in `index.html` ergänzen und den Update-Schritt im zugehörigen Build-Workflow eintragen.

## Hinweis StartingLightsDisplay

Der Web-Flasher überträgt nur die Firmware. Die Anzeigebilder für das StartingLightsDisplay liegen auf der SD-Karte und müssen weiterhin manuell aus [StartingLightsDisplay/sdcard/](../StartingLightsDisplay/sdcard/) auf die SD-Karte kopiert werden.
