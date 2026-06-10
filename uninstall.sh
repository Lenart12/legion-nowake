#!/usr/bin/env bash
# Remove legion-nowake. Run as root:  sudo ./uninstall.sh
set -uo pipefail

NAME=legion-nowake
VER=1.0

if [[ $EUID -ne 0 ]]; then
	echo "Run as root: sudo ./uninstall.sh" >&2
	exit 1
fi

rm -f /etc/modules-load.d/legion-nowake.conf
modprobe -r legion_nowake 2>/dev/null || rmmod legion_nowake 2>/dev/null || true
dkms remove -m "$NAME" -v "$VER" --all 2>/dev/null || true
rm -rf "/usr/src/${NAME}-${VER}"

echo "Removed. (Wake bits are restored to BIOS defaults on the next reboot.)"
