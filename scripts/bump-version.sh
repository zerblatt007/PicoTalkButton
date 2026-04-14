#!/bin/bash
# Bump the ptt-listen host package version in host/ptt-listen/DEBIAN/control.
#
# Usage:
#   scripts/bump-version.sh patch    # bug fixes / infrastructure (X.YY -> X.ZZ, ZZ = YY+1)
#   scripts/bump-version.sh minor    # new features              (X.YY -> X.ZZ, ZZ = YY+1)
#   scripts/bump-version.sh major    # breaking changes          (X.YY -> Z.00, Z = X+1)
#
# The host package uses a two-part version scheme (MAJOR.MINOR, e.g. 1.08).
# Both "patch" and "minor" increment the minor component; "major" increments
# the major component and resets minor to 00.

set -euo pipefail

CONTROL_FILE="host/ptt-listen/DEBIAN/control"
BUMP_TYPE="${1:-patch}"

# Validate arguments
case "$BUMP_TYPE" in
patch | minor | major) ;;
*)
	echo "Usage: $0 [patch|minor|major]" >&2
	echo "" >&2
	echo "  patch  — bug fixes / infra changes  (increments minor component)" >&2
	echo "  minor  — new features               (increments minor component)" >&2
	echo "  major  — breaking changes            (increments major, resets minor)" >&2
	exit 1
	;;
esac

# Ensure we run from repo root
if [ ! -f "$CONTROL_FILE" ]; then
	echo "Error: $CONTROL_FILE not found. Run this script from the repository root." >&2
	exit 1
fi

# Read current version
CURRENT_VERSION=$(awk '/^Version:/ { print $2 }' "$CONTROL_FILE")

if [[ ! "$CURRENT_VERSION" =~ ^[0-9]+\.[0-9]+$ ]]; then
	echo "Error: Unexpected version format '${CURRENT_VERSION}'. Expected MAJOR.MINOR (e.g. 1.08)." >&2
	exit 1
fi

MAJOR="${CURRENT_VERSION%%.*}"
MINOR_RAW="${CURRENT_VERSION#*.}"
MINOR=$((10#$MINOR_RAW)) # strip leading zeros before arithmetic

case "$BUMP_TYPE" in
patch | minor)
	MINOR=$((MINOR + 1))
	;;
major)
	MAJOR=$((MAJOR + 1))
	MINOR=0
	;;
esac

NEW_VERSION="${MAJOR}.$(printf '%02d' $MINOR)"

# Update DEBIAN/control in-place
sed -i "s/^Version: .*/Version: ${NEW_VERSION}/" "$CONTROL_FILE"

echo "Version bumped: ${CURRENT_VERSION}  ->  ${NEW_VERSION}  (${BUMP_TYPE})"
echo "Updated: ${CONTROL_FILE}"
