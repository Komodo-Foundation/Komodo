#!/usr/bin/env bash
#
# build-deb.sh — build komodo_<version>_<arch>.deb from this checkout.
#
#   packaging/build-deb.sh            writes dist/komodo_<version>_amd64.deb
#   sudo apt install ./dist/komodo_*.deb
#
# The package ships a prebuilt compiler (kmc) under /usr/lib/komodo, so users
# never write there: caches go in ~/.cache/komodo, projects in ./.komodo.
set -euo pipefail
here="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$here"
version="$(cat VERSION)"
arch=amd64
out="$here/dist"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT

./build-self.sh >/dev/null

lib="$stage/usr/lib/komodo"
doc="$stage/usr/share/doc/komodo"
mkdir -p "$lib" "$doc" "$stage/usr/bin" "$stage/DEBIAN"
install -m 755 komodo kmrepl kmc "$lib/"
install -m 644 VERSION "$lib/"
cp -r stdlib tools "$lib/"
chmod 755 "$lib/tools/km-bind"
ln -s ../lib/komodo/komodo "$stage/usr/bin/komodo"

cp -r docs examples README.md CHANGELOG.md design.md "$doc/"
find "$doc/examples" -type f ! -name '*.km' ! -name '*.expected' -delete
cp LICENSE "$doc/copyright"
gzip -9 -n "$doc/CHANGELOG.md"

size="$(du -sk "$stage/usr" | cut -f1)"
cat > "$stage/DEBIAN/control" <<CTL
Package: komodo
Version: $version
Section: devel
Priority: optional
Architecture: $arch
Installed-Size: $size
Depends: nasm, gcc, libc6-dev, libgc1, libgc-dev
Recommends: pkg-config, g++
Suggests: nodejs, emscripten
Maintainer: Lukas Perkins <lukasperks@gmail.com>
Homepage: https://github.com/lrperks/Komodo
Description: self-hosting systems language with seamless C/C++ interop
 Komodo is a small statically-typed language that compiles to native x86-64
 code (also portable C and WebAssembly). Running 'komodo' in a project builds
 and runs it; C and C++ libraries are used with 'import zlib' or by dropping
 sources in native/, and Komodo code can be exported to C, C++ and Python.
CTL

chmod -R go-w "$stage"; chmod 755 "$stage" "$stage/usr"
mkdir -p "$out"
dpkg-deb --root-owner-group --build "$stage" "$out/komodo_${version}_${arch}.deb" >/dev/null
echo "built $out/komodo_${version}_${arch}.deb"
