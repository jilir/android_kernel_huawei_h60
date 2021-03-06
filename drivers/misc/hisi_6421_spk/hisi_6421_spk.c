#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_platform.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>

#include <linux/poll.h>

#include "hisi_6421_spk.h"

#define LOG_TAG "HISI_6421_SPK"

#define PRINT_INFO  0
#define PRINT_WARN  0
#define PRINT_DEBUG 0
#define PRINT_ERR   1

#if PRINT_INFO
#define logi(fmt, ...) printk("[" LOG_TAG "][I]" fmt "\n", ##__VA_ARGS__)
#else
#define logi(fmt, ...) ((void)0)
#endif

#if PRINT_WARN
#define logw(fmt, ...) printk("[" LOG_TAG "][W]" fmt "\n", ##__VA_ARGS__)
#else
#define logw(fmt, ...) ((void)0)
#endif

#if PRINT_DEBUG
#define logd(fmt, ...) printk("[" LOG_TAG "][D]" fmt "\n", ##__VA_ARGS__)
#else
#define logd(fmt, ...) ((void)0)
#endif

#if PRINT_ERR
#define loge(fmt, ...) printk("[" LOG_TAG "][E]" fmt "\n", ##__VA_ARGS__)
#else
#define loge(fmt, ...) ((void)0)
#endif

struct hisi_6421_spk_platform_data *g_pdata = NULL;

extern void boost5v_classd_enable(bool enable);

#if 0
static int hisi_6421_spk_reg_read(unsigned int reg)
{
	BUG_ON(NULL == g_pdata);
	readl(g_pdata->reg_base_addr + reg);
}
#endif
static void hisi_6421_spk_reg_write(unsigned int reg, unsigned int value)
{
	BUG_ON(NULL == g_pdata);
	writel(value, g_pdata->reg_base_addr + reg);
}

static int hisi_6421_spk_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int hisi_6421_spk_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long hisi_6421_spk_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	BUG_ON(NULL == g_pdata);

	mutex_lock(&g_pdata->io_mutex);
	switch (cmd) {
	case HISI_6421_SPK_ENABLE:
		ret = regulator_bulk_enable(1, &g_pdata->regu_ldo8);
		if (0 != ret) {
			pr_err("%s : couldn't enable regulators(ldo8) %d\n",
					__FUNCTION__, ret);
			goto exit;
		}

#if 0
		ret = regulator_bulk_enable(1, &g_pdata->regu_boost5v);
		if (0 != ret) {
			pr_err("%s : couldn't enable regulators(boost5v) %d\n",
					__FUNCTION__, ret);
			goto exit;
		}
#endif
		boost5v_classd_enable(true);

		/*
		 * 1. enable ClassD
		 * 2. sleep 10us ~ 15us
		 * 3. config dead time as 7us
		 */
		hisi_6421_spk_reg_write(HISI_6421_SPK_CRTL_1, 0x31);
		usleep_range(10, 15);
		hisi_6421_spk_reg_write(HISI_6421_SPK_CRTL_3, 2);
		break;
	case HISI_6421_SPK_DISABLE:
		hisi_6421_spk_reg_write(HISI_6421_SPK_CRTL_3, 0);
		hisi_6421_spk_reg_write(HISI_6421_SPK_CRTL_1, 0x30);
#if 0
		regulator_bulk_disable(1, &g_pdata->regu_boost5v);
#endif
		boost5v_classd_enable(false);
		regulator_bulk_disable(1, &g_pdata->regu_ldo8);
		break;
	default:
		loge("%s: invalid command %d", __FUNCTION__, _IOC_NR(cmd));
		ret = -EINVAL;
		break;
	}

exit:
	mutex_unlock(&g_pdata->io_mutex);
	return ret;
}

static const struct file_operations hisi_6421_spk_misc_fops = {
	.owner = THIS_MODULE,
	.open = hisi_6421_spk_open,
	.release = hisi_6421_spk_release,
	.unlocked_ioctl = hisi_6421_spk_ioctl,
};

static const struct of_device_id hisi_6421_spk_of_match[] = {
	{
		.compatible = "hisilicon,hisi-6421-spk",
	},
	{ },
};
MODULE_DEVICE_TABLE(of, hisi_6421_spk_of_match);

static int hisi_6421_spk_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hisi_6421_spk_platform_data *pdata = NULL;
	int ret = 0;

	pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	if (NULL == pdata){
		dev_err(dev, "cannot allocate hisi 6421 spk platform data\n");
		return -ENOMEM;
	}

	pdata->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (NULL == pdata->res) {
		dev_err(dev, "platform_get_resource err\n");
		return -ENOENT;
	}

	pdata->reg_base_addr = devm_ioremap(dev, pdata->res->start,
					    resource_size(pdata->res));
	if (NULL == pdata->reg_base_addr) {
		dev_err(dev, "cannot map register memory\n");
		return -ENOMEM;
	}

	pdata->regu_ldo8.supply = "hisi-6421-spk-ldo8";
	ret = devm_regulator_bulk_get(dev, 1, &(pdata->regu_ldo8));
	if (0 != ret) {
		dev_err(dev, "couldn't get regulators(ldo8) %d\n", ret);
		return -ENOENT;
	}

#if 0
	pdata->regu_boost5v.supply = "hisi-6421-spk-boost5v";
	ret = devm_regulator_bulk_get(dev, 1, &(pdata->regu_boost5v));
	if (0 != ret) {
		dev_err(dev, "couldn't get regulators(boost5v) %d\n", ret);
		return -ENOENT;
	}
#endif
	mutex_init(&pdata->io_mutex);

	pdata->miscdev.minor = MISC_DYNAMIC_MINOR;
	pdata->miscdev.name = "hisi_6421_spk";
	pdata->miscdev.fops = &hisi_6421_spk_misc_fops;

	ret = misc_register(&pdata->miscdev);
	if (ret)
		loge("%s : hisi 6421 spk_device register failed", __FUNCTION__);

	g_pdata = pdata;

	/* config classD ocp threshold as 3.5A */
	hisi_6421_spk_reg_write(HISI_6421_SPK_CRTL_1, 0x30);

	return ret;
}

static int hisi_6421_spk_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver hisi_6421_spk_driver = {
	.driver = {
		.name	= "hisi_6421_spk",
		.owner	= THIS_MODULE,
		.of_match_table = hisi_6421_spk_of_match,
	},
	.probe	= hisi_6421_spk_probe,
	.remove	= hisi_6421_spk_remove,
};
module_platform_driver(hisi_6421_spk_driver);

MODULE_DESCRIPTION("hisi_6421_spk driver");
MODULE_LICENSE("GPL");
