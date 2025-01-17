/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2014 Google Inc.
 * Copyright (C) 2015 Intel Corporation.
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

#include <stddef.h>
#include <stdint.h>
#include <arch/cpu.h>
#include <arch/io.h>
#include <arch/cbfs.h>
#include <arch/stages.h>
#include <arch/early_variables.h>
#include <console/console.h>
#include <cbmem.h>
#include <cpu/x86/mtrr.h>
#include <ec/google/chromeec/ec.h>
#include <ec/google/chromeec/ec_commands.h>
#include <elog.h>
#include <ramstage_cache.h>
#include <reset.h>
#include <romstage_handoff.h>
#include <soc/intel/common/mrc_cache.h>
#include <soc/pei_wrapper.h>
#include <soc/pm.h>
#include <soc/romstage.h>
#include <soc/spi.h>
#include <timestamp.h>
#include <vendorcode/google/chromeos/chromeos.h>

/* Entry from cache-as-ram.inc. */
asmlinkage void *romstage_main(unsigned int bist,
				uint32_t tsc_low, uint32_t tsc_high)
{
	void *top_of_stack;
	struct pei_data pei_data;
	struct romstage_params params = {
		.bist = bist,
		.pei_data = &pei_data,
	};

	post_code(0x30);

	/* Save timestamp data */
	timestamp_early_init((((uint64_t)tsc_high) << 32) | (uint64_t)tsc_low);
	timestamp_add_now(TS_START_ROMSTAGE);

	memset(&pei_data, 0, sizeof(pei_data));

	/* Call into pre-console init code. */
	soc_pre_console_init(&params);
	mainboard_pre_console_init(&params);

	/* Start console drivers */
	console_init();

	/* Display parameters */
	printk(BIOS_SPEW, "bist: 0x%08x\n", bist);
	printk(BIOS_SPEW, "tsc_low: 0x%08x\n", tsc_low);
	printk(BIOS_SPEW, "tsc_hi: 0x%08x\n", tsc_high);
	printk(BIOS_SPEW, "CONFIG_MMCONF_BASE_ADDRESS: 0x%08x\n",
		CONFIG_MMCONF_BASE_ADDRESS);
	printk(BIOS_INFO, "Using: %s\n",
		IS_ENABLED(CONFIG_PLATFORM_USES_FSP) ? "FSP" :
		(IS_ENABLED(CONFIG_HAVE_MRC) ? "MRC" :
		"No Memory Support"));

	/* Display FSP banner */
#if IS_ENABLED(CONFIG_PLATFORM_USES_FSP)
	printk(BIOS_DEBUG, "FSP TempRamInit successful\n");
	print_fsp_info(find_fsp());
#endif	/* CONFIG_PLATFORM_USES_FSP */

	/* Get power state */
	params.power_state = fill_power_state();

	/* Print useful platform information */
	report_platform_info();

	/* Set CPU frequency to maximum */
	set_max_freq();

	/* Perform SOC specific initialization. */
	soc_romstage_init(&params);

	/* Call into mainboard. */
	mainboard_romstage_entry(&params);
	soc_after_ram_init(&params);
	post_code(0x38);

	top_of_stack = setup_stack_and_mtrrs();

#if IS_ENABLED(CONFIG_PLATFORM_USES_FSP)
	printk(BIOS_DEBUG, "Calling FspTempRamExit API\n");
	timestamp_add_now(TS_FSP_TEMP_RAM_EXIT_START);
#endif	/* CONFIG_PLATFORM_USES_FSP */

	return top_of_stack;
}

/* Entry from the mainboard. */
void romstage_common(struct romstage_params *params)
{
	const struct mrc_saved_data *cache;
	struct romstage_handoff *handoff;
	struct pei_data *pei_data;

	post_code(0x32);

	timestamp_add_now(TS_BEFORE_INITRAM);

	pei_data = params->pei_data;
	pei_data->boot_mode = params->power_state->prev_sleep_state;

#if IS_ENABLED(CONFIG_ELOG_BOOT_COUNT)
	if (params->power_state->prev_sleep_state != SLEEP_STATE_S3)
		boot_count_increment();
#endif

	/* Perform remaining SOC initialization */
	soc_pre_ram_init(params);
	post_code(0x33);

	/* Check recovery and MRC cache */
	params->pei_data->saved_data_size = 0;
	params->pei_data->saved_data = NULL;
	if (!params->pei_data->disable_saved_data) {
		if (recovery_mode_enabled()) {
			/* Recovery mode does not use MRC cache */
			printk(BIOS_DEBUG,
			       "Recovery mode: not using MRC cache.\n");
		} else if (!mrc_cache_get_current(&cache)) {
			/* MRC cache found */
			params->pei_data->saved_data_size = cache->size;
			params->pei_data->saved_data = &cache->data[0];
		} else if (params->pei_data->boot_mode == SLEEP_STATE_S3) {
			/* Waking from S3 and no cache. */
			printk(BIOS_DEBUG,
			       "No MRC cache found in S3 resume path.\n");
			post_code(POST_RESUME_FAILURE);
			hard_reset();
		} else {
			printk(BIOS_DEBUG, "No MRC cache found.\n");
			mainboard_check_ec_image(params);
		}
	}

	/* Initialize RAM */
	raminit(params);
	timestamp_add_now(TS_AFTER_INITRAM);

	/* Save MRC output */
	printk(BIOS_DEBUG, "MRC data at %p %d bytes\n", pei_data->data_to_save,
	       pei_data->data_to_save_size);
	if (params->pei_data->boot_mode != SLEEP_STATE_S3) {
		if (params->pei_data->data_to_save_size != 0 &&
		    params->pei_data->data_to_save != NULL) {
			mrc_cache_stash_data(params->pei_data->data_to_save,
				params->pei_data->data_to_save_size);
		}
	}

	/* Save DIMM information */
	mainboard_save_dimm_info(params);

	/* Create romstage handof information */
	handoff = romstage_handoff_find_or_add();
	if (handoff != NULL)
		handoff->s3_resume = (params->power_state->prev_sleep_state ==
				      SLEEP_STATE_S3);
	else {
		printk(BIOS_DEBUG, "Romstage handoff structure not added!\n");
		hard_reset();
	}

#if IS_ENABLED(CONFIG_CHROMEOS)
	/* Normalize the sleep state to what init_chromeos() wants for S3: 2 */
	init_chromeos((params->power_state->prev_sleep_state == SLEEP_STATE_S3)
		? 2 : 0);
#endif
}

asmlinkage void romstage_after_car(void)
{
#if IS_ENABLED(CONFIG_PLATFORM_USES_FSP)
	FSP_INFO_HEADER *fsp_info_header;
	FSP_SILICON_INIT fsp_silicon_init;
	EFI_STATUS status;

	timestamp_add_now(TS_FSP_TEMP_RAM_EXIT_END);
	printk(BIOS_DEBUG, "FspTempRamExit returned successfully\n");
	soc_after_temp_ram_exit();

	/* Find the FSP image */
	timestamp_add_now(TS_FSP_FIND_START);
	fsp_info_header = find_fsp();
	timestamp_add_now(TS_FSP_FIND_END);

	/* Perform silicon initialization after RAM is configured */
	printk(BIOS_DEBUG, "Calling FspSiliconInit\n");
	fsp_silicon_init = (FSP_SILICON_INIT)(fsp_info_header->ImageBase
		+ fsp_info_header->FspSiliconInitEntryOffset);
	timestamp_add_now(TS_FSP_SILICON_INIT_START);
	status = fsp_silicon_init(NULL);
	timestamp_add_now(TS_FSP_SILICON_INIT_END);
	printk(BIOS_DEBUG, "FspSiliconInit returned 0x%08x\n", status);
	soc_after_silicon_init();
#endif	/* CONFIG_PLATFORM_USES_FSP */

	timestamp_add_now(TS_END_ROMSTAGE);

	/* Run vboot verification if configured. */
	vboot_verify_firmware(romstage_handoff_find_or_add());

	/* Load the ramstage. */
	copy_and_run();
	die("ERROR - Failed to load ramstage!");
}

#if IS_ENABLED(CONFIG_PLATFORM_USES_FSP)
__attribute__((weak)) void board_fsp_memory_init_params(
	struct romstage_params *params,
	FSP_INFO_HEADER *fsp_header,
	FSP_MEMORY_INIT_PARAMS * fsp_memory_init_params)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
}
#endif	/* CONFIG_PLATFORM_USES_FSP */

/* Initialize the power state */
__attribute__((weak)) struct chipset_power_state *fill_power_state(void)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
	return NULL;
}

__attribute__((weak)) void mainboard_check_ec_image(
	struct romstage_params *params)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
#if IS_ENABLED(CONFIG_EC_GOOGLE_CHROMEEC)
	struct pei_data *pei_data;

	pei_data = params->pei_data;
	if (params->pei_data->boot_mode == SLEEP_STATE_S0) {
		/* Ensure EC is running RO firmware. */
		google_chromeec_check_ec_image(EC_IMAGE_RO);
	}
#endif
}

/* Board initialization before the console is enabled */
__attribute__((weak)) void mainboard_pre_console_init(
	struct romstage_params *params)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
}

/* Board initialization before and after RAM is enabled */
__attribute__((weak)) void mainboard_romstage_entry(
	struct romstage_params *params)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);

	post_code(0x31);

	/* Initliaze memory */
	romstage_common(params);
}

/* Used by MRC images to save DIMM information */
__attribute__((weak)) void mainboard_save_dimm_info(
	struct romstage_params *params)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
}

/* Get the memory configuration data */
__attribute__((weak)) int mrc_cache_get_current(
	const struct mrc_saved_data **cache)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
	return -1;
}

/* Save the memory configuration data */
__attribute__((weak)) int mrc_cache_stash_data(void *data, size_t size)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
	return -1;
}

/* Transition RAM from off or self-refresh to active */
__attribute__((weak)) void raminit(struct romstage_params *params)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
	post_code(0x34);
	die("ERROR - No RAM initialization specified!\n");
}

/* Display the memory configuration */
__attribute__((weak)) void report_memory_config(void)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
}

/* Display the platform configuration */
__attribute__((weak)) void report_platform_info(void)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
}

/* Choose top of stack and setup MTRRs */
__attribute__((weak)) void *setup_stack_and_mtrrs(void)
{
	printk(BIOS_ERR, "WEAK: %s/%s called\n", __FILE__, __func__);
	die("ERROR - Must specify top of stack!\n");
	return NULL;
}

/* Speed up the CPU to the maximum frequency */
__attribute__((weak)) void set_max_freq(void)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
}

/* SOC initialization after RAM is enabled */
__attribute__((weak)) void soc_after_ram_init(struct romstage_params *params)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
}

/* SOC initialization after FSP silicon init */
__attribute__((weak)) void soc_after_silicon_init(void)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
}

/* SOC initialization after temporary RAM is disabled */
__attribute__((weak)) void soc_after_temp_ram_exit(void)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
}

/* SOC initialization before the console is enabled */
__attribute__((weak)) void soc_pre_console_init(struct romstage_params *params)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
}

/* SOC initialization before RAM is enabled */
__attribute__((weak)) void soc_pre_ram_init(struct romstage_params *params)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
}

/* SOC initialization after console is enabled */
__attribute__((weak)) void soc_romstage_init(struct romstage_params *params)
{
	printk(BIOS_DEBUG, "WEAK: %s/%s called\n", __FILE__, __func__);
#if IS_ENABLED(CONFIG_EC_GOOGLE_CHROMEEC)
	/* Ensure the EC is in the right mode for recovery */
	google_chromeec_early_init();
#endif
}
