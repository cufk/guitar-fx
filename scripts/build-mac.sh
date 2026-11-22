#!/bin/bash
# Bygger Stagebox.app och Stagebox-<version>.dmg på en Mac.
#
#   ./scripts/build-mac.sh            # universell app (Apple Silicon + Intel)
#   ARCHS=arm64 ./scripts/build-mac.sh  # bara Apple Silicon (snabbare)
#
# Kräver Xcode (med kommandoradsverktyg) och CMake. Appen signeras "ad hoc" (utan
# Apple-utvecklarkonto). Första gången den öppnas på en annan Mac: högerklicka →
# Öppna, eller kör `xattr -dr com.apple.quarantine /Applications/Stagebox.app`.
set -euo pipefail
cd "$(dirname "$0")/.."

ARCHS="${ARCHS:-arm64;x86_64}"
BUILD_DIR="build-release"
DIST_DIR="dist"
VERSION=$(sed -n 's/^project(Stagebox VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)

echo "==> Konfigurerar (version $VERSION, arkitekturer: $ARCHS)"
cmake -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="$ARCHS"

echo "==> Bygger"
cmake --build "$BUILD_DIR" --config Release -j "$(sysctl -n hw.ncpu)"

echo "==> Kör tester"
ctest --test-dir "$BUILD_DIR" -C Release --output-on-failure

APP="$BUILD_DIR/app/Stagebox_artefacts/Release/Standalone/Stagebox.app"
[ -d "$APP" ] || { echo "Hittar inte $APP"; exit 1; }

echo "==> Signerar (ad hoc)"
codesign --force --deep --sign - "$APP"
codesign --verify --deep --strict "$APP"

echo "==> Skapar DMG"
rm -rf "$DIST_DIR" && mkdir -p "$DIST_DIR/dmg"
cp -R "$APP" "$DIST_DIR/dmg/"
ln -s /Applications "$DIST_DIR/dmg/Applications"   # dra appen hit för att installera
hdiutil create -volname "Stagebox" -srcfolder "$DIST_DIR/dmg" -ov -format UDZO "$DIST_DIR/Stagebox-$VERSION.dmg" >/dev/null
cp -R "$APP" "$DIST_DIR/"
rm -rf "$DIST_DIR/dmg"

echo
echo "Klart:"
echo "  $DIST_DIR/Stagebox.app"
echo "  $DIST_DIR/Stagebox-$VERSION.dmg"
