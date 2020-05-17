// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/pm.h>

static void __iomem *msm_ps_hold, *restart_reason;
static int deassert_pshold(struct notifier_block *nb, unsigned long action,
			   void *data)
{
	writel(0, msm_ps_hold);
	mdelay(10000);

	return NOTIFY_DONE;
}

static struct notifier_block restart_nb = {
	.notifier_call = deassert_pshold,
	.priority = 128,
};

static void do_msm_poweroff(void)
{
	printk(KERN_INFO "msm-poweroff: do_msm_poweroff\n");
	deassert_pshold(&restart_nb, 0, NULL);
}

static int msm_restart_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *mem;
	struct device_node *np;
	struct resource res;

	printk(KERN_INFO "msm-poweroff: probing\n");

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	msm_ps_hold = devm_ioremap_resource(dev, mem);
	if (IS_ERR(msm_ps_hold))
		return PTR_ERR(msm_ps_hold);

	np = of_find_compatible_node(NULL, NULL, "qcom,msm-imem-restart-reason");
	restart_reason = NULL;
	if (!np) {
		pr_err("unable to find msm-imem-restart-reason node\n");
	} else {
		if (of_address_to_resource(np, 0, &res)) {
			pr_err("unable to get resource for msm-imem-restart-reason\n");
		} else {
			printk("got resource for msm-imem-restart-reason, phys addr = %llx, sz = %llu\n", 
				res.start, resource_size(&res));
			restart_reason = ioremap(res.start, resource_size(&res));
			if (restart_reason) {
				printk(KERN_INFO "mapped msm-imem-restart-reason to %p\n", restart_reason);
			} else {
				pr_err("unable to map msm-imem-restart-reason\n");
			}
		}
	}

	printk(KERN_INFO "msm-poweroff: registering\n");

	register_restart_handler(&restart_nb);

	pm_power_off = do_msm_poweroff;

	return 0;
}

static const struct of_device_id of_msm_restart_match[] = {
	{ .compatible = "qcom,pshold", },
	{},
};
MODULE_DEVICE_TABLE(of, of_msm_restart_match);

static struct platform_driver msm_restart_driver = {
	.probe = msm_restart_probe,
	.driver = {
		.name = "msm-restart",
		.of_match_table = of_match_ptr(of_msm_restart_match),
	},
};

static int __init msm_restart_init(void)
{
	return platform_driver_register(&msm_restart_driver);
}
device_initcall(msm_restart_init);

// tmp: sdm665
static int dummy_reboot_mode = 0;
int set_reboot_mode(const char *cmd, const struct kernel_param *kp)
{
	printk(KERN_INFO "setting reboot mode %s\n", cmd);
	if (!restart_reason) {
		printk(KERN_INFO "restart_reason not mapped!\n");
	} else {
		if (!strncmp(cmd, "bootloader", 10)) {
			printk(KERN_INFO "rebooting to bootloader\n");
			__raw_writel(0x77665500, restart_reason);
		} else if (!strncmp(cmd, "recovery", 8)) {
			printk(KERN_INFO "rebooting to recovery\n");
			__raw_writel(0x77665502, restart_reason);
		} else {
			printk(KERN_INFO "rebooting to system\n");
			__raw_writel(0x77665501, restart_reason);
		}
	}
	do_msm_poweroff();
	return 0;
}
const struct kernel_param_ops reboot_mode_ops = {
	.set = &set_reboot_mode,
};
module_param_cb(reboot_mode, &reboot_mode_ops, &dummy_reboot_mode, S_IRUGO | S_IWUSR);


