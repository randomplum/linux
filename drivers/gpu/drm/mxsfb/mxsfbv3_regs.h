/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2010 Juergen Beisert, Pengutronix
 * Copyright (C) 2016 Marek Vasut <marex@denx.de>
 *
 * i.MX23/i.MX28/i.MX6SX MXSFB LCD controller driver.
 */

#ifndef __MXSFB_REGS_H__
#define __MXSFB_REGS_H__

#define LCDIFV3_CTRL			0x00
#define LCDIFV3_CTRL_SET		0x04
#define LCDIFV3_CTRL_CLR		0x08
#define LCDIFV3_CTRL_TOG		0x0c
#define LCDIFV3_DISP_PARA		0x10
#define LCDIFV3_DISP_SIZE		0x14
#define LCDIFV3_HSYN_PARA		0x18
#define LCDIFV3_VSYN_PARA		0x1c
#define LCDIFV3_VSYN_HSYN_WIDTH		0x20
#define LCDIFV3_INT_STATUS_D0		0x24
#define LCDIFV3_INT_ENABLE_D0		0x28
#define LCDIFV3_INT_STATUS_D1		0x30
#define LCDIFV3_INT_ENABLE_D1		0x34

#define LCDIFV3_CTRLDESCL0_1		0x200
#define LCDIFV3_CTRLDESCL0_3		0x208
#define LCDIFV3_CTRLDESCL_LOW0_4	0x20c
#define LCDIFV3_CTRLDESCL_HIGH0_4	0x210
#define LCDIFV3_CTRLDESCL0_5		0x214
#define LCDIFV3_CSC0_CTRL		0x21c
#define LCDIFV3_CSC0_COEF0		0x220
#define LCDIFV3_CSC0_COEF1		0x224
#define LCDIFV3_CSC0_COEF2		0x228
#define LCDIFV3_CSC0_COEF3		0x22c
#define LCDIFV3_CSC0_COEF4		0x230
#define LCDIFV3_CSC0_COEF5		0x234
#define LCDIFV3_PANIC0_THRES		0x238

#define CTRL_SW_RESET			BIT(31)

#define DISP_PARA_DISP_ON		BIT(31)
#define DISP_PARA_LINE_PATTERN(x)	(((x) & 0xf) << 26)
#define LINE_PATTERN_RGB888_OR_YUV444	0x0
#define LINE_PATTERN_RBG888		0x1
#define LINE_PATTERN_GBR888		0x2
#define LINE_PATTERN_GRB888_OR_UYV444	0x3
#define LINE_PATTERN_BRG888		0x4
#define LINE_PATTERN_BGR888		0x5
#define LINE_PATTERN_RGB555		0x6
#define LINE_PATTERN_RGB565		0x7
#define LINE_PATTERN_YUYV_16_0		0x8
#define LINE_PATTERN_UYVY_16_0		0x9
#define LINE_PATTERN_YVYU_16_0		0xa
#define LINE_PATTERN_VYUY_16_0		0xb
#define LINE_PATTERN_YUYV_23_8		0xc
#define LINE_PATTERN_UYVY_23_8		0xd
#define LINE_PATTERN_YVYU_23_8		0xe
#define LINE_PATTERN_VYUY_23_8		0xf
#define DISP_PARA_DISP_MODE(x)		(((x) & 0x3) << 24)

#define DISP_SIZE_DELTA_Y(x)		(((x) & 0xffff) << 16)
#define DISP_SIZE_DELTA_X(x)		(((x) & 0xffff) << 0)

#define HSYNC_PARA_BP_H(x)		(((x) & 0xffff) << 16)
#define HSYNC_PARA_FP_H(x)		(((x) & 0xffff) << 0)

#define VSYNC_PARA_BP_V(x)		(((x) & 0xffff) << 16)
#define VSYNC_PARA_FP_V(x)		(((x) & 0xffff) << 0)

#define VSYN_HSYN_WIDTH_PW_V(x)		(((x) & 0xffff) << 16)
#define VSYN_HSYN_WIDTH_PW_H(x)		(((x) & 0xffff) << 0)

#define INT_STATUS_D0_FIFO_EMPTY	BIT(24)
#define INT_STATUS_D0_DMA_DONE		BIT(16)
#define INT_STATUS_D0_DMA_ERR		BIT(8)
#define INT_STATUS_D0_VS_BLANK		BIT(2)
#define INT_STATUS_D0_UNDERRUN		BIT(1)
#define INT_STATUS_D0_VSYNC		BIT(0)

#define CTRLDESCL0_1_HEIGHT(x)		(((x) & 0xffff) << 16)
#define CTRLDESCL0_1_WIDTH(x)		(((x) & 0xffff) << 0)

#define CTRLDESCL0_3_PITCH(x)		(((x) & 0xffff) << 0)

#define CTRLDESCL0_5_EN			BIT(31)
#define CTRLDESCL0_5_SHADOW_LOAD_EN	BIT(30)
#define CTRLDESCL0_5_BPP(x)		(((x) & 0xf) << 24)
#define BPP_RGB565			0x4
#define BPP_ARGB1555			0x5
#define BPP_ARGB4444			0x6
#define BPP_RGB888			0x8
#define BPP_ARGB8888			0x9
#define BPP_ABGR8888			0xa








#define CTRL_SFTRST			BIT(31)
#define CTRL_CLKGATE			BIT(30)
#define CTRL_BYPASS_COUNT		BIT(19)
#define CTRL_VSYNC_MODE			BIT(18)
#define CTRL_DOTCLK_MODE		BIT(17)
#define CTRL_DATA_SELECT		BIT(16)
#define CTRL_BUS_WIDTH_16		(0 << 10)
#define CTRL_BUS_WIDTH_8		(1 << 10)
#define CTRL_BUS_WIDTH_18		(2 << 10)
#define CTRL_BUS_WIDTH_24		(3 << 10)
#define CTRL_BUS_WIDTH_MASK		(0x3 << 10)
#define CTRL_WORD_LENGTH_16		(0 << 8)
#define CTRL_WORD_LENGTH_8		(1 << 8)
#define CTRL_WORD_LENGTH_18		(2 << 8)
#define CTRL_WORD_LENGTH_24		(3 << 8)
#define CTRL_MASTER			BIT(5)
#define CTRL_DF16			BIT(3)
#define CTRL_DF18			BIT(2)
#define CTRL_DF24			BIT(1)
#define CTRL_RUN			BIT(0)

#define CTRL1_FIFO_CLEAR		BIT(21)
#define CTRL1_SET_BYTE_PACKAGING(x)	(((x) & 0xf) << 16)
#define CTRL1_GET_BYTE_PACKAGING(x)	(((x) >> 16) & 0xf)
#define CTRL1_CUR_FRAME_DONE_IRQ_EN	BIT(13)
#define CTRL1_CUR_FRAME_DONE_IRQ	BIT(9)

#define TRANSFER_COUNT_SET_VCOUNT(x)	(((x) & 0xffff) << 16)
#define TRANSFER_COUNT_GET_VCOUNT(x)	(((x) >> 16) & 0xffff)
#define TRANSFER_COUNT_SET_HCOUNT(x)	((x) & 0xffff)
#define TRANSFER_COUNT_GET_HCOUNT(x)	((x) & 0xffff)

#define VDCTRL0_ENABLE_PRESENT		BIT(28)
#define VDCTRL0_VSYNC_ACT_HIGH		BIT(27)
#define VDCTRL0_HSYNC_ACT_HIGH		BIT(26)
#define VDCTRL0_DOTCLK_ACT_FALLING	BIT(25)
#define VDCTRL0_ENABLE_ACT_HIGH		BIT(24)
#define VDCTRL0_VSYNC_PERIOD_UNIT	BIT(21)
#define VDCTRL0_VSYNC_PULSE_WIDTH_UNIT	BIT(20)
#define VDCTRL0_HALF_LINE		BIT(19)
#define VDCTRL0_HALF_LINE_MODE		BIT(18)
#define VDCTRL0_SET_VSYNC_PULSE_WIDTH(x) ((x) & 0x3ffff)
#define VDCTRL0_GET_VSYNC_PULSE_WIDTH(x) ((x) & 0x3ffff)

#define VDCTRL2_SET_HSYNC_PERIOD(x)	((x) & 0x3ffff)
#define VDCTRL2_GET_HSYNC_PERIOD(x)	((x) & 0x3ffff)

#define VDCTRL3_MUX_SYNC_SIGNALS	BIT(29)
#define VDCTRL3_VSYNC_ONLY		BIT(28)
#define SET_HOR_WAIT_CNT(x)		(((x) & 0xfff) << 16)
#define GET_HOR_WAIT_CNT(x)		(((x) >> 16) & 0xfff)
#define SET_VERT_WAIT_CNT(x)		((x) & 0xffff)
#define GET_VERT_WAIT_CNT(x)		((x) & 0xffff)

#define VDCTRL4_SET_DOTCLK_DLY(x)	(((x) & 0x7) << 29) /* v4 only */
#define VDCTRL4_GET_DOTCLK_DLY(x)	(((x) >> 29) & 0x7) /* v4 only */
#define VDCTRL4_SYNC_SIGNALS_ON		BIT(18)
#define SET_DOTCLK_H_VALID_DATA_CNT(x)	((x) & 0x3ffff)

#define DEBUG0_HSYNC			BIT(26)
#define DEBUG0_VSYNC			BIT(25)

#define AS_CTRL_PS_DISABLE		BIT(23)
#define AS_CTRL_ALPHA_INVERT		BIT(20)
#define AS_CTRL_ALPHA(a)		(((a) & 0xff) << 8)
#define AS_CTRL_FORMAT_RGB565		(0xe << 4)
#define AS_CTRL_FORMAT_RGB444		(0xd << 4)
#define AS_CTRL_FORMAT_RGB555		(0xc << 4)
#define AS_CTRL_FORMAT_ARGB4444		(0x9 << 4)
#define AS_CTRL_FORMAT_ARGB1555		(0x8 << 4)
#define AS_CTRL_FORMAT_RGB888		(0x4 << 4)
#define AS_CTRL_FORMAT_ARGB8888		(0x0 << 4)
#define AS_CTRL_ENABLE_COLORKEY		BIT(3)
#define AS_CTRL_ALPHA_CTRL_ROP		(3 << 1)
#define AS_CTRL_ALPHA_CTRL_MULTIPLY	(2 << 1)
#define AS_CTRL_ALPHA_CTRL_OVERRIDE	(1 << 1)
#define AS_CTRL_ALPHA_CTRL_EMBEDDED	(0 << 1)
#define AS_CTRL_AS_ENABLE		BIT(0)

#define MXSFB_MIN_XRES			120
#define MXSFB_MIN_YRES			120
#define MXSFB_MAX_XRES			0xffff
#define MXSFB_MAX_YRES			0xffff

#endif /* __MXSFB_REGS_H__ */
