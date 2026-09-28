#!/bin/sh
# Run the K8 Renode tests (or another script) against an emulation build.
#   ./run.sh <firmware.elf> [script.resc]      default script: tests.resc
# Needs Renode 1.17 or newer: renode on PATH or RENODE=/path/to/renode.
# Prints one line per check; exit status 1 when a check fails.
set -eu

main() {
    here=$(cd "$(dirname "$0")" && pwd)
    renode=${RENODE:-renode}
    elf=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
    script=${2:-tests.resc}
    mkdir -p "$here/out"
    # Unprogrammed flash reads 0xFF; the wear-leveling EEPROM relies on it.
    [ -f "$here/ff.bin" ] || python3 -c "open('$here/ff.bin','wb').write(b'\xff'*0x10000)"
    python3 "$here/symbols.py" "$elf" >"$here/out/symbols.txt"
    export K8_SIM_DIR="$here" K8_ELF="$elf" K8_SIM_VERBOSE="${K8_SIM_VERBOSE:-0}"
    log="$here/out/$(basename "$script" .resc).log"
    cd "$here"
    "$renode" --disable-gui --console --plain -e "i @$here/$script" </dev/null >"$log" 2>&1 || true
    grep -aE '^(PASS|FAIL|KNOWN|RESULT)' "$log" || true
    grep -aq '^RESULT pass=[0-9]* fail=0 ' "$log"
}

main "$@"
exit
