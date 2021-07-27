// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2016 Marek Vasut <marex@denx.de>
 *
 * This code is based on drivers/gpu/mxsfb/mxsfb.c :
 * Copyright (C) 2010 Juergen Beisert, Pengutronix
 * Copyright (C) 2008-2009 Freescale Semiconductor, Inc. All Rights Reserved.
 * Copyright (C) 2008 Embedded Alley Solutions, Inc All Rights Reserved.
 */

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/pm_runtime.h>
#include <linux/spinlock.h>
#include <video/videomode.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_crtc.h>
#include <drm/drm_encoder.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_plane.h>
#include <drm/drm_plane_helper.h>
#include <drm/drm_vblank.h>

#include "mxsfbv3_drv.h"
#include "mxsfbv3_regs.h"

/* 1 second delay should be plenty of time for block reset */
#define RESET_TIMEOUT		1000000

/* -----------------------------------------------------------------------------
 * CRTC
 */

/*
 * Setup the MXSFB registers for decoding the pixels out of the framebuffer and
 * outputting them on the bus.
 */
static void mxsfbv3_set_formats(struct mxsfbv3_drm_private *mxsfb)
{
	struct drm_device *drm = mxsfb->drm;
	u32 bus_format = MEDIA_BUS_FMT_RGB888_1X24;
	u32 disp_para = 0;

	if (mxsfb->connector->display_info.num_bus_formats)
		bus_format = mxsfb->connector->display_info.bus_formats[0];

	disp_para = readl(mxsfb->base + LCDIFV3_DISP_PARA);

	/* clear line pattern bits */
	disp_para &= ~DISP_PARA_LINE_PATTERN(0xf);

	switch (bus_format) {
	case MEDIA_BUS_FMT_RGB565_1X16:
		disp_para |= DISP_PARA_LINE_PATTERN(LINE_PATTERN_RGB565);
		break;
	case MEDIA_BUS_FMT_RGB888_1X24:
		disp_para |= DISP_PARA_LINE_PATTERN(LINE_PATTERN_RGB888_OR_YUV444);
		break;
	default:
		dev_err(drm->dev, "unknown bus format: %#x\n", bus_format);
		return;
	}

	/* set normal operating mode */
	disp_para &= ~DISP_PARA_DISP_MODE(3);
	disp_para |= DISP_PARA_DISP_MODE(0);

	writel(disp_para, mxsfb->base + LCDIFV3_DISP_PARA);
}

static void mxsfbv3_enable_controller(struct mxsfbv3_drm_private *mxsfb)
{
	u32 disp_para, ctrldescl0_5;

	if (mxsfb->clk_disp_axi)
		clk_prepare_enable(mxsfb->clk_disp_axi);
	clk_prepare_enable(mxsfb->clk);

	disp_para = readl(mxsfb->base + LCDIFV3_DISP_PARA);
	ctrldescl0_5 = readl(mxsfb->base + LCDIFV3_CTRLDESCL0_5);

	/* disp on */
	disp_para |= DISP_PARA_DISP_ON;
	writel(disp_para, mxsfb->base + LCDIFV3_DISP_PARA);

	/* enable layer dma */
	ctrldescl0_5 |= CTRLDESCL0_5_EN;
	writel(ctrldescl0_5, mxsfb->base + LCDIFV3_CTRLDESCL0_5);
}

static void mxsfbv3_disable_controller(struct mxsfbv3_drm_private *mxsfb)
{
	u32 disp_para, ctrldescl0_5;

	disp_para = readl(mxsfb->base + LCDIFV3_DISP_PARA);
	ctrldescl0_5 = readl(mxsfb->base + LCDIFV3_CTRLDESCL0_5);

	/* disable dma */
	ctrldescl0_5 &= ~CTRLDESCL0_5_EN;
	writel(ctrldescl0_5, mxsfb->base + LCDIFV3_CTRLDESCL0_5);

	/* dma config only takes effect at the end of
	 * one frame, so add delay to wait dma disable
	 * done before turn off disp.
	 */
	usleep_range(20000, 25000);

	/* disp off */
	disp_para &= ~DISP_PARA_DISP_ON;
	writel(disp_para, mxsfb->base + LCDIFV3_DISP_PARA);

	clk_disable_unprepare(mxsfb->clk);
	if (mxsfb->clk_disp_axi)
		clk_disable_unprepare(mxsfb->clk_disp_axi);
}

static dma_addr_t mxsfbv3_get_fb_paddr(struct drm_plane *plane)
{
	struct drm_framebuffer *fb = plane->state->fb;
	struct drm_gem_cma_object *gem;

	if (!fb)
		return 0;

	gem = drm_fb_cma_get_gem_obj(fb, 0);
	if (!gem)
		return 0;

	return gem->paddr;
}

static void mxsfbv3_crtc_mode_set_nofb(struct mxsfbv3_drm_private *mxsfb)
{
	struct drm_device *drm = mxsfb->crtc.dev;
	struct drm_display_mode *m = &mxsfb->crtc.state->adjusted_mode;
	struct videomode vm;
	u32 bus_flags = mxsfb->connector->display_info.bus_flags;
	u32 vdctrl0, vsync_pulse_len, hsync_pulse_len;
	int err;

	mxsfbv3_set_formats(mxsfb);

	clk_set_rate(mxsfb->clk, m->crtc_clock * 1000);

	if (mxsfb->bridge && mxsfb->bridge->timings)
		bus_flags = mxsfb->bridge->timings->input_bus_flags;

	DRM_DEV_DEBUG_DRIVER(drm->dev, "Pixel clock: %dkHz (actual: %dkHz)\n",
			     m->crtc_clock,
			     (int)(clk_get_rate(mxsfb->clk) / 1000));
	DRM_DEV_DEBUG_DRIVER(drm->dev, "Connector bus_flags: 0x%08X\n",
			     bus_flags);
	DRM_DEV_DEBUG_DRIVER(drm->dev, "Mode flags: 0x%08X\n", m->flags);

	drm_display_mode_to_videomode(m, &vm);

	writel(DISP_SIZE_DELTA_Y(m->crtc_vdisplay) |
	       DISP_SIZE_DELTA_X(m->crtc_hdisplay),
	       mxsfb->base + LCDIFV3_DISP_SIZE);

	writel(HSYNC_PARA_BP_H(vm.hback_porch) |
	       HSYNC_PARA_FP_H(vm.hfront_porch),
	       mxsfb->base + LCDIFV3_HSYN_PARA);

	writel(VSYNC_PARA_BP_V(vm.vback_porch) |
	       VSYNC_PARA_FP_V(vm.vfront_porch),
	       mxsfb->base + LCDIFV3_VSYN_PARA);

	writel(VSYN_HSYN_WIDTH_PW_V(vm.vsync_len) |
	       VSYN_HSYN_WIDTH_PW_H(vm.hsync_len),
	       mxsfb->base + LCDIFV3_VSYN_HSYN_WIDTH);

	writel(CTRLDESCL0_1_HEIGHT(vm.vactive) |
	       CTRLDESCL0_1_WIDTH(vm.hactive),
	       mxsfb->base + LCDIFV3_CTRLDESCL0_1);

}

static int mxsfbv3_crtc_atomic_check(struct drm_crtc *crtc,
				   struct drm_atomic_state *state)
{
	struct drm_crtc_state *crtc_state = drm_atomic_get_new_crtc_state(state,
									  crtc);
	bool has_primary = crtc_state->plane_mask &
			   drm_plane_mask(crtc->primary);

	/* The primary plane has to be enabled when the CRTC is active. */
	if (crtc_state->active && !has_primary)
		return -EINVAL;

	/* TODO: Is this needed ? */
	return drm_atomic_add_affected_planes(state, crtc);
}

static void mxsfbv3_crtc_atomic_flush(struct drm_crtc *crtc,
				    struct drm_atomic_state *state)
{
	struct drm_pending_vblank_event *event;
	struct mxsfbv3_drm_private *mxsfb = to_mxsfbv3_drm_private(crtc->dev);
	u32 ctrldescl0_5;

	event = crtc->state->event;
	crtc->state->event = NULL;

	if (!event)
		return;

	spin_lock_irq(&crtc->dev->event_lock);
	if (drm_crtc_vblank_get(crtc) == 0)
		drm_crtc_arm_vblank_event(crtc, event);
	else
		drm_crtc_send_vblank_event(crtc, event);
	spin_unlock_irq(&crtc->dev->event_lock);

	ctrldescl0_5 = readl(mxsfb->base + LCDIFV3_CTRLDESCL0_5);
	ctrldescl0_5 |= CTRLDESCL0_5_SHADOW_LOAD_EN;

	writel(ctrldescl0_5, mxsfb->base + LCDIFV3_CTRLDESCL0_5);
}

static void mxsfbv3_crtc_atomic_enable(struct drm_crtc *crtc,
				     struct drm_atomic_state *state)
{
	struct mxsfbv3_drm_private *mxsfb = to_mxsfbv3_drm_private(crtc->dev);
	struct drm_device *drm = mxsfb->drm;
	dma_addr_t paddr;

	pm_runtime_get_sync(drm->dev);
	mxsfbv3_enable_axi_clk(mxsfb);

	drm_crtc_vblank_on(crtc);

	mxsfbv3_crtc_mode_set_nofb(mxsfb);

	/* Write cur_buf as well to avoid an initial corrupt frame */
	paddr = mxsfbv3_get_fb_paddr(crtc->primary);
	if (paddr)
		writel(paddr, mxsfb->base + LCDIFV3_CTRLDESCL_LOW0_4);

	mxsfbv3_enable_controller(mxsfb);
}

static void mxsfbv3_crtc_atomic_disable(struct drm_crtc *crtc,
				      struct drm_atomic_state *state)
{
	struct mxsfbv3_drm_private *mxsfb = to_mxsfbv3_drm_private(crtc->dev);
	struct drm_device *drm = mxsfb->drm;
	struct drm_pending_vblank_event *event;

	mxsfbv3_disable_controller(mxsfb);

	spin_lock_irq(&drm->event_lock);
	event = crtc->state->event;
	if (event) {
		crtc->state->event = NULL;
		drm_crtc_send_vblank_event(crtc, event);
	}
	spin_unlock_irq(&drm->event_lock);

	drm_crtc_vblank_off(crtc);

	mxsfbv3_disable_axi_clk(mxsfb);
	pm_runtime_put_sync(drm->dev);
}

static int mxsfbv3_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct mxsfbv3_drm_private *mxsfb = to_mxsfbv3_drm_private(crtc->dev);
	u32 int_enable_d0;

	int_enable_d0 = readl(mxsfb->base + LCDIFV3_INT_ENABLE_D0);
	int_enable_d0 |= INT_STATUS_D0_VS_BLANK;

	/* clear flag */
	writel(INT_STATUS_D0_VS_BLANK, mxsfb->base + LCDIFV3_INT_STATUS_D0);
	/* enable */
	writel(int_enable_d0, mxsfb->base + LCDIFV3_INT_ENABLE_D0);

	return 0;
}

static void mxsfbv3_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct mxsfbv3_drm_private *mxsfb = to_mxsfbv3_drm_private(crtc->dev);
	u32 int_enable_d0;

	int_enable_d0 = readl(mxsfb->base + LCDIFV3_INT_ENABLE_D0);
	int_enable_d0 &= ~INT_STATUS_D0_VS_BLANK;

	/* disable */
	writel(int_enable_d0, mxsfb->base + LCDIFV3_INT_ENABLE_D0);
	/* clear flag */
	writel(INT_STATUS_D0_VS_BLANK, mxsfb->base + LCDIFV3_INT_STATUS_D0);
}

static const struct drm_crtc_helper_funcs mxsfbv3_crtc_helper_funcs = {
	.atomic_check = mxsfbv3_crtc_atomic_check,
	.atomic_flush = mxsfbv3_crtc_atomic_flush,
	.atomic_enable = mxsfbv3_crtc_atomic_enable,
	.atomic_disable = mxsfbv3_crtc_atomic_disable,
};

static const struct drm_crtc_funcs mxsfbv3_crtc_funcs = {
	.reset = drm_atomic_helper_crtc_reset,
	.destroy = drm_crtc_cleanup,
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
	.enable_vblank = mxsfbv3_crtc_enable_vblank,
	.disable_vblank = mxsfbv3_crtc_disable_vblank,
};

/* -----------------------------------------------------------------------------
 * Encoder
 */

static const struct drm_encoder_funcs mxsfbv3_encoder_funcs = {
	.destroy = drm_encoder_cleanup,
};

/* -----------------------------------------------------------------------------
 * Planes
 */

static int mxsfbv3_plane_atomic_check(struct drm_plane *plane,
				    struct drm_atomic_state *state)
{
	struct drm_plane_state *plane_state = drm_atomic_get_new_plane_state(state,
									     plane);
	struct mxsfbv3_drm_private *mxsfb = to_mxsfbv3_drm_private(plane->dev);
	struct drm_crtc_state *crtc_state;

	crtc_state = drm_atomic_get_new_crtc_state(state,
						   &mxsfb->crtc);

	return drm_atomic_helper_check_plane_state(plane_state, crtc_state,
						   DRM_PLANE_HELPER_NO_SCALING,
						   DRM_PLANE_HELPER_NO_SCALING,
						   false, true);
}

static void mxsfbv3_plane_atomic_update(struct drm_plane *plane,
					      struct drm_atomic_state *state)
{
	struct mxsfbv3_drm_private *mxsfb = to_mxsfbv3_drm_private(plane->dev);
	struct drm_plane_state *new_pstate = drm_atomic_get_new_plane_state(state,
								    plane);
	dma_addr_t paddr;
	u32 ctrl;

	ctrl = readl(mxsfb->base + LCDIFV3_CTRLDESCL0_5);

	ctrl &= ~CTRLDESCL0_5_BPP(0xf);

	switch (new_pstate->fb->format->format) {
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_ARGB1555:
		ctrl |= CTRLDESCL0_5_BPP(BPP_ARGB1555);
		break;
	case DRM_FORMAT_RGB565:
		ctrl |=  CTRLDESCL0_5_BPP(BPP_RGB565);
		break;
	case DRM_FORMAT_XRGB4444:
	case DRM_FORMAT_ARGB4444:
		ctrl |=  CTRLDESCL0_5_BPP(BPP_ARGB4444);
		break;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_ARGB8888:
		ctrl |=  CTRLDESCL0_5_BPP(BPP_ARGB8888);
		break;
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_ABGR8888:
		ctrl |=  CTRLDESCL0_5_BPP(BPP_ABGR8888);
		break;

	}
	writel(ctrl, mxsfb->base + LCDIFV3_CTRLDESCL0_5);

	paddr = mxsfbv3_get_fb_paddr(plane);
	if (paddr)
		writel(paddr, mxsfb->base + LCDIFV3_CTRLDESCL_LOW0_4);
}

static bool mxsfbv3_format_mod_supported(struct drm_plane *plane,
				       uint32_t format,
				       uint64_t modifier)
{
	return modifier == DRM_FORMAT_MOD_LINEAR;
}

static const struct drm_plane_helper_funcs mxsfbv3_plane_helper_funcs = {
	.prepare_fb = drm_gem_plane_helper_prepare_fb,
	.atomic_check = mxsfbv3_plane_atomic_check,
	.atomic_update = mxsfbv3_plane_atomic_update,
};

static const struct drm_plane_funcs mxsfbv3_plane_funcs = {
	.format_mod_supported	= mxsfbv3_format_mod_supported,
	.update_plane		= drm_atomic_helper_update_plane,
	.disable_plane		= drm_atomic_helper_disable_plane,
	.destroy		= drm_plane_cleanup,
	.reset			= drm_atomic_helper_plane_reset,
	.atomic_duplicate_state	= drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state	= drm_atomic_helper_plane_destroy_state,
};

static const uint32_t mxsfbv3_plane_formats[] = {
	DRM_FORMAT_XRGB1555,
	DRM_FORMAT_ARGB1555,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_ARGB4444,
	DRM_FORMAT_XRGB4444,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_ABGR8888,
};

static const uint64_t mxsfbv3_modifiers[] = {
	DRM_FORMAT_MOD_LINEAR,
	DRM_FORMAT_MOD_INVALID
};

/* -----------------------------------------------------------------------------
 * Initialization
 */

int mxsfbv3_kms_init(struct mxsfbv3_drm_private *mxsfb)
{
	struct drm_encoder *encoder = &mxsfb->encoder;
	struct drm_crtc *crtc = &mxsfb->crtc;
	int ret;

	drm_plane_helper_add(&mxsfb->plane,
			     &mxsfbv3_plane_helper_funcs);
	ret = drm_universal_plane_init(mxsfb->drm, &mxsfb->plane, 1,
				       &mxsfbv3_plane_funcs,
				       mxsfbv3_plane_formats,
				       ARRAY_SIZE(mxsfbv3_plane_formats),
				       mxsfbv3_modifiers, DRM_PLANE_TYPE_PRIMARY,
				       NULL);
	if (ret)
		return ret;

	drm_crtc_helper_add(crtc, &mxsfbv3_crtc_helper_funcs);
	ret = drm_crtc_init_with_planes(mxsfb->drm, crtc,
					&mxsfb->plane, NULL,
					&mxsfbv3_crtc_funcs, NULL);
	if (ret)
		return ret;

	encoder->possible_crtcs = drm_crtc_mask(crtc);
	return drm_encoder_init(mxsfb->drm, encoder, &mxsfbv3_encoder_funcs,
				DRM_MODE_ENCODER_NONE, NULL);
}
