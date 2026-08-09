/*******************************************************************************
 * Copyright ï¿½ 2016, STMicroelectronics International N.V.
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright
 notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 notice, this list of conditions and the following disclaimer in the
 documentation and/or other materials provided with the distribution.
 * Neither the name of STMicroelectronics nor the
 names of its contributors may be used to endorse or promote products
 derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND
 NON-INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS ARE DISCLAIMED.
 IN NO EVENT SHALL STMICROELECTRONICS INTERNATIONAL N.V. BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

#include "vl53l0x_api.h"
#include "vl53l0x_tuning.h"
#include "vl53l0x_interrupt_threshold_settings.h"
#include "vl53l0x_api_core.h"
#include "vl53l0x_api_calibration.h"
#include "vl53l0x_api_strings.h"

#ifndef __KERNEL__
#include <stdlib.h>
#endif
#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(TRACE_MODULE_API, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(TRACE_MODULE_API, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(TRACE_MODULE_API, status, fmt, ##__VA_ARGS__)

#ifdef VL53L0X_LOG_ENABLE
#define trace_print(level, ...) trace_print_module_function(TRACE_MODULE_API, \
	level, TRACE_FUNCTION_NONE, ##__VA_ARGS__)
#endif

/* Group PAL General Functions */

VL53L0X_Error VL53L0X_GetVersion(VL53L0X_Version_t *pVersion)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;

	LOG_FUNCTION_START("");

	pVersion->major = VL53L0X_IMPLEMENTATION_VER_MAJOR;
	pVersion->minor = VL53L0X_IMPLEMENTATION_VER_MINOR;
	pVersion->build = VL53L0X_IMPLEMENTATION_VER_SUB;

	pVersion->revision = VL53L0X_IMPLEMENTATION_VER_REVISION;

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0X_Error VL53L0X_GetPalSpecVersion(VL53L0X_Version_t *pPalSpecVersion)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;

	LOG_FUNCTION_START("");

	pPalSpecVersion->major = VL53L0X_SPECIFICATION_VER_MAJOR;
	pPalSpecVersion->minor = VL53L0X_SPECIFICATION_VER_MINOR;
	pPalSpecVersion->build = VL53L0X_SPECIFICATION_VER_SUB;

	pPalSpecVersion->revision = VL53L0X_SPECIFICATION_VER_REVISION;

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0X_Error VL53L0X_GetProductRevision(VL53L0X_DEV Dev,
	uint8_t *pProductRevisionMajor, uint8_t *pProductRevisionMinor)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	uint8_t revision_id;

	LOG_FUNCTION_START("");

	Status = VL53L0X_RdByte(Dev, VL53L0X_REG_IDENTIFICATION_REVISION_ID,
		&revision_id);
	*pProductRevisionMajor = 1;
	*pProductRevisionMinor = (revision_id & 0xF0) >> 4;

	LOG_FUNCTION_END(Status);
	return Status;

}

VL53L0X_Error VL53L0X_GetDeviceInfo(VL53L0X_DEV Dev,
	VL53L0X_DeviceInfo_t *pVL53L0X_DeviceInfo)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L0X_get_device_info(Dev, pVL53L0X_DeviceInfo);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0X_Error VL53L0X_GetDeviceErrorStatus(VL53L0X_DEV Dev,
	VL53L0X_DeviceError *pDeviceErrorStatus)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	uint8_t RangeStatus;

	LOG_FUNCTION_START("");

	Status = VL53L0X_RdByte(Dev, VL53L0X_REG_RESULT_RANGE_STATUS,
		&RangeStatus);

	*pDeviceErrorStatus = (VL53L0X_DeviceError)((RangeStatus & 0x78) >> 3);

	LOG_FUNCTION_END(Status);
	return Status;
}


VL53L0X_Error VL53L0X_GetDeviceErrorString(VL53L0X_DeviceError ErrorCode,
	char *pDeviceErrorString)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L0X_get_device_error_string(ErrorCode, pDeviceErrorString);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0X_Error VL53L0X_GetRangeStatusString(uint8_t RangeStatus,
	char *pRangeStatusString)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L0X_get_range_status_string(RangeStatus,
		pRangeStatusString);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0X_Error VL53L0X_GetPalErrorString(VL53L0X_Error PalErrorCode,
	char *pPalErrorString)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L0X_get_pal_error_string(PalErrorCode, pPalErrorString);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0X_Error VL53L0X_GetPalStateString(VL53L0X_State PalStateCode,
	char *pPalStateString)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L0X_get_pal_state_string(PalStateCode, pPalStateString);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0X_Error VL53L0X_GetPalState(VL53L0X_DEV Dev, VL53L0X_State *pPalState)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;

	LOG_FUNCTION_START("");

	*pPalState = PALDevDataGet(Dev, PalState);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0X_Error VL53L0X_SetPowerMode(VL53L0X_DEV Dev,
				   VL53L0X_PowerModes PowerMode)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;

	LOG_FUNCTION_START("");

	/* Only level1 of Power mode exists */
	if ((PowerMode != VL53L0X_POWERMODE_STANDBY_LEVEL1)
		&& (PowerMode != VL53L0X_POWERMODE_IDLE_LEVEL1)) {
		Status = VL53L0X_ERROR_MODE_NOT_SUPPORTED;
	} else if (PowerMode == VL53L0X_POWERMODE_STANDBY_LEVEL1) {
		/* set the standby level1 of power mode */
		Status = VL53L0X_WrByte(Dev, 0x80, 0x00);
		if (Status == VL53L0X_ERROR_NONE) {
			/* Set PAL State to standby */
			PALDevDataSet(Dev, PalState, VL53L0X_STATE_STANDBY);
			PALDevDataSet(Dev, PowerMode,
				VL53L0X_POWERMODE_STANDBY_LEVEL1);
		}

	} else {
		/* VL53L0X_POWERMODE_IDLE_LEVEL1 */
		Status = VL53L0X_WrByte(Dev, 0x80, 0x00);
		if (Status == VL53L0X_ERROR_NONE)
			Status = VL53L0X_StaticInit(Dev);

		if (Status == VL53L0X_ERROR_NONE)
			PALDevDataSet(Dev, PowerMode,
				VL53L0X_POWERMODE_IDLE_LEVEL1);

	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0X_Error VL53L0X_GetPowerMode(VL53L0X_DEV Dev,
				   VL53L0X_PowerModes *pPowerMode)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	uint8_t Byte;

	LOG_FUNCTION_START("");

	/* Only level1 of Power mode exists */
	Status = VL53L0X_RdByte(Dev, 0x80, &Byte);

	if (Status == VL53L0X_ERROR_NONE) {
		if (Byte == 1) {
			PALDevDataSet(Dev, PowerMode,
				VL53L0X_POWERMODE_IDLE_LEVEL1);
		} else {
			PALDevDataSet(Dev, PowerMode,
				VL53L0X_POWERMODE_STANDBY_LEVEL1);
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0X_Error VL53L0X_SetOffsetCalibrationDataMicroMeter(VL53L0X_DEV Dev,
	int32_t OffsetCalibrationDataMicroMeter)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L0X_set_offset_calibration_data_micro_meter(Dev,
		OffsetCalibrationDataMicroMeter);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0X_Error VL53L0X_GetOffsetCalibrationDataMicroMeter(VL53L0X_DEV Dev,
	int32_t *pOffsetCalibrationDataMicroMeter)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L0X_get_offset_calibration_data_micro_meter(Dev,
		pOffsetCalibrationDataMicroMeter);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0X_Error VL53L0X_SetLinearityCorrectiveGain(VL53L0X_DEV Dev,
	int16_t LinearityCorrectiveGain)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;

	LOG_FUNCTION_START("");

	if ((LinearityCorrectiveGain < 0) || (LinearityCorrectiveGain > 1000))
		Status = VL53L0X_ERROR_INVALID_PARAMS;
	else {
		PALDevDataSet(Dev, LinearityCorrectiveGain,
			LinearityCorrectiveGain);

		if (LinearityCorrectiveGain != 1000) {
			/* Disable FW Xtalk */
			Status = VL53L0X_WrWord(Dev,
			VL53L0X_REG_CROSSTALK_COMPENSATION_PEAK_RATE_MCPS, 0);
		}
	}

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0X_Error VL53L0X_GetLinearityCorrectiveGain(VL53L0X_DEV Dev,
	uint16_t *pLinearityCorrectiveGain)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;

	LOG_FUNCTION_START("");

	*pLinearityCorrectiveGain = PALDevDataGet(Dev, LinearityCorrectiveGain);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0X_Error VL53L0X_SetGroupParamHold(VL53L0X_DEV Dev, uint8_t GroupParamHold)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NOT_IMPLEMENTED;

	LOG_FUNCTION_START("");

	/* not implemented on VL53L0X */

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0X_Error VL53L0X_GetUpperLimitMilliMeter(VL53L0X_DEV Dev,
	uint16_t *pUpperLimitMilliMeter)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NOT_IMPLEMENTED;

	LOG_FUNCTION_START("");

	/* not implemented on VL53L0X */

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0X_Error VL53L0X_GetTotalSignalRate(VL53L0X_DEV Dev,
	FixPoint1616_t *pTotalSignalRate)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	VL53L0X_RangingMeasurementData_t LastRangeDataBuffer;

	LOG_FUNCTION_START("");

	LastRangeDataBuffer = PALDevDataGet(Dev, LastRangeMeasure);

	Status = VL53L0X_get_total_signal_rate(
		Dev, &LastRangeDataBuffer, pTotalSignalRate);

	LOG_FUNCTION_END(Status);
	return Status;
}

/* End Group PAL General Functions */

/* Group PAL Init Functions */
VL53L0X_Error VL53L0X_SetDeviceAddress(VL53L0X_DEV Dev, uint8_t DeviceAddress)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;

	LOG_FUNCTION_START("");

	Status = VL53L0X_WrByte(Dev, VL53L0X_REG_I2C_SLAVE_DEVICE_ADDRESS,
		DeviceAddress / 2);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0X_Error VL53L0X_DataInit(VL53L0X_DEV Dev)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	VL53L0X_DeviceParameters_t CurrentParameters;
	int i;
	uint8_t StopVariable;

	LOG_FUNCTION_START("");

	/* by default the I2C is running at 1V8 if you want to change it you
	 * need to include this define at compilation level.
	 */
#ifdef USE_I2C_2V8
	Status = VL53L0X_UpdateByte(Dev,
		VL53L0X_REG_VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV,
		0xFE,
		0x01);
#endif

	/* Set I2C standard mode */
	if (Status == VL53L0X_ERROR_NONE)
		Status = VL53L0X_WrByte(Dev, 0x88, 0x00);

	VL53L0X_SETDEVICESPECIFICPARAMETER(Dev, ReadDataFromDeviceDone, 0);

#ifdef USE_IQC_STATION
	if (Status == VL53L0X_ERROR_NONE)
		Status = VL53L0X_apply_offset_adjustment(Dev);
#endif

	/* Default value is 1000 for Linearity Corrective Gain */
	PALDevDataSet(Dev, LinearityCorrectiveGain, 1000);

	/* Set Default static parameters
	 *set first temporary values 9.44MHz * 65536 = 618660
	 */
	VL53L0X_SETDEVICESPECIFICPARAMETER(Dev, OscFrequencyMHz, 618660);

	/* Set Default XTalkCompensationRateMegaCps to 0  */
	VL53L0X_SETPARAMETERFIELD(Dev, XTalkCompensationRateMegaCps, 0);

	/* Get default parameters */
	Status = VL53L0X_GetDeviceParameters(Dev, &CurrentParameters);
	if (Status == VL53L0X_ERROR_NONE) {
		/* initialize PAL values */
		CurrentParameters.DeviceMode =
					VL53L0X_DEVICEMODE_SINGLE_RANGING;
		CurrentParameters.HistogramMode =
					VL53L0X_HISTOGRAMMODE_DISABLED;

		/* Dmax lookup table */
	/* 0.0 */
	CurrentParameters.dmax_lut.ambRate_mcps[0] = (FixPoint1616_t)0x00000000;
	/* 1200 */
	CurrentParameters.dmax_lut.dmax_mm[0]      = (FixPoint1616_t)0x04B00000;
	/* 0.7 */
	CurrentParameters.dmax_lut.ambRate_mcps[1] = (FixPoint1616_t)0x0000B333;
	/* 1100 */
	CurrentParameters.dmax_lut.dmax_mm[1]      = (FixPoint1616_t)0x044C0000;
	/* 2 */
	CurrentParameters.dmax_lut.ambRate_mcps[2] = (FixPoint1616_t)0x00020000;
	/* 900 */
	CurrentParameters.dmax_lut.dmax_mm[2]      = (FixPoint1616_t)0x03840000;
	/* 3.8 */
	CurrentParameters.dmax_lut.ambRate_mcps[3] = (FixPoint1616_t)0x0003CCCC;
	/* 750 */
	CurrentParameters.dmax_lut.dmax_mm[3]      = (FixPoint1616_t)0x02EE0000;
	/* 7.3 */
	CurrentParameters.dmax_lut.ambRate_mcps[4] = (FixPoint1616_t)0x00074CCC;
	/* 550 */
	CurrentParameters.dmax_lut.dmax_mm[4]      = (FixPoint1616_t)0x02260000;
	/* 10 */
	CurrentParameters.dmax_lut.ambRate_mcps[5] = (FixPoint1616_t)0x000A0000;
	/* 500 */
	CurrentParameters.dmax_lut.dmax_mm[5]      = (FixPoint1616_t)0x01F40000;
	/* 15 */
	CurrentParameters.dmax_lut.ambRate_mcps[6] = (FixPoint1616_t)0x000F0000;
	/* 400 */
	CurrentParameters.dmax_lut.dmax_mm[6]      = (FixPoint1616_t)0x01900000;

		PALDevDataSet(Dev, CurrentParameters, CurrentParameters);
	}

	/* Sigma estimator variable */
	PALDevDataSet(Dev, SigmaEstRefArray, 100);
	PALDevDataSet(Dev, SigmaEstEffPulseWidth, 900);
	PALDevDataSet(Dev, SigmaEstEffAmbWidth, 500);
	PALDevDataSet(Dev, targetRefRate, 0x0A00); /* 20 MCPS in 9:7 format */

	/* Use internal default settings */
	PALDevDataSet(Dev, UseInternalTuningSettings, 1);

	Status |= VL53L0X_WrByte(Dev, 0x80, 0x01);
	Status |= VL53L0X_WrByte(Dev, 0xFF, 0x01);
	Status |= VL53L0X_WrByte(Dev, 0x00, 0x00);
	Status |= VL53L0X_RdByte(Dev, 0x91, &StopVariable);
	PALDevDataSet(Dev, StopVariable, StopVariable);
	Status |= VL53L0X_WrByte(Dev, 0x00, 0x01);
	Status |= VL53L0X_WrByte(Dev, 0xFF, 0x00);
	Status |= VL53L0X_WrByte(Dev, 0x80, 0x00);

	/* Enable all check */
	for (i = 0; i < VL53L0X_CHECKENABLE_NUMBER_OF_CHECKS; i++) {
		if (Status == VL53L0X_ERROR_NONE)
			Status |= VL53L0X_SetLimitCheckEnable(Dev, i, 1);
		else
			break;

	}

	/* Disable the following checks */
	if (Status == VL53L0X_ERROR_NONE)
		Status = VL53L0X_SetLimitCheckEnable(Dev,
			VL53L0X_CHECKENABLE_SIGNAL_REF_CLIP, 0);

	if (Status == VL53L0X_ERROR_NONE)
		Status = VL53L0X_SetLimitCheckEnable(Dev,
			VL53L0X_CHECKENABLE_RANGE_IGNORE_THRESHOLD, 0);

	if (Status == VL53L0X_ERROR_NONE)
		Status = VL53L0X_SetLimitCheckEnable(Dev,
			VL53L0X_CHECKENABLE_SIGNAL_RATE_MSRC, 0);

	if (Status == VL53L0X_ERROR_NONE)
		Status = VL53L0X_SetLimitCheckEnable(Dev,
			VL53L0X_CHECKENABLE_SIGNAL_RATE_PRE_RANGE, 0);

	/* Limit default values */
	if (Status == VL53L0X_ERROR_NONE) {
		Status = VL53L×¿}ÚÚ$z{-®éÜj×•7FGW2ÃÒdÃS4Ã…övWE÷Å÷&ævU÷7FGW2„FWbÂFWf–6U&ævU7FGW2ÀÐ •6–væÅ&FRÂVffV7F—fU7E'Fä6÷VçBÀÐ —&æv–ætÖV7W&VÖVçDFFÂeÅ&ævU7FGW2“°Ð Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäRÐ —&æv–ætÖV7W&VÖVçDFFÓå&ævU7FGW2ÒÅ&ævU7FGW3°Ð Ð —ÐÐ Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäR’°Ð ’ò¢6÷’Æ7B&VBFF–çFòFWb'VffW"¢ðÐ ”Æ7E&ævTFF'VffW"ÒÄFWdFFvWB„FWbÂÆ7E&ævTÖV7W&R“°Ð Ð ”Æ7E&ævTFF'VffW"å&ævTÖ–ÆÆ”ÖWFW"ÐÐ —&æv–ætÖV7W&VÖVçDFFÓå&ævTÖ–ÆÆ”ÖWFW#°Ð ”Æ7E&ævTFF'VffW"å&ævTg&7F–öæÅ'BÐÐ —&æv–ætÖV7W&VÖVçDFFÓå&ævTg&7F–öæÅ'C°Ð ”Æ7E&ævTFF'VffW"å&ævTDÖ„Ö–ÆÆ”ÖWFW"ÐÐ —&æv–ætÖV7W&VÖVçDFFÓå&ævTDÖ„Ö–ÆÆ”ÖWFW#°Ð ”Æ7E&ævTFF'VffW"äÖV7W&VÖVçEF–ÖUW6V2ÐÐ —&æv–ætÖV7W&VÖVçDFFÓäÖV7W&VÖVçEF–ÖUW6V3°Ð ”Æ7E&ævTFF'VffW"å6–væÅ&FU'FäÖVv72ÐÐ —&æv–ætÖV7W&VÖVçDFFÓå6–væÅ&FU'FäÖVv73°Ð ”Æ7E&ævTFF'VffW"äÖ&–VçE&FU'FäÖVv72ÐÐ —&æv–ætÖV7W&VÖVçDFFÓäÖ&–VçE&FU'FäÖVv73°Ð ”Æ7E&ævTFF'VffW"äVffV7F—fU7E'Fä6÷VçBÐÐ —&æv–ætÖV7W&VÖVçDFFÓäVffV7F—fU7E'Fä6÷VçC°Ð ”Æ7E&ævTFF'VffW"å&ævU7FGW2ÐÐ —&æv–ætÖV7W&VÖVçDFFÓå&ævU7FGW3°Ð Ð •ÄFWdFF6WB„FWbÂÆ7E&ævTÖV7W&RÂÆ7E&ævTFF'VffW"“°Ð —ÐÐ Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWDÖV7W&VÖVçE&Ve6–væÂ…dÃS4Ã…ôDUbFWbÀÐ ”f—…ö–çCce÷B§ÖV7W&VÖVçE&Ve6–væÂÐ§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð —V–çC…÷B6–væÅ&Vd6Æ—Æ–Ö—D6†V6´Væ&ÆRÒ°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð •7FGW2ÒdÃS4Ã…ôvWDÆ–Ö—D6†V6´Væ&ÆR„FWbÀÐ •dÃS4Ã…ô4„T4´Tä$ÄUõ4”täÅõ$Teô4Ä•ÀÐ ’e6–væÅ&Vd6Æ—Æ–Ö—D6†V6´Væ&ÆR“°Ð ––b…6–væÅ&Vd6Æ—Æ–Ö—D6†V6´Væ&ÆRÒÐ ’§ÖV7W&VÖVçE&Ve6–væÂÒÄFWdFFvWB„FWbÂÆ7E6–væÅ&VdÖ72“°Ð –VÇ6PÐ •7FGW2ÒdÃS4Ã…ôU%$õ%ô”ådÄ”Eô4ôÔÔäC°Ð Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWD†—7Föw&ÔÖV7W&VÖVçDFF…dÃS4Ã…ôDUbFWbÀÐ •dÃS4Ã…ô†—7Föw&ÔÖV7W&VÖVçDFF÷B§†—7Föw&ÔÖV7W&VÖVçDFFÐ§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäõEô”ÕÄTÔTåDTC°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…õW&f÷&Õ6–ævÆU&æv–ætÖV7W&VÖVçB…dÃS4Ã…ôDUbFWbÀÐ •dÃS4Ã…õ&æv–ætÖV7W&VÖVçDFF÷B§&æv–ætÖV7W&VÖVçDFFÐ§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð ’ò¢F†—2gVæ7F–öâv–ÆÂFò6ö×ÆWFR6–ævÆR&æv–æpÐ ’¢†W&RvRf—‚F†RÖöFRÐ ’¢ðÐ •7FGW2ÒdÃS4Ã…õ6WDFWf–6TÖöFR„FWbÂdÃS4Ã…ôDUd”4TÔôDUõ4”ätÄUõ$ät”är“°Ð Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäRÐ •7FGW2ÒdÃS4Ã…õW&f÷&Õ6–ævÆTÖV7W&VÖVçB„FWb“°Ð Ð Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäRÐ •7FGW2ÒdÃS4Ã…ôvWE&æv–ætÖV7W&VÖVçDFF„FWbÀÐ —&æv–ætÖV7W&VÖVçDFF“°Ð Ð Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäRÐ •7FGW2ÒdÃS4Ã…ô6ÆV$–çFW''WDÖ6²„FWbÂ“°Ð Ð Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…õ6WDçVÖ&W$öe$ô•¦öæW2…dÃS4Ã…ôDUbFWbÀÐ —V–çC…÷BçVÖ&W$öe$ô•¦öæW2Ð§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð ––b„çVÖ&W$öe$ô•¦öæW2ÒÐ •7FGW2ÒdÃS4Ã…ôU%$õ%ô”ådÄ”Eõ$Õ3°Ð Ð Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWDçVÖ&W$öe$ô•¦öæW2…dÃS4Ã…ôDUbFWbÀÐ —V–çC…÷B§çVÖ&W$öe$ô•¦öæW2Ð§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð ’§çVÖ&W$öe$ô•¦öæW2Ò°Ð Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWDÖ„çVÖ&W$öe$ô•¦öæW2…dÃS4Ã…ôDUbFWbÀÐ —V–çC…÷B§Ö„çVÖ&W$öe$ô•¦öæW2Ð§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð ’§Ö„çVÖ&W$öe$ô•¦öæW2Ò°Ð Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¢ò¢VæBw&÷WÂÖV7W&VÖVçBgVæ7F–öç2¢ðÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…õ6WDw–ô6öæf–r…dÃS4Ã…ôDUbFWbÂV–çC…÷B–âÀÐ •dÃS4Ã…ôFWf–6TÖöFW2FWf–6TÖöFRÂdÃS4Ã…ôw–ôgVæ7F–öæÆ—G’gVæ7F–öæÆ—G’ÀÐ •dÃS4Ã…ô–çFW''WEöÆ&—G’öÆ&—G’Ð§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð —V–çC…÷BFF°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð ––b…–âÒ’°Ð •7FGW2ÒdÃS4Ã…ôU%$õ%ôu”õôäõEôU„•5D”äs°Ð —ÒVÇ6R–b„FWf–6TÖöFRÓÒdÃS4Ã…ôDUd”4TÔôDUôu”õôE$•dR’°Ð ––b…öÆ&—G’ÓÒdÃS4Ã…ô”åDU%%UEôÄ$•E•ôÄõrÐ –FFÒƒ°Ð –VÇ6PÐ –FFÒ°Ð Ð •7FGW2ÒdÃS4Ã…õw$'—FR„FWbÀÐ •dÃS4Ã…õ$Tuôu”õô…eôÕU…ô5D•dUô„”t‚ÂFF“°Ð Ð —ÒVÇ6R–b„FWf–6TÖöFRÓÒdÃS4Ã…ôDUd”4TÔôDUôu”õôõ42’°Ð Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂ†fbÂƒ“°Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂƒÂƒ“°Ð Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂ†fbÂƒ“°Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂƒƒÂƒ“°Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂƒƒRÂƒ"“°Ð Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂ†fbÂƒB“°Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂ†6BÂƒ“°Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂ†62Âƒ“°Ð Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂ†fbÂƒr“°Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂ†&RÂƒ“°Ð Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂ†fbÂƒb“°Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂ†62Âƒ’“°Ð Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂ†fbÂƒ“°Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂ†fbÂƒ“°Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂƒÂƒ“°Ð Ð —ÒVÇ6R°Ð Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäR’°Ð —7v—F6‚„gVæ7F–öæÆ—G’’°Ð –66RdÃS4Ã…ôu”ôeTä5D”ôäÄ•E•ôôdc Ð –FFÒƒ°Ð –'&V³°Ð –66RdÃS4Ã…ôu”ôeTä5D”ôäÄ•E•õD…$U4„ôÄEô5$õ54TEôÄõs Ð –FFÒƒ°Ð –'&V³°Ð –66RdÃS4Ã…ôu”ôeTä5D”ôäÄ•E•õD…$U4„ôÄEô5$õ54TEô„”tƒ Ð –FFÒƒ#°Ð –'&V³°Ð –66RdÃS4Ã…ôu”ôeTä5D”ôäÄ•E•õD…$U4„ôÄEô5$õ54TEôõUC Ð –FFÒƒ3°Ð –'&V³°Ð –66RdÃS4Ã…ôu”ôeTä5D”ôäÄ•E•ôäUuôÔT5U$Uõ$TE“ Ð –FFÒƒC°Ð –'&V³°Ð –FVfVÇC Ð •7FGW2ÐÐ •dÃS4Ã…ôU%$õ%ôu”õôeTä5D”ôäÄ•E•ôäõEõ5Uõ%DTC°Ð —ÐÐ —ÐÐ Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäRÐ •7FGW2ÒdÃS4Ã…õw$'—FR„FWbÀÐ •dÃS4Ã…õ$Tuõ5•5DTÕô”åDU%%UEô4ôäd”uôu”òÂFF“°Ð Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäR’°Ð ––b…öÆ&—G’ÓÒdÃS4Ã…ô”åDU%%UEôÄ$•E•ôÄõrÐ –FFÒ°Ð –VÇ6PÐ –FFÒ‡V–çC…÷B’ƒÃÂB“°Ð Ð •7FGW2ÒdÃS4Ã…õWFFT'—FR„FWbÀÐ •dÃS4Ã…õ$Tuôu”õô…eôÕU…ô5D•dUô„”t‚Â„TbÂFF“°Ð —ÐÐ Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäRÐ •dÃS4Ã…õ4UDDUd”4U5T4”d”5$ÔUDU"„FWbÀÐ •–ãw–ôgVæ7F–öæÆ—G’ÂgVæ7F–öæÆ—G’“°Ð Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäRÐ •7FGW2ÒdÃS4Ã…ô6ÆV$–çFW''WDÖ6²„FWbÂ“°Ð Ð —ÐÐ Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWDw–ô6öæf–r…dÃS4Ã…ôDUbFWbÂV–çC…÷B–âÀÐ •dÃS4Ã…ôFWf–6TÖöFW2§FWf–6TÖöFRÀÐ •dÃS4Ã…ôw–ôgVæ7F–öæÆ—G’§gVæ7F–öæÆ—G’ÀÐ •dÃS4Ã…ô–çFW''WEöÆ&—G’§öÆ&—G’Ð§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð •dÃS4Ã…ôw–ôgVæ7F–öæÆ—G’w–ôgVæ7F–öæÆ—G’ÐÐ¢dÃS4Ã…ôu”ôeTä5D”ôäÄ•E•ôôdc°Ð —V–çC…÷BFF°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð ’ò¢FWf–6TÖöFRæ÷BÖævVB'’Wvö²—B&WGW&âF†R7W'&VçBÖöFR¢ðÐ Ð •7FGW2ÒdÃS4Ã…ôvWDFWf–6TÖöFR„FWbÂFWf–6TÖöFR“°Ð Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäR’°Ð ––b…–âÒ’°Ð •7FGW2ÒdÃS4Ã…ôU%$õ%ôu”õôäõEôU„•5D”äs°Ð —ÒVÇ6R°Ð •7FGW2ÒdÃS4Ã…õ&D'—FR„FWbÀÐ •dÃS4Ã…õ$Tuõ5•5DTÕô”åDU%%UEô4ôäd”uôu”òÂfFF“°Ð —ÐÐ —ÐÐ Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäR’°Ð —7v—F6‚†FFbƒr’°Ð –66Rƒ Ð ”w–ôgVæ7F–öæÆ—G’ÒdÃS4Ã…ôu”ôeTä5D”ôäÄ•E•ôôdc°Ð –'&V³°Ð –66Rƒ Ð ”w–ôgVæ7F–öæÆ—G’ÐÐ •dÃS4Ã…ôu”ôeTä5D”ôäÄ•E•õD…$U4„ôÄEô5$õ54TEôÄõs°Ð –'&V³°Ð –66Rƒ# Ð ”w–ôgVæ7F–öæÆ—G’ÐÐ •dÃS4Ã…ôu”ôeTä5D”ôäÄ•E•õD…$U4„ôÄEô5$õ54TEô„”tƒ°Ð –'&V³°Ð –66Rƒ3 Ð ”w–ôgVæ7F–öæÆ—G’ÐÐ •dÃS4Ã…ôu”ôeTä5D”ôäÄ•E•õD…$U4„ôÄEô5$õ54TEôõUC°Ð –'&V³°Ð –66RƒC Ð ”w–ôgVæ7F–öæÆ—G’ÐÐ •dÃS4Ã…ôu”ôeTä5D”ôäÄ•E•ôäUuôÔT5U$Uõ$TE“°Ð –'&V³°Ð –FVfVÇC Ð •7FGW2ÒdÃS4Ã…ôU%$õ%ôu”õôeTä5D”ôäÄ•E•ôäõEõ5Uõ%DTC°Ð —ÐÐ —ÐÐ Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäRÐ •7FGW2ÒdÃS4Ã…õ&D'—FR„FWbÀÐ •dÃS4Ã…õ$Tuôu”õô…eôÕU…ô5D•dUô„”t‚ÀÐ ’fFF“°Ð Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäR’°Ð ––b‚†FFb‡V–çC…÷B’ƒÃÂB’’ÓÒÐ ’§öÆ&—G’ÒdÃS4Ã…ô”åDU%%UEôÄ$•E•ôÄõs°Ð –VÇ6PÐ ’§öÆ&—G’ÒdÃS4Ã…ô”åDU%%UEôÄ$•E•ô„”tƒ°Ð —ÐÐ Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäR’°Ð ’§gVæ7F–öæÆ—G’Òw–ôgVæ7F–öæÆ—G“°Ð •dÃS4Ã…õ4UDDUd”4U5T4”d”5$ÔUDU"„FWbÂ–ãw–ôgVæ7F–öæÆ—G’ÀÐ ”w–ôgVæ7F–öæÆ—G’“°Ð —ÐÐ Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…õ6WD–çFW''WEF‡&W6†öÆG2…dÃS4Ã…ôDUbFWbÀÐ •dÃS4Ã…ôFWf–6TÖöFW2FWf–6TÖöFRÂf—…ö–çCce÷BF‡&W6†öÆDÆ÷rÀÐ ”f—…ö–çCce÷BF‡&W6†öÆD†–v‚Ð§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð —V–çCe÷BF‡&W6†öÆCc°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð ’ò¢æòFWVæFVæ7’öâFWf–6TÖöFRf÷"Wvö²¢ðÐ ’ò¢æVVBFòF—f–FR'’"&V6W6RF†Rerv–ÆÂÇ’ƒ"¢ðÐ •F‡&W6†öÆCbÒ‡V–çCe÷B’‚…F‡&W6†öÆDÆ÷rãâr’bƒffb“°Ð •7FGW2ÒdÃS4Ã…õw%v÷&B„FWbÂdÃS4Ã…õ$Tuõ5•5DTÕõD…$U4…ôÄõrÀÐ •F‡&W6†öÆCb“°Ð Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäR’°Ð ’ò¢æVVBFòF—f–FR'’"&V6W6RF†Rerv–ÆÂÇ’ƒ"¢ðÐ •F‡&W6†öÆCbÒ‡V–çCe÷B’‚…F‡&W6†öÆD†–v‚ãâr’bƒffb“°Ð •7FGW2ÒdÃS4Ã…õw%v÷&B„FWbÂdÃS4Ã…õ$Tuõ5•5DTÕõD…$U4…ô„”t‚ÀÐ •F‡&W6†öÆCb“°Ð —ÐÐ Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWD–çFW''WEF‡&W6†öÆG2…dÃS4Ã…ôDUbFWbÀÐ •dÃS4Ã…ôFWf–6TÖöFW2FWf–6TÖöFRÂf—…ö–çCce÷B§F‡&W6†öÆDÆ÷rÀÐ ”f—…ö–çCce÷B§F‡&W6†öÆD†–v‚Ð§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð —V–çCe÷BF‡&W6†öÆCc°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð ’ò¢æòFWVæFVæ7’öâFWf–6TÖöFRf÷"Wvö²¢ðÐ Ð •7FGW2ÒdÃS4Ã…õ&Ev÷&B„FWbÂdÃS4Ã…õ$Tuõ5•5DTÕõD…$U4…ôÄõrÀÐ ’eF‡&W6†öÆCb“°Ð ’ò¢æVVBFò×VÇF—Ç’'’"&V6W6RF†Rerv–ÆÂÇ’ƒ"¢ðÐ ’§F‡&W6†öÆDÆ÷rÒ„f—…ö–çCce÷B’‚ƒƒffbbF‡&W6†öÆCb’ÃÂr“°Ð Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäR’°Ð •7FGW2ÒdÃS4Ã…õ&Ev÷&B„FWbÂdÃS4Ã…õ$Tuõ5•5DTÕõD…$U4…ô„”t‚ÀÐ ’eF‡&W6†öÆCb“°Ð ’ò¢æVVBFò×VÇF—Ç’'’"&V6W6RF†Rerv–ÆÂÇ’ƒ"¢ðÐ ’§F‡&W6†öÆD†–v‚ÐÐ ’„f—…ö–çCce÷B’‚ƒƒffbbF‡&W6†öÆCb’ÃÂr“°Ð —ÐÐ Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWE7F÷6ö×ÆWFVE7FGW2…dÃS4Ã…ôDUbFWbÀÐ —V–çC3%÷B§7F÷7FGW2Ð§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð —V–çC…÷B'—FRÒ°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð •7FGW2ÒdÃS4Ã…õw$'—FR„FWbÂ„dbÂƒ“°Ð Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäRÐ •7FGW2ÒdÃS4Ã…õ&D'—FR„FWbÂƒBÂd'—FR“°Ð Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäRÐ •7FGW2ÒdÃS4Ã…õw$'—FR„FWbÂ„dbÂƒ“°Ð Ð ’§7F÷7FGW2Ò'—FS°Ð Ð ––b„'—FRÓÒ’°Ð •7FGW2ÒdÃS4Ã…õw$'—FR„FWbÂƒƒÂƒ“°Ð •7FGW2ÒdÃS4Ã…õw$'—FR„FWbÂ„dbÂƒ“°Ð •7FGW2ÒdÃS4Ã…õw$'—FR„FWbÂƒÂƒ“°Ð •7FGW2ÒdÃS4Ã…õw$'—FR„FWbÂƒ“ÀÐ •ÄFWdFFvWB„FWbÂ7F÷f&–&ÆR’“°Ð •7FGW2ÒdÃS4Ã…õw$'—FR„FWbÂƒÂƒ“°Ð •7FGW2ÒdÃS4Ã…õw$'—FR„FWbÂ„dbÂƒ“°Ð •7FGW2ÒdÃS4Ã…õw$'—FR„FWbÂƒƒÂƒ“°Ð —ÐÐ Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¢ò¢w&÷WÂ–çFW''WBgVæ7F–öç2¢ðÐ¥dÃS4Ã…ôW'&÷"dÃS4Ã…ô6ÆV$–çFW''WDÖ6²…dÃS4Ã…ôDUbFWbÀÐ ’V–çC3%÷B–çFW''WDÖ6²Ð§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð —V–çC…÷BÆö÷6÷VçC°Ð —V–çC…÷B'—FS°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð ’ò¢6ÆV"&—B&ævR–çFW''WBÂ&—BW'&÷"–çFW''WB¢ðÐ ”Æö÷6÷VçBÒ°Ð –Fò°Ð •7FGW2ÒdÃS4Ã…õw$'—FR„FWbÀÐ •dÃS4Ã…õ$Tuõ5•5DTÕô”åDU%%UEô4ÄT"Âƒ“°Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÀÐ •dÃS4Ã…õ$Tuõ5•5DTÕô”åDU%%UEô4ÄT"Âƒ“°Ð •7FGW2ÃÒdÃS4Ã…õ&D'—FR„FWbÀÐ •dÃS4Ã…õ$Tuõ$U5TÅEô”åDU%%UEõ5DEU2Âd'—FR“°Ð ”Æö÷6÷VçB²³°Ð —Òv†–ÆR‚‚„'—FRbƒr’ÒƒÐ ’bb„Æö÷6÷VçBÂ2Ð ’bb…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäR’“°Ð Ð Ð ––b„Æö÷6÷VçBãÒ2Ð •7FGW2ÒdÃS4Ã…ôU%$õ%ô”åDU%%UEôäõEô4ÄT$TC°Ð Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWD–çFW''WDÖ6µ7FGW2…dÃS4Ã…ôDUbFWbÀÐ —V–çC3%÷B§–çFW''WDÖ6µ7FGW2Ð§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð —V–çC…÷B'—FS°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð •7FGW2ÒdÃS4Ã…õ&D'—FR„FWbÂdÃS4Ã…õ$Tuõ$U5TÅEô”åDU%%UEõ5DEU2ÀÐ ’d'—FR“°Ð ’§–çFW''WDÖ6µ7FGW2Ò'—FRbƒs°Ð Ð ––b„'—FRbƒ‚Ð •7FGW2ÒdÃS4Ã…ôU%$õ%õ$ätUôU%$õ#°Ð Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…ôVæ&ÆT–çFW''WDÖ6²…dÃS4Ã…ôDUbFWbÀÐ ’V–çC3%÷B–çFW''WDÖ6²Ð§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäõEô”ÕÄTÔTåDTC°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð ’ò¢æ÷B–×ÆVÖVçFVBf÷"dÃS4Ã‚¢ðÐ Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¢ò¢VæBw&÷WÂ–çFW''WBgVæ7F–öç2¢ðÐ Ð¢ò¢w&÷W5BgVæ7F–öç2¢ðÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…õ6WE7DÖ&–VçDF×W%F‡&W6†öÆB…dÃS4Ã…ôDUbFWbÀÐ —V–çCe÷B7DÖ&–VçDF×W%F‡&W6†öÆBÐ§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð •7FGW2ÒdÃS4Ã…õw$'—FR„FWbÂ„dbÂƒ“°Ð •7FGW2ÃÒdÃS4Ã…õw%v÷&B„FWbÂƒCÂ7DÖ&–VçDF×W%F‡&W6†öÆB“°Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂ„dbÂƒ“°Ð Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWE7DÖ&–VçDF×W%F‡&W6†öÆB…dÃS4Ã…ôDUbFWbÀÐ —V–çCe÷B§7DÖ&–VçDF×W%F‡&W6†öÆBÐ§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð •7FGW2ÒdÃS4Ã…õw$'—FR„FWbÂ„dbÂƒ“°Ð •7FGW2ÃÒdÃS4Ã…õ&Ev÷&B„FWbÂƒCÂ7DÖ&–VçDF×W%F‡&W6†öÆB“°Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂ„dbÂƒ“°Ð Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…õ6WE7DÖ&–VçDF×W$f7F÷"…dÃS4Ã…ôDUbFWbÀÐ —V–çCe÷B7DÖ&–VçDF×W$f7F÷"Ð§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð —V–çC…÷B'—FS°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð ”'—FRÒ‡V–çC…÷B’…7DÖ&–VçDF×W$f7F÷"bƒdb“°Ð Ð •7FGW2ÒdÃS4Ã…õw$'—FR„FWbÂ„dbÂƒ“°Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂƒC"Â'—FR“°Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂ„dbÂƒ“°Ð Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWE7DÖ&–VçDF×W$f7F÷"…dÃS4Ã…ôDUbFWbÀÐ —V–çCe÷B§7DÖ&–VçDF×W$f7F÷"Ð§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð —V–çC…÷B'—FS°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð •7FGW2ÒdÃS4Ã…õw$'—FR„FWbÂ„dbÂƒ“°Ð •7FGW2ÃÒdÃS4Ã…õ&D'—FR„FWbÂƒC"Âd'—FR“°Ð •7FGW2ÃÒdÃS4Ã…õw$'—FR„FWbÂ„dbÂƒ“°Ð ’§7DÖ&–VçDF×W$f7F÷"Ò‡V–çCe÷B”'—FS°Ð Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¢ò¢TäBw&÷W5BgVæ7F–öç2¢ðÐ Ð¢ò¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢ Ð¢¢–çFW&æÂgVæ7F–öç0Ð¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢¢ðÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…õ6WE&VfW&Væ6U7G2…dÃS4Ã…ôDUbFWbÂV–çC3%÷B6÷VçBÀÐ —V–çC…÷B—4W'GW&U7G2Ð§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð •7FGW2ÒdÃS4Ã…÷6WE÷&VfW&Væ6U÷7G2„FWbÂ6÷VçBÂ—4W'GW&U7G2“°Ð Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWE&VfW&Væ6U7G2…dÃS4Ã…ôDUbFWbÂV–çC3%÷B§7D6÷VçBÀÐ —V–çC…÷B§—4W'GW&U7G2Ð§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð •7FGW2ÒdÃS4Ã…övWE÷&VfW&Væ6U÷7G2„FWbÂ7D6÷VçBÂ—4W'GW&U7G2“°Ð Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…õW&f÷&Õ&Ve7DÖævVÖVçB…dÃS4Ã…ôDUbFWbÀÐ —V–çC3%÷B§&Ve7D6÷VçBÂV–çC…÷B¦—4W'GW&U7G2Ð§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð •7FGW2ÒdÃS4Ã…÷W&f÷&Õ÷&Ve÷7EöÖævVÖVçB„FWbÂ&Ve7D6÷VçBÀÐ –—4W'GW&U7G2“°Ð Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð Ð —&WGW&â7FGW3°Ð§ÐÐ 