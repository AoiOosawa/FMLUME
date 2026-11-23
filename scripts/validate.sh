#!/usr/bin/env bash
# Checks the built plug-ins with host-independent validators.
#   scripts/validate.sh [build dir]     default: build   (Release configuration)
# - auval (macOS): the AU, found by the ids read from the built component's Info.plist, so this
#   works for both "FMLUME Dev" and the release build. Run it right after a build that copied the
#   plug-ins into ~/Library/Audio/Plug-Ins.
# - pluginval, if installed: VST3 and AU. Strictness via PLUGINVAL_STRICTNESS (1-10, default 10).
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/build}"
ARTEFACTS="$BUILD/FMLUME_artefacts/Release"
STATUS=0

VST3_BUNDLE="$(ls -d "$ARTEFACTS"/VST3/*.vst3 2>/dev/null | head -1)"
AU_BUNDLE="$(ls -d "$ARTEFACTS"/AU/*.component 2>/dev/null | head -1)"
if [ -z "$VST3_BUNDLE" ] || [ -z "$AU_BUNDLE" ]; then
    echo "no plug-ins in $ARTEFACTS: build first (cmake --build build --config Release)" >&2
    exit 1
fi

plist() { /usr/libexec/PlistBuddy -c "Print :AudioComponents:0:$1" "$AU_BUNDLE/Contents/Info.plist"; }
AU_TYPE="$(plist type)"; AU_SUBTYPE="$(plist subtype)"; AU_MANUFACTURER="$(plist manufacturer)"
echo "==> $(plist name)  ($AU_TYPE $AU_SUBTYPE $AU_MANUFACTURER)"

echo "==> auval -v $AU_TYPE $AU_SUBTYPE $AU_MANUFACTURER"
if auval -v "$AU_TYPE" "$AU_SUBTYPE" "$AU_MANUFACTURER" | tail -3; then echo "auval: PASS"; else echo "auval: FAIL" >&2; STATUS=1; fi

PLUGINVAL_BIN="${PLUGINVAL:-}"
if [ -z "$PLUGINVAL_BIN" ]; then
    if command -v pluginval >/dev/null 2>&1; then PLUGINVAL_BIN="$(command -v pluginval)"
    elif [ -x "/Applications/pluginval.app/Contents/MacOS/pluginval" ]; then PLUGINVAL_BIN="/Applications/pluginval.app/Contents/MacOS/pluginval"; fi
fi
if [ -n "$PLUGINVAL_BIN" ]; then
    for PLUGIN in "$VST3_BUNDLE" "$AU_BUNDLE"; do
        echo "==> pluginval $PLUGIN"
        if "$PLUGINVAL_BIN" --strictness-level "${PLUGINVAL_STRICTNESS:-10}" --validate "$PLUGIN" | tail -1 | grep -q SUCCESS; then echo "pluginval: PASS"; else echo "pluginval: FAIL ($PLUGIN)" >&2; STATUS=1; fi
    done
else
    echo "==> pluginval: SKIPPED (not installed, https://github.com/Tracktion/pluginval)"
fi
exit $STATUS
