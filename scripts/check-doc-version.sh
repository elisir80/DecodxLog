#!/usr/bin/env bash
set -euo pipefail

root_dir="${1:-$(cd "$(dirname "$0")/.." && pwd)}"
cmake_file="$root_dir/CMakeLists.txt"
readme_file="$root_dir/README.md"
features_file="$root_dir/docs/FEATURES.md"
api_file="$root_dir/docs/API.md"
manifest_file="${2:-$root_dir/build/decodxlog-capabilities.json}"

version="$(sed -n 's/^[[:space:]]*VERSION \([0-9][0-9.]*\)$/\1/p' "$cmake_file" | head -1)"
readme_version="$(sed -n 's/^## Stato: \(.*\)$/\1/p' "$readme_file" | head -1)"
features_version="$(sed -n 's/^# DecoDXLog \([^ ]*\).*/\1/p' "$features_file" | head -1)"

test -n "$version"
test "$version" = "$readme_version" || { echo "README version mismatch: CMake=$version README=$readme_version" >&2; exit 1; }
test "$version" = "$features_version" || { echo "FEATURES version mismatch: CMake=$version FEATURES=$features_version" >&2; exit 1; }
grep -Fq 'DECODXLOG_CTC_ONNX=ON' "$features_file"
grep -Fq 'standard binaries do not include it' "$features_file"
grep -Fq 'build/DecoDXLog.app/Contents/MacOS/DecoDXLog' "$readme_file"
grep -Fq './build/DecoDXLog' "$readme_file"
grep -Fq "\"version\":\"$version\"" "$api_file"
test -s "$manifest_file" || { echo "capabilities manifest missing: $manifest_file" >&2; exit 1; }
grep -Fq '"application": "DecoDXLog"' "$manifest_file"
grep -Fq "\"version\": \"$version\"" "$manifest_file"
grep -Fq '"repository": "elisir80/DecodxLog"' "$manifest_file"
grep -Fq '"channel": "fork"' "$manifest_file"
if grep -Fq 'DECODXLOG_CTC_ONNX=ON' "$features_file"; then
    if grep -Fq '"cw_decoder_ctc_onnx": true' "$manifest_file"; then
        :
    elif grep -Fq '"cw_decoder_ctc_onnx": false' "$manifest_file"; then
        :
    else
        echo "capabilities manifest has no CTC state" >&2
        exit 1
    fi
fi
echo "documentation gate passed: version $version"
