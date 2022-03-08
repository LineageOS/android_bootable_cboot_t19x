/*
 * Copyright (c) 2015-2021, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA Corporation and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto.  Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA Corporation is strictly prohibited.
 */

#define MODULE TEGRABL_ERR_FUSE

#include "build_config.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <tegrabl_error.h>
#include <tegrabl_ar_macro.h>
#include <tegrabl_addressmap.h>
#include <tegrabl_drf.h>
#include <tegrabl_debug.h>
#include <tegrabl_io.h>
#include <tegrabl_clock.h>
#include <tegrabl_fuse.h>
#include <arfuse.h>
#include <tegrabl_timer.h>
#include <tegrabl_fuse_bitmap.h>
#include <tegrabl_malloc.h>
#include <tegrabl_soc_misc.h>
#include "tegrabl_fuse_err_aux.h"

/* Stores the base address of the fuse module */
static uintptr_t fuse_base_address = NV_ADDRESS_MAP_FUSE_BASE;

#define NV_FUSE_READ(reg) NV_READ32((fuse_base_address + (uint32_t)(reg)))
#define NV_FUSE_WRITE(reg, data) NV_WRITE32((fuse_base_address + (uint32_t)(reg)), (data))
#define FUSE_DISABLEREGPROGRAM_0_VAL_MASK 0x1
#define FUSE_STROBE_PROGRAMMING_PULSE 5

static uint32_t fuse_word;

#define write_fuse_word_0(name, data)	\
{											\
	fuse_word = ((uint32_t)(name##_ADDR_0_MASK) & data) << (name##_ADDR_0_SHIFT);	\
}

#define write_fuse_word_1(name, data)	\
{											\
	fuse_word = ((uint64_t)(name##_ADDR_1_MASK) & data) >> (name##_ADDR_1_SHIFT);	\
}


static bool is_fuse_write_disabled(void)
{
	uint32_t val;

	val = NV_FUSE_READ(FUSE_DISABLEREGPROGRAM_0);

	if ((val & FUSE_DISABLEREGPROGRAM_0_VAL_MASK) != 0U)
		return true;
	else
		return false;
}

static tegrabl_error_t program_fuse_strobe(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t oscillator_frequency_khz;
	uint32_t oscillator_frequency;
	uint32_t strobe_pulse = FUSE_STROBE_PROGRAMMING_PULSE;
	uint32_t strobe_width;
	uint32_t data;

	err = tegrabl_car_get_osc_freq_khz(&oscillator_frequency_khz);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}
	oscillator_frequency = oscillator_frequency_khz * 1000U;

	strobe_width = (oscillator_frequency * strobe_pulse) / (1000U * 1000U);

	/* Program FUSE_FUSETIME_PGM2_0 with strobe_width */
	data = NV_FUSE_READ(FUSE_FUSETIME_PGM2_0);
	data = NV_FLD_SET_DRF_NUM(FUSE, FUSETIME_PGM2, FUSETIME_PGM2_TWIDTH_PGM,
		strobe_width, data);
	NV_FUSE_WRITE(FUSE_FUSETIME_PGM2_0, data);

fail:
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_CONFIG_FAILED, "strobe");
	}
	return err;
}

static void fuse_assert_pd(bool is_assert)
{
	uint32_t data;
	bool pd_ctrl = false;

	data = NV_FUSE_READ(FUSE_FUSECTRL_0);
	pd_ctrl = NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_PD_CTRL, data) > 0UL;

	if (is_assert) {
		if (pd_ctrl) {
			return;
		} else {
			data = NV_FLD_SET_DRF_NUM(FUSE, FUSECTRL,
				FUSECTRL_PD_CTRL, 0x1, data);
			NV_FUSE_WRITE(FUSE_FUSECTRL_0, data);
			data = NV_FUSE_READ(FUSE_FUSECTRL_0);
			tegrabl_udelay(1);
		}
	} else {
		if (!pd_ctrl) {
			return;
		} else {
			data = NV_FLD_SET_DRF_NUM(FUSE, FUSECTRL,
				FUSECTRL_PD_CTRL, 0x0, data);
			tegrabl_udelay(1);
			NV_FUSE_WRITE(FUSE_FUSECTRL_0, data);
			data = NV_FUSE_READ(FUSE_FUSECTRL_0);
		}
	}
}

static void fuse_burn_setup(bool enable)
{
	if (enable == true) {
		/* Disable fuse mirroring and set PD to 0, wait for the required setup time
		 * This insures that the fuse macro is not power gated
		 */
		tegrabl_fuse_program_mirroring(false);
		fuse_assert_pd(false);

		/* Assert ps18 to enable programming voltage */
		tegrabl_pmc_fuse_control_ps18_latch_set();
	} else {
		/* Clear PS18 latch to gate programming voltage */
		tegrabl_pmc_fuse_control_ps18_latch_clear();

		/* Enable back fuse mirroring and set PD to 1,
		 * wait for the required setup time
		 */
		tegrabl_fuse_program_mirroring(true);
		fuse_assert_pd(true);
	}
}

static tegrabl_error_t fuse_write_pre_process(void)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t data;

	/* Confirm fuse option write access hasn't already
	 * been permanently disabled
	 */
	if (is_fuse_write_disabled()) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_ACCESS, AUX_INFO_FUSE_WRITE_PRE_PROCESS);
		goto fail;
	}

	/* Enable software writes to fuse registers.*/
	data = NV_FUSE_READ(FUSE_WRITE_ACCESS_SW_0);
	data = NV_FLD_SET_DRF_NUM(FUSE, WRITE_ACCESS_SW,
		WRITE_ACCESS_SW_CTRL, 0x1, data);
	NV_FUSE_WRITE(FUSE_WRITE_ACCESS_SW_0, data);


	/* Set the fuse strobe programming width */
	err = program_fuse_strobe();
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Increase SOC core voltage to at least 0.85V and wait for it to be stable */
	err = tegrabl_set_soc_core_voltage(850);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	fuse_burn_setup(true);

	/* Make sure the fuse burning voltage is present and stable
	 * (Assuming this is already set)
	 */

	/* Confirm the fuse wrapper's state machine is idle */
	data = NV_FUSE_READ(FUSE_FUSECTRL_0);
	data = NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_STATE, data);
	if (data != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE) {
		err = TEGRABL_ERROR(TEGRABL_ERR_BUSY, AUX_INFO_FUSE_WRITE_PRE_PROCESS);
		TEGRABL_SET_ERROR_STRING(err, "fuse wrapper's state machine");
		fuse_burn_setup(false);
		goto fail;
	}

fail:
	return err;
}

static void fuse_write_post_process(void)
{
	uint32_t data;

	fuse_burn_setup(false);

	/* If desired the newly burned raw fuse values can take effect without
	 * a reset, cold boot, or SC7LP0 resume
	 */
	data = NV_FUSE_READ(FUSE_FUSECTRL_0);
	data = NV_FLD_SET_DRF_DEF(FUSE, FUSECTRL, FUSECTRL_CMD, SENSE_CTRL, data);
	NV_FUSE_WRITE(FUSE_FUSECTRL_0, data);

	/* Wait at least 400ns as per IAS. Waiting 50us here to make sure h/w is
	 * stable and eliminate any issue with our timer driver. Since fuse burning
	 * is invoked rarely, KPIs doesn't matter here.
	 */
	tegrabl_udelay(50);

	/* Poll FUSE_FUSECTRL_0_FUSECTRL_STATE until it reads back STATE_IDLE */
	do {
		data = NV_FUSE_READ(FUSE_FUSECTRL_0);
		data = NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_STATE, data);
	} while (data != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);

	/* Simultaneously set FUSE_PRIV2INTFC_START_0_PRIV2INTFC_START_DATA &
	 * _PRIV2INTFC_SKIP_RECORDS
	 */
	data = NV_FUSE_READ(FUSE_PRIV2INTFC_START_0);
	data = NV_FLD_SET_DRF_NUM(FUSE, PRIV2INTFC_START,
								PRIV2INTFC_START_DATA, 1, data);
	data = NV_FLD_SET_DRF_NUM(FUSE, PRIV2INTFC_START,
								PRIV2INTFC_SKIP_RECORDS, 1, data);
	NV_FUSE_WRITE(FUSE_PRIV2INTFC_START_0, data);

	/* Wait at least 400ns as per IAS. Waiting 50us here to make sure h/w is
	 * stable and eliminate any issue with our timer driver. Since fuse burning
	 * is invoked rarely, KPIs doesn't matter here.
	 */
	tegrabl_udelay(50);

	/* Poll FUSE_FUSECTRL_0 until both FUSECTRL_FUSE_SENSE_DONE is set,
	 * and FUSECTRL_STATE is STATE_IDLE
	 */
	do {
		data = NV_FUSE_READ(FUSE_FUSECTRL_0);
		data = NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_FUSE_SENSE_DONE, data);
	} while (data == 0U);

	do {
		data = NV_FUSE_READ(FUSE_FUSECTRL_0);
		data = NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_STATE, data);
	} while (data != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);

}

static void fuse_initiate_burn(void)
{
	uint32_t data;

	/* Initiate the fuse burn */
	data = NV_FUSE_READ(FUSE_FUSECTRL_0);
	data = NV_FLD_SET_DRF_DEF(FUSE, FUSECTRL, FUSECTRL_CMD, WRITE, data);
	NV_FUSE_WRITE(FUSE_FUSECTRL_0, data);

	/* Wait at least 400ns as per IAS. Waiting 50us here to make sure h/w is
	 * stable and eliminate any issue with our timer driver. Since fuse burning
	 * is invoked rarely, KPIs doesn't matter here.
	 */
	tegrabl_udelay(50);

	/* Wait for the fuse burn to complete */
	do {
		data = NV_FUSE_READ(FUSE_FUSECTRL_0);
		data = NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_STATE, data);
	} while (data != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);


	/* check that the correct data has been burned correctly
	 * by reading back the data
	 */
	do {
		data = NV_FUSE_READ(FUSE_FUSECTRL_0);
		data = NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_STATE, data);
	} while (data != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);

	data = NV_FUSE_READ(FUSE_FUSECTRL_0);
	data = NV_FLD_SET_DRF_DEF(FUSE, FUSECTRL, FUSECTRL_CMD, READ, data);
	NV_FUSE_WRITE(FUSE_FUSECTRL_0, data);

	/* Wait at least 400ns as per IAS. Waiting 50us here to make sure h/w is
	 * stable and eliminate any issue with our timer driver. Since fuse burning
	 * is invoked rarely, KPIs doesn't matter here.
	 */
	tegrabl_udelay(50);

	do {
		data = NV_FUSE_READ(FUSE_FUSECTRL_0);
		data = NV_DRF_VAL(FUSE, FUSECTRL, FUSECTRL_STATE, data);
	} while (data != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);

	data = NV_FUSE_READ(FUSE_FUSERDATA_0);
}

static tegrabl_error_t fuse_burn(uint32_t addr)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	if (fuse_word == 0U) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_INVALID, "fuse_word:0");
		goto fail;
	}

	err = fuse_write_pre_process();
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Set the desired fuse dword address */
	NV_FUSE_WRITE(FUSE_FUSEADDR_0, addr);

	/* Set the desired fuses to burn */
	NV_FUSE_WRITE(FUSE_FUSEWDATA_0, fuse_word);

	fuse_initiate_burn();

	fuse_write_post_process();
fail:
	return err;
}

static tegrabl_error_t fuse_set_macro_and_burn(
	uint32_t fuse_type, uint32_t *buffer, uint32_t size)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	uint32_t temp_size = 0;
	uint32_t *temp_buffer = NULL;
	uint32_t i = 0;

	TEGRABL_ASSERT(buffer != NULL);

	err = tegrabl_fuse_query_size(fuse_type, &temp_size);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	if (temp_size > size) {
		err = TEGRABL_ERROR(TEGRABL_ERR_TOO_LARGE, AUX_INFO_SET_MACRO_AND_BURN);
		goto fail;
	}

	temp_buffer = tegrabl_malloc(temp_size);
	if (temp_buffer == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, AUX_INFO_SET_MACRO_AND_BURN);
		goto fail;
	}
	memset(temp_buffer, 0, temp_size);

	err = tegrabl_fuse_read(fuse_type, temp_buffer, temp_size);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	while (i < (temp_size >> 2)) {
		buffer[i] = buffer[i] ^ temp_buffer[i];
		if ((buffer[i] & temp_buffer[i]) != 0U) {
			err = TEGRABL_ERROR(TEGRABL_ERR_INVALID_CONFIG, AUX_INFO_SET_MACRO_AND_BURN);
			TEGRABL_SET_ERROR_STRING(err, "buffer[%u]", "%u", i, buffer[i] ^ temp_buffer[i]);
			goto fail;
		}
		i++;
	}

	switch (fuse_type) {
	case FUSE_RESERVED_ODM8:
		write_fuse_word_0(FUSE_RESERVED_ODM8, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM8_ADDR_0);
		if (err) {
			goto fail;
		}

		write_fuse_word_1(FUSE_RESERVED_ODM8, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM8_ADDR_1);
		if (err) {
			goto fail;
		}

		write_fuse_word_0(FUSE_RESERVED_ODM8_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM8_REDUNDANT_ADDR_0);
		if (err) {
			goto fail;
		}

		write_fuse_word_1(FUSE_RESERVED_ODM8_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM8_REDUNDANT_ADDR_1);
		if (err) {
			goto fail;
		}
		break;
	case FUSE_RESERVED_ODM9:
		write_fuse_word_0(FUSE_RESERVED_ODM9, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM9_ADDR_0);
		if (err) {
			goto fail;
		}

		write_fuse_word_1(FUSE_RESERVED_ODM9, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM9_ADDR_1);
		if (err) {
			goto fail;
		}

		write_fuse_word_0(FUSE_RESERVED_ODM9_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM9_REDUNDANT_ADDR_0);
		if (err) {
			goto fail;
		}

		write_fuse_word_1(FUSE_RESERVED_ODM9_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM9_REDUNDANT_ADDR_1);
		if (err) {
			goto fail;
		}
		break;
	case FUSE_RESERVED_ODM10:
		write_fuse_word_0(FUSE_RESERVED_ODM10, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM10_ADDR_0);
		if (err) {
			goto fail;
		}

		write_fuse_word_1(FUSE_RESERVED_ODM10, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM10_ADDR_1);
		if (err) {
			goto fail;
		}

		write_fuse_word_0(FUSE_RESERVED_ODM10_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM10_REDUNDANT_ADDR_0);
		if (err) {
			goto fail;
		}

		write_fuse_word_1(FUSE_RESERVED_ODM10_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM10_REDUNDANT_ADDR_1);
		if (err) {
			goto fail;
		}
		break;
	case FUSE_RESERVED_ODM11:
		write_fuse_word_0(FUSE_RESERVED_ODM11, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM11_ADDR_0);
		if (err) {
			goto fail;
		}

		write_fuse_word_1(FUSE_RESERVED_ODM11, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM11_ADDR_1);
		if (err) {
			goto fail;
		}

		write_fuse_word_0(FUSE_RESERVED_ODM11_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM11_REDUNDANT_ADDR_0);
		if (err) {
			goto fail;
		}

		write_fuse_word_1(FUSE_RESERVED_ODM11_REDUNDANT, buffer[0])
		err = fuse_burn(FUSE_RESERVED_ODM11_REDUNDANT_ADDR_1);
		if (err) {
			goto fail;
		}
		break;
	default:
		err = TEGRABL_ERROR(TEGRABL_ERR_INVALID, AUX_INFO_SET_MACRO_AND_BURN);
		TEGRABL_SET_ERROR_STRING(err, "type: %u", fuse_type);
		break;
	}

fail:
	if (temp_buffer != NULL) {
		tegrabl_free(temp_buffer);
	}
	if (err != TEGRABL_NO_ERROR) {
		TEGRABL_PRINT_ERROR_STRING(TEGRABL_ERR_FAILED_GENERIC, "set macro and burn");
	}
	return err;
}

static tegrabl_error_t tegrabl_fuse_confirm_burn(fuse_type_t type,
												 uint32_t val_written)
{
	uint32_t size;
	uint32_t *val = NULL;
	tegrabl_error_t err = TEGRABL_NO_ERROR;

	err = tegrabl_fuse_query_size(type, &size);
	if (err) {
		goto fail;
	}

	val = tegrabl_malloc(size);
	if (val == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_NO_MEMORY, AUX_INFO_FUSE_CONFIRM_BURN);
		goto fail;
	}

	memset(val, 0, size);

	if (type == FUSE_TYPE_BOOT_SECURITY_INFO) {
		*val = tegrabl_fuse_get_security_info();
	} else {
		err = tegrabl_fuse_read(type, val, size);
		if (err) {
			goto cleanup;
		}
	}
	pr_info("fuse read is %0x\n", *val);
	if (*val != val_written) {
		err = TEGRABL_ERROR(TEGRABL_ERR_WRITE_FAILED, AUX_INFO_FUSE_CONFIRM_BURN);
		TEGRABL_SET_ERROR_STRING(err, "type %u: written val %u, read val %u", type, val_written, *val);
	} else {
		pr_info("Fuse (%u) burnt successfully with val 0x%08x\n", type, *val);
	}

cleanup:
	if (val) {
		tegrabl_free(val);
	}

fail:
	return err;
}

tegrabl_error_t tegrabl_fuse_write(
	uint32_t fuse_type, uint32_t *buffer, uint32_t size)
{
	tegrabl_error_t err = TEGRABL_NO_ERROR;
	bool original_visibility;
	uint32_t val_bef_burn;

	if (buffer == NULL) {
		err = TEGRABL_ERROR(TEGRABL_ERR_BAD_PARAMETER, AUX_INFO_FUSE_WRITE);
		goto fail;
	}

	val_bef_burn = *buffer;

	/* Make all fuse registers visible */
	original_visibility = tegrabl_set_fuse_reg_visibility(true);
	tegrabl_pmc_fuse_control_ps18_latch_set();

	err = fuse_set_macro_and_burn(fuse_type, buffer, size);
	if (err != TEGRABL_NO_ERROR) {
		goto fail;
	}

	/* Wait to make sure fuses are burnt */
	tegrabl_mdelay(2);

	tegrabl_pmc_fuse_control_ps18_latch_clear();

	/* Restore back the original visibility */
	tegrabl_set_fuse_reg_visibility(original_visibility);

	/* confirm fuses are burnt */
	err = tegrabl_fuse_confirm_burn(fuse_type, val_bef_burn);
	if (err) {
		goto fail;
	}

fail:
	if (err != TEGRABL_NO_ERROR) {
		pr_error("error = 0x%x in tegrabl_fuse_write\n", err);
	}
	return err;
}
