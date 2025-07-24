// SPDX-License-Identifier: GPL-2.0+
/*
 * Spreadtrum Pike2 EIC (debounce) driver U-Boot
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

#define SPRD_EIC_DBNC_DATA     0x00
#define SPRD_EIC_DBNC_DMSK     0x04

#define SPRD_EIC_MAX_BANK      8
#define SPRD_EIC_PER_BANK_NR   8
#define SPRD_EIC_NR            64
#define SPRD_EIC_BIT(x)        ((x) & (SPRD_EIC_PER_BANK_NR - 1))

struct sprd_eic_priv {
	void __iomem *reg_base[SPRD_EIC_MAX_BANK];
};

static inline void __iomem *sprd_eic_bank_base(struct sprd_eic_priv *priv, u32 offset)
{
	u32 bank = offset / SPRD_EIC_PER_BANK_NR;

	if (bank >= SPRD_EIC_MAX_BANK || !priv->reg_base[bank])
		return NULL;

	return priv->reg_base[bank];
}

static void sprd_eic_update(struct sprd_eic_priv *priv, u32 offset, u16 reg, int value)
{
	void __iomem *base = sprd_eic_bank_base(priv, offset);
	u32 bit = BIT(SPRD_EIC_BIT(offset));
	u32 tmp;

	if (!base)
		return;

	tmp = readl(base + reg);

	if (value)
		tmp |= bit;
	else
		tmp &= ~bit;

	writel(tmp, base + reg);
}

static int sprd_eic_read(struct sprd_eic_priv *priv, u32 offset, u16 reg)
{
	void __iomem *base = sprd_eic_bank_base(priv, offset);

	if (!base)
		return -EINVAL;

	return !!(readl(base + reg) & BIT(SPRD_EIC_BIT(offset)));
}

static int sprd_eic_request(struct udevice *dev, u32 offset, const char *label)
{
	struct sprd_eic_priv *priv = dev_get_priv(dev);

	sprd_eic_update(priv, offset, SPRD_EIC_DBNC_DMSK, 1);
	return 0;
}

static int sprd_eic_free(struct udevice *dev, u32 offset)
{
	struct sprd_eic_priv *priv = dev_get_priv(dev);

	sprd_eic_update(priv, offset, SPRD_EIC_DBNC_DMSK, 0);
	return 0;
}

static int sprd_eic_direction_input(struct udevice *dev, u32 offset)
{
	/* No special setup needed */
	return 0;
}

static int sprd_eic_get_value(struct udevice *dev, u32 offset)
{
	struct sprd_eic_priv *priv = dev_get_priv(dev);

	return sprd_eic_read(priv, offset, SPRD_EIC_DBNC_DATA);
}

static const struct dm_gpio_ops sprd_eic_ops = {
	.request		= sprd_eic_request,
	.free			= sprd_eic_free,
	.get_value		= sprd_eic_get_value,
	.direction_input	= sprd_eic_direction_input,
};

static int sprd_eic_probe(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct sprd_eic_priv *priv = dev_get_priv(dev);
	int i;

	uc_priv->gpio_count = SPRD_EIC_NR;
	uc_priv->bank_name = dev_read_string(dev, "gpio-bank-name");

	for (i = 0; i < SPRD_EIC_MAX_BANK; i++) {
		priv->reg_base[i] = dev_read_addr_index(dev, i);
		if (priv->reg_base[i] == (void __iomem *)FDT_ADDR_T_NONE)
			break;
	}

	return 0;
}

static const struct udevice_id sprd_eic_ids[] = {
	{ .compatible = "sprd,sc9860-eic-debounce" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(eic_sprd) = {
	.name	= "eic_sprd",
	.id	= UCLASS_GPIO,
	.of_match = sprd_eic_ids,
	.probe	= sprd_eic_probe,
	.ops	= &sprd_eic_ops,
	.priv_auto_alloc_size = sizeof(struct sprd_eic_priv),
};
