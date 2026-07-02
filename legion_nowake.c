// SPDX-License-Identifier: GPL-2.0
/*
 * legion_nowake - disarm BIOS-set AMD GPIO wake bits that cause instant
 * resume from S3/s2idle when an external display is connected.
 *
 * Lenovo Legion Pro 5 16ARX8 (82WM): the BIOS arms AGPIO #2 and #4 (the
 * external-display HPD lines) as S0i3/S3 wake sources directly in the FCH
 * GPIO registers, with no ACPI/_AEI owner. pinctrl_amd faithfully preserves
 * them, so the HPD edge produced while entering sleep wakes the machine
 * ~1s later. There is no userspace lever (IO_STRICT_DEVMEM blocks the MMIO
 * and there is no ACPI description for gpiolib_acpi.ignore_wake to match),
 * so we clear the wake-enable bits from a tiny kernel module.
 *
 * This only removes the *wake-from-sleep* capability of those two pins; the
 * interrupt-enable bits are left intact, so runtime hotplug detection still
 * works. Power button, USB and other wake sources are unaffected.
 */
#include <linux/module.h>
#include <linux/io.h>
#include <linux/dmi.h>
#include <linux/suspend.h>
#include <linux/notifier.h>

#define GPIO_BASE   0xFED81500UL   /* AMDI0030 _CRS Memory32Fixed base   */
#define GPIO_SIZE   0x400
/* pinctrl-amd per-pin register: S-state wake-enable bits */
#define WAKE_S0I3   BIT(13)
#define WAKE_S3     BIT(14)
#define WAKE_S4S5   BIT(15)
#define WAKE_MASK   (WAKE_S0I3 | WAKE_S3 | WAKE_S4S5)

static void __iomem *gpio_base;

static int pins[] = { 2, 4 };
static int npins = ARRAY_SIZE(pins);
module_param_array(pins, int, &npins, 0444);
MODULE_PARM_DESC(pins, "AMD GPIO pin numbers whose wake bits to clear (default: 2,4)");

static bool force;
module_param(force, bool, 0444);
MODULE_PARM_DESC(force, "Load even if the DMI model does not match");

static const struct dmi_system_id legion_nowake_dmi[] = {
	{
		.ident = "Lenovo Legion Pro 5 16ARX8",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "Legion Pro 5 16ARX8"),
		},
	},
	{
		.ident = "Lenovo Legion R900P ADR10",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "Legion R9000P ADR10"),
		},
	},
	{ }
};
MODULE_DEVICE_TABLE(dmi, legion_nowake_dmi);

static void legion_clear_wakes(void)
{
	int i;

	for (i = 0; i < npins; i++) {
		u32 off = pins[i] * 4;
		u32 val = readl(gpio_base + off);
		u32 new = val & ~WAKE_MASK;

		if (new != val) {
			writel(new, gpio_base + off);
			pr_info("legion_nowake: GPIO#%d %08x -> %08x (wake disarmed)\n",
				pins[i], val, new);
		}
	}
}

static int legion_pm_notify(struct notifier_block *nb,
			    unsigned long action, void *ptr)
{
	if (action == PM_SUSPEND_PREPARE)
		legion_clear_wakes();
	return NOTIFY_OK;
}

static struct notifier_block legion_pm_nb = {
	.notifier_call = legion_pm_notify,
};

static int __init legion_nowake_init(void)
{
	if (!force && !dmi_check_system(legion_nowake_dmi)) {
		pr_info("legion_nowake: machine not matched; pass force=1 to override. Not loading.\n");
		return -ENODEV;
	}

	gpio_base = ioremap(GPIO_BASE, GPIO_SIZE);
	if (!gpio_base)
		return -ENOMEM;

	legion_clear_wakes();			/* once now ... */
	register_pm_notifier(&legion_pm_nb);	/* ... and before every suspend */
	return 0;
}

static void __exit legion_nowake_exit(void)
{
	unregister_pm_notifier(&legion_pm_nb);
	iounmap(gpio_base);
}

module_init(legion_nowake_init);
module_exit(legion_nowake_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lenart");
MODULE_DESCRIPTION("Disarm BIOS-set AMD GPIO #2/#4 sleep-wake (Legion Pro 5 16ARX8 instant-wake fix)");
MODULE_VERSION("1.0");
