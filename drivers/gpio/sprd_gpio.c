// SPDX-License-Identifier: GPL-2.0+
/*
 * Spreadtrum GPIO driver U-Boot
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

/* GPIO registers offset */
#define SPRD_GPIO_DATA     0x00
#define SPRD_GPIO_DMSK     0x04
#define SPRD_GPIO_DIR      0x08
#define SPRD_GPIO_INEN     0x28

#define SPRD_GPIO_NR       256
#define SPRD_GPIO_BANK_NR  16
#define SPRD_GPIO_BANK_SIZE 0x80
#define SPRD_GPIO_BIT(x)   ((x) & (SPRD_GPIO_BANK_NR - 1))

struct sprd_gpio_priv {
	void __iomem *base;
};

static inline void __iomem *sprd_gpio_bank_base(struct sprd_gpio_priv *priv, u32 bank)
{
	return priv->base + SPRD_GPIO_BANK_SIZE * bank;
}

static void sprd_gpio_update(struct udevice *dev, u32 offset, u16 reg, int value)
{
	struct sprd_gpio_priv *priv = dev_get_priv(dev);
	void __iomem *bank = sprd_gpio_bank_base(priv, offset / SPRD_GPIO_BANK_NR);
	u32 bit = BIT(SPRD_GPIO_BIT(offset));
	u32 tmp = readl(bank + reg);

	if (value)
		tmp |= bit;
	else
		tmp &= ~bit;

	writel(tmp, bank + reg);
}

static int sprd_gpio_read(struct udevice *dev, u32 offset, u16 reg)
{
	struct sprd_gpio_priv *priv = dev_get_priv(dev);
	void __iomem *bank = sprd_gpio_bank_base(priv, offset / SPRD_GPIO_BANK_NR);

	return !!(readl(bank + reg) & BIT(SPRD_GPIO_BIT(offset)));
}

static int sprd_gpio_request(struct udevice *dev, u32 offset, const char *label)
{
	sprd_gpio_update(dev, offset, SPRD_GPIO_DMSK, 1);
	return 0;
}

static int sprd_gpio_free(struct udevice *dev, u32 offset)
{
	sprd_gpio_update(dev, offset, SPRD_GPIO_DMSK, 0);
	return 0;
}

static int sprd_gpio_direction_input(struct udevice *dev, u32 offset)
{
	sprd_gpio_update(dev, offset, SPRD_GPIO_DIR, 0);
	sprd_gpio_update(dev, offset, SPRD_GPIO_INEN, 1);
	return 0;
}

static int sprd_gpio_direction_output(struct udevice *dev, u32 offset, int value)
{
	sprd_gpio_update(dev, offset, SPRD_GPIO_DATA, !!value);
	sprd_gpio_update(dev, offset, SPRD_GPIO_DIR, 1);
	sprd_gpio_update(dev, offset, SPRD_GPIO_INEN, 0);
	return 0;
}

static int sprd_gpio_get_value(struct udevice *dev, u32 offset)
{
	return sprd_gpio_read(dev, offset, SPRD_GPIO_DATA);
}

static int sprd_gpio_set_value(struct udevice *dev, u32 offset, int value)
{
	sprd_gpio_update(dev, offset, SPRD_GPIO_DATA, !!value);
	return 0;
}

static const struct dm_gpio_ops sprd_gpio_ops = {
	.request		= sprd_gpio_request,
	.rfree			= sprd_gpio_free,
	.direction_input	= sprd_gpio_direction_input,
	.direction_output	= sprd_gpio_direction_output,
	.get_value		= sprd_gpio_get_value,
	.set_value		= sprd_gpio_set_value,
};

static int sprd_gpio_probe(struct udevice *dev)
{
	struct sprd_gpio_priv *priv = dev_get_priv(dev);
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);

	priv->base = dev_read_addr_ptr(dev);
	if (!priv->base)
		return -EINVAL;

	uc_priv->gpio_count = SPRD_GPIO_NR;
	uc_priv->bank_name = dev_read_string(dev, "gpio-bank-name");

	return 0;
}

static const struct udevice_id sprd_gpio_ids[] = {
	{ .compatible = "sprd,sc9860-gpio" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(gpio_sprd) = {
	.name	= "gpio_sprd",
	.id	= UCLASS_GPIO,
	.of_match = sprd_gpio_ids,
	.probe	= sprd_gpio_probe,
	.ops	= &sprd_gpio_ops,
};
