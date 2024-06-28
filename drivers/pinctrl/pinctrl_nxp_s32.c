/*
 * Copyright 2022 NXP
 * Copyright 2024 Manuel Arguelles
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/pinctrl.h>

/* SIUL2 register definitions */
#define SIUL2_MSCR(n) (0x240 + 4 * (n))
#define SIUL2_IMCR(n) (0xA40 + 4 * (n))

#define SIUL2_MSCR_MAX_IDX 2048
#define SIUL2_IMCR_MAX_IDX 512


/* Utility macro that expands to the SIUL2 base address if it exists or zero.
 * Note that some devices may have instance gaps, hence the need to keep them in the array.
 */
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
