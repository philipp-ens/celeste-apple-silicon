#!/bin/bash
# Converts the Steam release of Celeste for macOS
# into a native Apple Silicon app. See README.md.
#
#   ./convert.sh           convert (or re-convert after a Steam update)
#   ./convert.sh --undo    put the original Intel app back
#
# Options:
#   --game-dir <dir>   folder that contains Celeste.app (default: found via Steam)
#   --force            convert even if Celeste.exe is not the known 1.4.0.0 build
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
PAYLOAD="$HERE/payload"

# Celeste 1.4.0.0, Steam, macOS
KNOWN_EXE_SHA256="3ee734ec8c767f638e9a7c2cbc07109f0a0bf1eb1973dbe447e99144ca9e7dd9"
# Official Steamworks.NET release; only its arm64-capable libsteam_api.dylib is used.
STEAMWORKS_URL="https://github.com/rlabrecque/Steamworks.NET/releases/download/2025.165.0/Steamworks.NET-Standalone_2025.165.0.zip"
STEAMWORKS_SHA256="9b86dda48b257013a350c8fea87edf3ab3f6f8fd6a812563fddec73fb1189fee"
STEAMWORKS_LIB="OSX-Linux-x64/steam_api.bundle/Contents/MacOS/libsteam_api.dylib"

GAME_DIR=""
UNDO=0
FORCE=0
WORK=""

say() { printf '==> %s\n' "$*"; }
die() { printf 'error: %s\n' "$*" >&2; exit 1; }
cleanup() {
    if [ -n "$WORK" ]; then rm -rf "$WORK"; fi
}
trap cleanup EXIT

while [ $# -gt 0 ]; do
    case "$1" in
        --undo) UNDO=1 ;;
        --force) FORCE=1 ;;
        --game-dir) shift; GAME_DIR="${1:?--game-dir needs a path}" ;;
        -h|--help) sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        -*) die "unknown option $1" ;;
        *) die "unexpected argument $1" ;;
    esac
    shift
done

is_arm64() { file -b "$1" | grep -q 'arm64'; }

# --- checks ------------------------------------------------------------------

[ "$(uname -s)" = "Darwin" ] || die "this converter is for macOS"
[ "$(sysctl -n hw.optional.arm64 2>/dev/null || echo 0)" = "1" ] || die "this Mac is not Apple Silicon; the Intel build is the right one for it"
OS_MAJOR="$(sw_vers -productVersion | cut -d. -f1)"
[ "$OS_MAJOR" -ge 11 ] || die "macOS 11 or newer is required"
[ -d "$PAYLOAD/MacOS/osx" ] || die "payload folder is missing next to convert.sh"

if [ -z "$GAME_DIR" ]; then
    STEAM="$HOME/Library/Application Support/Steam"
    CANDIDATES=("$STEAM/steamapps/common/Celeste")
    if [ -f "$STEAM/steamapps/libraryfolders.vdf" ]; then
        while IFS= read -r lib; do
            CANDIDATES+=("$lib/steamapps/common/Celeste")
        done < <(sed -n 's/^[[:space:]]*"path"[[:space:]]*"\(.*\)"[[:space:]]*$/\1/p' "$STEAM/steamapps/libraryfolders.vdf")
    fi
    for dir in "${CANDIDATES[@]}"; do
        if [ -d "$dir/Celeste.app" ]; then GAME_DIR="$dir"; break; fi
    done
    [ -n "$GAME_DIR" ] || die "could not find Celeste in your Steam library; pass --game-dir <folder containing Celeste.app>"
fi
GAME_DIR="$(cd "$GAME_DIR" && pwd)"
APP="$GAME_DIR/Celeste.app"
KEEP="$GAME_DIR/arm64-converter-files"
[ -d "$APP" ] || die "no Celeste.app in $GAME_DIR"
say "Celeste folder: $GAME_DIR"

if pgrep -f "$APP/Contents/MacOS/Celeste" >/dev/null 2>&1; then
    die "Celeste is running; quit it first"
fi

latest_intel_backup() { ls -d "$KEEP"/Celeste.app.intel-* 2>/dev/null | tail -1 || true; }

# --- undo --------------------------------------------------------------------

if [ "$UNDO" = "1" ]; then
    is_arm64 "$APP/Contents/MacOS/Celeste" || die "Celeste.app is already the Intel build"
    BACKUP="$(latest_intel_backup)"
    [ -n "$BACKUP" ] || die "no saved Intel app in $KEEP (use Steam's 'Verify integrity of game files' instead)"
    rm -rf "$APP"
    mv "$BACKUP" "$APP"
    say "Restored the original Intel Celeste.app"
    exit 0
fi

if is_arm64 "$APP/Contents/MacOS/Celeste"; then
    say "Celeste.app is already the native arm64 build; nothing to do"
    say "(use --undo to go back to the Intel build)"
    exit 0
fi

EXE="$APP/Contents/Resources/Celeste.exe"
[ -f "$EXE" ] || die "$EXE not found; this does not look like the Steam release of Celeste"
EXE_SHA="$(shasum -a 256 "$EXE" | cut -d' ' -f1)"
if [ "$EXE_SHA" != "$KNOWN_EXE_SHA256" ]; then
    if [ "$FORCE" = "1" ]; then
        say "warning: Celeste.exe is not the known 1.4.0.0 Steam build; continuing because of --force"
    else
        die "Celeste.exe is not the known 1.4.0.0 Steam build (modded, or a different version/store). Re-run with --force to try anyway."
    fi
fi

mkdir -p "$KEEP/cache"
WORK="$(mktemp -d "$GAME_DIR/.arm64-convert.XXXXXX")"

CACHE="$KEEP/cache"

# --- Steam library -----------------------------------------------------------

if [ ! -f "$CACHE/libsteam_api.dylib" ]; then
    say "Downloading the Steam API library (Steamworks.NET release, about 1 MB)"
    curl -fsSL -o "$WORK/steamworks.zip" "$STEAMWORKS_URL" || die "download failed: $STEAMWORKS_URL"
    [ "$(shasum -a 256 "$WORK/steamworks.zip" | cut -d' ' -f1)" = "$STEAMWORKS_SHA256" ] || die "the downloaded Steamworks.NET archive does not match the expected checksum"
    unzip -q -o "$WORK/steamworks.zip" "$STEAMWORKS_LIB" -d "$WORK/steamworks"
    is_arm64 "$WORK/steamworks/$STEAMWORKS_LIB" || die "downloaded libsteam_api.dylib has no arm64 slice"
    cp "$WORK/steamworks/$STEAMWORKS_LIB" "$CACHE/libsteam_api.dylib"
fi

# --- build the native app ----------------------------------------------------

say "Building the native app"
NEW="$WORK/Celeste.app"
cp -Rc "$APP" "$NEW" 2>/dev/null || cp -R "$APP" "$NEW"
C="$NEW/Contents"

# Intel-only pieces: launcher with embedded Mono, native libraries, the old
# class libraries, FNA, FMOD 1.10, and the Steam wrapper. The payload brings
# the arm64 replacements, including the FMOD Engine 2.03 runtime libraries.
rm -f "$C/MacOS/Celeste"
rm -rf "$C/MacOS/osx"
( cd "$C/Resources" && rm -rf \
    FNA.dll FNA.dll.config FNA3D.dll Steamworks.NET.dll monoconfig monomachineconfig vulkan \
    mscorlib.dll System.dll System.Core.dll System.Xml.dll System.Configuration.dll System.Data.dll \
    System.Drawing.dll System.Numerics.dll System.Runtime.Serialization.dll System.Security.dll \
    System.Xml.Linq.dll Mono.Posix.dll Mono.Security.dll steam_appid.txt errorLog.txt )

cp -R "$PAYLOAD/MacOS/" "$C/MacOS/"
cp -R "$PAYLOAD/Resources/" "$C/Resources/"
cp "$CACHE/libsteam_api.dylib" "$C/MacOS/osx/"
chmod +x "$C/MacOS/Celeste"

for f in "$C/MacOS/Celeste" "$C"/MacOS/osx/*.dylib; do
    is_arm64 "$f" || die "internal error: $f is not arm64"
done

xattr -dr com.apple.quarantine "$NEW" 2>/dev/null || true
codesign -f --deep -s - "$NEW" >/dev/null 2>&1 || die "could not sign the converted app"

# --- install -----------------------------------------------------------------

SAVES="$HOME/Library/Application Support/Celeste/Saves"
if [ -d "$SAVES" ] && ! ls -d "$KEEP"/Saves-backup-* >/dev/null 2>&1; then
    cp -R "$SAVES" "$KEEP/Saves-backup-$(date +%Y%m%d-%H%M%S)"
    say "Backed up your saves to $KEEP"
fi

# Keep only the newest Intel app; Steam can always re-download it.
for old in "$KEEP"/Celeste.app.intel-*; do [ -e "$old" ] && rm -rf "$old"; done
mv "$APP" "$KEEP/Celeste.app.intel-$(date +%Y%m%d-%H%M%S)"
mv "$NEW" "$APP"

say "Done. Celeste.app is now a native Apple Silicon app; start it from Steam as usual."
say "If a Steam update or file verification brings the Intel build back, run this script again."
