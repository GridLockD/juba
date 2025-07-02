#!/bin/bash
set -e

PROJECT_DIR=$(pwd)
LINUX_DIR="$PROJECT_DIR/linux"
BUILD_DIR="$LINUX_DIR/juba-bin"
DEB_PKG_DIR="$LINUX_DIR/juba-deb"

echo "Building with qmake and make"
qmake -r -spec linux-g++ CONFIG+=release
make clean
make -j$(nproc)

echo "Preparing Debian package structure"
rm -rf "$DEB_PKG_DIR"
mkdir -p "$DEB_PKG_DIR/DEBIAN" "$DEB_PKG_DIR/usr/bin" "$DEB_PKG_DIR/usr/share/icons/hicolor/48x48/apps" "$DEB_PKG_DIR/usr/share/applications"

cp ./juba "$BUILD_DIR/"
chmod +x "$BUILD_DIR/juba"
cp "$BUILD_DIR/juba" "$DEB_PKG_DIR/usr/bin/"
cp "$LINUX_DIR/juba.png" "$DEB_PKG_DIR/usr/share/icons/hicolor/48x48/apps/"
cp "$LINUX_DIR/juba.desktop" "$DEB_PKG_DIR/usr/share/applications/"
cp "$LINUX_DIR/control" "$DEB_PKG_DIR/DEBIAN/"

echo "Building Debian package"
dpkg-deb --build "$DEB_PKG_DIR"

echo "Creating tar.gz archive for USB"
tar czvf juba_usb_distribution_linux.tar.gz linux

echo "Done."
