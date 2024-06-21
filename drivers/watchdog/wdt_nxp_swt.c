/*
 * Copyright 2022-2023 NXP
 * Copyright 2024 Manuel Arguelles
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT nxp_swt

#include <zephyr/kernel.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/irq.h>

#define LOG_LEVEL CONFIG_WDT_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(nxp_swt);

/*
 * Software Watchdog Timer (SWT) register definitions
 */

/* Control */
#define SWT_CR           0x0

#define SWT_CR_MAP       GENMASK(31, 24)
#define SWT_CR_SMD       GENMASK(10, 9)
#define SWT_CR_RIA       BIT(8)
#define SWT_CR_WND       BIT(7)
#define SWT_CR_ITR       BIT(6)
#define SWT_CR_HLK       BIT(5)
#define SWT_CR_SLK       BIT(4)
#define SWT_CR_STP       BIT(2)
#define SWT_CR_FRZ       BIT(1)
#define SWT_CR_WEN       BIT(0)

/* Interrupt */
#define SWT_IR           0x4
#define SWT_IR_TIF       BIT(0)

/* Timeout */
#define SWT_TO           0x8
#define SWT_TO_WTO       GENMASK(31, 0)

/* Window */
#define SWT_WN           0xC
#define SWT_WN_WST       GENMASK(31, 0)

/* Service */
#define SWT_SR           0x10
#define SWT_SR_WSC       GENMASK(15, 0)

/* Counter Output */
#define SWT_CO           0x14
#define SWT_CO_CNT       GENMASK(31, 0)

/* Service Key */
#define SWT_SK           0x18
#define SWT_SK_SK        GENMASK(15, 0)

/* Event Request */
#define SWT_RRR          0x1C
#define SWT_RRR_RRF      BIT(0)


#define SWT_TO_WTO_MIN 0x100

#define SWT_SR_WSC_UNLOCK_KEY1  0xC520U
#define SWT_SR_WSC_UNLOCK_KEY2  0xD928U

#define SWT_SR_WSC_SERVICE_KEY1 0xA602U
#define SWT_SR_WSC_SERVICE_KEY2 0xB480U

#define SWT_SOFT_LOCK_TIMEOUT_US 3000

/* Handy accessors */
#define REG_READ(r)      sys_read32(config->base + (r))
#define REG_WRITE(r, v)  sys_write32((v), config->base + (r))

enum swt_service_mode {
	SWT_FIXED_SERVICE = 0,
	SWT_KEYED_SERVICE = 1,
};

enum swt_lock_mode {
	SWT_UNLOCKED  = 0,
	SWT_SOFT_LOCK = 1,
	SWT_HARD_LOCK = 2,
};

struct nxp_swt_timeout {
	uint32_t period;
	uint32_t window_start;
	bool window_mode;
};

struct nxp_swt_config {
	mem_addr_t base;
	const struct device *clock_dev;
	clock_control_subsys_t clock_subsys;
	uint8_t master_access_mask;
	enum swt_lock_mode lock_mode;
	enum swt_service_mode service_mode;
	uint16_t initial_key;
	bool reset_on_invalid_access;
};

struct nxp_swt_data {
	wdt_callback_t callback;
	bool timeout_valid;
	struct nxp_swt_timeout timeout;
};

static void swt_lock(const struct nxp_swt_config *config)
{
	switch (config->lock_mode) {
	case SWT_HARD_LOCK:
		REG_WRITE(SWT_CR, REG_READ(SWT_CR) | FIELD_PREP(SWT_CR_HLK, 1U));
		break;
	case SWT_SOFT_LOCK:
		REG_WRITE(SWT_CR, REG_READ(SWT_CR) | FIELD_PREP(SWT_CR_SLK, 1U));
		break;
	case SWT_UNLOCKED:
		__fallthrough;
	default:
		break;
	}
}

static int swt_unlock(const struct nxp_swt_config *config)
{
	int err = 0;

	if (FIELD_GET(SWT_CR_HLK, REG_READ(SWT_CR)) != 0U) {
		LOG_ERR("Watchdog hard-locked");
		err = -EFAULT;

	} else if (FIELD_GET(SWT_CR_SLK, REG_READ(SWT_CR)) != 0U) {
		/* watchdog is software-locked, try to unlock it */
		REG_WRITE(SWT_SR, FIELD_PREP(SWT_SR_WSC, SWT_SR_WSC_UNLOCK_KEY1));
		REG_WRITE(SWT_SR, FIELD_PREP(SWT_SR_WSC, SWT_SR_WSC_UNLOCK_KEY2));

		if (WAIT_FOR(FIELD_GET(SWT_CR_SLK, REG_READ(SWT_CR) != 0),
			     SWT_SOFT_LOCK_TIMEOUT_US, NULL) == false) {

			/* make sure is locked again before we leave */
			REG_WRITE(SWT_CR, REG_READ(SWT_CR) | FIELD_PREP(SWT_CR_SLK, 1U));
			LOG_ERR("Timedout while trying to unlock");
			err = -ETIMEDOUT;
		}
	}

	return err;
}

static inline uint16_t swt_gen_service_key(const struct nxp_swt_config *config)
{
	return (uint16_t)((FIELD_GET(SWT_SK_SK, REG_READ(SWT_SK)) * 17U) + 3U);
}

static int nxp_swt_setup(const struct device *dev, uint8_t options)
{
	const struct nxp_swt_config *config = dev->config;
	struct nxp_swt_data *data = dev->data;
	int err;
	uint32_t reg_val;

	if (!data->timeout_valid) {
		LOG_ERR("No valid timeouts installed");
		return -EINVAL;
	}

	err = swt_unlock(config);
	if (err) {
		return err;
	}

	reg_val = REG_READ(SWT_CR) & ~(SWT_CR_WND | SWT_CR_STP | SWT_CR_FRZ | SWT_CR_ITR);
	if (data->timeout.window_mode) {
		reg_val |= FIELD_PREP(SWT_CR_WND, 1U);
	}
	if (data->callback) {
		reg_val |= FIELD_PREP(SWT_CR_ITR, 1U);
	}
	if (options & WDT_OPT_PAUSE_IN_SLEEP) {
		reg_val |= FIELD_PREP(SWT_CR_STP, 1U);
	}
	if (options & WDT_OPT_PAUSE_HALTED_BY_DBG) {
		reg_val |= FIELD_PREP(SWT_CR_FRZ, 1U);
	}
	REG_WRITE(SWT_CR, reg_val);

	REG_WRITE(SWT_IR, FIELD_PREP(SWT_IR_TIF, 1U));
	REG_WRITE(SWT_TO, FIELD_PREP(SWT_TO_WTO, data->timeout.period));
	REG_WRITE(SWT_WN, FIELD_PREP(SWT_WN_WST, data->timeout.window_start));

	if (config->service_mode == SWT_KEYED_SERVICE) {
		REG_WRITE(SWT_SK, FIELD_PREP(SWT_SK_SK, config->initial_key));
	}

	REG_WRITE(SWT_CR, REG_READ(SWT_CR) | FIELD_PREP(SWT_CR_WEN, 1U));
	swt_lock(config);

	return 0;
}

static int nxp_swt_disable(const struct device *dev)
{
	const struct nxp_swt_config *config = dev->config;
	struct nxp_swt_data *data = dev->data;
	int err;

	if (!FIELD_GET(SWT_CR_WEN, REG_READ(SWT_CR))) {
		LOG_ERR("Watchdog is not enabled");
		return -EFAULT;
	}

	err = swt_unlock(config);
	if (err) {
		return err;
	}

	/* Disable the watchdog and clear interrupt flags */
	REG_WRITE(SWT_CR, REG_READ(SWT_CR) & ~SWT_CR_WEN);
	REG_WRITE(SWT_IR, FIELD_PREP(SWT_IR_TIF, 1U));

	swt_lock(config);

	data->timeout_valid = false;

	return 0;
}

static int nxp_swt_install_timeout(const struct device *dev, const struct wdt_timeout_cfg *cfg)
{
	const struct nxp_swt_config *config = dev->config;
	struct nxp_swt_data *data = dev->data;
	bool window_mode = false;
	uint32_t window = 0;
	uint32_t period;
	uint32_t clock_rate;
	int err;

	if (data->timeout_valid) {
		LOG_ERR("No more timeouts can be installed");
		return -ENOMEM;
	}

	err = clock_control_get_rate(config->clock_dev, config->clock_subsys, &clock_rate);
	if (err) {
		LOG_ERR("Failed to get module clock frequency");
		return err;
	}

	period = clock_rate / 1000U * cfg->window.max;

	if (cfg->window.min) {
		window_mode = true;
		window = clock_rate / 1000U * (cfg->window.max - cfg->window.min);
	}

	if ((period < SWT_TO_WTO_MIN) || (period < window)) {
		LOG_ERR("Invalid timeout");
		return -EINVAL;
	}

	data->timeout.period = period;
	data->timeout.window_start = window;
	data->timeout.window_mode = window_mode;
	data->callback = cfg->callback;
	data->timeout_valid = true;
	LOG_DBG("Installed timeout: period=%d, window=%d (%s)",
		period, window, window_mode ? "enabled" : "disabled");

	return 0;
}

static int nxp_swt_feed(const struct device *dev, int channel)
{
	const struct nxp_swt_config *config = dev->config;
	bool match_unlock_seq = false;

	ARG_UNUSED(channel);

	switch (config->service_mode) {
	case SWT_KEYED_SERVICE:
		if (swt_gen_service_key(config) == SWT_SR_WSC_UNLOCK_KEY1) {
			match_unlock_seq = true;
		}
		REG_WRITE(SWT_SR, FIELD_PREP(SWT_SR_WSC, swt_gen_service_key(config)));

		if (swt_gen_service_key(config) == SWT_SR_WSC_UNLOCK_KEY1) {
			match_unlock_seq = true;
		}
		REG_WRITE(SWT_SR, FIELD_PREP(SWT_SR_WSC, swt_gen_service_key(config)));

		if (match_unlock_seq && (config->lock_mode == SWT_SOFT_LOCK)) {
			/*
			 * Service key generated matched the unlock sequence, complete the
			 * unlock sequence and reinitiate the soft lock.
			 */
			REG_WRITE(SWT_SR, FIELD_PREP(SWT_SR_WSC, SWT_SR_WSC_UNLOCK_KEY2));
			swt_lock(config);
		}
		break;
	case SWT_FIXED_SERVICE:
		REG_WRITE(SWT_SR, FIELD_PREP(SWT_SR_WSC, SWT_SR_WSC_SERVICE_KEY1));
		REG_WRITE(SWT_SR, FIELD_PREP(SWT_SR_WSC, SWT_SR_WSC_SERVICE_KEY2));
		break;
	default:
		/* Nothing to do */
		break;
	}

	LOG_DBG("Fed the watchdog");

	return 0;
}

static void nxp_swt_isr(const struct device *dev)
{
	const struct nxp_swt_config *config = dev->config;
	struct nxp_swt_data *data = dev->data;
	uint32_t reg_val;

	if (FIELD_GET(SWT_IR_TIF, REG_READ(SWT_IR)) && FIELD_GET(SWT_CR_ITR, REG_READ(SWT_CR))) {
		/* Clear interrupt flag */
		reg_val = REG_READ(SWT_IR);
		reg_val &= SWT_IR_TIF;
		REG_WRITE(SWT_IR, reg_val);

		if (data->callback) {
			/* SWT only has one channel */
			data->callback(dev, 0U);
		}
	}
}

static int nxp_swt_init(const struct device *dev)
{
	const struct nxp_swt_config *config = dev->config;
	int err;

	if (!device_is_ready(config->clock_dev)) {
		return -ENODEV;
	}

	err = clock_control_on(config->clock_dev, config->clock_subsys);
	if (err) {
		return err;
	}

	REG_WRITE(SWT_CR,
		  FIELD_PREP(SWT_CR_MAP, config->master_access_mask) |
		  FIELD_PREP(SWT_CR_RIA, config->reset_on_invalid_access) |
		  FIELD_PREP(SWT_CR_SMD, config->service_mode));

	return 0;
}

static const struct wdt_driver_api nxp_swt_driver_api = {
	.setup = nxp_swt_setup,
	.disable = nxp_swt_disable,
	.install_timeout = nxp_swt_install_timeout,
	.feed = nxp_swt_feed,
};

#define NXP_SWT_DEVICE_INIT(n)								\
	static struct nxp_swt_data nxp_swt_data_##n;					\
											\
	static const struct nxp_swt_config nxp_swt_config_##n = {			\
		.base = DT_INST_REG_ADDR(n),						\
		.clock_dev = DEVICE_DT_GET(DT_INST_CLOCKS_CTLR(n)),			\
		.clock_subsys = (clock_control_subsys_t)DT_INST_CLOCKS_CELL(n, name),	\
		.master_access_mask = DT_INST_PROP(n, master_access_mask),		\
		.reset_on_invalid_access = DT_INST_PROP(n, reset_on_invalid_access),	\
		.service_mode = DT_INST_ENUM_IDX(n, service_mode),			\
		.initial_key = (uint16_t)DT_INST_PROP(n, initial_key),			\
		.lock_mode = DT_INST_ENUM_IDX(n, lock_mode),				\
	};										\
											\
	static int nxp_swt_##n##_init(const struct device *dev)				\
	{										\
		int err;								\
											\
		err = nxp_swt_init(dev);						\
		if (err) {								\
			return err;							\
		}									\
											\
		IRQ_CONNECT(DT_INST_IRQN(n), DT_INST_IRQ(n, priority),			\
			    nxp_swt_isr, DEVICE_DT_INST_GET(n),				\
			    COND_CODE_1(DT_INST_IRQ_HAS_CELL(n, flags),			\
					(DT_INST_IRQ(n, flags)), (0)));			\
		irq_enable(DT_INST_IRQN(n));						\
											\
		return 0;								\
	}										\
											\
	DEVICE_DT_INST_DEFINE(n,							\
			 nxp_swt_##n##_init,						\
			 NULL,								\
			 &nxp_swt_data_##n,						\
			 &nxp_swt_config_##n,						\
			 POST_KERNEL,							\
			 CONFIG_KERNEL_INIT_PRIORITY_DEVICE,				\
			 &nxp_swt_driver_api);

DT_INST_FOREACH_STATUS_OKAY(NXP_SWT_DEVICE_INIT)
