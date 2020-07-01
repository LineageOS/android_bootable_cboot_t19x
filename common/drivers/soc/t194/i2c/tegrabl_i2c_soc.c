/*
 * Copyright (c) 2017-2018, NVIDIA CORPORATION.  All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited
 */

#include <stdint.h>
#include <tegrabl_utils.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_i2c_local.h>
#include <tegrabl_i2c_soc_common.h>
#include <tegrabl_dpaux.h>

static struct i2c_soc_info i2c_info[] = {
	{
		.base_addr = NV_ADDRESS_MAP_I2C1_BASE,
		.mode = STD_SPEED,
		.is_muxed_dpaux = false,
	},
	{
		.base_addr = NV_ADDRESS_MAP_I2C2_BASE,
		.mode = STD_SPEED,
		.is_muxed_dpaux = false,
	},
	{
		.base_addr = NV_ADDRESS_MAP_I2C3_BASE,
		.mode = STD_SPEED,
		.is_muxed_dpaux = false,
	},
	{
		.base_addr = NV_ADDRESS_MAP_I2C4_BASE,
		.mode = STD_SPEED,
		.is_muxed_dpaux = true,
		.dpaux_instance = DPAUX_INSTANCE_1,
	},
	{
		.base_addr = NV_ADDRESS_MAP_I2C5_BASE,
		.mode = STD_SPEED,
		.is_bpmpfw_controlled = true,
		.is_cldvfs_required = true,
		.is_muxed_dpaux = false,
	},
	{
		.base_addr = NV_ADDRESS_MAP_I2C6_BASE,
		.mode = STD_SPEED,
		.is_muxed_dpaux = true,
		.dpaux_instance = DPAUX_INSTANCE_0,
	},
	{
		.base_addr = NV_ADDRESS_MAP_I2C7_BASE,
		.mode = STD_SPEED,
		.is_muxed_dpaux = true,
		.dpaux_instance = DPAUX_INSTANCE_2,
	},
	{
		.base_addr = NV_ADDRESS_MAP_I2C8_BASE,
		.mode = STD_SPEED,
		.is_muxed_dpaux = false,
	},
	{
		.base_addr = NV_ADDRESS_MAP_I2C9_BASE,
		.mode = STD_SPEED,
		.is_muxed_dpaux = true,
		.dpaux_instance = DPAUX_INSTANCE_3,
	},
	{
		.base_addr = NV_ADDRESS_MAP_I2C10_BASE,
		.mode = STD_SPEED,
		.is_muxed_dpaux = false,
	}
};

void i2c_get_soc_info(struct i2c_soc_info **hi2c_info,
	uint32_t *num_of_instances)
{
	*hi2c_info = &i2c_info[0];
	*num_of_instances = ARRAY_SIZE(i2c_info);
}

