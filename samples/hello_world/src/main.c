/*
 * Copyright (c) 2012-2014 Wind River Systems, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/linker/linker-defs.h>
#include <zephyr/arch/arm/cortex_a_r/lib_helpers.h>

int main(void)
{
	printf("Hello World! %s\n", CONFIG_BOARD_TARGET);
	printf("SCTLR.FI=%lx\n", read_sctlr() & BIT(21));

	uint64_t tmp = read_sysreg64(0, 15);
	printf("CPUACTLR=%llx (.OOODIVDIS=%lld)\n", tmp, (tmp >> 32) & BIT(13));

	return 0;
}
