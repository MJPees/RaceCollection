#!/bin/bash
# Extrahiert die Firmware-Binaries aus den script-flasher-ZIPs in web-flasher/firmware/.
# Nach einem neuen Firmware-Build (aktualisierte ZIPs in script-flasher/) einfach erneut ausführen.
set -euo pipefail
shopt -s nullglob # ohne ZIPs bleibt die Schleife leer statt mit dem Literal-Glob zu laufen

cd "$(dirname "$0")"

found=0
for z in ../script-flasher/*.zip; do
  found=1
  name=$(basename "$z" .zip)
  mkdir -p "firmware/$name"
  unzip -j -o -q "$z" "*/bin/*.bin" -d "firmware/$name/"
  echo "aktualisiert: firmware/$name"
done

if [ "$found" -eq 0 ]; then
  echo "Keine ZIPs in ../script-flasher/ gefunden." >&2
  exit 1
fi
