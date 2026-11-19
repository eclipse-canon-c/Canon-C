#!/usr/bin/env bash
# .github/actions/apt-install/fetch.sh LIST DIR
#
# Fetches the package files LIST names into DIR, which may hold some of
# them already, restored from the Actions cache (VERIFY-093). LIST is the
# output of `apt-get install --print-uris -o Acquire::ForceHash=SHA256`,
# run after `apt-get update` has checked Ubuntu's signed indexes: a line
# 'URI' FILE SIZE SHA256:HASH for each file. A file in DIR is kept if its
# SHA-256 is the index's. Another is fetched from Canonical's archive, and
# then from Azure's mirror, and kept only if its SHA-256 is the index's. A
# mirror that fails a file, sending less than 500 kB/s for 30 s or no file,
# is not asked for the next ones, so that two slow mirrors cost a few
# minutes, not the step's thirty. A file no mirror gives is left to apt,
# which fetches it at any speed. Files that LIST does not name go, so that
# DIR, and what the cache saves of it, holds this list's files and no
# others.
set -u
list=$1
dir=$2
mirrors="https://archive.ubuntu.com/ubuntu http://azure.archive.ubuntu.com/ubuntu"
mkdir -p "$dir"
kept=0
fetched=0
left=0
while read -r uri file size hash; do
    uri=${uri//\'/}
    f="$dir/$file"
    case $hash in
        SHA256:*) sha=${hash#SHA256:} ;;
        *) echo "$file: no SHA-256 in the list: left to apt"
           left=$((left + 1)); continue ;;
    esac
    if [ -f "$f" ] && echo "$sha  $f" | sha256sum -c --status -; then
        kept=$((kept + 1)); continue
    fi
    rm -f "$f"
    case $uri in
        */pool/*) path="pool/${uri#*/pool/}" ;;
        *) echo "$file: no pool path in $uri: left to apt"
           left=$((left + 1)); continue ;;
    esac
    for base in $mirrors; do
        if curl -fsSL --retry 1 --connect-timeout 20 \
                --speed-limit 500000 --speed-time 30 -o "$f" "$base/$path" \
           && echo "$sha  $f" | sha256sum -c --status -; then
            echo "$file: $size bytes from $base"
            fetched=$((fetched + 1))
            break
        fi
        echo "$file: not from $base, an error or under 500 kB/s: not asked again"
        rm -f "$f"
        mirrors=$(echo "$mirrors" | tr ' ' '\n' | grep -vxF "$base" | tr '\n' ' ')
    done
    [ -f "$f" ] || left=$((left + 1))
done < "$list"
for f in "$dir"/*.deb; do
    [ -e "$f" ] || continue
    b=$(basename "$f")
    awk -v b="$b" '$2 == b { found = 1 } END { exit !found }' "$list" || rm -f "$f"
done
echo "$(wc -l < "$list") package file(s): ${kept} from the cache, ${fetched} fetched, ${left} left to apt"
