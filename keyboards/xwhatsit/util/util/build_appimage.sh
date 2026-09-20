#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
UTIL_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$UTIL_ROOT/build"
APPDIR="$BUILD_DIR/AppDir"

echo "Building Cap-Util AppImage..."

rm -rf "$APPDIR"
mkdir -p \
    "$APPDIR/usr/bin" \
    "$APPDIR/usr/share/applications" \
    "$APPDIR/usr/share/icons/hicolor/256x256/apps"

command cp -f "$SCRIPT_DIR/util" "$APPDIR/usr/bin/xwhatsit-util"
command cp -f "$SCRIPT_DIR/Cap-Util.png" "$APPDIR/Cap-Util.png"
command cp -f "$SCRIPT_DIR/Cap-Util.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/Cap-Util.png"

ln -sfn Cap-Util.png "$APPDIR/.DirIcon"

cat > "$APPDIR/usr/share/applications/xwhatsit-util.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=Cap-Util
Exec=xwhatsit-util
Icon=Cap-Util
Categories=Utility;
Terminal=false
StartupWMClass=xwhatsit-util
DESKTOP

ln -sfn usr/share/applications/xwhatsit-util.desktop "$APPDIR/xwhatsit-util.desktop"

rm -f "$BUILD_DIR/Cap-Util-x86_64.AppImage"

ARCH=x86_64 "$SCRIPT_DIR/appimagetool-x86_64.AppImage" \
    "$APPDIR" \
    "$BUILD_DIR/Cap-Util-x86_64.AppImage"

rm -rf "$APPDIR"

echo
echo "Built:"
echo "$BUILD_DIR/Cap-Util-x86_64.AppImage"
