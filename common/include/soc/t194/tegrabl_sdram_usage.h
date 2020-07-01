/*
 * Copyright (c) 2015-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

#ifndef INCLUDED_TEGRABL_SDRAM_USAGE_H
#define INCLUDED_TEGRABL_SDRAM_USAGE_H

/*                     ________________________________________
 *
 *                       Memory map of SDRAM usage by nvtboot
 *                     ________________________________________
 *
 * [Address]                                                          [Size]
 * Carveouts start from MIN(0xFFFF_FFFF, DRAM_END) and grow downwards
 *             ____________________________________________________
 *      ^      |                                                  |
 *      |      |                   MTS Carveout                   |  <- 128 MB
 *      |      |__________________________________________________|
 *     This    |                                                  |
 *   portion   |                  GSC Carveouts                   |  <- 128 MB
 *    is not   |__________________________________________________|
 *  visible to |                                                  |
 *      OS     |                 SecureOS carveout                |  <- 6 MB
 *      |      |__________________________________________________|
 *      |      |                                                  |
 *      |      |                 BPMP-FW carveout                 |  <- 1 MB
 *      V      |__________________________________________________|
 *             |                                                  |
 *             |                                                  |
 *             |                                                  |
 *             |                  .............                   |
 *             |                                                  |
 *             |                                                  |
 *             |                                                  |
 *     ...     |__________________________________________________|
 *             |                                                  |
 *             |                     ^         ^                  |
 *             |                     |   Heap  |  Boot.img        |
 *             |                     |         |  NCT             |  <- 128 MB
 *             |    CPU-BL carveout  |         V                  |
 *             |                     |   Cboot                    |
 *             |                     V                            |
 *     ...     |__________________________________________________|
 *             |                                                  |
 *             |                     ^    .......                 |
 *             |                     |                            |
 *             |                     |    Ramdisk                 |
 *             |                     |                            |
 *             |        OS carveout  |    DTB                     |  <- 512 MB
 *             |                     |                            |
 *             |                     |    Kernel                  |
 *             |                     |                            |
 *             |                     V    .......                 |
 * Bottom    ->|__________________________________________________|
 * of Mem
 * (0x80000000)
 */

#define EKS_MAXIMUM_CODE_SIZE	(20 * 1024)

#define BOOT_IMAGE_MAX_SIZE		0x04000000U		/* 64 MB */
#define BOOT_IMAGE_ALIGNMENT	0x00010000U		/* 64 KB */
#define NCT_MAX_SIZE			0x00200000U

#define RAMDISK_ALIGNMENT		0x01000000U
#define RAMDISK_MAX_SIZE		0x04000000U

#define DTB_ALIGNMENT			0x01000000U
#define DTB_MAX_SIZE			0x00400000U

#define KERNEL_ALIGNMENT		0x00200000U
#define MAX_KERNEL_IMAGE_SIZE	0x10000000U

#define SDRAM_PAGE_SIZE			(64 * 1024LLU)
#define SDRAM_START_ADDRESS		0x80000000U

#endif
