/*
 * This file is part of the coreboot project.
 *
 * Copyright (C) 2013-2014 Sage Electronic Engineering, LLC.
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

#ifndef FSP_UTIL_H
#define FSP_UTIL_H

#include <types.h>
#include <arch/cpu.h>

/*
 * The following are functions with prototypes defined in the EDK2 headers. The
 * EDK2 headers are included with chipset_fsp_util.h.  Define the following
 * names to reduce the use of CamelCase in the other source files.
 */
#define GetHobList	get_hob_list
#define GetNextHob	get_next_hob
#define GetFirstHob	get_first_hob
#define GetNextGuidHob	get_next_guid_hob
#define GetFirstGuidHob	get_first_guid_hob

/* Include the EDK2 headers */
#include <chipset_fsp_util.h>

#if IS_ENABLED(CONFIG_ENABLE_MRC_CACHE)
int save_mrc_data(void *hob_start);
void * find_and_set_fastboot_cache(void);
#endif

FSP_INFO_HEADER *find_fsp(void);
void fsp_early_init(FSP_INFO_HEADER *fsp_info);
void fsp_check_reserved_mem_size(void *hob_list_ptr, void* end_of_region);
void *fsp_find_reserved_mem(void *hob_list_ptr);
void fsp_notify(u32 phase);
void print_hob_type_structure(u16 hob_type, void *hob_list_ptr);
void print_fsp_info(FSP_INFO_HEADER *fsp_header);
void set_hob_list(void *hob_list_ptr);

/* The following are chipset support routines */
#if IS_ENABLED(CONFIG_USING_FSP_1_0)
void chipset_fsp_early_init(FSP_INIT_PARAMS * fsp_init_params,
	FSP_INFO_HEADER *fsp_ptr);
void chipset_fsp_return_point(EFI_STATUS status, VOID *hob_list_ptr);
#endif

/* The following are board support routines */
#if IS_ENABLED(CONFIG_USING_FSP_1_0)
void romstage_fsp_rt_buffer_callback(
	FSP_INIT_RT_COMMON_BUFFER * fsp_rt_common_buffer);
#endif


/* Additional HOB types not included in the FSP:
 * #define EFI_HOB_TYPE_HANDOFF 0x0001
 * #define EFI_HOB_TYPE_MEMORY_ALLOCATION 0x0002
 * #define EFI_HOB_TYPE_RESOURCE_DESCRIPTOR 0x0003
 * #define EFI_HOB_TYPE_GUID_EXTENSION 0x0004
 * #define EFI_HOB_TYPE_FV 0x0005
 * #define EFI_HOB_TYPE_CPU 0x0006
 * #define EFI_HOB_TYPE_MEMORY_POOL 0x0007
 * #define EFI_HOB_TYPE_CV 0x0008
 * #define EFI_HOB_TYPE_UNUSED 0xFFFE
 * #define EFI_HOB_TYPE_END_OF_HOB_LIST 0xffff
 */
#define EFI_HOB_TYPE_HANDOFF		0x0001
#define EFI_HOB_TYPE_MEMORY_POOL	0x0007

#if IS_ENABLED(CONFIG_ENABLE_MRC_CACHE)
#define MRC_DATA_ALIGN			0x1000
#define MRC_DATA_SIGNATURE		(('M'<<0)|('R'<<8)|('C'<<16)|('D'<<24))

struct mrc_data_container {
	u32	mrc_signature;	// "MRCD"
	u32	mrc_data_size;	// Actual total size of this structure
	u32	mrc_checksum;	// IP style checksum
	u32	reserved;		// For header alignment
	u8	mrc_data[0];	// Variable size, platform/run time dependent.
} __attribute__ ((packed));

struct mrc_data_container *find_current_mrc_cache(void);

void update_mrc_cache(void *unused);

#endif	/* CONFIG_ENABLE_MRC_CACHE */

/* The offset in bytes from the start of the info structure */
#define FSP_IMAGE_SIG_LOC			0
#define FSP_IMAGE_ID_LOC			16
#define FSP_IMAGE_BASE_LOC			28

#define FSP_SIG					0x48505346	/* 'FSPH' */

#define ERROR_NO_FV_SIG				1
#define ERROR_NO_FFS_GUID			2
#define ERROR_NO_INFO_HEADER			3
#define ERROR_IMAGEBASE_MISMATCH		4
#define ERROR_INFO_HEAD_SIG_MISMATCH		5
#define ERROR_FSP_SIG_MISMATCH			6

#ifndef __PRE_RAM__
extern void *FspHobListPtr;
#endif

#endif	/* FSP_UTIL_H */
