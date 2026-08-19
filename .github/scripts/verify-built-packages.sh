#!/bin/sh
# Verify sub-converter and luci-app-sub-converter were produced by gh-action-sdk.
# Packages land in bin/packages/<arch>/<feed>/, not in .../packages/.

set -e

FEED="${FEEDNAME:-action}"

missing=0
for pkg in sub-converter luci-app-sub-converter; do
	if ! find bin/packages -path "*/${FEED}/${pkg}*" \( -name '*.ipk' -o -name '*.apk' \) -print -quit | grep -q .; then
		echo "Missing built package: ${pkg} (expected under bin/packages/*/${FEED}/)" >&2
		missing=1
	fi
done

if [ "$missing" -ne 0 ]; then
	echo "Packages present under bin/packages:" >&2
	find bin/packages -type f \( -name '*.ipk' -o -name '*.apk' \) -ls >&2 || true
	exit 1
fi

echo "Built packages:"
find bin/packages -path "*/${FEED}/*" \( -name '*.ipk' -o -name '*.apk' \) -ls
