#!/usr/bin/env bash
# The release's package on Linux, as release-package.bat makes it on Windows: the exes, SDL3.dll and the compiler
# folder the game builds its code with on the player's PC (game_code.cpp), no game code. One package serves the USA
# and the PAL disc. Run scripts/build-win.sh first.
#   Usage: scripts/release-package.sh [zip]      (default NightFire-package.zip, launcher/game.json's "package")
#   Writes the zip and <zip>.json (size, sha256) into .local/release/, for the tag's release.
# The compiler folder: xbe_recomp.exe cross-compiled here, clang.exe and lld-link.exe from LLVM's Windows release
# (launcher/toolchain.json's, checked against its sha256), all three on the static C runtime (no DLLs beside them),
# the configs, the XDK layouts, the headers the generated code includes and the exes' import libraries.
set -euo pipefail
cd "$(dirname "$0")/.."

ZIP_NAME=${1:-NightFire-package.zip}
EXES=(Nightfire NightfireDriving)
WIN=.local/win
WSDK=.local/win-sdk
MSVC=$WSDK/msvc
OUT=.local/release
STAGE=$OUT/package
OBJ=$OUT/obj

for exe in "${EXES[@]}"; do
  for f in "$WIN/$exe.exe" "$WIN/$exe.lib"; do
    [ -f "$f" ] || { echo "release-package: $f is missing: run scripts/build-win.sh first"; exit 2; }
  done
done
[ -f "$WIN/SDL3.dll" ] || { echo "release-package: $WIN/SDL3.dll is missing: run scripts/build-win.sh first"; exit 2; }
[ -f "$MSVC/sdk/lib/um/x86_64/kernel32.lib" ] || { echo "release-package: no $MSVC: run scripts/build-win.sh first"; exit 2; }
export PATH="$(realpath "$WSDK/bin"):$PATH"

# 1. LLVM's Windows release, once (about 850 MB), unpacked to the files the compiler folder takes
LLVM_URL=$(python3 -c 'import json; t = [t for t in json.load(open("launcher/toolchain.json"))["tools"] if t["id"] == "llvm"][0]; print(t["url"]); print(t["sha256"])')
LLVM_SHA=$(echo "$LLVM_URL" | sed -n 2p)
LLVM_URL=$(echo "$LLVM_URL" | sed -n 1p)
LLVM_TAR=$WSDK/$(basename "${LLVM_URL//%2B/+}")
LLVM=$WSDK/llvm-win
if [ ! -f "$LLVM/bin/clang.exe" ]; then
  if [ ! -f "$LLVM_TAR" ] || ! echo "$LLVM_SHA  $LLVM_TAR" | sha256sum -c --status; then
    echo "release-package: fetching $(basename "$LLVM_TAR") (once, about 850 MB)..."
    curl -fL -o "$LLVM_TAR" "$LLVM_URL"
  fi
  echo "$LLVM_SHA  $LLVM_TAR" | sha256sum -c --status || { echo "release-package: $LLVM_TAR fails its sha256"; exit 1; }
  rm -rf "$LLVM" && mkdir -p "$LLVM"
  tar -xJf "$LLVM_TAR" -C "$LLVM" --strip-components=1 --wildcards '*/bin/clang.exe' '*/bin/lld-link.exe' '*/bin/lld.exe'
  # lld-link is lld under another name; the release's may be a link to lld.exe
  [ -s "$LLVM/bin/lld-link.exe" ] || cp -f "$LLVM/bin/lld.exe" "$LLVM/bin/lld-link.exe"
fi

# 2. xbe_recomp.exe, the same sources as the Linux tool's, for Windows
DEPS=build/linux/_deps
ZYDIS=$DEPS/zydis-src
TOML=$DEPS/tomlplusplus-src/include
[ -d "$ZYDIS/src" ] && [ -d "$TOML" ] || { echo "release-package: no $ZYDIS or $TOML: run scripts/linux-tools.sh first"; exit 2; }
RES_INC="$(clang -print-resource-dir)/include"
WININC="-isystem $RES_INC -isystem $MSVC/crt/include -isystem $MSVC/sdk/include/ucrt -isystem $MSVC/sdk/include/um -isystem $MSVC/sdk/include/shared"
WINLIB="-Wl,/libpath:$MSVC/crt/lib/x86_64 -Wl,/libpath:$MSVC/sdk/lib/ucrt/x86_64 -Wl,/libpath:$MSVC/sdk/lib/um/x86_64"
ZYINC="-I$ZYDIS/include -I$ZYDIS/src -I$ZYDIS/dependencies/zycore/include -DZYDIS_STATIC_BUILD -DZYCORE_STATIC_BUILD"
TARGET="--target=x86_64-pc-windows-msvc -fms-compatibility-version=19.40 -O2 -D_CRT_SECURE_NO_WARNINGS -Wno-everything"
rm -rf "$OBJ" && mkdir -p "$OBJ"
objs=()
for f in "$ZYDIS"/src/*.c "$ZYDIS"/dependencies/zycore/src/*.c "$ZYDIS"/dependencies/zycore/src/API/*.c; do
  o="$OBJ/$(echo "$f" | tr / _).obj"; objs+=("$o")
  clang $TARGET $WININC $ZYINC -c "$f" -o "$o" &
  [ "$(jobs -r | wc -l)" -ge "$(nproc)" ] && wait -n
done
for f in sdk/tools/recomp/*.cpp; do
  o="$OBJ/$(echo "$f" | tr / _).obj"; objs+=("$o")
  clang++ -std=c++20 $TARGET $WININC $ZYINC -isystem "$TOML" -Isdk/runtime -Isdk/tools/recomp -c "$f" -o "$o" &
  [ "$(jobs -r | wc -l)" -ge "$(nproc)" ] && wait -n
done
wait
for o in "${objs[@]}"; do [ -f "$o" ] || { echo "release-package: compiling xbe_recomp.exe failed"; exit 1; }; done
clang++ --target=x86_64-pc-windows-msvc -fuse-ld=lld $WINLIB "${objs[@]}" -o "$OBJ/xbe_recomp.exe"

# 3. the package's files
rm -rf "$STAGE"
C=$STAGE/compiler
mkdir -p "$C/config" "$C/include/cpu" "$C/include/freestanding"
for exe in "${EXES[@]}"; do
  cp "$WIN/$exe.exe" "$STAGE/"
  cp "$WIN/$exe.lib" "$C/"
done
cp "$WIN/SDL3.dll" "$STAGE/"
[ -f "$WIN/d3dcompiler_47.dll" ] && cp "$WIN/d3dcompiler_47.dll" "$STAGE/"
cp "$OBJ/xbe_recomp.exe" "$C/"
cp "$LLVM/bin/clang.exe" "$LLVM/bin/lld-link.exe" sdk/tools/LLVM-LICENSE.txt "$C/"
cp config/*.toml "$C/config/"
cp sdk/runtime/cpu/x86_ops.h sdk/runtime/cpu/x86_context.h "$C/include/cpu/"
cp -r sdk/runtime/cpu/freestanding/. "$C/include/freestanding/"
for layout in sdk/xdk/*/layout.toml; do
  build=$(basename "$(dirname "$layout")")
  mkdir -p "$C/xdk/$build" && cp "$layout" "$C/xdk/$build/"
done

# 4. the zip and its description
ZIP=$OUT/$ZIP_NAME
rm -f "$ZIP" "$ZIP.json"
python3 - "$STAGE" "$ZIP" <<'EOF'
import os, sys, zipfile
stage, out = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
    for root, dirs, files in os.walk(stage):
        dirs.sort()
        for name in sorted(files):
            path = os.path.join(root, name)
            z.write(path, os.path.relpath(path, stage).replace(os.sep, "/"))
EOF
printf '{"size": %s, "sha256": "%s"}\n' "$(stat -c %s "$ZIP")" "$(sha256sum "$ZIP" | cut -d' ' -f1)" > "$ZIP.json"
echo "$ZIP: $(stat -c %s "$ZIP") bytes"
cat "$ZIP.json"
