# legion-nowake

Fixes **instant wake from suspend when an external display is connected** on the
**Lenovo Legion Pro 5 16ARX8** (machine type **82WM**, AMD Ryzen 7 7745HX +
RTX 4060).

## Symptom

With a monitor plugged into HDMI (or USB‑C/DP), the laptop enters S3/s2idle and
resumes again after ~1 second. Unplug the display and suspend works fine. The
kernel log shows:

```
PM: Triggering wakeup from IRQ 7        # IRQ 7 = pinctrl_amd (AMD GPIO controller)
GPIO 4 is active: 0x30057c00            # bit 14 (S3 wake) + bit 29 (wake latched)
```

## Root cause

The external‑display **HPD (hot‑plug detect)** lines are wired to AMD SoC GPIO
pins **#2** (USB‑C/DP path) and **#4** (HDMI/dGPU path). The **BIOS arms those
GPIOs as S0i3/S3 wake sources** directly in the FCH GPIO registers
(`AMDI0030` @ `0xFED81500`), and provides **no ACPI description** for them
(there is no `_AEI` / `GpioInt` / `_PRW` anywhere in the DSDT or the 16 SSDTs).

`pinctrl_amd` faithfully preserves any hardware‑armed wake pin across suspend, so
the HPD edge produced while the GPU powers down on the way into sleep is latched
and immediately wakes the machine.

Because there is **no ACPI owner**, none of the usual levers can disarm it:

* `/proc/acpi/wakeup` and PCI `power/wakeup` — wrong layer (this is not a PCIe PME).
* `gpiolib_acpi.ignore_wake=` — only works for ACPI‑declared wake GPIOs; these
  aren't declared.
* `/dev/mem` writes — blocked by `CONFIG_IO_STRICT_DEVMEM=y` (pinctrl_amd owns
  the MMIO region).

So the fix is a tiny kernel module that clears the wake‑enable bits.

## What the module does

On load (and again before every suspend) it clears bits **13/14/15**
(S0i3 / S3 / S4‑S5 wake‑enable) of the GPIO registers for pins **#2** and **#4**.

It is deliberately surgical:

* It leaves the **interrupt‑enable** bits intact, so runtime hotplug detection
  (plugging a monitor in while the machine is running) still works.
* It touches **only** pins #2 and #4. Power button, USB/xHCI, RTC, LAN and every
  other wake source are unaffected.
* It guards on **DMI** (`LENOVO` / `Legion Pro 5 16ARX8`) and refuses to load on
  other machines unless you pass `force=1`.

## Install (DKMS — survives kernel updates)

```bash
git clone https://github.com/Lenart12/legion-nowake
cd legion-nowake
sudo ./install.sh
```

Then suspend with your monitor attached to confirm it holds.

## Uninstall

```bash
sudo ./uninstall.sh
```

## Manual build / test (no install)

```bash
make
sudo insmod ./legion_nowake.ko        # add force=1 on a non-matching machine
sudo dmesg | grep legion_nowake       # should show "GPIO#2 ... / GPIO#4 ... wake disarmed"
sudo rmmod legion_nowake              # to remove
```

### Module parameters

| param   | default | meaning                                            |
|---------|---------|----------------------------------------------------|
| `pins`  | `2,4`   | GPIO pin numbers whose wake bits to clear          |
| `force` | `0`     | load even if the DMI model doesn't match           |

## Caveats / scope

* Other Legion / AMD models may use **different pin numbers**. Find yours with
  `sudo cat /sys/kernel/debug/gpio` (look for pins with the `S0i3`/`S3` wake
  columns set and an interrupt latched), then load with e.g. `pins=2,4,8`.
* If pin #2 or #4 on your unit also carried a wake you *want* (e.g. lid‑open),
  narrow the list with the `pins=` parameter.

## Status / upstream

This is a workaround for a firmware bug. The proper fix is either a Lenovo BIOS
update that stops arming display‑HPD GPIOs as sleep wakes (or describes them in
ACPI), or a DMI‑matched quirk in `drivers/pinctrl/pinctrl-amd.c`. See the issue
tracker / linked report for status.

## License

GPL‑2.0. See [LICENSE](LICENSE).
