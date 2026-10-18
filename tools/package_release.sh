#!/bin/sh
# Packages a built firmware for release: a complete image to flash at 0x0 (first install) and the bare app
# image to flash at 0x10000 (update, keeps the saved settings).
#   tools/package_release.sh <pio env> <name> <output dir>
# Run `pio run -e <env>` first. Needs esptool (pip install esptool).
set -eu
env=$1 name=$2 out=$3
build=.pio/build/$env
boot_app0=$(find "${PLATFORMIO_CORE_DIR:-$HOME/.platformio}/packages" -name boot_app0.bin | head -n 1)
[ -n "$boot_app0" ] || { echo "boot_app0.bin not found" >&2; exit 1; }

mkdir -p "$out"
cp "$build/firmware.bin" "$out/$name-update.bin"
# Same layout as the Arduino framework's own "factory" image (ESP32-S3: bootloader at 0x0).
esptool --chip esp32s3 merge-bin -o "$out/$name-full.bin" --flash-mode keep --flash-size keep \
  0x0 "$build/bootloader.bin" 0x8000 "$build/partitions.bin" 0xe000 "$boot_app0" 0x10000 "$build/firmware.bin"
