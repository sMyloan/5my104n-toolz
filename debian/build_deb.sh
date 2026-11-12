#!/bin/bash
set -e

VERSION="1.1.29"
ARCH="amd64"
PKG="5my104n-toolz_${VERSION}_${ARCH}.deb"

echo "=== Building DEB ==="

cd ..
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --parallel
cp 5my104n_toolz ../debian/usr/local/bin/

cd ../debian
chmod 755 DEBIAN/postinst DEBIAN/prerm DEBIAN/postrm
chmod 755 usr/local/bin/5my104n_toolz
chmod 644 usr/share/applications/5my104n_toolz.desktop
chmod 644 usr/share/mime/packages/5my104n_toolz.xml
chmod 644 usr/share/icons/hicolor/128x128/apps/5my104n_toolz.png
chmod 644 usr/share/icons/hicolor/64x64/apps/smy-file.png

cd ..
dpkg-deb --build debian "$PKG"

echo "=== Created: $PKG ==="
echo "Install: sudo dpkg -i $PKG"