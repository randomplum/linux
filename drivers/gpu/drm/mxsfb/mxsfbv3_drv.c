// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2016 Marek Vasut <marex@denx.de>
 *
 * This code is based on drivers/video/fbdev/mxsfb.c :
 * Copyright (C) 2010 Juergen Beisert, Pengutronix
 * Copyright (C) 2008-2009 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright (C) 2008 Embedded Alley Solutions, Inc All Rights Reserved.
 */

#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>

#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_connector.h>
#include <drm/drm_drv.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_gem_framebuffer_helper.h>
#include <drm/drm_irq.h>
#include <drm/drm_mode_config.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_vblank.h>

#include "mxsfbv3_drv.h"
#include "mxsfbv3_regs.h"

void mxsfbv3_enable_axi_clk(struct mxsfbv3_drm_private *mxsfb)
{
	if (mxsfb->clk_axi)
		clk_prepare_enable(mxsfb->clk_axi);
}

void mxsfbv3_disable_axi_clk(struct mxsfbv3_drm_private *mxsfb)
{
	if (mxsfb->clk_axi)
		clk_disable_unprepare(mxsfb->clk_axi);
}

static struct drm_framebuffer *
mxsfbv3_fb_create(struct drm_device *dev, struct drm_file *file_priv,
		const struct drm_mode_fb_cmd2 *mode_cmd)
{
	const struct drm_format_info *info;

	info = drm_get_format_info(dev, mode_cmd);
	if (!info)
		return ERR_PTR(-EINVAL);

	if (mode_cmd->width * info->cpp[0] != mode_cmd->pitches[0]) {
		dev_dbg(dev->dev, "Invalid pitch: fb width must match pitch\n");
		return ERR_PTR(-EINVAL);
	}

	return drm_gem_fb_create(dev, file_priv, mode_cmd);
}

static const struct drm_mode_config_funcs mxsfbv3_mode_config_funcs = {
	.fb_create		= mxsfbv3_fb_create,
	.atomic_check		= drm_atomic_helper_check,
	.atomic_commit		= drm_atomic_helper_commit,
};

static const struct drm_mode_config_helper_funcs mxsfbv3_mode_config_helpers = {
	.atomic_commit_tail = drm_atomic_helper_commit_tail_rpm,
};

static int mxsfbv3_attach_bridge(struct mxsfbv3_drm_private *mxsfb)
{
	struct drm_device *drm = mxsfb->drm;
	struct drm_connector_list_iter iter;
	struct drm_panel *panel;
	struct drm_bridge *bridge;
	int ret;

	ret = drm_of_find_panel_or_bridge(drm->dev->of_node, 0, 0, &panel,
					  &bridge);
	if (ret)
		return ret;

	if (panel) {
		bridge = devm_drm_panel_bridge_add_typed(drm->dev, panel,
							 DRM_MODE_CONNECTOR_DPI);
		if (IS_ERR(bridge))
			return PTR_ERR(bridge);
	}

	if (!bridge)
		return -ENODEV;

	ret = drm_bridge_attach(&mxsfb->encoder, bridge, NULL, 0);
	if (ret)
		return dev_err_probe(drm->dev, ret, "Failed to attach bridge\n");

	mxsfb->bridge = bridge;

	/*
	 * Get hold of the connector. This is a bit of a hack, until the bridge
	 * API gives us bus flags and formats.
	 */
	drm_connector_list_iter_begin(drm, &iter);
	mxsfb->connector = drm_connector_list_iter_next(&iter);
	drm_connector_list_iter_end(&iter);

	return 0;
}

static int mxsfbv3_load(struct drm_device *drm,
		      const struct mxsfbv3_devdata *devdata)
{
	struct platform_device *pdev = to_platform_device(drm->dev);
	struct mxsfbv3_drm_private *mxsfb;
	struct resource *res;
	int ret;

	mxsfb = devm_kzalloc(&pdev->dev, sizeof(*mxsfb), GFP_KERNEL);
	if (!mxsfb)
		return -ENOMEM;

	mxsfb->drm = drm;
	drm->dev_private = mxsfb;
	mxsfb->devdata = devdata;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	mxsfb->base = devm_ioremap_resource(drm->dev, res);
	if (IS_ERR(mxsfb->base))
		return PTR_ERR(mxsfb->base);

	mxsfb->clk = devm_clk_get(drm->dev, NULL);
	if (IS_ERR(mxsfb->clk))
		return PTR_ERR(mxsfb->clk);

	mxsfb->clk_axi = devm_clk_get(drm->dev, "axi");
	if (IS_ERR(mxsfb->clk_axi))
		mxsfb->clk_axi = NULL;

	mxsfb->clk_disp_axi = devm_clk_get(drm->dev, "disp_axi");
	if (IS_ERR(mxsfb->clk_disp_axi))
		mxsfb->clk_disp_axi = NULL;

	ret = dma_set_mask_and_coherent(drm->dev, DMA_BIT_MASK(32));
	if (ret)
		return ret;

	pm_runtime_enable(drm->dev);

	/* Modeset init */
	drm_mode_config_init(drm);

	ret = mxsfbv3_kms_init(mxsfb);
	if (ret < 0) {
		dev_err(drm->dev, "Failed to initialize KMS pipeline\n");
		goto err_vblank;
	}

	ret = drm_vblank_init(drm, drm->mode_config.num_crtc);
	if (ret < 0) {
		dev_err(drm->dev, "Failed to initialise vblank\n");
		goto err_vblank;
	}

	/* Start with vertical blanking interrupt reporting disabled. */
	drm_crtc_vblank_off(&mxsfb->crtc);

	ret = mxsfbv3_attach_bridge(mxsfb);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			dev_err(drm->dev, "Cannot connect bridge: %d\n", ret);
		goto err_vblank;
	}

	drm->mode_config.min_width	= MXSFB_MIN_XRES;
	drm->mode_config.min_height	= MXSFB_MIN_YRES;
	drm->mode_config.max_width	= MXSFB_MAX_XRES;
	drm->mode_config.max_height	= MXSFB_MAX_YRES;
	drm->mode_config.funcs		= &mxsfbv3_mode_config_funcs;
	drm->mode_config.helper_private	= &mxsfbv3_mode_config_helpers;

	drm_mode_config_reset(drm);

	pm_runtime_get_sync(drm->dev);
	ret = drm_irq_install(drm, platform_get_irq(pdev, 0));
	pm_runtime_put_sync(drm->dev);

	if (ret < 0) {
		dev_err(drm->dev, "Failed to install IRQ handler\n");
		goto err_vblank;
	}

	drm_kms_helper_poll_init(drm);

	platform_set_drvdata(pdev, drm);

	drm_helper_hpd_irq_event(drm);

	return 0;

err_vblank:
	pm_runtime_disable(drm->dev);

	return ret;
}

static void mxsfbv3_unload(struct drm_device *drm)
{
	drm_kms_helper_poll_fini(drm);
	drm_mode_config_cleanup(drm);

	pm_runtime_get_sync(drm->dev);
	drm_irq_uninstall(drm);
	pm_runtime_put_sync(drm->dev);

	drm->dev_private = NULL;

	pm_runtime_disable(drm->dev);
}

static void mxsfbv3_irq_disable(struct drm_device *drm)
{
	struct mxsfbv3_drm_private *mxsfb = drm->dev_private;

	mxsfbv3_enable_axi_clk(mxsfb);
	mxsfb->crtc.funcs->disable_vblank(&mxsfb->crtc);
	mxsfbv3_disable_axi_clk(mxsfb);
}

static irqreturn_t mxsfbv3_irq_handler(int irq, void *data)
{
	struct drm_device *drm = data;
	struct mxsfbv3_drm_private *mxsfb = drm->dev_private;
	u32 reg;

	reg = readl(mxsfb->base + LCDIFV3_INT_STATUS_D0);

	if (reg & INT_STATUS_D0_VS_BLANK)
		drm_crtc_handle_vblank(&mxsfb->crtc);

	writel(INT_STATUS_D0_VS_BLANK, mxsfb->base + LCDIFV3_INT_STATUS_D0);

	return IRQ_HANDLED;
}

DEFINE_DRM_GEM_CMA_FOPS(fops);

static const struct drm_driver mxsfbv3_driver = {
	.driver_features	= DRIVER_GEM | DRIVER_MODESET | DRIVER_ATOMIC,
	.irq_handler		= mxsfbv3_irq_handler,
	.irq_preinstall		= mxsfbv3_irq_disable,
	.irq_uninstall		= mxsfbv3_irq_disable,
	DRM_GEM_CMA_DRIVER_OPS,
	.fops	= &fops,
	.name	= "mxsfbv3-drm",
	.desc	= "LCDIFv3 Controller DRM",
	.date	= "20210727",
	.major	= 1,
	.minor	= 0,
};

static const struct of_device_id mxsfbv3_dt_ids[] = {
	{ .compatible = "fsl,imx8mp-lcdifv3", },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, mxsfbv3_dt_ids);

static int mxsfbv3_probe(struct platform_device *pdev)
{
	struct drm_device *drm;
	const struct of_device_id *of_id =
			of_match_device(mxsfbv3_dt_ids, &pdev->dev);
	int ret;

	if (!pdev->dev.of_node)
		return -ENODEV;

	drm = drm_dev_alloc(&mxsfbv3_driver, &pdev->dev);
	if (IS_ERR(drm))
		return PTR_ERR(drm);

	ret = mxsfbv3_load(drm, of_id->data);
	if (ret)
		goto err_free;

	ret = drm_dev_register(drm, 0);
	if (ret)
		goto err_unload;

	drm_fbdev_generic_setup(drm, 32);

	return 0;

err_unload:
	mxsfbv3_unload(drm);
err_free:
	drm_dev_put(drm);

	return ret;
}

static int mxsfbv3_remove(struct platform_device *pdev)
{
	struct drm_device *drm = platform_get_drvdata(pdev);

	drm_dev_unregister(drm);
	mxsfbv3_unload(drm);
	drm_dev_put(drm);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int mxsfbv3_suspend(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	return drm_mode_config_helper_suspend(drm);
}

static int mxsfbv3_resume(struct device *dev)
{
	struct drm_device *drm = dev_get_drvdata(dev);

	return drm_mode_config_helper_resume(drm);
}
#endif

static const struct dev_pm_ops mxsfbv3_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(mxsfbv3_suspend, mxsfbv3_resume)
};

static struct platform_driver mxsfbv3_platform_driver = {
	.probe		= mxsfbv3_probe,
	.remove		= mxsfbv3_remove,
	.driver	= {
		.name		= "mxsfbv3",
		.of_match_table	= mxsfbv3_dt_ids,
		.pm		= &mxsfbv3_pm_ops,
	},
};

module_platform_driver(mxsfbv3_platform_driver);

MODULE_AUTHOR("Marek Vasut <marex@denx.de>");
MODULE_DESCRIPTION("NXP LCDIFv3 driver");
MODULE_LICENSE("GPL");
