#!/bin/sh
set -eu

MANIFEST="openspec/coverage.txt"
DOC="docs/specifications.md"
TMP_DECLARED="${TMPDIR:-/tmp}/wibox-openspec-declared.$$"
TMP_ACTUAL="${TMPDIR:-/tmp}/wibox-openspec-actual.$$"

cleanup() {
    rm -f "$TMP_DECLARED" "$TMP_ACTUAL"
}
trap cleanup EXIT INT TERM

sed -n '/^[a-z0-9][a-z0-9-]*$/p' "$MANIFEST" | sort -u > "$TMP_DECLARED"
find openspec/specs -mindepth 2 -maxdepth 2 -type f -name spec.md \
    | sed 's#^openspec/specs/##; s#/spec.md$##' \
    | sort -u > "$TMP_ACTUAL"

if ! diff -u "$TMP_DECLARED" "$TMP_ACTUAL"; then
    echo "OpenSpec capability manifest and spec directories differ" >&2
    exit 1
fi

while IFS= read -r capability; do
    if ! grep -Fq "\`$capability\`" "$DOC"; then
        echo "Capability $capability is missing from $DOC" >&2
        exit 1
    fi
done < "$TMP_DECLARED"

COUNT=$(wc -l < "$TMP_DECLARED" | tr -d ' ')
if [ "$COUNT" -ne 18 ]; then
    echo "Expected 18 baseline capabilities, found $COUNT" >&2
    exit 1
fi

echo "OpenSpec capability coverage OK: $COUNT capabilities"
python3 scripts/verify_spec_test_coverage.py
