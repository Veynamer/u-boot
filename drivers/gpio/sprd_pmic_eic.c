// SPDX-License-Identifier: GPL-2.0+
/*
 * Spreadtrum SC2731 PMIC EIC GPIO driver U-Boot
 */

#include <dm.h>
#include <errno.h>
#include <fdtdec.h>
#include <malloc.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <asm/io.h>
#include <asm/gpio.h>
#include <dm/device-internal.h>
#include <dt-bindings/gpio/gpio.h>

/* PMIC EIC Registers */
#define SPRD_PMIC_EIC_DATA    0x00
#define SPRD_PMIC_EIC_DMSK    0x04

#define SPRD_PMIC_EIC_NR      16
#define SPRD_PMIC_EIC_BIT(x)  ((x) & (SPRD_PMIC_EIC_NR - 1))

struct sprd_pmic_eic_priv {
	struct regmap *map;
	u32 reg_base;
};

static int sprd_pmic_eic_update(struct udevice *dev, uint offset,
				uint reg, int value)
{
	struct sprd_pmic_eic_priv *priv = dev_get_priv(dev);
	u32 bit = BIT(SPRD_PMIC_EIC_BIT(offset));
	u32 addr = priv->reg_base + reg;

	return regmap_update_bits(priv->map, addr, bit, value ? bit : 0);
}

static int sprd_pmic_eic_read(struct udevice *dev, uint offset, uint reg)
{
	struct sprd_pmic_eic_priv *priv = dev_get_priv(dev);
	u32 val;
	int ret;

	ret = regmap_read(priv->map, priv->reg_base + reg, &val);
	if (ret)
		return ret;

	return !!(val & BIT(SPRD_PMIC_EIC_BIT(offset)));
}

static int sprd_pmic_eic_request(struct udevice *dev, uint offset,
				 const char *label)
{
	return sprd_pmic_eic_update(dev, offset, SPRD_PMIC_EIC_DMSK, 1);
}

static int sprd_pmic_eic_free(struct udevice *dev, uint offset)
{
	return sprd_pmic_eic_update(dev, offset, SPRD_PMIC_EIC_DMSK, 0);
}

static int sprd_pmic_eic_get_value(struct udevice *dev, uint offset)
{
	return sprd_pmic_eic_read(dev, offset, SPRD_PMIC_EIC_DATA);
}

static int sprd_pmic_eic_direction_input(struct udevice *dev, uint offset)
{
	/* EICs are always input */
	return 0;
}

static const struct dm_gpio_ops sprd_pmic_eic_ops = {
	.request	= sprd_pmic_eic_request,
	.free		= sprd_pmic_eic_free,
	.direction_input = sprd_pmic_eic_direction_input,
	.get_value	= sprd_pmic_eic_get_value,
};

static int sprd_pmic_eic_probe(struct udevice *dev)
{
	struct sprd_pmic_eic_priv *priv = dev_get_priv(dev);
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	priv->reg_base = dev_read_addr(dev);
	if (priv->reg_base == FDT_ADDR_T_NONE)
		return -EINVAL;

	priv->map = syscon_get_regmap(dev->parent);
	if (!priv->map) {
		dev_err(dev, "failed to get regmap from parent\n");
		return -ENODEV;
	}

	uc_priv->gpio_count = SPRD_PMIC_EIC_NR;
	uc_priv->bank_name = dev_read_string(dev, "gpio-bank-name");

	return 0;
}

static const struct udevice_id sprd_pmic_eic_ids[] = {
	{ .compatible = "sprd,sc2731-eic" },
	{ }
};

U_BOOT_DRIVER(sprd_pmic_eic) = {
	.name	= "sprd_pmic_eic",
	.id	= UCLASS_GPIO,
	.of_match = sprd_pmic_eic_ids,
	.probe	= sprd_pmic_eic_probe,
	.priv_auto_alloc_size = sizeof(struct sprd_pmic_eic_priv),
	.ops	= &sprd_pmic_eic_ops,
};
