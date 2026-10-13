#!/usr/bin/env bash
#
# Builds a redistributable DJIReframe.ofx.bundle that loads on distros other
# than the one you are sitting on.
#
# Two things make a plugin unportable, and this script addresses both:
#
#   1. glibc.  A plugin built on a rolling-release distro records symbol
#      versions (GLIBC_2.43 and friends) that older hosts do not have, and
#      Resolve fails to load it. glibc is backward compatible but not forward
#      compatible, so the fix is to build against an OLD glibc. We use the
#      manylinux2014 image (glibc 2.17) -- older than any distro Resolve
#      supports, so the result loads everywhere.
#
#   2. libstdc++.  Handled by -DOFX_PORTABLE=ON in CMakeLists.txt, which links
#      the C++ runtime statically and exports only the OFX entry points.
#
# Usage:
#   tools/build-portable.sh            # build + verify, output in dist/
#   tools/build-portable.sh --verify   # verify an existing dist/ build only
#
set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE="${OFX_BUILD_IMAGE:-quay.io/pypa/manylinux2014_x86_64}"
DIST="$REPO/dist"
BUNDLE="$DIST/DJIReframe.ofx.bundle"
PLUGIN="$BUNDLE/Contents/Linux-x86-64/DJIReframe.ofx"

# Oldest glibc we promise to run on. Resolve's own Linux baseline is Rocky 8
# (glibc 2.28); we aim lower so CentOS 7 era installs work too.
GLIBC_BASELINE="2.17"

die() { printf '\033[31merror:\033[0m %s\n' "$*" >&2; exit 1; }
info() { printf '\033[36m==>\033[0m %s\n' "$*"; }

verify() {
    [[ -f "$PLUGIN" ]] || die "no plugin at $PLUGIN -- build first"

    info "Verifying $PLUGIN"

    # --- glibc floor -------------------------------------------------------
    local max
    max="$(objdump -T "$PLUGIN" | grep -oE 'GLIBC_[0-9.]+' | sed 's/GLIBC_//' \
           | sort -u -V | tail -1)"
    if [[ "$(printf '%s\n%s\n' "$GLIBC_BASELINE" "$max" | sort -V | tail -1)" \
          != "$GLIBC_BASELINE" ]]; then
        die "needs glibc $max, baseline is $GLIBC_BASELINE -- not portable"
    fi
    printf '    glibc required:   %s (baseline %s) OK\n' "$max" "$GLIBC_BASELINE"

    # --- no C++ runtime dependency ----------------------------------------
    if objdump -p "$PLUGIN" | grep -qE 'NEEDED.*libstdc\+\+'; then
        die "links libstdc++ dynamically -- rebuild with -DOFX_PORTABLE=ON"
    fi
    printf '    libstdc++:        static OK\n'
    printf '    shared deps:      %s\n' \
        "$(objdump -p "$PLUGIN" | awk '/NEEDED/{printf "%s ", $2}')"

    # --- only OFX entry points exported ----------------------------------
    local exports
    exports="$(nm -D --defined-only "$PLUGIN" | awk '{print $3}' | grep -v '^$' || true)"
    if grep -qv '^Ofx' <<<"$exports"; then
        die "non-OFX symbols exported:"$'\n'"$(grep -v '^Ofx' <<<"$exports")"
    fi
    printf '    exported symbols: %s OK\n' "$(tr '\n' ' ' <<<"$exports")"

    # --- bundle layout ----------------------------------------------------
    [[ -f "$BUNDLE/Contents/Info.plist" ]] || die "bundle missing Info.plist"
    printf '    bundle layout:    OK\n'

    info "Portable. Loads on glibc >= $GLIBC_BASELINE."
}

if [[ "${1:-}" == "--verify" ]]; then verify; exit 0; fi

[[ -f "$REPO/vendor/openfx/include/ofxImageEffect.h" ]] \
    || die "OpenFX submodule missing. Run: git submodule update --init --depth 1"

command -v docker >/dev/null || die "docker not found"

info "Building in $IMAGE (glibc 2.17)"
rm -rf "$DIST"
mkdir -p "$DIST"

docker run --rm \
    -v "$REPO:/src:ro" \
    -v "$DIST:/out" \
    -e HOME=/tmp \
    --user "$(id -u):$(id -g)" \
    "$IMAGE" \
    bash -euo pipefail -c '
        cmake -S /src -B /tmp/build \
              -DCMAKE_BUILD_TYPE=Release \
              -DOFX_PORTABLE=ON > /dev/null
        cmake --build /tmp/build -j"$(nproc)"
        cp -r /tmp/build/DJIReframe.ofx.bundle /out/
        # Build and run the geometry checks with the same toolchain.
        g++ -std=c++17 -O2 -I/src/src -o /tmp/test_reframe /src/tests/test_reframe.cpp
        mkdir -p /tmp/testout
        /tmp/test_reframe /tmp/testout | tail -25
    '

info "Bundle at $BUNDLE"
verify

info "Packaging tarball"
tar -C "$DIST" -czf "$DIST/DJIReframe-linux-x86_64.ofx.bundle.tar.gz" \
    DJIReframe.ofx.bundle
printf '    %s\n' "$DIST/DJIReframe-linux-x86_64.ofx.bundle.tar.gz"
