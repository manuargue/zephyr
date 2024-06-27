/*
 * Copyright 2022-2023 NXP
 * Copyright 2024 Manuel Arguelles
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_SOC_ARM_NXP_S32_COMMON_PINCTRL_SOC_H_
#define ZEPHYR_SOC_ARM_NXP_S32_COMMON_PINCTRL_SOC_H_

#include <zephyr/devicetree.h>
#include <zephyr/dt-bindings/pinctrl/nxp-s32-pinctrl.h>
#include <zephyr/sys/util.h>
#include <zephyr/types.h>

/** @cond INTERNAL_HIDDEN */

/** @brief Type for NXP SIUL2 pin configuration. */
typedef struct {
	uint8_t siul2_idx;
	uint32_t imcr_idx;
	uint32_t imcr_val;
	uint32_t mscr_idx;
	uint32_t mscr_val;
} pinctrl_soc_pin_t;

#define SIUL2_MSCR_MAX_IDX 2048
#define SIUL2_IMCR_MAX_IDX 512

// TODO: move to the soc folder and include it here
// #include "pinctrl_soc_device.h"

#if defined(CONFIG_SOC_SERIES_S32K3)
#define SIUL2_MSCR(n)  (0x240 + 4 * (n))
#define SIUL2_MSCR_OBE BIT(21)
#define SIUL2_MSCR_IBE BIT(19)
#define SIUL2_MSCR_INV BIT(17)
#define SIUL2_MSCR_PKE BIT(16)
#define SIUL2_MSCR_SRC BIT(14)
#define SIUL2_MSCR_PUE BIT(13)
#define SIUL2_MSCR_PUS BIT(11)
#define SIUL2_MSCR_DSE BIT(8)
#define SIUL2_MSCR_IFE BIT(6)
#define SIUL2_MSCR_SMC BIT(5)
#define SIUL2_MSCR_SSS GENMASK(3, 0)

#define SIUL2_IMCR(n)  (0xA40 + 4 * (n))
#define SIUL2_IMCR_SSS GENMASK(3, 0)

#elif defined(CONFIG_SOC_SERIES_S32ZE)
#define SIUL2_MSCR      0x240
#define SIUL2_MSCR_OBE  BIT(21)
#define SIUL2_MSCR_ODE  BIT(20)
#define SIUL2_MSCR_IBE  BIT(19)
#define SIUL2_MSCR_RXCB BIT(17)
#define SIUL2_MSCR_SRE  GENMASK(16, 14)
#define SIUL2_MSCR_PUE  BIT(13)
#define SIUL2_MSCR_PUS  BIT(12)
#define SIUL2_MSCR_CREF BIT(11)
#define SIUL2_MSCR_RCVR BIT(10)
#define SIUL2_MSCR_TRC  BIT(8)
#define SIUL2_MSCR_SMC  GENMASK(7, 4)
#define SIUL2_MSCR_SSS  GENMASK(2, 0)

#define SIUL2_IMCR      0xA40
#define SIUL2_IMCR_SSS  GENMASK(2, 0)

#else
#error "platform not supported"
#endif /* CONFIG_SOC_SERIES_S32K3 */

#define SIUL2_HAS_ONE_BIT_SLEW_RATE(group)                                                         \
	UTIL_OR(DT_ENUM_HAS_VALUE(group, slew_rate, slowest),                                      \
		DT_ENUM_HAS_VALUE(group, slew_rate, fastest))

#define NXP_S32_PINMUX_INIT(group, val)                                                            \
	.siul2_idx = NXP_S32_PINMUX_GET_SIUL2_IDX(val),                                            \
	.imcr_idx = NXP_S32_PINMUX_GET_IMCR_IDX(val),                                              \
	.imcr_val = FIELD_PREP(SIUL2_IMCR_SSS, NXP_S32_PINMUX_GET_IMCR_SSS(val)),                  \
	.mscr_idx = NXP_S32_PINMUX_GET_MSCR_IDX(val),                                              \
	.mscr_val = FIELD_PREP(SIUL2_MSCR_SSS, NXP_S32_PINMUX_GET_MSCR_SSS(val)) |                 \
		    FIELD_PREP(SIUL2_MSCR_OBE, DT_PROP(group, output_enable)) |                    \
		    FIELD_PREP(SIUL2_MSCR_IBE, DT_PROP(group, input_enable)) |                     \
		    FIELD_PREP(SIUL2_MSCR_PUE,                                                     \
			       DT_PROP(group, bias_pull_up) || DT_PROP(group, bias_pull_down)) |   \
		    FIELD_PREP(SIUL2_MSCR_PUS, DT_PROP(group, bias_pull_up)) |                     \
		    UTIL_AND(SIUL2_HAS_ONE_BIT_SLEW_RATE(group),                                   \
			     FIELD_PREP(SIUL2_MSCR_SRC, DT_ENUM_IDX(group, slew_rate))) |          \
		    UTIL_AND(UTIL_NOT(SIUL2_HAS_ONE_BIT_SLEW_RATE(group)),                         \
			     FIELD_PREP(SIUL2_MSCR_SRE, DT_PROP(group, slew_rate))) |              \
		    UTIL_AND(DT_NODE_HAS_PROP(group, nxp_drive_strength),                          \
			     (FIELD_PREP(SIUL2_MSCR_DSE, DT_PROP(group, nxp_drive_strength)))) |   \
		    UTIL_AND(DT_NODE_HAS_PROP(group, nxp_invert),                                  \
			     (FIELD_PREP(SIUL2_MSCR_INV, DT_PROP(group, nxp_invert)))) |           \
		    UTIL_AND(DT_NODE_HAS_PROP(group, drive_open_drain),                            \
			     (FIELD_PREP(SIUL2_MSCR_ODE, (DT_PROP(group, drive_open_drain) &&      \
							  DT_PROP(group, output_enable)))))

/**
 * @brief Utility macro to initialize each pin.
 *
 *
 * @param group Group node identifier.
 * @param prop Property name.
 * @param idx Property entry index.
 */
#define Z_PINCTRL_STATE_PIN_INIT(group, prop, idx)                                                 \
	{NXP_S32_PINMUX_INIT(group, DT_PROP_BY_IDX(group, prop, idx))},

/**
 * @brief Utility macro to initialize state pins contained in a given property.
 *
 * @param node_id Node identifier.
 * @param prop Property name describing state pins.
 */
#define Z_PINCTRL_STATE_PINS_INIT(node_id, prop)                                                   \
	{                                                                                          \
		DT_FOREACH_CHILD_VARGS(DT_PHANDLE(node_id, prop), DT_FOREACH_PROP_ELEM, pinmux,    \
				       Z_PINCTRL_STATE_PIN_INIT)                                   \
	}

/** @endcond */

#endif /* ZEPHYR_SOC_ARM_NXP_S32_COMMON_PINCTRL_SOC_H_ */
