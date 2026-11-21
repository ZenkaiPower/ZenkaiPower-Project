#!/bin/sh
# Seed the wrapper distribution; module dependencies use GRADLE_RO_DEP_CACHE.
set -eu
seed=${ZENKAIPOWER_GRADLE_SEED:-/opt/zenkaipower-gradle}
cache_dir=${GRADLE_USER_HOME:-${HOME:-/root}/.gradle}
marker=.zenkaipower-image-seed
if [ ! -f "$seed/$marker" ]; then exit 0; fi # Images built before cache bundling.
if [ -f "$cache_dir/$marker" ] && cmp -s "$seed/$marker" "$cache_dir/$marker"; then exit 0; fi
mkdir -p "$cache_dir"
if [ -d "$seed/wrapper" ]; then cp -an "$seed/wrapper" "$cache_dir/"; fi
# Write the marker last so an interrupted copy is retried on the next build.
cp "$seed/$marker" "$cache_dir/$marker"
