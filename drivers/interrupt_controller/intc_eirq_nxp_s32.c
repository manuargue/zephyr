/*
 * Copyright 2022-2023 NXP
 * Copyright 2024 Manuel Arguelles
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nxp_s32_siul2_eirq

#include <soc.h>
#include <zephyr/irq.h>
#include <zephyr/sys/sys_io.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/interrupt_controller/intc_eirq_nxp_s32.h>

/* System Integration Unit Lite2 (SIUL2) External Interrupt controller (EIRQ) registers */
// offsets from DISR0

/* SIUL2 DMA/Interrupt Status Flag 0 */
#define SIUL2_DISR0 0x0

/* SIUL2 DMA/Interrupt Request Enable 0 */
#define SIUL2_DIRER0 0x8

/* SIUL2 DMA/Interrupt Request Select 0 */
#define SIUL2_DIRSR0 0x10

/* SIUL2 Interrupt Rising-Edge Event Enable 0 */
#define SIUL2_IREER0 0x18

/* SIUL2 Interrupt Falling-Edge Event Enable 0 */
#define SIUL2_IFEER0 0x20

/* SIUL2 Interrupt Filter Enable 0 */
#define SIUL2_IFER0 0x28

/* SIUL2 Interrupt Filter Maximum Counter */
#define SIUL2_IFMCR(n)     (0x30 + 4 * (n))
#define SIUL2_IFMCR_MAXCNT GENMASK(3, 0)

/* SIUL2 Interrupt Filter Clock Prescaler */
#define SIUL2_IFCPR      0xB0
#define SIUL2_IFCPR_IFCP GENMASK(3, 0)

// TODO
#if defined(CONFIG_SOC_SERIES_S32ZE)
#define HAS_SINGLE_INTERRUPT 1
#define PINS_MAX             8
#define PINS_PER_INTERRUPT   8
#elif defined(CONFIG_SOC_SERIES_S32K3)
#define PINS_MAX           32
#define PINS_PER_INTERRUPT 8
#else
#error "platform not supported"
#endif

/* Handy accessors */
#define REG_READ(r)     sys_read32(config->base + (r))
#define REG_WRITE(r, v) sys_write32((v), config->base + (r))

struct nxp_siul2_eirq_interrupt {
	/** The interrupt ID */
	uint8_t id;
	/** Digital filter enabled */
	bool digital_filter;
	/** Maximum interrupt filter counter */
	uint8_t max_filter_counter;
};

struct nxp_siul2_eirq_controller {
	/** Interrupt filter clock prescaler */
	uint8_t filter_clock_prescaler;
	/** Number of channels configured */
	uint8_t interrupts_count;
	/** Channels configuration */
	const struct nxp_siul2_eirq_interrupt *interrupts;
};

struct nxp_siul2_eirq_config {
	mem_addr_t base;
	const struct pinctrl_dev_config *pincfg;
	const struct nxp_siul2_eirq_controller *controller;
};

struct nxp_siul2_eirq_cb {
	nxp_siul2_eirq_callback_t cb;
	uint8_t pin;
	void *data;
};

struct nxp_siul2_eirq_data {
	struct nxp_siul2_eirq_cb *cb;
};

int nxp_siul2_eirq_set_callback(const struct device *dev, uint8_t irq, uint8_t pin,
				nxp_siul2_eirq_callback_t cb, void *arg)
{
	struct nxp_siul2_eirq_data *data = dev->data;

	__ASSERT(irq < PINS_MAX, "Interrupt number is out of range");

	if (data->cb[irq].cb) {
		return -EBUSY;
	}

	data->cb[irq].cb = cb;
	data->cb[irq].pin = pin;
	data->cb[irq].data = arg;

	return 0;
}

void nxp_siul2_eirq_unset_callback(const struct device *dev, uint8_t irq)
{
	struct nxp_siul2_eirq_data *data = dev->data;

	__ASSERT(irq < PINS_MAX, "Interrupt number is out of range");

	data->cb[irq].cb = NULL;
	data->cb[irq].pin = 0;
	data->cb[irq].data = NULL;
}

void nxp_siul2_eirq_set_trigger(const struct nxp_siul2_eirq_config *config, uint8_t irq,
				enum nxp_siul2_eirq_trigger trigger)
{
	uint32_t reg_val;

	reg_val = REG_READ(SIUL2_IREER0);
	if ((trigger == NXP_SIUL2_EIRQ_RISING_EDGE) ||
	    (trigger == NXP_SIUL2_EIRQ_BOTH_EDGES)) {
		reg_val |= BIT(irq);
	} else {
		reg_val &= ~BIT(irq);
	}
	REG_WRITE(SIUL2_IREER0, reg_val);

	reg_val = REG_READ(SIUL2_IFEER0);
	if ((trigger == NXP_SIUL2_EIRQ_FALLING_EDGE) ||
	    (trigger == NXP_SIUL2_EIRQ_BOTH_EDGES)) {
		reg_val |= BIT(irq);
	} else {
		reg_val &= ~BIT(irq);
	}
	REG_WRITE(SIUL2_IFEER0, reg_val);
}

void nxp_siul2_eirq_enable_interrupt(const struct device *dev, uint8_t irq,
				     enum nxp_siul2_eirq_trigger trigger)
{
	const struct nxp_siul2_eirq_config *config = dev->config;

	__ASSERT(irq < PINS_MAX, "Interrupt number is out of range");

	nxp_siul2_eirq_set_trigger(config, irq, trigger);

	/* Clear status flag and unmask interrupt */
	REG_WRITE(SIUL2_DISR0, REG_READ(SIUL2_DISR0) | BIT(irq));
	REG_WRITE(SIUL2_DIRER0, REG_READ(SIUL2_DIRER0) | BIT(irq));
}

void nxp_siul2_eirq_disable_interrupt(const struct device *dev, uint8_t irq)
{
	const struct nxp_siul2_eirq_config *config = dev->config;

	__ASSERT(irq < PINS_MAX, "Interrupt number is out of range");

	/* Clear status flag and mask interrupt */
	REG_WRITE(SIUL2_DISR0, REG_READ(SIUL2_DISR0) | BIT(irq));
	REG_WRITE(SIUL2_DIRER0, REG_READ(SIUL2_DIRER0) & ~BIT(irq));

	// nxp_siul2_eirq_set_trigger(config, irq, NXP_SIUL2_EIRQ_DISABLE);
}

uint32_t nxp_siul2_eirq_get_pending(const struct device *dev)
{
	const struct nxp_siul2_eirq_config *config = dev->config;

	return REG_READ(SIUL2_DISR0) & REG_READ(SIUL2_DIRER0);
}

static int nxp_siul2_eirq_init(const struct device *dev)
{
	const struct nxp_siul2_eirq_config *config = dev->config;
	const struct nxp_siul2_eirq_controller *controller = config->controller;
	const struct nxp_siul2_eirq_interrupt *interrupt;
	uint32_t reg_val;
	int err;

	err = pinctrl_apply_state(config->pincfg, PINCTRL_STATE_DEFAULT);
	if (err) {
		return err;
	}

	REG_WRITE(SIUL2_IFCPR, FIELD_PREP(SIUL2_IFCPR_IFCP, controller->filter_clock_prescaler));

	for (uint8_t i = 0; i < controller->interrupts_count; i++) {
		interrupt = &controller->interrupts[i];

		/* Mask and clear status flag */
		REG_WRITE(SIUL2_DIRER0, REG_READ(SIUL2_DIRER0) & ~BIT(interrupt->id));
		REG_WRITE(SIUL2_DISR0, REG_READ(SIUL2_DISR0) | BIT(interrupt->id));

		/* Select the request type as interrupt */
		REG_WRITE(SIUL2_DIRSR0, REG_READ(SIUL2_DIRSR0) & ~BIT(interrupt->id));

		REG_WRITE(SIUL2_IFMCR(interrupt->id),
			  FIELD_PREP(SIUL2_IFMCR_MAXCNT, interrupt->max_filter_counter));

		reg_val = REG_READ(SIUL2_IFER0) & ~BIT(interrupt->id);
		if (interrupt->digital_filter) {
			reg_val |= BIT(interrupt->id);
		}
		REG_WRITE(SIUL2_IFER0, reg_val);
	}

	return 0;
}

static inline void nxp_siul2_eirq_interrupt_handler(const struct device *dev, uint32_t irq_idx)
{
	const struct nxp_siul2_eirq_config *config = dev->config;
	struct nxp_siul2_eirq_data *data = dev->data;
	uint8_t irq_start = irq_idx * PINS_PER_INTERRUPT;
	uint8_t irq_end = irq_start + PINS_PER_INTERRUPT;
	uint32_t irq_mask;
	uint8_t irq;

	irq_mask = REG_READ(SIUL2_DISR0) & REG_READ(SIUL2_DIRER0);

	for (irq = irq_start; irq < irq_end; irq++) {
		if (BIT(irq) & irq_mask) {
			/* Clear status flag */
			REG_WRITE(SIUL2_DISR0, REG_READ(SIUL2_DISR0) | BIT(irq));

			if (data->cb[irq].cb != NULL) {
				data->cb[irq].cb(data->cb[irq].pin, data->cb[irq].data);
			}
		}
	}
}

#if defined(HAS_SINGLE_INTERRUPT)
static void nxp_siul2_eirq_isr(const struct device *dev)
{
	nxp_siul2_eirq_interrupt_handler(dev, 0U);
}

#else
#define NXP_SIUL2_EIRQ_ISR_DEFINE(idx, n)                                                          \
	static void nxp_siul2_eirq_isr_##idx##_##n(const struct device *dev)                       \
	{                                                                                          \
		nxp_siul2_eirq_interrupt_handler(dev, idx);                                        \
	}
#endif /* HAS_SINGLE_INTERRUPT */

#define NXP_SIUL2_EIRQ_CONTROLLER_INTERRUPT_CONFIG(idx, n)                                         \
	{                                                                                          \
		.id = idx,                                                                         \
		.digital_filter = DT_INST_PROP_OR(DT_CHILD(n, line_##idx), filter_enable, 0),      \
		.max_filter_counter =                                                              \
			DT_INST_PROP_OR(DT_CHILD(n, line_##idx), max_filter_counter, 0),           \
	}

#define NXP_SIUL2_EIRQ_CONTROLLER_CONFIG(n)                                                        \
	static const struct nxp_siul2_eirq_interrupt nxp_siul2_eirq_interrupts_##n[] = {           \
		LISTIFY(PINS_MAX, NXP_SIUL2_EIRQ_CONTROLLER_INTERRUPT_CONFIG, (, ), n),            \
	};                                                                                         \
	static const struct nxp_siul2_eirq_controller nxp_siul2_eirq_controller_##n = {            \
		.filter_clock_prescaler = DT_INST_PROP_OR(n, filter_prescaler, 0),                 \
		.interrupts_count = PINS_MAX,                                                      \
		.interrupts = nxp_siul2_eirq_interrupts_##n,                                       \
	}

#define _NXP_SIUL2_EIRQ_IRQ_CONFIG(idx, n)                                                         \
	do {                                                                                       \
		IRQ_CONNECT(DT_INST_IRQ_BY_IDX(n, idx, irq), DT_INST_IRQ_BY_IDX(n, idx, priority), \
			    COND_CODE_1(HAS_SINGLE_INTERRUPT, (nxp_siul2_eirq_isr),                \
					(nxp_siul2_eirq_isr_##idx##_##n)),                         \
			    DEVICE_DT_INST_GET(n),                                                 \
			    COND_CODE_1(CONFIG_GIC, (DT_INST_IRQ_BY_IDX(n, idx, flags)), (0)));    \
		irq_enable(DT_INST_IRQ_BY_IDX(n, idx, irq));                                       \
	} while (false);

#define NXP_SIUL2_EIRQ_IRQ_CONFIG(n)                                                               \
	LISTIFY(DT_NUM_IRQS(DT_DRV_INST(n)), _NXP_SIUL2_EIRQ_IRQ_CONFIG, (), n)

#define NXP_SIUL2_EIRQ_INIT_DEVICE(n)                                                              \
	IF_DISABLED(HAS_SINGLE_INTERRUPT,                                                          \
		    (LISTIFY(DT_NUM_IRQS(DT_DRV_INST(n)), NXP_SIUL2_EIRQ_ISR_DEFINE, (), n)))      \
	PINCTRL_DT_INST_DEFINE(n);                                                                 \
	NXP_SIUL2_EIRQ_CONTROLLER_CONFIG(n);                                                       \
	static const struct nxp_siul2_eirq_config nxp_siul2_eirq_conf_##n = {                      \
		.base = DT_INST_REG_ADDR(n),                                                       \
		.pincfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),                                       \
		.controller = &nxp_siul2_eirq_controller_##n,                                      \
	};                                                                                         \
	static struct nxp_siul2_eirq_cb nxp_siul2_eirq_cb_##n[PINS_MAX];                           \
	static struct nxp_siul2_eirq_data nxp_siul2_eirq_data_##n = {                              \
		.cb = nxp_siul2_eirq_cb_##n,                                                       \
	};                                                                                         \
	static int nxp_siul2_eirq_init_##n(const struct device *dev)                               \
	{                                                                                          \
		int err;                                                                           \
                                                                                                   \
		err = nxp_siul2_eirq_init(dev);                                                    \
		if (err) {                                                                         \
			return err;                                                                \
		}                                                                                  \
                                                                                                   \
		NXP_SIUL2_EIRQ_IRQ_CONFIG(n);                                                      \
                                                                                                   \
		return 0;                                                                          \
	}                                                                                          \
	DEVICE_DT_INST_DEFINE(n, nxp_siul2_eirq_init_##n, NULL, &nxp_siul2_eirq_data_##n,          \
			      &nxp_siul2_eirq_conf_##n, PRE_KERNEL_2, CONFIG_INTC_INIT_PRIORITY,   \
			      NULL);

DT_INST_FOREACH_STATUS_OKAY(NXP_SIUL2_EIRQ_INIT_DEVICE)
