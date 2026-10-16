#!/usr/bin/env bash
# Make a macOS Qt bundle self-contained after macdeployqt.
#
# macdeployqt normally rewrites these paths, but Qt 6.11/Homebrew can leave
# @rpath dependencies (and, in particular, QtDBus) resolving through the
# build machine's Homebrew prefix. That loads a second Qt beside the bundled
# one. Rewrite every dependency for which the corresponding bundled file is
# present, then reject remaining non-system absolute dependencies.

set -euo pipefail

app="${1:?usage: $0 path/to/DecoDXLog.app}"
frameworks="$app/Contents/Frameworks"

if [[ ! -d "$app/Contents" || ! -d "$frameworks" ]]; then
    echo "macOS bundle is missing Contents/Frameworks: $app" >&2
    exit 1
fi

bundle_files=()
while IFS= read -r -d '' file; do
    bundle_files+=("$file")
done < <(find "$app/Contents" -type f -print0)

bundled_target() {
    local dependency="$1"
    local relative candidate

    case "$dependency" in
        @rpath/*)
            relative="${dependency#@rpath/}"
            candidate="$frameworks/$relative"
            ;;
        /*)
            if [[ "$dependency" == *.framework/* ]]; then
                relative="${dependency##*/lib/}"
                candidate="$frameworks/$relative"
            else
                relative="${dependency##*/}"
                candidate="$frameworks/$relative"
            fi
            ;;
        *)
            return 1
            ;;
    esac

    if [[ -f "$candidate" ]]; then
        printf '@executable_path/../Frameworks/%s\n' "$relative"
        return 0
    fi
    return 1
}

for file in "${bundle_files[@]}"; do
    # A framework/dylib can carry an absolute install name even after its
    # dependants have been rewritten. Give the copy in the app one stable ID.
    relative="${file#"$frameworks"/}"
    case "$relative" in
        *.framework/Versions/*/*|*.dylib)
            desired="@executable_path/../Frameworks/$relative"
            current="$(otool -D "$file" 2>/dev/null | sed -n '2p' || true)"
            if [[ -n "$current" && "$current" != "$desired" ]]; then
                install_name_tool -id "$desired" "$file"
            fi
            ;;
    esac
done

for file in "${bundle_files[@]}"; do
    dependencies="$(otool -L "$file" 2>/dev/null | sed '1d; s/^[[:space:]]*//; s/ (.*$//' || true)"
    while IFS= read -r dependency; do
        [[ -n "$dependency" ]] || continue
        desired="$(bundled_target "$dependency" || true)"
        [[ -n "$desired" && "$desired" != "$dependency" ]] || continue
        install_name_tool -change "$dependency" "$desired" "$file"
    done <<< "$dependencies"
done

unbundled=0
for file in "${bundle_files[@]}"; do
    dependencies="$(otool -L "$file" 2>/dev/null | sed '1d; s/^[[:space:]]*//; s/ (.*$//' || true)"
    while IFS= read -r dependency; do
        [[ -n "$dependency" ]] || continue
        case "$dependency" in
            /System/*|/usr/lib/*) ;;
            @rpath/*)
                relative="${dependency#@rpath/}"
                if [[ ! -f "$frameworks/$relative" ]]; then
                    echo "Unresolved @rpath dependency in $file: $dependency" >&2
                    unbundled=1
                fi
                ;;
            @*) ;;
            /*)
                echo "Unbundled macOS dependency in $file: $dependency" >&2
                unbundled=1
                ;;
        esac
    done <<< "$dependencies"
done

if [[ "$unbundled" -ne 0 ]]; then
    exit 1
fi

echo "macOS bundle dependencies point to the app bundle or system frameworks"
