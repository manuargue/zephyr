/*
 * Copyright 2022 NXP
 * Copyright 2024 Manuel Arguelles
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/pinctrl.h>

// TODO: this fixup is already done in the pinmuxing script generator

// #if defined(CONFIG_SOC_SERIES_S32K3)
// #define SIUL2_0_IMCR_IDX_END 378
// #elif defined(CONFIG_SOC_SERIES_S32ZE)
// #define SIUL2_0_IMCR_IDX_END 89
// #define SIUL2_1_IMCR_IDX_END 209
// #define SIUL2_3_IMCR_IDX_END 23
// #define SIUL2_4_IMCR_IDX_END 371
// #define SIUL2_5_IMCR_IDX_END 474
// #else
// #error "platform not supported"
// #endif /* CONFIG_SOC_SERIES_S32K3 */

// static inline void siul2_get_imcr_reg(uint32_t *imcr_idx, uint8_t *siul2_idx)
// {
// #if defined(CONFIG_SOC_SERIES_S32K3)
// 	if (*imcr_idx < SIUL2_IMCR_MAX_IDX) {
// 		*siul2_idx = 0;
// 	} else {
// 		*siul2_idx = 1;
// 		*imcr_idx = imcr_idx - SIUL2_IMCR_MAX_IDX;
// 	}

// #elif defined(CONFIG_SOC_SERIES_S32ZE)
// 	if (*imcr_idx < SIUL2_IMCR_MAX_IDX) {
// 		if (*imcr_idx <= SIUL2_0_IMCR_IDX_END) {
// 			*siul2_idx = 0;
// 		} else if (*imcr_idx <= SIUL2_1_IMCR_IDX_END) {
// 			*siul2_idx = 1;
// 		} else if (*imcr_idx <= SIUL2_4_IMCR_IDX_END) {
// 			*siul2_idx = 4;
// 		} else if (*imcr_idx <= SIUL2_5_IMCR_IDX_END) {
// 			*siul2_idx = 5;
// 		}
// 	} else {
// 		*siul2_idx = 3;
// 		*imcr_idx = *imcr_idx - SIUL2_IMCR_MAX_IDX;
// 	}

// #else
// #error "platform not supported"
// #endif /* CONFIG_SOC_SERIES_S32K3 */
// }

/** Utility macro that expands to the SIUL2 base address if it exists */
#define SIUL2_BASE_OR_ZERO(nodelabel)                                                              \
	COND_CODE_1(DT_NODE_EXISTS(DT_NODELABEL(nodelabel)),                                       \
		    (DT_REG_ADDR(DT_NODELABEL(nodelabel))), (0U))

static mem_addr_t siul2_bases[] = {
	SIUL2_BASE_OR_ZERO(siul2_0), SIUL2_BASE_OR_ZERO(siul2_1), SIUL2_BASE_OR_ZERO(siul2_2),
	SIUL2_BASE_OR_ZERO(siul2_3), SIUL2_BASE_OR_ZERO(siul2_4), SIUL2_BASE_OR_ZERO(siul2_5),
};

static void pinctrl_configure_pin(const pinctrl_soc_pin_t *pin)
{
	mem_addr_t base;

	__ASSERT_NO_MSG(pin->siul2_idx < ARRAY_SIZE(siul2_bases));
	base = siul2_bases[pin->siul2_idx];
	__ASSERT_NO_MSG(base != 0);

	/* Multiplexed Signal Configuration */
	__ASSERT_NO_MSG(pin->mscr_idx < SIUL2_MSCR_MAX_IDX);
	sys_write32(pin->mscr_val, (base + SIUL2_MSCR(pin->mscr_idx)));

	if (pin->mscr_val & SIUL2_MSCR_IBE) {
		/* Input Multiplexed Signal Configuration */
		__ASSERT_NO_MSG(pin->imcr_idx < SIUL2_IMCR_MAX_IDX);
		sys_write32(pin->imcr_val, (base + SIUL2_IMCR(pin->imcr_idx)));
	}
}

int pinctrl_configure_pins(const pinctrl_soc_pin_t *pins, uint8_t pin_cnt, uintptr_t reg)
{
	ARG_UNUSED(reg);

	for (uint8_t i = 0; i < pin_cnt; i++) {
		pinctrl_configure_pin(pins++);
	}

	return 0;
}
