/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2016 Marek Vasut <marex@denx.de>
 *
 * i.MX23/i.MX28/i.MX6SX MXSFB LCD controller driver.
 */

#ifndef __MXSFB_DRV_H__
#define __MXSFB_DRV_H__

#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_encoder.h>
#include <drm/drm_plane.h>

struct clk;


struct mxsfbv3_drm_private {
	const struct mxsfbv3_devdata	*devdata;

	void __iomem			*base;	/* registers */
	struct clk			*clk;
	struct clk			*clk_axi;
	struct clk			*clk_disp_axi;

	struct drm_device		*drm;

	struct drm_plane		plane;

	struct drm_crtc			crtc;
	struct drm_encoder		encoder;
	struct drm_connector		*connector;
	struct drm_bridge		*bridge;
};

static inline struct mxsfbv3_drm_private *
to_mxsfbv3_drm_private(struct drm_device *drm)
{
	return drm->dev_private;
}

void mxsfbv3_enable_axi_clk(struct mxsfbv3_drm_private *mxsfb);
void mxsfbv3_disable_axi_clk(struct mxsfbv3_drm_private *mxsfb);

int mxsfbv3_kms_init(struct mxsfbv3_drm_private *mxsfb);

#endif /* __MXSFB_DRV_H__ */
