/*
 * This file is part of the coreboot project.
 *
 * Copyright (c) 2014-2015, NVIDIA CORPORATION.
 * Copyright 2014 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <delay.h>
#include <soc/addressmap.h>
#include <soc/clk_rst.h>
#include <soc/clock.h>
#include <soc/nvidia/tegra/i2c.h>
#include <soc/padconfig.h>
#include <soc/power.h>

#define I2C6_PADCTL		0xC001
#define DPAUX_HYBRID_PADCTL	0x545C0124

static struct tegra_pmc_regs * const pmc = (void *)TEGRA_PMC_BASE;

static int partition_clamp_on(int id)
{
	return read32(&pmc->clamp_status) & (1 << id);
}

static void remove_clamps(int id)
{
	if (!partition_clamp_on(id))
		return;

	/* Remove clamp */
	write32(&pmc->remove_clamping_cmd, (1 << id));

	/* Wait for clamp off */
	while (partition_clamp_on(id))
		;
}

static void enable_sor_periph_clocks(void)
{
	clock_enable(CLK_L_HOST1X, 0, 0, 0, 0, CLK_X_DPAUX, 0);

	/* Give clocks time to stabilize. */
	udelay(IO_STABILIZATION_DELAY);
}

static void disable_sor_periph_clocks(void)
{
	clock_disable(CLK_L_HOST1X, 0, 0, 0, 0, CLK_X_DPAUX, 0);

	/* Give clocks time to stabilize. */
	udelay(IO_STABILIZATION_DELAY);
}

static void unreset_sor_periphs(void)
{
	clock_clr_reset(CLK_L_HOST1X, 0, 0, 0, 0, CLK_X_DPAUX, 0);
}

void soc_configure_i2c6pad(void)
{
	/*
	 * I2C6 on Tegra1xx requires some special init.
	 * The SOR block must be unpowergated, and a couple of
	 * display-based peripherals must be clocked and taken
	 * out of reset so that a DPAUX register can be
	 * configured to enable the I2C6 mux routing.
	 * Afterwards, we can disable clocks to the display blocks
	 * and put Host1X back in reset. DPAUX must remain out of
	 * reset and the SOR partition must remained unpowergated.
	 */
	soc_configure_host1x();

	/* Now we can write the I2C6 mux in DPAUX */
	write32((void *)DPAUX_HYBRID_PADCTL, I2C6_PADCTL);

	/*
	 * Delay before turning off Host1X/DPAUX clocks.
	 * This delay is needed to keep the sequence from
	 * hanging the system.
	 */
	udelay(CLOCK_PLL_STABLE_DELAY_US);

	/* Stop Host1X/DPAUX clocks and reset Host1X */
	disable_sor_periph_clocks();
	clock_set_reset_l(CLK_L_HOST1X);
}

void soc_configure_host1x(void)
{
	power_ungate_partition(POWER_PARTID_SOR);

	/* Host1X needs a valid clock source so DPAUX can be accessed. */
	clock_configure_source(host1x, PLLP, 204000);

	enable_sor_periph_clocks();
	remove_clamps(POWER_PARTID_SOR);
	unreset_sor_periphs();
}
