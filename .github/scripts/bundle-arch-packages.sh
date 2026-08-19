#!/bin/sh
# Create per-architecture tar.gz archives with both .apk and .ipk packages.
#
# Usage:
#   bundle-arch-packages.sh <label> [<label> ...]
#
# Expects artifact directories under dist/ named:
#   dist/<label>-snapshot-packages/
#   dist/<label>-openwrt-24.10-packages/
#
# Or for single-target develop builds:
#   dist/snapshot-packages/
#   dist/openwrt-24.10-packages/
# with DEVELOP_BUNDLE=1 and DEVELOP_LABEL set.

set -e

DIST="${DIST:-dist}"
OUT="$DIST/bundles"
mkdir -p "$OUT"

bundle_label() {
	label="$1"
	bundle_dir="$OUT/$label"
	mkdir -p "$bundle_dir"

	for variant in snapshot openwrt-24.10; do
		artifact_dir="$DIST/${label}-${variant}-packages"
		if [ -d "$artifact_dir" ]; then
			find "$artifact_dir" -type f \( -name '*.ipk' -o -name '*.apk' \) -exec cp -n {} "$bundle_dir/" \;
		fi
	done

	apk_count="$(find "$bundle_dir" -name '*.apk' 2> /dev/null | wc -l | tr -d ' ')"
	ipk_count="$(find "$bundle_dir" -name '*.ipk' 2> /dev/null | wc -l | tr -d ' ')"

	echo "Bundle ${label}: ${apk_count} apk, ${ipk_count} ipk file(s)"
	if [ "$apk_count" -eq 0 ] || [ "$ipk_count" -eq 0 ]; then
		echo "Missing package format in bundle for ${label} (need both apk and ipk)" >&2
		ls -la "$bundle_dir" >&2 || true
		return 1
	fi

	for pkg in sub-converter luci-app-sub-converter; do
		if ! find "$bundle_dir" -maxdepth 1 -name "${pkg}*" \( -name '*.apk' -o -name '*.ipk' \) -print -quit | grep -q .; then
			echo "Missing ${pkg} in bundle for ${label}" >&2
			return 1
		fi
	done

	archive="$DIST/sub-converter-${label}-packages.tar.gz"
	tar -czf "$archive" -C "$bundle_dir" .
	echo "Created ${archive}"
	ls -la "$bundle_dir"
}

if [ "${DEVELOP_BUNDLE:-0}" = "1" ]; then
	label="${DEVELOP_LABEL:-aarch64_cortex-a53}"
	bundle_dir="$OUT/$label"
	mkdir -p "$bundle_dir"

	for variant in snapshot openwrt-24.10; do
		artifact_dir="$DIST/${variant}-packages"
		if [ -d "$artifact_dir" ]; then
			find "$artifact_dir" -type f \( -name '*.ipk' -o -name '*.apk' \) -exec cp -n {} "$bundle_dir/" \;
		fi
	done

	apk_count="$(find "$bundle_dir" -name '*.apk' 2> /dev/null | wc -l | tr -d ' ')"
	ipk_count="$(find "$bundle_dir" -name '*.ipk' 2> /dev/null | wc -l | tr -d ' ')"

	echo "Develop bundle ${label}: ${apk_count} apk, ${ipk_count} ipk file(s)"
	if [ "$apk_count" -eq 0 ] || [ "$ipk_count" -eq 0 ]; then
		echo "Missing package format in develop bundle (need both apk and ipk)" >&2
		ls -la "$bundle_dir" >&2 || true
		exit 1
	fi

	archive="$DIST/sub-converter-${label}-packages.tar.gz"
	tar -czf "$archive" -C "$bundle_dir" .
	echo "Created ${archive}"
	ls -la "$bundle_dir"
	exit 0
fi

if [ "$#" -eq 0 ]; then
	echo "Usage: $0 <arch-label> [<arch-label> ...]" >&2
	exit 1
fi

for label in "$@"; do
	bundle_label "$label"
done
