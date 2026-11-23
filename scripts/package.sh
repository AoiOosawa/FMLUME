#!/usr/bin/env bash
# Builds the distribution installer (.pkg): Universal (Apple Silicon + Intel) VST3 / Audio Unit /
# Standalone app, release identity ("FMLUME", not "FMLUME Dev"). Same layout as POLYBRISK's.
#
#   scripts/package.sh
#
# Environment (all optional):
#   SIGN_APP        "Developer ID Application: Name (TEAMID)"  signs the plug-ins and the app
#   SIGN_PKG        "Developer ID Installer: Name (TEAMID)"    signs the installer
#   NOTARY_PROFILE  keychain profile made with `xcrun notarytool store-credentials`; when set the
#                   installer is sent to Apple for notarisation and stapled (needs SIGN_APP and SIGN_PKG)
#
# With none of them an unsigned installer is produced. That is enough to try on this Mac, but
# other Macs will refuse it (Gatekeeper). A distributed installer needs all three.
#
# Output: build-dist/package/FMLUME-<version>-macOS.pkg
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/build-dist"
OUT="$BUILD/package"
ARTEFACTS="$BUILD/FMLUME_artefacts/Release"

VERSION="$(sed -nE 's/^project\(FMLUME VERSION ([0-9.]+).*/\1/p' "$ROOT/CMakeLists.txt")"
[ -n "$VERSION" ] || { echo "cannot read the version from CMakeLists.txt" >&2; exit 1; }

if [ -n "${NOTARY_PROFILE:-}" ] && { [ -z "${SIGN_APP:-}" ] || [ -z "${SIGN_PKG:-}" ]; }; then
    echo "notarisation (NOTARY_PROFILE) needs both SIGN_APP and SIGN_PKG" >&2
    exit 1
fi

echo "== Building version $VERSION (arm64 + x86_64)"
mkdir -p "$BUILD"
rm -rf "$ARTEFACTS"        # no leftovers from another name or version
cmake -S "$ROOT" -B "$BUILD" -G "Unix Makefiles" \
      -DCMAKE_BUILD_TYPE=Release \
      "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" \
      -DFMLUME_DEV_BUILD=OFF \
      -DFMLUME_COPY_PLUGIN=OFF \
      -DFMLUME_BUILD_TESTS=OFF > "$BUILD/configure.log" 2>&1 || { tail -30 "$BUILD/configure.log"; exit 1; }
cmake --build "$BUILD" --target FMLUME_VST3 FMLUME_AU FMLUME_Standalone \
      --parallel "$(sysctl -n hw.ncpu)" > "$BUILD/build.log" 2>&1 || { grep -E "error" "$BUILD/build.log" | head -30; exit 1; }

VST3="$(ls -d "$ARTEFACTS"/VST3/*.vst3 | head -1)"
AU="$(ls -d "$ARTEFACTS"/AU/*.component | head -1)"
APP="$(ls -d "$ARTEFACTS"/Standalone/*.app | head -1)"
NAME="$(basename "$VST3" .vst3)"
[ "$NAME" = "FMLUME" ] || { echo "not the release name: $NAME (was it built with FMLUME_DEV_BUILD=OFF?)" >&2; exit 1; }
BUNDLE_ID="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' "$APP/Contents/Info.plist")"

for bundle in "$VST3" "$AU" "$APP"; do
    binary="$bundle/Contents/MacOS/$NAME"
    archs="$(lipo -archs "$binary")"
    case "$archs" in *arm64*x86_64*|*x86_64*arm64*) ;; *) echo "$bundle is not Universal: $archs" >&2; exit 1;; esac
done

# ---- signing -----------------------------------------------------------------
if [ -n "${SIGN_APP:-}" ]; then
    echo "== Signing with: $SIGN_APP"
    for bundle in "$VST3" "$AU" "$APP"; do
        codesign --force --timestamp --options runtime --sign "$SIGN_APP" "$bundle"
    done
    codesign --verify --strict "$VST3" "$AU" "$APP"
else
    echo "== SIGN_APP not set: the plug-ins keep their build-time (ad hoc) signature"
fi

# ---- one package per component --------------------------------------------------
rm -rf "$OUT"
mkdir -p "$OUT/parts" "$OUT/resources"

component() {   # <bundle> <install location> <id suffix>
    local stage="$OUT/stage-$3"
    mkdir -p "$stage"
    ditto "$1" "$stage/$(basename "$1")"

    # always install into the given folder (never over a copy that lives somewhere else)
    local plist="$OUT/parts/$3.plist"
    pkgbuild --analyze --root "$stage" "$plist" > /dev/null
    if /usr/libexec/PlistBuddy -c "Print :0:BundleIsRelocatable" "$plist" > /dev/null 2>&1; then
        /usr/libexec/PlistBuddy -c "Set :0:BundleIsRelocatable false" "$plist"
    fi

    pkgbuild --root "$stage" --component-plist "$plist" \
             --install-location "$2" \
             --identifier "$BUNDLE_ID.pkg.$3" --version "$VERSION" \
             "$OUT/parts/$3.pkg" > /dev/null
    rm -rf "$stage"
}

component "$VST3" "/Library/Audio/Plug-Ins/VST3"       vst3
component "$AU"   "/Library/Audio/Plug-Ins/Components" au
component "$APP"  "/Applications"                      app

# ---- installer --------------------------------------------------------------------
{
    cat "$ROOT/LICENSE"
    printf '\n\n==========================================================================\n\n'
    cat "$ROOT/THIRD_PARTY_NOTICES.md"
} > "$OUT/resources/LICENSE.txt"

cat > "$OUT/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>$NAME $VERSION</title>
    <license file="LICENSE.txt"/>
    <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <os-version min="12.0"/>
    <choices-outline>
        <line choice="vst3"/>
        <line choice="au"/>
        <line choice="app"/>
    </choices-outline>
    <choice id="vst3" title="VST3 plugin" description="For Cubase, Studio One, REAPER and other VST3 hosts.">
        <pkg-ref id="$BUNDLE_ID.pkg.vst3"/>
    </choice>
    <choice id="au" title="Audio Unit plugin" description="For Logic Pro, GarageBand and other Audio Unit hosts.">
        <pkg-ref id="$BUNDLE_ID.pkg.au"/>
    </choice>
    <choice id="app" title="Standalone app" description="Runs without a host, in the Applications folder.">
        <pkg-ref id="$BUNDLE_ID.pkg.app"/>
    </choice>
    <pkg-ref id="$BUNDLE_ID.pkg.vst3" version="$VERSION">vst3.pkg</pkg-ref>
    <pkg-ref id="$BUNDLE_ID.pkg.au" version="$VERSION">au.pkg</pkg-ref>
    <pkg-ref id="$BUNDLE_ID.pkg.app" version="$VERSION">app.pkg</pkg-ref>
</installer-gui-script>
XML

PKG="$OUT/$NAME-$VERSION-macOS.pkg"
SIGN_ARGS=()
[ -n "${SIGN_PKG:-}" ] && SIGN_ARGS=(--sign "$SIGN_PKG" --timestamp)

productbuild --distribution "$OUT/distribution.xml" --package-path "$OUT/parts" \
             --resources "$OUT/resources" ${SIGN_ARGS[@]+"${SIGN_ARGS[@]}"} "$PKG" > /dev/null

[ -n "${SIGN_PKG:-}" ] || echo "== SIGN_PKG not set: the installer is unsigned"

# ---- notarisation -----------------------------------------------------------------
if [ -n "${NOTARY_PROFILE:-}" ]; then
    echo "== Sending to Apple for notarisation (takes a few minutes)"
    xcrun notarytool submit "$PKG" --keychain-profile "$NOTARY_PROFILE" --wait
    xcrun stapler staple "$PKG"
    spctl --assess --type install -vv "$PKG"
fi

echo "== Done: $PKG"
