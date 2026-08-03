#!/bin/bash
# Extrahiert die Firmware-Binaries aus den script-flasher-ZIPs in web-flasher/firmware/.
# Nach einem neuen Firmware-Build (aktualisierte ZIPs in script-flasher/) einfach erneut ausführen.
set -euo pipefail

cd "$(dirname "$0")"

for z in ../script-flasher/*.zip; do
  name=$(basename "$z" .zip)
  mkdir -p "firmware/$name"
  unzip -j -o -q "$z" "*/bin/*.bin" -d "firmware/$name/"
  echo "aktualisiert: firmware/$name"
done
