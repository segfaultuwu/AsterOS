#!/usr/bin/env bash
set -euo pipefail

PROJECT_NAME="AsterOS"
VERSION_FILE=".version"
OUT_DIR="include/aster/generated"
OUT_FILE="$OUT_DIR/version.h"

mkdir -p "$OUT_DIR"

if [[ -f "$VERSION_FILE" ]]; then
    VERSION_RAW="$(tr -d '[:space:]' < "$VERSION_FILE")"
else
    VERSION_RAW="0.1.0"
fi

if [[ ! "$VERSION_RAW" =~ ^[0-9]+(\.[0-9]+){0,2}$ ]]; then
    echo "error: invalid .version format: '$VERSION_RAW'"
    echo "expected: MAJOR.MINOR.PATCH, e.g. 0.1.0"
    exit 1
fi

IFS='.' read -r VERSION_MAJOR VERSION_MINOR VERSION_PATCH <<< "$VERSION_RAW"

VERSION_MAJOR="${VERSION_MAJOR:-0}"
VERSION_MINOR="${VERSION_MINOR:-0}"
VERSION_PATCH="${VERSION_PATCH:-0}"

VERSION_STRING="$VERSION_MAJOR.$VERSION_MINOR.$VERSION_PATCH"

if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    GIT_COMMIT="$(git rev-parse --short HEAD)"
    GIT_BRANCH="$(git rev-parse --abbrev-ref HEAD)"
    GIT_DIRTY="false"

    if ! git diff --quiet || ! git diff --cached --quiet; then
        GIT_DIRTY="true"
    fi
else
    GIT_COMMIT="unknown"
    GIT_BRANCH="unknown"
    GIT_DIRTY="false"
fi

BUILD_DATE="$(date -u '+%Y-%m-%d')"
BUILD_TIME="$(date -u '+%H:%M:%S UTC')"

cat > "$OUT_FILE" <<EOF
#ifndef ASTER_GENERATED_VERSION_H
#define ASTER_GENERATED_VERSION_H

#define ASTER_PROJECT_NAME "$PROJECT_NAME"

#define ASTER_VERSION_MAJOR $VERSION_MAJOR
#define ASTER_VERSION_MINOR $VERSION_MINOR
#define ASTER_VERSION_PATCH $VERSION_PATCH

#define ASTER_VERSION_STRING "$VERSION_STRING"

#define ASTER_GIT_COMMIT "$GIT_COMMIT"
#define ASTER_GIT_BRANCH "$GIT_BRANCH"
#define ASTER_GIT_DIRTY $GIT_DIRTY

#define ASTER_BUILD_DATE "$BUILD_DATE"
#define ASTER_BUILD_TIME "$BUILD_TIME"

#define ASTER_FULL_VERSION "$VERSION_STRING-$GIT_COMMIT"

#endif
EOF

echo "Generated $OUT_FILE"
echo "$PROJECT_NAME $VERSION_STRING-$GIT_COMMIT"
