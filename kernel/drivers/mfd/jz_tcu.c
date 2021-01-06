/*
 * jz_tcu.c - JZ Soc TCU MFD driver.
 *
 * Copyright (C) 2015 Ingenic Semiconductor Co., Ltd.
 * Written by Zoro <yakun.li@ingenic.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/slab.h>

#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/syscore_ops.h>
#include <irq.h>
#include <linux/wakelock.h>
#include <linux/mutex.h>
#include <linux/clk.h>

#include <asm/div64.h>

#include <soc/base.h>
#include <soc/extal.h>
#include <soc/gpio.h>

#include <soc/irq.h>

#include <linux/mfd/core.h>
#include <linux/mfd/jz_tcu.h>

#define NR_TCU_CHNS TCU_NR_IRQS

static struct jz_tcu_chn g_tcu_chn[NR_TCU_CHNS] = {{0}};

static inline void jz_tcu_mask_full_irq(struct jz_tcu_chn *tcu_chn)
{
	tcu_writel(tcu_chn->tcu, TMSR, 1 << tcu_chn->index);
}

static inline void jz_tcu_unmask_full_irq(struct jz_tcu_chn *tcu_chn)
{
	tcu_writel(tcu_chn->tcu, TMCR, 1 << tcu_chn->index);
}

static inline void jz_tcu_clear_full_irq(struct jz_tcu_chn *tcu_chn)
{
	tcu_writel(tcu_chn->tcu, TFCR, 1 << tcu_chn->index);
}

static inline void jz_tcu_mask_half_irq(struct jz_tcu_chn *tcu_chn)
{
	tcu_writel(tcu_chn->tcu, TMSR, 1 << (tcu_chn->index + 16));
}

static inline void jz_tcu_unmask_half_irq(struct jz_tcu_chn *tcu_chn)
{
	tcu_writel(tcu_chn->tcu, TMCR, 1 << (tcu_chn->index + 16));
}

static inline void jz_tcu_clear_half_irq(struct jz_tcu_chn *tcu_chn)
{
	tcu_writel(tcu_chn->tcu, TFCR, 1 << (tcu_chn->index + 16));
}

static inline void jz_tcu_set_prescale(struct jz_tcu_chn *tcu_chn, enum tcu_prescale prescale)
{
	u32 tcsr = tcu_chn_readl(tcu_chn, CHN_TCSR) & ~(0x7 << 3);
	tcu_chn_writel(tcu_chn, CHN_TCSR, tcsr | (prescale << 3));
}


static inline void jz_tcu_set_pwm_output_init_level(struct jz_tcu_chn *tcu_chn, int level)
{
	if (level) {
		tcu_chn_writel(tcu_chn, CHN_TCSR, tcu_chn_readl(tcu_chn, CHN_TCSR) | (TCSR_PWM_HIGH));
	}
	else {
		tcu_chn_writel(tcu_chn, CHN_TCSR, tcu_chn_readl(tcu_chn, CHN_TCSR) & ~(TCSR_PWM_HIGH));
	}
}

static inline void jz_tcu_set_clksrc(struct jz_tcu_chn *tcu_chn, enum tcu_clksrc src)
{
	u32 tcsr = tcu_chn_readl(tcu_chn, CHN_TCSR) & ~0x7;
	tcu_chn_writel(tcu_chn, CHN_TCSR, tcsr | src);
}

void jz_tcu_config_chn(struct jz_tcu_chn *tcu_chn)
{
	/*update irq chip data*/
	irq_set_chip_data(tcu_chn->tcu->irq_base + tcu_chn->index, tcu_chn);

	/* Clear IRQ flag */
	jz_tcu_clear_full_irq(tcu_chn);
	jz_tcu_clear_half_irq(tcu_chn);

	/* Config IRQ */
	switch (tcu_chn->irq_type) {
	case NULL_IRQ_MODE :
		jz_tcu_mask_full_irq(tcu_chn);
		jz_tcu_mask_half_irq(tcu_chn);
		break;
	case FULL_IRQ_MODE :
		jz_tcu_unmask_full_irq(tcu_chn);
		jz_tcu_mask_half_irq(tcu_chn);
		break;
	case HALF_IRQ_MODE :
		jz_tcu_mask_full_irq(tcu_chn);
		jz_tcu_unmask_half_irq(tcu_chn);
		break;
	case FULL_HALF_IRQ_MODE :
		jz_tcu_unmask_full_irq(tcu_chn);
		jz_tcu_unmask_half_irq(tcu_chn);
		break;
	default:
		break;
	}


	/* init level */
	if(tcu_chn->init_level)
		jz_tcu_set_pwm_output_init_level(tcu_chn, 1);
	/* TCU mode */
	if(tcu_chn->index == 0 || tcu_chn->index == 3) {
		tcu_chn->tcu_mode = TCU_MODE_1;
		/* shutdown mode */
		if(tcu_chn->shutdown_mode) {
			tcu_chn_writel(tcu_chn, CHN_TCSR, tcu_chn_readl(tcu_chn, CHN_TCSR) | (TCSR_PWM_SD));
		}else {
			tcu_chn_writel(tcu_chn, CHN_TCSR, tcu_chn_readl(tcu_chn, CHN_TCSR) & ~(TCSR_PWM_SD));
		}
	} else {
		tcu_chn->tcu_mode = TCU_MODE_2;
	}

	/* clk source */
	jz_tcu_set_clksrc(tcu_chn, tcu_chn->clk_src);

	/* prescale */
	jz_tcu_set_prescale(tcu_chn, tcu_chn->prescale);

	/* pwm_out */
	if (tcu_chn->is_pwm) {
		tcu_chn_writel(tcu_chn, CHN_TCSR, tcu_chn_readl(tcu_chn, CHN_TCSR) | (TCSR_PWM_EN));
	} else {
		tcu_chn_writel(tcu_chn, CHN_TCSR, tcu_chn_readl(tcu_chn, CHN_TCSR) & ~(TCSR_PWM_EN));
	}
	/* pwm_bapass_mode */
	if(tcu_chn->pwm_bapass_mode) {
		tcu_chn_writel(tcu_chn, CHN_TCSR, tcu_chn_readl(tcu_chn, CHN_TCSR) | (TCSR_PWM_BYPASS));
	} else {
		tcu_chn_writel(tcu_chn, CHN_TCSR, tcu_chn_readl(tcu_chn, CHN_TCSR) & ~(TCSR_PWM_BYPASS));
	}
	/* pwm_in */
	if(tcu_chn->pwm_in) {
		tcu_chn_writel(tcu_chn, CHN_TCSR, tcu_chn_readl(tcu_chn, CHN_TCSR) | (TCSR_PWM_IN));
	} else {
		tcu_chn_writel(tcu_chn, CHN_TCSR, tcu_chn_readl(tcu_chn, CHN_TCSR) & ~(TCSR_PWM_IN));
	}



}
EXPORT_SYMBOL_GPL(jz_tcu_config_chn);

static void jz_tcu_irq_mask(struct irq_data *data)
{
	unsigned long flags;
	struct jz_tcu_chn *tcu_chn = irq_data_get_irq_chip_data(data);

	spin_lock_irqsave(&tcu_chn->tcu->lock, flags);
	switch (tcu_chn->irq_type) {
	case FULL_IRQ_MODE :
		jz_tcu_mask_full_irq(tcu_chn);
		break;
	case HALF_IRQ_MODE :
		jz_tcu_mask_half_irq(tcu_chn);
		break;
	case FULL_HALF_IRQ_MODE :
		jz_tcu_mask_full_irq(tcu_chn);
		jz_tcu_mask_half_irq(tcu_chn);
		break;
	default:
		break;
	}
	spin_unlock_irqrestore(&tcu_chn->tcu->lock, flags);
}

static void jz_tcu_irq_unmask(struct irq_data *data)
{
	unsigned long flags;
	struct jz_tcu_chn *tcu_chn = irq_data_get_irq_chip_data(data);

	spin_lock_irqsave(&tcu_chn->tcu->lock, flags);
	switch (tcu_chn->irq_type) {
	case FULL_IRQ_MODE :
		jz_tcu_unmask_full_irq(tcu_chn);
		break;
	case HALF_IRQ_MODE :
		jz_tcu_unmask_half_irq(tcu_chn);
		break;
	case FULL_HALF_IRQ_MODE :
		jz_tcu_unmask_full_irq(tcu_chn);
		jz_tcu_unmask_half_irq(tcu_chn);
		break;
	default:
		break;
	}
	spin_unlock_irqrestore(&tcu_chn->tcu->lock, flags);
}

static void jz_tcu_irq_ack(struct irq_data *data)
{
	unsigned long flags;
	struct jz_tcu_chn *tcu_chn = irq_data_get_irq_chip_data(data);

	spin_lock_irqsave(&tcu_chn->tcu->lock, flags);
	switch (tcu_chn->irq_type) {
	case FULL_IRQ_MODE :
		jz_tcu_clear_full_irq(tcu_chn);
		break;
	case HALF_IRQ_MODE :
		jz_tcu_clear_half_irq(tcu_chn);
		break;
	case FULL_HALF_IRQ_MODE :
		jz_tcu_clear_full_irq(tcu_chn);
		jz_tcu_clear_half_irq(tcu_chn);
		break;
	default:
		break;
	}
	spin_unlock_irqrestore(&tcu_chn->tcu->lock, flags);
}

static void jz_tcu_irq_demux(unsigned int irq, struct irq_desc *desc)
{
	struct jz_tcu *tcu = irq_desc_get_handler_data(desc);
	uint8_t status;
	unsigned int i;

	status = tcu_readl(tcu, TFR);
	for (i = 0; i < TCU_NR_IRQS; i++) {
		if (status & (1 << i))
			generic_handle_irq(tcu->irq_base + i);
	}
}

static struct irq_chip jz_tcu_irq_chip = {
	.name = "jz-tcu",
	.irq_mask = jz_tcu_irq_mask,
	.irq_disable = jz_tcu_irq_mask,
	.irq_unmask = jz_tcu_irq_unmask,
	.irq_ack = jz_tcu_irq_ack,
};

#define TCU_CELL_RES(NO)						\
	static struct resource tcucell##NO##_resources[] = {	\
		{						\
			.start = NO,				\
			.flags = IORESOURCE_IRQ,		\
		},						\
	}
TCU_CELL_RES(0);
TCU_CELL_RES(1);
TCU_CELL_RES(2);
TCU_CELL_RES(3);
#undef TCU_CELL_RES

static struct mfd_cell jz_tcu_cells[] = {
#define DEF_TCU_CELL_NAME(NO,NAME)					\
	{								\
		.id = NO,						\
		.name = NAME,						\
		.num_resources = ARRAY_SIZE(tcucell##NO##_resources),	\
		.resources = tcucell##NO##_resources,			\
		.platform_data = &g_tcu_chn[NO],			\
		.pdata_size = sizeof(struct jz_tcu_chn)			\
	}

	DEF_TCU_CELL_NAME(0, "tcu_chn0"),
	DEF_TCU_CELL_NAME(1, "tcu_chn1"),
	DEF_TCU_CELL_NAME(2, "tcu_chn2"),
	DEF_TCU_CELL_NAME(3, "tcu_chn3"),
};
#undef DEF_TCU_CELL_NAME

#define TX2_FREQ 1000  /* frequency of TX2 waveform */
#define GPIO_TX2_OUT 55  /* UART1 TX */
#define	GPIO_IOBASE	0x10010000
#define GPIO_PORT_OFF	0x100
#define PXPAT0S		0x44   /* Port Pattern 0 Set Register */
#define PXPAT0C		0x48   /* Port Pattern 0 Clear Register */

void jz_stop_console(void);

/* get command from user */
static ssize_t tx2_cmd_write(struct file *filp, struct kobject *kobj,
				struct bin_attribute *bin_attr,
				char *buffer, loff_t pos, size_t count)
	{
	unsigned long flags;
	struct device *dev = container_of(kobj, struct device, kobj);
	struct jz_tcu *tcu = dev_get_drvdata(dev);
	struct tx2_setup *p_tx2_cmd = (struct tx2_setup *) buffer;
	
		/* grab GPIO if first time */
	if (!(tcu->tx2_flags & TX2_FLAGS_INIT)) {
//		jz_stop_console();  /* debug only */
		jz_gpio_set_func(GPIO_TX2_OUT, GPIO_OUTPUT1);  /* set as GPIO output */
		tcu->tx2_flags |= TX2_FLAGS_INIT;
		}
	spin_lock_irqsave(&tcu->lock, flags);
	if (p_tx2_cmd->command <= TX2_MAX_SEQ * 2) {
		tcu->tx2_cmd.command = p_tx2_cmd->command; // memcpy(&tcu->tx2_cmd, p_tx2_cmd, count);
		}
	else {
		count = -1;
		}
	spin_unlock_irqrestore(&tcu->lock, flags);
//	printk(KERN_INFO "Rick debug tx2_cmd = %d\n", tcu->tx2_cmd.command);
	return count;
	}

/* shows up in /sys/devices/platform/jz-tcu/cmd */
static const struct bin_attribute tx2_cmd_attr = {
	.attr = {.name = "cmd", .mode = 0666},
	.size = sizeof(struct tx2_setup),	/* Limit image size */
	.write = tx2_cmd_write,
//	.read = tx2_cmd_read,
};

/* interrupt routine running at 2000 Hz */
static irqreturn_t tx2_driver_irq(int irq, void *devid)
	{
	struct jz_tcu_chn *tcu_chn = devid;
	struct jz_tcu *tcu = tcu_chn->tcu;
	int bit = (1 << tcu_chn->index) | (1 << (tcu_chn->index + 16));

	if (tcu_readl(tcu, TFR) & bit) {
		tcu_writel(tcu, TFCR, bit);  /* ack interrupt */
		if (tcu->tx2_cmd.command && !tcu->tx2_seq_max) {  /* did we get a command? */
			tcu->tx2_seq_max = tcu->tx2_cmd.command * 2 + TX2_START_CODE_MAX * 4;
			tcu->duration = 100 / (tcu->tx2_cmd.command + TX2_START_CODE_MAX * 2);  /* translate duration to ticks */
//			printk(KERN_INFO "Rick debug tx2_cmd = %d max = %d dur = %d\n", tcu->tx2_cmd.command, tcu->tx2_seq_max, tcu->duration);
			tcu->tx2_cmd.command = 0;
			}
		if (tcu->tx2_seq_max) {
			bit = 1 << (GPIO_TX2_OUT % 32);
			if (tcu->tx2_seq[tcu->tx2_seq_cntr++] == GPIO_OUTPUT1)
				* (int *) (tcu->gpio_reg + PXPAT0S) = bit;  /* GPIO high */
			else
				* (int *) (tcu->gpio_reg + PXPAT0C) = bit;  /* GPIO low */
			if (tcu->tx2_seq_cntr >= tcu->tx2_seq_max) {
				tcu->tx2_seq_cntr = 0;  /* send another */
				if (--tcu->duration <= 0) {
					tcu->tx2_seq_max = 0;  /* stop sending */
					}
				}
			}
		}
	return IRQ_HANDLED;
	}

static int jztcu_probe(struct platform_device *pdev)
{
	struct jz_tcu *tcu;
	struct resource *mem_base;
	int irq, irq_base, i, ret = 0;

	struct jzpwm_platform_data  *pdata = pdev->dev.platform_data;
	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "Failed to get platform irq\n");
		return irq;
	}

	irq_base = platform_get_irq(pdev, 1);
	if (irq_base < 0) {
		dev_err(&pdev->dev, "Failed to get irq base\n");
		return irq_base;
	}

	mem_base = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_base) {
		dev_err(&pdev->dev, "No iomem resource\n");
		return -ENXIO;
	}

	tcu = kzalloc(sizeof(struct jz_tcu), GFP_KERNEL);
	if (!tcu) {
		dev_err(&pdev->dev, "Failed to allocate driver struct\n");
		return -ENOMEM;
	}

	tcu->irq = irq;
	tcu->irq_base = irq_base;

	tcu->iomem = ioremap_nocache(mem_base->start, resource_size(mem_base));
	if (!tcu->iomem)
		goto err_ioremap;

	spin_lock_init(&tcu->lock);

	platform_set_drvdata(pdev, tcu);

	for (i = 0; i < NR_TCU_CHNS; i++) {
		g_tcu_chn[i].index = i;
		g_tcu_chn[i].reg_base = 0x40 + i * 0x10;
		g_tcu_chn[i].irq_type = NULL_IRQ_MODE;
		g_tcu_chn[i].pwm_bapass_mode = 0;
		g_tcu_chn[i].pwm_in = 0;
		g_tcu_chn[i].is_pwm = 0;
		g_tcu_chn[i].clk_src = TCU_CLKSRC_EXT;
		g_tcu_chn[i].tcu_mode = TCU_MODE_1;
		g_tcu_chn[i].prescale = TCU_PRESCALE_1;
		g_tcu_chn[i].init_level = 0;
		g_tcu_chn[i].shutdown_mode = 0;
		g_tcu_chn[i].gpio = pdata->pwm_gpio[i];
		g_tcu_chn[i].tcu = tcu;
	}

	for (i = tcu->irq_base; i < tcu->irq_base + TCU_NR_IRQS; i++) {
		irq_set_chip_data(i, &g_tcu_chn);
		irq_set_chip_and_handler(i, &jz_tcu_irq_chip,
				handle_level_irq);
	}

	irq_set_handler_data(tcu->irq, tcu);
	irq_set_chained_handler(tcu->irq, jz_tcu_irq_demux);

	ret = mfd_add_devices(&pdev->dev, 0, jz_tcu_cells,
			ARRAY_SIZE(jz_tcu_cells), mem_base, tcu->irq_base,NULL);
	if (ret < 0) {
		goto err_mfd_add;
	}
	
		{  /* TX2 test code */
		int cntr;
		int my_ch = 3;  /* debug shows chan 0 and 2 are init'd via jz_tcu_config_chn
steal a channel and interrupt
when 3:
264:       8317    jz-tcu
266:          0    jz-tcu  jz_timer_interrupt
267:       8317    jz-tcu  tx2-driver

when 1:
264:       9578    jz-tcu
265:       9578    jz-tcu  tx2-driver
266:          0    jz-tcu  jz_timer_interrupt
 */
		struct jz_tcu_chn *tcu_chn = &g_tcu_chn[my_ch];
		struct clk *ext_clk = clk_get(NULL,"ext1");
		int rate = clk_get_rate(ext_clk);
		int ret;
		
		printk("Rick debug jz_tcu_config_chn chan = %d, irq = %d, is_pwm = %d, irq_base = %d rate = %d\n", tcu_chn->index, tcu_chn->tcu->irq, tcu_chn->is_pwm, tcu_chn->tcu->irq_base, rate);

		ret = request_threaded_irq(tcu->irq_base + my_ch, tx2_driver_irq,
			0, 0, "tx2-driver", tcu_chn);
		jz_tcu_set_period(tcu_chn, (rate / (TX2_FREQ * 2)) - 1);  /* CH_TDFR set to 2000 Hz */
		jz_tcu_set_duty(tcu_chn, 0);  /* CH_TCNT */
		tcu_writel(tcu, TMCR, 1 << my_ch);
		tcu_writel(tcu, TFCR, 1 << my_ch);
		jz_tcu_set_clksrc(tcu_chn, TCU_CLKSRC_EXT);
		jz_tcu_set_prescale(tcu_chn, TCU_PRESCALE_1);
		jz_tcu_enable_counter(tcu_chn); /* TESR */

		tcu->gpio_reg = ioremap(GPIO_IOBASE + (GPIO_TX2_OUT / 32) * GPIO_PORT_OFF, GPIO_PORT_OFF - 1);
		ret = sysfs_create_bin_file(&pdev->dev.kobj, &tx2_cmd_attr);
		if (ret) {
			printk(KERN_ERR "Rick pwm-tx2: Failed to create sysfs files\n");
			}
		for (cntr = 0; cntr < TX2_START_CODE_MAX * 4;) {  /* start code */
			tcu->tx2_seq[cntr++] = GPIO_OUTPUT0;
			tcu->tx2_seq[cntr++] = GPIO_OUTPUT0;
			tcu->tx2_seq[cntr++] = GPIO_OUTPUT0;
			tcu->tx2_seq[cntr++] = GPIO_OUTPUT1;
			}
		for (; cntr < sizeof(tcu->tx2_seq); ) {  /* fill in rest with square wave */
			tcu->tx2_seq[cntr++] = GPIO_OUTPUT0;
			tcu->tx2_seq[cntr++] = GPIO_OUTPUT1;
			}
		}

	printk("jz TCU driver register completed\n");

	return 0;
err_mfd_add:
	iounmap(tcu->iomem);
err_ioremap:
	kfree(tcu);

	return ret;
}

static int jztcu_remove(struct platform_device *pdev)
{
	struct jz_tcu *tcu = platform_get_drvdata(pdev);

	mfd_remove_devices(&pdev->dev);

	irq_set_handler_data(tcu->irq, NULL);
	irq_set_chained_handler(tcu->irq, NULL);

	iounmap(tcu->iomem);

	platform_set_drvdata(pdev, NULL);

	kfree(tcu);

	return 0;
}

struct platform_driver jztcu_driver = {
	.probe	= jztcu_probe,
	.remove	= jztcu_remove,
	.driver = {
		.name	= "jz-tcu",
		.owner	= THIS_MODULE,
	},
};

static int __init jztcu_init(void)
{
	return platform_driver_register(&jztcu_driver);
}
module_init(jztcu_init);

static void __exit jztcu_exit(void)
{
	platform_driver_unregister(&jztcu_driver);
}
module_exit(jztcu_exit);

MODULE_LICENSE("GPL v2");
