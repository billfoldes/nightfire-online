#!/usr/bin/env bash
# Cross-compiles the Windows executables (Nightfire.exe and NightfireDriving.exe) on Linux using clang++, lld-link
# and xwin (the MSVC CRT and Windows SDK headers/libs, fetched once into .local/win-sdk).
#   Usage:
#     scripts/build-win.sh [path/to/your/NightFire/play]
#   Outputs:
#     .local/win/Nightfire.exe
#     .local/win/NightfireDriving.exe
#     .local/win/SDL3.dll
#   When a target game folder is given (the folder with Nightfire.exe and Nightfire.dll that you run in Steam/Proton),
#   the new executables and SDL3.dll are installed there (backing up the originals to *.orig.exe once) and the
#   existing Nightfire.dll and NightfireDriving.dll are touched so they load at once without rebuilding.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/.."

for dir in "$@"; do
  if [ ! -d "$dir" ]; then
    echo "build-win: no folder at $dir"
    exit 2
  fi
done

GEN_GAME=${GEN_GAME:-generated/game}
GEN_DRIVING=${GEN_DRIVING:-generated/driving}
[ -f "$GEN_GAME/image_symbols.h" ] && [ -f "$GEN_GAME/imports.def" ] || {
  echo "build-win: no $GEN_GAME/imports.def; run scripts/linux-tools.sh and scripts/recompile.sh first"
  exit 2
}
if [ ! -f "$GEN_DRIVING/image_symbols.h" ] || [ ! -f "$GEN_DRIVING/imports.def" ]; then
  echo "build-win: generating $GEN_DRIVING (once)"
  cmake --build build/linux --target recompile_NightfireDriving
fi

WSDK=.local/win-sdk
OUT=.local/win
mkdir -p "$WSDK/bin" "$OUT/obj/common" "$OUT/obj/game" "$OUT/obj/driving"

# 1. lld-link: on PATH, or a symlink to lld / ld.lld / rust-lld
if ! command -v lld-link >/dev/null 2>&1 && [ ! -x "$WSDK/bin/lld-link" ]; then
  for cand in "$(command -v ld.lld 2>/dev/null || true)" "$(command -v lld 2>/dev/null || true)" \
              /usr/bin/ld.lld /usr/bin/lld /usr/local/rustup/toolchains/*/lib/rustlib/*/bin/rust-lld; do
    if [ -x "$cand" ]; then
      ln -sf "$cand" "$WSDK/bin/lld-link"
      ln -sf "$cand" "$WSDK/bin/ld.lld"
      break
    fi
  done
fi
export PATH="$(realpath "$WSDK/bin"):$PATH"
command -v lld-link >/dev/null 2>&1 || {
  echo "build-win: lld-link not found; install lld (on Artix/Arch: sudo pacman -S lld)"
  exit 2
}

# 2. MSVC CRT + Windows SDK via xwin (fetched once into .local/win-sdk/msvc)
MSVC="$WSDK/msvc"
if [ ! -f "$MSVC/sdk/lib/um/x86_64/d3d12.lib" ]; then
  echo "build-win: fetching MSVC CRT & Windows SDK into $MSVC (once, ~1 min)..."
  if [ ! -x "$WSDK/bin/xwin" ]; then
    curl -fsSL https://github.com/Jake-Shadle/xwin/releases/download/0.6.5/xwin-0.6.5-x86_64-unknown-linux-musl.tar.gz \
      | tar -xz -C "$WSDK/bin" --strip-components=1
  fi
  "$WSDK/bin/xwin" --accept-license --arch x86_64 splat --output "$MSVC" --copy
fi

# 3. Prebuilt SDL3 VC devel package (fetched once into .local/win-sdk/SDL3-3.4.16)
SDL="$WSDK/SDL3-3.4.16"
if [ ! -f "$SDL/lib/x64/SDL3.lib" ]; then
  echo "build-win: fetching SDL3-devel-3.4.16-VC.zip (once)..."
  curl -fsSL https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-devel-3.4.16-VC.zip -o "$WSDK/sdl3.zip"
  unzip -q -o "$WSDK/sdl3.zip" -d "$WSDK"
  rm -f "$WSDK/sdl3.zip"
fi

# 4. ImGui + toml++ headers (from build/linux/_deps, or fetched if missing)
DEPS=build/linux/_deps
mkdir -p "$DEPS"
[ -d "$DEPS/tomlplusplus-src/include" ] || git clone --depth 1 --branch v3.4.0 https://github.com/marzer/tomlplusplus.git "$DEPS/tomlplusplus-src"
[ -d "$DEPS/imgui-src" ] || git clone --depth 1 --branch v1.91.0 https://github.com/ocornut/imgui.git "$DEPS/imgui-src"

CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)"
JOBS=$(nproc)
RES_INC="$(clang -print-resource-dir)/include"
WININC="-isystem $RES_INC -isystem $MSVC/crt/include -isystem $MSVC/sdk/include/ucrt -isystem $MSVC/sdk/include/um -isystem $MSVC/sdk/include/shared -isystem $MSVC/sdk/include/winrt"
WINLIB="-Wl,/libpath:$MSVC/crt/lib/x86_64 -Wl,/libpath:$MSVC/sdk/lib/ucrt/x86_64 -Wl,/libpath:$MSVC/sdk/lib/um/x86_64"
IMGUI="$DEPS/imgui-src"
TOML="$DEPS/tomlplusplus-src/include"

COMMON_FLAGS="-std=c++20 --target=x86_64-pc-windows-msvc -fms-compatibility-version=19.40 -O2 -march=x86-64-v2 -fno-strict-aliasing $WININC -Isdk/runtime -Isrc -isystem $IMGUI -isystem $TOML -isystem $SDL/include -DXBE_GAME_DLL -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH -DUNICODE -D_UNICODE -D_CRT_SECURE_NO_WARNINGS -DIMGUI_IMPL_WIN32_DISABLE_GAMEPAD -Wno-everything"

newest_hdr=$(find sdk/runtime src -name '*.h' -printf '%T@ %p\n' | sort -n | tail -1 | cut -d' ' -f2)

# Shared objects (do not depend on image_symbols.h or hook_exports.h)
COMMON_SRCS=(
  sdk/runtime/app/handover.cpp
  sdk/runtime/app/overlay.cpp
  sdk/runtime/app/screenshot.cpp
  sdk/runtime/app/settings.cpp
  sdk/runtime/app/splash.cpp
  sdk/runtime/audio/*.cpp
  sdk/runtime/core/*.cpp
  sdk/runtime/cpu/*.cpp
  sdk/runtime/gpu/*.cpp
  sdk/runtime/kernel/*.cpp
  sdk/runtime/xbe/*.cpp
  src/common/*.cpp
  "$IMGUI"/imgui.cpp
  "$IMGUI"/imgui_draw.cpp
  "$IMGUI"/imgui_tables.cpp
  "$IMGUI"/imgui_widgets.cpp
  "$IMGUI"/backends/imgui_impl_win32.cpp
  "$IMGUI"/backends/imgui_impl_dx11.cpp
  "$IMGUI"/backends/imgui_impl_dx12.cpp
)

common_objs=()
for f in ${COMMON_SRCS[@]}; do
  [ -f "$f" ] || continue
  o="$OUT/obj/common/$(echo "$f" | tr / _).obj"
  common_objs+=("$o")
  if [ "$o" -nt "$f" ] && [ "$o" -nt "$newest_hdr" ]; then continue; fi
  echo "compile (win) $f"
  rm -f "$o"
  $CXX $COMMON_FLAGS -c "$f" -o "$o" &
  [ "$(jobs -r | wc -l)" -ge "$JOBS" ] && wait -n
done
wait
for o in "${common_objs[@]}"; do [ -f "$o" ] || exit 1; done

build_exe() {
  local target=$1 dir=$2 gen=$3 sha1=$4
  lld-link /lib /machine:x64 "/def:$gen/imports.def" "/out:$OUT/obj/$dir/code.lib"
  printf 'extern const char kImageSha1[] = "%s";\n' "$sha1" > "$OUT/obj/$dir/xbe_sha1.cpp"
  local target_srcs=(
    sdk/runtime/app/main.cpp
    sdk/runtime/app/game_code.cpp
    sdk/runtime/app/game_files.cpp
    sdk/runtime/app/system_info.cpp
    sdk/runtime/xdk/*.cpp
    "src/$dir"/*.cpp
    "$OUT/obj/$dir/xbe_sha1.cpp"
  )
  local target_objs=()
  for f in ${target_srcs[@]}; do
    [ -f "$f" ] || continue
    local o="$OUT/obj/$dir/$(echo "$f" | tr / _).obj"
    target_objs+=("$o")
    if [ "$o" -nt "$f" ] && [ "$o" -nt "$newest_hdr" ] && [ "$o" -nt "$gen/image_symbols.h" ]; then continue; fi
    echo "compile ($target) $f"
    rm -f "$o"
    $CXX $COMMON_FLAGS -I"$gen" -c "$f" -o "$o" &
    [ "$(jobs -r | wc -l)" -ge "$JOBS" ] && wait -n
  done
  wait
  for o in "${target_objs[@]}"; do [ -f "$o" ] || exit 1; done
  echo "link $OUT/$target.exe"
  clang++ --target=x86_64-pc-windows-msvc -fuse-ld=lld $WINLIB \
    "${common_objs[@]}" "${target_objs[@]}" "$OUT/obj/$dir/code.lib" "$SDL/lib/x64/SDL3.lib" \
    -ld3d12 -ld3d11 -ldinput8 -ldxgi -ldxguid -ld3dcompiler -lxaudio2 -lole32 -lwindowscodecs -ldbghelp -lwinmm -ldwmapi -lws2_32 -lwinhttp -luser32 -lgdi32 -lshell32 -ladvapi32 -limm32 -ldelayimp \
    -Wl,/subsystem:windows "-Wl,/delayload:$target.dll" "-Wl,/implib:$OUT/$target.lib" \
    "-Wl,/map:$OUT/$target.map" -o "$OUT/$target.exe"

  # Build <target>.dll (the recompiled game code) when x86_func_mapping.cpp is present in $gen
  if [ -f "$gen/x86_func_mapping.cpp" ] && [ -f "$gen/exports.def" ]; then
    local code_objs=()
    for f in "$gen"/x86_func_mapping.cpp "$gen"/x86_recomp.*.cpp; do
      [ -f "$f" ] || continue
      local o="$OUT/obj/$dir/$(basename "$f" .cpp).obj"
      code_objs+=("$o")
      if [ "$o" -nt "$f" ] && [ "$o" -nt "$gen/x86_recomp_shared.h" ]; then continue; fi
      echo "compile ($target.dll) $f"
      rm -f "$o"
      $CXX -std=c++20 --target=x86_64-pc-windows-msvc -fms-compatibility-version=19.40 -O2 -march=x86-64-v2 \
        -fno-strict-aliasing -DNDEBUG -DX86_GAME_CODE -nostdinc -nostdinc++ -w \
        -Isdk/runtime -I"$gen" -isystem sdk/runtime/cpu/freestanding -c "$f" -o "$o" &
      [ "$(jobs -r | wc -l)" -ge "$JOBS" ] && wait -n
    done
    wait
    for o in "${code_objs[@]}"; do [ -f "$o" ] || exit 1; done
    echo "link $OUT/$target.dll"
    lld-link /dll /noentry /nodefaultlib /machine:x64 /opt:noicf "/def:$gen/exports.def" \
      "${code_objs[@]}" "$OUT/$target.lib" "/out:$OUT/$target.dll"
  fi
}

GAME_SHA1="796222e048be3595c323b4c6d9795107132dc4ca"
DRIVING_SHA1="e0c41209d8979fcd03ac3d656be78759006023cb"
[ -f assets/default.xbe ] && GAME_SHA1=$(sha1sum assets/default.xbe | cut -d' ' -f1)
[ -f assets/Driving.xbe ] && DRIVING_SHA1=$(sha1sum assets/Driving.xbe | cut -d' ' -f1)

build_exe Nightfire game "$GEN_GAME" "$GAME_SHA1"
build_exe NightfireDriving driving "$GEN_DRIVING" "$DRIVING_SHA1"
cp -f "$SDL/lib/x64/SDL3.dll" "$OUT/SDL3.dll"
# Microsoft's shader compiler (the Windows SDK's redistributable, as release-package.bat ships it): Windows has it,
# Wine and Proton put their own in its place. D3DCOMPILER_DLL=<file>, or one kept at .local/win-sdk/d3dcompiler_47.dll
D3DCOMPILER=${D3DCOMPILER_DLL:-$WSDK/d3dcompiler_47.dll}
[ -f "$D3DCOMPILER" ] && cp -f "$D3DCOMPILER" "$OUT/d3dcompiler_47.dll"

echo
echo "BUILT:"
ls -lh "$OUT"/Nightfire* "$OUT/SDL3.dll"

# link assets/ beside the built exes so .local/win/Nightfire.exe is runnable in place too
if [ ! -e "$OUT/assets" ] && [ -d assets ]; then
  ln -s "$(realpath assets)" "$OUT/assets"
fi

install_into() {
  local dest=$1
  [ -d "$dest/play" ] && [ -f "$dest/play/Nightfire.exe" ] && dest="$dest/play"
  [ "$(realpath "$dest")" = "$(realpath "$OUT")" ] && return 0
  for exe in Nightfire.exe NightfireDriving.exe; do
    if [ -f "$dest/$exe" ] && [ ! -f "$dest/$exe.orig" ]; then
      cp "$dest/$exe" "$dest/$exe.orig"
      echo "backed up $dest/$exe -> $exe.orig"
    fi
    cp -f "$OUT/$exe" "$dest/$exe"
  done
  cp -f "$OUT/SDL3.dll" "$dest/SDL3.dll"
  [ -f "$OUT/d3dcompiler_47.dll" ] && [ ! -f "$dest/d3dcompiler_47.dll" ] && cp "$OUT/d3dcompiler_47.dll" "$dest/"
  if [ -d "$dest/compiler" ]; then
    cp -f "$OUT/Nightfire.lib" "$dest/compiler/Nightfire.lib"
    cp -f "$OUT/NightfireDriving.lib" "$dest/compiler/NightfireDriving.lib"
  fi
  sleep 1
  link_installed_dll() {
    local target=$1 gen=$2
    if [ -f "$OUT/$target.dll" ]; then
      cp -f "$OUT/$target.dll" "$dest/$target.dll"
      return 0
    fi
    local work="$dest/compiler/work/$target"
    if [ -f "$work/x86_func_mapping.obj" ] && [ -f "$gen/exports.def" ]; then
      local objs=("$work/x86_func_mapping.obj")
      for o in "$work"/x86_recomp.*.obj; do
        [ -f "$o" ] && objs+=("$o")
      done
      echo "link $dest/$target.dll (from $work/*.obj + $OUT/$target.lib)"
      lld-link /dll /noentry /nodefaultlib /machine:x64 /opt:noicf "/def:$gen/exports.def" \
        "${objs[@]}" "$OUT/$target.lib" "/out:$dest/$target.dll"
      return 0
    fi
    [ -f "$dest/$target.dll" ] && touch "$dest/$target.dll"
  }
  link_installed_dll Nightfire "$GEN_GAME"
  link_installed_dll NightfireDriving "$GEN_DRIVING"
  echo "INSTALLED into $dest (ready to launch in Steam/Proton)"
}

# Auto-discover existing Nightfire.exe installations under $HOME outside this repo's .local/win, plus any paths passed in
SELF_OUT=$(realpath "$OUT")
found=0
while IFS= read -r f; do
  d=$(dirname "$(realpath "$f")")
  [ "$d" = "$SELF_OUT" ] && continue
  install_into "$d"
  found=$((found + 1))
done < <(find "$HOME" -maxdepth 6 -iname "Nightfire.exe" 2>/dev/null || true)
for dir in "$@"; do
  install_into "$dir"
  found=$((found + 1))
done
echo "ready in $(realpath "$OUT/Nightfire.exe")"
if [ "$found" -eq 0 ]; then
  echo "note: no external Nightfire.exe folder found under $HOME; you can point Steam at $(realpath "$OUT/Nightfire.exe")"
  echo "      or run: scripts/build-win.sh /path/to/your/NightFire/play"
fi
