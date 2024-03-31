/*
 * Copyright 2024 NXP
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT snps_designware_ethernet_mdio

#define LOG_MODULE_NAME dwmac_mdio
#define LOG_LEVEL       CONFIG_MDIO_LOG_LEVEL
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/mdio.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/drivers/clock_control.h>

struct dwmac_config {
	uint8_t instance;
	bool suppress_preamble;
	const struct pinctrl_dev_config *pincfg;
	const struct device *clock_dev;
	clock_control_subsys_t clock_subsys;
};

struct dwmac_priv {
	struct k_mutex bus_mutex;
};

static int dwmac_read_c45(const struct device *dev, uint8_t prtad, uint8_t devad, uint16_t regad,
			  uint16_t *regval)
{
	// const struct dwmac_config *const cfg = dev->config;
	struct dwmac_priv *p = dev->data;

	k_mutex_lock(&p->bus_mutex, K_FOREVER);

	/* Configure MDIO controller before initiating a transmission */
	// Gmac_Ip_EnableMDIO(cfg->instance, cfg->suppress_preamble, data->clock_freq);

	// status = Gmac_Ip_MDIOReadMMD(cfg->instance, prtad, devad, regad, regval,
	// 			     CONFIG_MDIO_NXP_S32_TIMEOUT);

	k_mutex_unlock(&p->bus_mutex);

	return 0;
}

static int dwmac_write_c45(const struct device *dev, uint8_t prtad, uint8_t devad, uint16_t regad,
			   uint16_t regval)
{
	// const struct dwmac_config *const cfg = dev->config;
	struct dwmac_priv *p = dev->data;

	k_mutex_lock(&p->bus_mutex, K_FOREVER);

	/* Configure MDIO controller before initiating a transmission */
	// Gmac_Ip_EnableMDIO(cfg->instance, cfg->suppress_preamble, data->clock_freq);

	// status = Gmac_Ip_MDIOWriteMMD(cfg->instance, prtad, devad, regad, regval,
	// 			      CONFIG_MDIO_NXP_S32_TIMEOUT);

	k_mutex_unlock(&p->bus_mutex);

	return 0;
}

static int dwmac_read_c22(const struct device *dev, uint8_t prtad, uint8_t regad, uint16_t *regval)
{
	// const struct dwmac_config *const cfg = dev->config;
	struct dwmac_priv *p = dev->data;

	k_mutex_lock(&p->bus_mutex, K_FOREVER);

	/* Configure MDIO controller before initiating a transmission */
	// Gmac_Ip_EnableMDIO(cfg->instance, cfg->suppress_preamble, data->clock_freq);

	// status = Gmac_Ip_MDIORead(cfg->instance, prtad, regad, regval,
	// 			  CONFIG_MDIO_NXP_S32_TIMEOUT);

	k_mutex_unlock(&p->bus_mutex);

	return 0;
}

static int dwmac_write_c22(const struct device *dev, uint8_t prtad, uint8_t regad, uint16_t regval)
{
	// const struct dwmac_config *const cfg = dev->config;
	struct dwmac_priv *p = dev->data;

	k_mutex_lock(&p->bus_mutex, K_FOREVER);

	/* Configure MDIO controller before initiating a transmission */
	// Gmac_Ip_EnableMDIO(cfg->instance, cfg->suppress_preamble, data->clock_freq);

	// status = Gmac_Ip_MDIOWrite(cfg->instance, prtad, regad, regval,
	// 			   CONFIG_MDIO_NXP_S32_TIMEOUT);

	k_mutex_unlock(&p->bus_mutex);

	return 0;
}

static int dwmac_csr_init(uint32_t clk_rate)
{
	uint32_t divider;

	clk_rate /= MHZ(1);
	if (clk_rate >= 20 && clk_rate < 35) {
		divider = 2;
	} else if (clk_rate < 60) {
		divider = 3;
	} else if (clk_rate < 100) {
		divider = 0;
	} else if (clk_rate < 150) {
		divider = 1;
	} else if (clk_rate < 250) {
		divider = 4;
	} else if (clk_rate < 300) {
		divider = 5;
	} else if (clk_rate < 500) {
		divider = 6;
	} else if (clk_rate < 800) {
		divider = 7;
	} else {
		LOG_ERR("MDIO clock range not supported");
		return -ENOTSUP;
	}

	// config->base->MAC_MDIO_ADDRESS = ENET_QOS_REG_PREP(MAC_MDIO_ADDRESS, CR, divider);

	return 0;
}

static int dwmac_init(const struct device *dev)
{
	const struct dwmac_config *const cfg = dev->config;
	struct dwmac_priv *p = dev->data;
	uint32_t clock_freq;
	int err;

	if (cfg->clock_dev) {
		if (!device_is_ready(cfg->clock_dev)) {
			LOG_ERR("MDIO clock control device not ready");
			return -ENODEV;
		}

		err = clock_control_get_rate(cfg->clock_dev, cfg->clock_subsys, &clock_freq);
		if (err != 0) {
			LOG_ERR("Failed to get MDIO clock frequency (%d)", err);
			return err;
		}

		err = dwmac_csr_init(clock_freq);
		if (err != 0) {
			return err;
		}
	}

	if (cfg->pincfg) {
		err = pinctrl_apply_state(cfg->pincfg, PINCTRL_STATE_DEFAULT);
		if (err != 0) {
			LOG_ERR("Failed to initialize MDIO pins (%d)", err);
			return err;
		}
	}

	k_mutex_init(&p->bus_mutex);

	return 0;
}

static void dwmac_noop(const struct device *dev)
{
	ARG_UNUSED(dev);
}

static const struct mdio_driver_api dwmac_api = {
	.read = dwmac_read_c22,
	.write = dwmac_write_c22,
	.read_c45 = dwmac_read_c45,
	.write_c45 = dwmac_write_c45,
	/* Controller does not support enabling/disabling MDIO bus */
	.bus_enable = dwmac_noop,
	.bus_disable = dwmac_noop,
};

#define DWMAC_INST_INIT(n)                                                                         \
	PINCTRL_DT_INST_DEFINE(n);                                                                 \
	static struct dwmac_priv dwmac_priv##n;                                                    \
	static const struct dwmac_config dwmac_config##n = {                                       \
		.pincfg = PINCTRL_DT_INST_DEV_CONFIG_GET(n),                                       \
		.clock_dev = DEVICE_DT_GET_OR_NULL(DT_INST_CLOCKS_CTLR(n)),                        \
		.clock_subsys = (clock_control_subsys_t)DT_INST_CLOCKS_CELL(n, name),              \
		.suppress_preamble = (bool)DT_INST_PROP(n, suppress_preamble),                     \
	};                                                                                         \
	DEVICE_DT_INST_DEFINE(n, &dwmac_init, NULL, &dwmac_priv##n, &dwmac_config##n, POST_KERNEL, \
			      CONFIG_MDIO_INIT_PRIORITY, &dwmac_api);

DT_INST_FOREACH_STATUS_OKAY(DWMAC_INST_INIT)
