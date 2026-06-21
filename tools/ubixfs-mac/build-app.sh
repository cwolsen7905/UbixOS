#!/bin/sh
# Build "UbixFS Browser.app" with the command-line Swift toolchain (no Xcode).
# Steps: build the UbixFSKit static library (C core + vdev shim + Obj-C facade),
# compile the SwiftUI sources against it, and assemble a .app bundle.
#
#   sh build-app.sh            build ./build/UbixFS Browser.app
#   sh build-app.sh run        build, then open the app
#
# See docs/design/ubixfs-mac-browser-plan.md.
set -e

HERE=$(cd "$(dirname "$0")" && pwd)
KIT="$HERE/UbixFSKit"
SRC="$HERE/UbixFSBrowser"
OUT="$HERE/build"
APP="$OUT/UbixFS Browser.app"
MACOS="$APP/Contents/MacOS"

CORE="$HERE/../../lib/ubixfs_core"
INC="$HERE/../../include/fs/ubixfs"

echo "== building UbixFSKit static library =="
( cd "$KIT" && bmake lib >/dev/null )

echo "== compiling AppKit app =="
rm -rf "$APP"
mkdir -p "$MACOS"

swiftc \
	-O \
	-o "$MACOS/UbixFSBrowser" \
	-import-objc-header "$SRC/UbixFSBrowser-Bridging.h" \
	-I "$KIT" -I "$CORE" -I "$INC" \
	-framework Foundation -framework AppKit \
	-L "$KIT" -lubixfskit \
	"$SRC/main.swift" "$SRC/PoolStore.swift" "$SRC/BrowserViewController.swift"

cp "$SRC/Info.plist" "$APP/Contents/Info.plist"
printf 'APPL????' > "$APP/Contents/PkgInfo"

# Ad-hoc sign so the window server accepts the bundle locally.
codesign --force --sign - "$APP" >/dev/null 2>&1 || true

echo "built: $APP"

if [ "$1" = "run" ]; then
	open "$APP"
fi
