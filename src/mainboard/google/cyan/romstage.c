/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2013 Google Inc.
 * Copyright (C) 2015 Intel Corp.
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

#include <cbfs.h>
#include <console/console.h>
#include <lib.h>
#include <soc/gpio.h>
#include <soc/pci_devs.h>
#include <soc/romstage.h>
#include <string.h>

/*
 * 0b0000 - 4GiB total - 2 x 2GiB Samsung K4B4G1646Q-HYK0 1600MHz
 * 0b0001 - 4GiB total - 2 x 2GiB Hynix  H5TC4G63CFR-PBA 1600MHz
 * 0b0010- 2GiB total - 1 x 2GiB Samsung K4B4G1646Q-HYK0 1600MHz
 * 0b0011 - 2GiB total - 1 x 2GiB Hynix  H5TC4G63CFR-PBA 1600MHz
 */
static const uint32_t dual_channel_config = (1 << 0);

#define SPD_SIZE 256
#define SATA_GP3_PAD_CFG0	0x5828
#define I2C3_SCL_PAD_CFG0	0x5438
#define MF_PLT_CLK1_PAD_CFG0	0x4410
#define I2C3_SDA_PAD_CFG0	0x5420

static void *get_spd_pointer(char *spd_file_content, int total_spds, int *dual)
{
	int ram_id = 0;
	ram_id |= get_gpio(COMMUNITY_GPSOUTHWEST_BASE, SATA_GP3_PAD_CFG0) << 0;
	ram_id |= get_gpio(COMMUNITY_GPSOUTHWEST_BASE, I2C3_SCL_PAD_CFG0) << 1;
	ram_id |= get_gpio(COMMUNITY_GPSOUTHEAST_BASE, MF_PLT_CLK1_PAD_CFG0)
		<< 2;
	ram_id |= get_gpio(COMMUNITY_GPSOUTHWEST_BASE, I2C3_SDA_PAD_CFG0) << 3;

	/*
	 * There are only 2 SPDs supported on Cyan Board:
	 * Samsung 4G:0000 & Hynix 2G:0011
	 */

	/*
	 * RAMID0 on the first boot does not read the correct value,so checking
	 * bit 1 is enough as WA
	 */
	if (ram_id > 0)
		ram_id = 3;
	printk(BIOS_DEBUG, "ram_id=%d, total_spds: %d\n", ram_id, total_spds);

	if (ram_id >= total_spds)
		return NULL;

	/* Single channel configs */
	if (dual_channel_config & (1 << ram_id))
		*dual = 1;

	return &spd_file_content[SPD_SIZE * ram_id];
}

/* All FSP specific code goes in this block */
void mainboard_romstage_entry(struct romstage_params *rp)
{
	struct cbfs_file *spd_file;
	void *spd_content;
#if IS_ENABLED(CONFIG_GOP_SUPPORT)
	void *vbt_content;
	struct cbfs_file *vbt_file;
#endif
	int dual_channel = 0;
	struct pei_data *ps = rp->pei_data;

	/* Find the SPD data in CBFS. */
	spd_file = cbfs_get_file(CBFS_DEFAULT_MEDIA, "spd.bin");
	if (!spd_file)
		die("SPD data not found.");

	/*
	 * Both channels are always present in SPD data. Always use matched
	 * DIMMs so use the same SPD data for each DIMM.
	 */
	spd_content = get_spd_pointer(CBFS_SUBHEADER(spd_file),
				      ntohl(spd_file->len) / SPD_SIZE,
				      &dual_channel);
	if (IS_ENABLED(CONFIG_DISPLAY_SPD_DATA) && spd_content != NULL) {
		printk(BIOS_DEBUG, "SPD Data:\n");
		hexdump(spd_content, SPD_SIZE);
		printk(BIOS_DEBUG, "\n");
	}

	/*
	 * Set SPD and memory configuration:
	 * Memory type: 0=DimmInstalled,
	 *              1=SolderDownMemory,
	 *              2=DimmDisabled
	 */
	if (spd_content != NULL) {
		ps->spd_data_ch0 = spd_content;
		ps->spd_ch0_config = 1;
		if (dual_channel) {
			ps->spd_data_ch1 = spd_content;
			ps->spd_ch1_config = 1;
		} else {
			ps->spd_ch1_config = 2;
		}
	}

#if IS_ENABLED(CONFIG_GOP_SUPPORT)
	/* Getting VBT data */
	vbt_file = cbfs_get_file(CBFS_DEFAULT_MEDIA, "vbt.bin");
	if (!vbt_file)
		die("VBT data not found.");
	printk(BIOS_DEBUG, "Vbt table found!\n");
	vbt_content = CBFS_SUBHEADER(vbt_file);
	ps->vbt_data = vbt_content;
#endif

	/* Set device state/enable information */
	ps->sdcard_mode = PCH_ACPI_MODE;
	ps->emmc_mode = PCH_ACPI_MODE;
	ps->enable_azalia = 1;

	/* Call back into chipset code with platform values updated. */
	romstage_common(rp);
}
