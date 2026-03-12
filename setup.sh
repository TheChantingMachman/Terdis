#!/usr/bin/env bash
# setup.sh — launch CUSTODIAN (alpha 0.1) on Linux x86_64.
#
# Usage (from extracted release tarball):
#   bash setup.sh
set -euo pipefail

if [ ! -f "./custodian" ]; then
    echo "ERROR: custodian binary not found. Are you in the right directory?"
    exit 1
fi

chmod +x ./custodian

echo ""
echo "  CUSTODIAN — alpha 0.1"
echo "  Linux x86_64"
echo ""
read -rp "  Launch? [Y/n] " yn
case "${yn,,}" in
    ""|y|yes) exec ./custodian ;;
    *) echo "  Run ./custodian when ready." ;;
esac
