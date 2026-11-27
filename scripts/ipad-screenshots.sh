#!/bin/bash
# Bygger Stagebox för iPad-simulatorn, startar appen på iPad Pro 13" och iPad mini och tar
# skärmdumpar (liggande). Körs i CI (macOS) – kräver Xcode med iOS-simulator.
#
#   ./scripts/ipad-screenshots.sh   -> screenshots/ipad-*.png
set -euo pipefail
cd "$(dirname "$0")/.."

BUILD_DIR="build-ios-sim"
OUT="screenshots"
BUNDLE_ID="se.flatmate.stagebox"

cmake -B "$BUILD_DIR" -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_DEPLOYMENT_TARGET=16.0 \
      -DCMAKE_OSX_SYSROOT=iphonesimulator -DCMAKE_OSX_ARCHITECTURES=arm64
xcodebuild -project "$BUILD_DIR/Stagebox.xcodeproj" -scheme Stagebox_Standalone -configuration Debug \
           -sdk iphonesimulator -destination 'generic/platform=iOS Simulator' \
           CODE_SIGNING_ALLOWED=NO -quiet build
APP=$(find "$BUILD_DIR" -path "*Standalone*" -name "Stagebox.app" -type d | head -1)
[ -d "$APP" ] || { echo "Hittar inte Stagebox.app för simulatorn"; exit 1; }
echo "App: $APP"

mkdir -p "$OUT"
xcrun simctl list runtimes -j > /tmp/runtimes.json
RUNTIME=$(python3 -c "import json; r=[x for x in json.load(open('/tmp/runtimes.json'))['runtimes'] if x['platform']=='iOS' and x['isAvailable']]; print(r[-1]['identifier'])")
for MODEL in "iPad Pro 13-inch" "iPad mini"; do
    # Bara modeller som den installerade iOS-versionen stöder (annars "Incompatible device").
    TYPE=$(python3 -c "
import json, sys
m, rid = sys.argv[1], sys.argv[2]
rt = [x for x in json.load(open('/tmp/runtimes.json'))['runtimes'] if x['identifier'] == rid][0]
t = [d for d in rt.get('supportedDeviceTypes', []) if d['name'].startswith(m)]
print(t[-1]['identifier'] if t else '')" "$MODEL" "$RUNTIME")
    [ -n "$TYPE" ] || { echo "Ingen simulator för $MODEL – hoppar över"; continue; }
    NAME="ipad-$(echo "$MODEL" | tr ' A-Z' '-a-z' | tr -cd 'a-z0-9-')"
    UDID=$(xcrun simctl create "$NAME" "$TYPE" "$RUNTIME") || { echo "Kunde inte skapa $MODEL – hoppar över"; continue; }
    xcrun simctl boot "$UDID"
    xcrun simctl bootstatus "$UDID" -b
    xcrun simctl install "$UDID" "$APP"
    xcrun simctl privacy "$UDID" grant microphone "$BUNDLE_ID" || true
    xcrun simctl launch "$UDID" "$BUNDLE_ID"
    sleep 20
    xcrun simctl io "$UDID" screenshot "$OUT/$NAME.png"
    xcrun simctl shutdown "$UDID"
    xcrun simctl delete "$UDID"
    echo "Skärmdump: $OUT/$NAME.png"
done
