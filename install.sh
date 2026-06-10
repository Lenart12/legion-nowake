#!/usr/bin/env bash
# Install legion-nowake as a DKMS module so it survives kernel updates,
# and auto-load it at boot. Run as root:  sudo ./install.sh
set -euo pipefail

NAME=legion-nowake
VER=1.0
SRC="/usr/src/${NAME}-${VER}"
HERE="$(cd "$(dirname "$0")" && pwd)"

if [[ $EUID -ne 0 ]]; then
	echo "Run as root: sudo ./install.sh" >&2
	exit 1
fi

# dependencies (Fedora). Adjust for your distro if needed.
if ! command -v dkms >/dev/null 2>&1; then
	echo ">> installing dkms + kernel-devel ..."
	dnf install -y dkms "kernel-devel-$(uname -r)" || dnf install -y dkms kernel-devel
fi

echo ">> staging source in ${SRC}"
rm -rf "$SRC"
mkdir -p "$SRC"
cp "$HERE"/legion_nowake.c "$HERE"/Makefile "$HERE"/dkms.conf "$SRC"/

echo ">> dkms add/build/install"
dkms add     -m "$NAME" -v "$VER" 2>/dev/null || true
dkms build   -m "$NAME" -v "$VER"
dkms install -m "$NAME" -v "$VER" --force

echo ">> enabling auto-load at boot"
echo legion_nowake > /etc/modules-load.d/legion-nowake.conf

echo ">> loading now"
modprobe legion_nowake 2>/dev/null || insmod "/lib/modules/$(uname -r)/extra/legion_nowake.ko" || true

echo
echo "== dkms status =="
dkms status | grep "$NAME" || true
echo "== dmesg =="
dmesg | grep legion_nowake | tail -5 || true
echo
echo "Done. Suspend with your monitor attached to confirm it holds."
