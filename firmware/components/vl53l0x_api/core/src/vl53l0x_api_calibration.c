/*******************************************************************************
 * Copyright © 2016, STMicroelectronics International N.V.
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
#include "vl53l0x_api_core.h"
#include "vl53l0x_api_calibration.h"

#ifndef __KERNEL__
#include <stdlib.h>
#endif

#define LOG_FUNCTION_START(fmt, ...) \
	_LOG_FUNCTION_START(TRACE_MODULE_API, fmt, ##__VA_ARGS__)
#define LOG_FUNCTION_END(status, ...) \
	_LOG_FUNCTION_END(TRACE_MODULE_API, status, ##__VA_ARGS__)
#define LOG_FUNCTION_END_FMT(status, fmt, ...) \
	_LOG_FUNCTION_END_FMT(TRACE_MODULE_API, status, fmt, ##__VA_ARGS__)

#define REF_ARRAY_SPAD_0  0
#define REF_ARRAY_SPAD_5  5
#define REF_ARRAY_SPAD_10 10

uint32_t refArrayQuadrants[4] = {REF_ARRAY_SPAD_10, REF_ARRAY_SPAD_5,
		REF_ARRAY_SPAD_0, REF_ARRAY_SPAD_5 };

VL53L0X_Error VL53L0X_perform_xtalk_calibration(VL53L0X_DEV Dev,
			FixPoint1616_t XTalkCalDistance,
			FixPoint1616_t *pXTalkCompensationRateMegaCps)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	uint16_t sum_ranging = 0;
	uint16_t sum_spads = 0;
	FixPoint1616_t sum_signalRate = 0;
	FixPoint1616_t total_count = 0;
	uint8_t xtalk_meas = 0;
	VL53L0X_RangingMeasurementData_t RangingMeasurementData;
	FixPoint1616_t xTalkStoredMeanSignalRate;
	FixPoint1616_t xTalkStoredMeanRange;
	FixPoint1616_t xTalkStoredMeanRtnSpads;
	uint32_t signalXTalkTotalPerSpad;
	uint32_t xTalkStoredMeanRtnSpadsAsInt;
	uint32_t xTalkCalDistanceAsInt;
	FixPoint1616_t XTalkCompensationRateMegaCps;

	if (XTalkCalDistance <= 0)
		Status = VL53L0X_ERROR_INVALID_PARAMS;

	/* Disable the XTalk compensation */
	if (Status == VL53L0X_ERROR_NONE)
		Status = VL53L0X_SetXTalkCompensationEnable(Dev, 0);

	/* Disable the RIT */
	if (Status == VL53L0X_ERROR_NONE) {
		Status = VL53L0X_SetLimitCheckEnable(Dev,
				VL53L0X_CHECKENABLE_RANGE_IGNORE_THRESHOLD, 0);
	}

	/* Perform 50 measurements and compute the averages */
	if (Status == VL53L0X_ERROR_NONE) {
		sum_ranging = 0;
		sum_spads = 0;
		sum_signalRate = 0;
		total_count = 0;
		for (xtalk_meas = 0; xtalk_meas < 50; xtalk_meas++) {
			Status = VL53L0X_PerformSingleRangingMeasurement(Dev,
				&RangingMeasurementData);

			if (Status != VL53L0X_ERROR_NONE)
				break;

			/* The range is valid when RangeStatus = 0 */
			if (RangingMeasurementData.RangeStatus == 0) {
				sum_ranging = sum_ranging +
					RangingMeasurementData.RangeMilliMeter;
				sum_signalRate = sum_signalRate +
				RangingMeasurementData.SignalRateRtnMegaCps;
				sum_spads = sum_spads +
				RangingMeasurementData.EffectiveSpadRtnCount
					/ 256;
				total_count = total_count + 1;
			}
		}

		/* no valid values found */
		if (total_count == 0)
			Status = VL53L0X_ERROR_RANGE_ERROR;

	}


	if (Status == VL53L0X_ERROR_NONE) {
		/* FixPoint1616_t / uint16_t = FixPoint1616_t */
		xTalkStoredMeanSignalRate = sum_signalRate / total_count;
		xTalkStoredMeanRange = (FixPoint1616_t)((uint32_t)(
			sum_ranging << 16) / total_count);
		xTalkStoredMeanRtnSpads = (FixPoint1616_t)((uint32_t)(
			sum_spads << 16) / total_count);

		/* Round Mean Spads to Whole Number.
		 * Typically the calculated mean SPAD count is a whole number
		 * or very close to a whole
		 * number, therefore any truncation will not result in a
		 * significant loss in accuracy.
		 * Also, for a grey target at a typical distance of around
		 * 400mm, around 220 SPADs will
		 * be enabled, therefore, any truncation will result in a loss
		 * of accuracy of less than
		 * 0.5%.
		 */
		xTalkStoredMeanRtnSpadsAsInt = (xTalkStoredMeanRtnSpads +
			0x8000) >> 16;

		/* Round Cal Distance to Whole Number.
		 * Note that the cal distance is in mm, therefore no resolution
		 * is lost.
		 */
		 xTalkCalDistanceAsInt = (XTalkCalDistance + 0x8000) >> 16;

		if (xTalkStoredMeanRtnSpadsAsInt == 0 ||
		   xTalkCalDistanceAsInt == 0 ||
		   xTalkStoredMeanRange >= XTalkCalDistance) {
			XTalkCompensationRateMegaCps = 0;
		} else {
			/* Round Cal Distance to Whole Number.
			 * Note that the cal distance is in mm, therefore no
			 * resolution is lost.
			 */
			xTalkCalDistanceAsInt = (XTalkCalDistance +
				0x8000) >> 16;

			/* Apply division by mean spad count early in the
			 * calculation to keep the numbers small.
			 * This ensures we can maintain a 32bit calculation.
			 * Fixed1616 / int := Fixed1616
			 */
			signalXTalkTotalPerSpad = (xTalkStoredMeanSignalRate) /
				xTalkStoredMeanRtnSpadsAsInt;

			/* Complete the calculation for total Signal XTalk per
			 * SPAD
			 * Fixed1616 * (Fixed1616 - Fixed1616/int) :=
			 * (2^16 * Fixed1616)
			 */
			signalXTalkTotalPerSpad *= ((1 << 16) -
				(xTalkStoredMeanRange / xTalkCalDistanceAsInt));

			/* Round from 2^16 * Fixed1616, to Fixed1616. */
			XTalkCompensationRateMegaCps = (signalXTalkTotalPerSpad
				+ 0x8000) >> 16;
		}

		*pXTalkCompensationRateMegaCps = XTalkCompensationRateMegaCps;

		/* Enable the XTalk compensation */
		if (Status == VL53L0X_ERROR_NONE)
			Status = VL53L0X_SetXTalkCompensationEnable(Dev, 1);

		/* Enable the XTalk compensation */
		if (Status == VL53L0X_ERROR_NONE)
			Status = VL53L0X_SetXTalkCompensationRateMegaCps(Dev,
					XTalkCompensationRateMegaCps);

	}

	return Status;
}

VL53L0X_Error VL53L0X_perform_offset_calibration(VL53L0X_DEV Dev,
			FixPoint1616_t CalDistanceMilliMeter,
			int32_t *pOffsetMicroMeter)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	uint16_t sum_ranging = 0;
	FixPoint1616_t total_count = 0;
	VL53L0X_RangingMeasurementData_t RangingMeasurementData;
	FixPoint1616_t StoredMeanRange;
	uint32_t StoredMeanRangeAsInt;
	uint32_t CalDistanceAsInt_mm;
	uint8_t SequenceStepEnabled;
	int meas = 0;

	if (CalDistanceMilliMeter <= 0)
		Status = VL53L0X_ERROR_INVALID_PARAMS;

	if (Status == VL53L0X_ERROR_NONE)
		Status = VL53L0X_SetOffsetCalibrationDataMicroMeter(Dev, 0);


	/* Get the value of the TCC */
	if (Status == VL53L0X_ERROR_NONE)
		Status = VL53L0X_GetSequenceStepEnable(Dev,
				VL53L0X_SEQUENCESTEP_TCC, &SequenceStepEnabled);


	/* Disable the TCC */
	if (Status == VL53L0X_ERROR_NONE)
		Status = VL53L0X_SetSequenceStepEnable(Dev,
				VL53L0X_SEQUENCESTEP_TCC, 0);


	/* Disable the RIT */
	if (Status == VL53L0X_ERROR_NONE)
		Status = VL53L0X_SetLimitCheckEnable(Dev,
				VL53L0X_CHECKENABLE_RANGE_IGNORE_THRESHOLD, 0);

	/* Perform 50 measurements and compute the averages */
	if (Status == VL53L0X_ERROR_NONE) {
		sum_ranging = 0;
		total_count = 0;
		for (meas = 0; meas < 50; meas++) {
			Status = VL53L0X_PerformSingleRangingMeasurement(Dev,
					&RangingMeasurementData);

			if (Status != VL53L0X_ERROR_NONE)
				break;

			/* The range is valid when RangeStatus = 0 */
			if (RangingMeasurementData.RangeStatus == 0) {
				sum_ranging = sum_ranging +
					RangingMeasurementData.RangeMilliMeter;
				total_count = total_count + 1;
			}
		}

		/* no valid values found */
		if (total_count == 0)
			Status = VL53L0X_ERROR_RANGE_ERROR;
	}


	if (Status == VL53L0X_ERROR_NONE) {
		/* FixPoint1616_t / uint16_t = FixPoint1616_t */
		StoredMeanRange = (FixPoint1616_t)((uint32_t)(sum_ranging << 16)
			/ total_count);

		StoredMeanRangeAsInt = (StoredMeanRange + 0x8000) >> 16;

		/* Round Cal Distance to Whole Number.
		 * Note that the cal distance is in mm, therefore no resolution
		 * is lost.
		 */
		 CalDistanceAsInt_mm = (CalDistanceMilliMeter + 0x8000) >> 16;

		 *pOffsetMicroMeter = (CalDistanceAsInt_mm -
				 StoredMeanRangeAsInt) * 1000;

		/* Apply the calculated offset */
		if (Status == VL53L0X_ERROR_NONE) {
			VL53L0X_SETPARAMETERFIELD(Dev, RangeOffsetMicroMeters,
					*pOffsetMicroMeter);
			Status = VL53L0X_SetOffsetCalibrationDataMicroMeter(Dev,
					*pOffsetMicroMeter);
		}

	}

	/* Restore the TCC */
	if (Status == VL53L0X_ERROR_NONE) {
		if (SequenceStepEnabled != 0)
			Status = VL53L0X_SetSequenceStepEnable(Dev,
					VL53L0X_SEQUENCESTEP_TCC, 1);
	}

	return Status;
}


VL53L0X_Error VL53L0X_set_offset_calibration_data_micro_meter(VL53L0X_DEV Dev,
		int32_t OffsetCalibrationDataMicroMeter)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	int32_t cMaxOffsetMicroMeter = 511000;
	int32_t cMinOffsetMicroMeter = -512000;
	int16_t cOffsetRange = 4096;
	uint32_t encodedOffsetVal;

	LOG_FUNCTION_START("");

	if (OffsetCalibrationDataMicroMeter > cMaxOffsetMicroMeter)
		OffsetCalibrationDataMicroMeter = cMaxOffsetMicroMeter;
	else if (OffsetCalibrationDataMicroMeter < cMinOffsetMicroMeter)
		OffsetCalibrationDataMicroMeter = cMinOffsetMicroMeter;

	/* The offset register is 10.2 format and units are mm
	 * therefore conversion is applied by a division of
	 * 250.
	 */
	if (OffsetCalibrationDataMicroMeter >= 0) {
		encodedOffsetVal =
			OffsetCalibrationDataMicroMeter/250;
	} else {
		encodedOffsetVal =
			cOffsetRange +
			OffsetCalibrationDataMicroMeter/250;
	}

	Status = VL53L0X_WrWord(Dev,
		VL53L0X_REG_ALGO_PART_TO_PART_RANGE_OFFSET_MM,
		encodedOffsetVal);

	LOG_FUNCTION_END(Status);
	return Status;
}

VL53L0X_Error VL53L0X_get_offset_calibration_data_micro_meter(VL53L0X_DEV Dev,
		int32_t *pOffsetCalibrationDataMicroMeter)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	uint16_t RangeOffsetRegister;
	int16_t cMaxOffset = 2047;
	int16_t cOffsetRange = 4096;

	/* Note that offset has 10.2 format */

	Status = VL53L0X_RdWord(Dev,
				VL53L0X_REG_ALGO_PART_TO_PART_RANGE_OFFSET_MM,
				&RangeOffsetRegister);

	if (Status == VL53L0X_ERROR_NONE) {
		RangeOffsetRegister = (RangeOffsetRegister & 0x0fff);

		/* Apply 12 bit 2's compliment conversion */
		if (RangeOffsetRegister > cMaxOffset)
			*pOffsetCalibrationDataMicroMeter =
				(int16_t)(RangeOffsetRegister - cOffsetRange)
					* 250;
		else
			*pOffsetCalibrationDataMicroMeter =
				(int16_t)RangeOffsetRegister * 250;

	}

	return Status;
}


VL53L0X_Error VL53L0X_apply_offset_adjustment(VL53L0X_DEV Dev)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	int32_t CorrectedOffsetMicroMeters;
	int32_t CurrentOffsetMicroMeters;

	/* if we run on this function we can read all the NVM info
	 * used by the API
	 */
	Status = VL53L0X_get_info_from_device(Dev, 7);

	/* Read back current device offset */
	if (Status == VL53L0X_ERROR_NONE) {
		Status = VL53L0X_GetOffsetCalibrationDataMicroMeter(Dev,
					&CurrentOffsetMicroMeters);
	}

	/* Apply Offset Adjustment derived from 400mm measurements */
	if (Status == VL53L0X_ERROR_NONE) {

		/* Store initial device offset */
		PALDevDataSet(Dev, Part2PartOffsetNVMMicroMeter,
			CurrentOffsetMicroMeters);

		CorrectedOffsetMicroMeters = CurrentOffsetMicroMeters +
			(int32_t)PALDevDataGet(Dev,
				Part2PartOffsetAdjustmentNVMMicroMeter);

		Status = VL53L0X_SetOffsetCalibrationDataMicroMeter(Dev,
					CorrectedOffsetMicroMeters);

		/* store current, adjusted offset */
		if (Status == VL53L0X_ERROR_NONE) {
			VL53L0X_SETPARAMETERFIELD(Dev, RangeOffsetMicroMeters,
					CorrectedOffsetMicroMeters);
		}
	}

	return Status;
}

void get_next_good_spad(uint8_t goodSpadArray[], uint32_t size,
			uint32_t curr, int32_t *next)
{
	uint32_t startIndex;
	uint32_t fineOffset;
	uint32_t cSpadsPerByte = 8;
	uint32_t coarseIndex;
	uint32_t fineIndex;
	uint8_t dataByte;
	uint8_t success = 0;

	/*
	 * Starting with the current good spad, loop through the array to find
	 * the next. i.e. the next bit set in the sequence.
	 *
	 * The coarse index is the byte index of the array and the fine index is
	 * the index of the bit within each byte.
	 */

	*next = -1;

	startIndex = curr / cSpadsPerByte;
	fineOffset = curr % cSpadsPerByte;

	for (coarseIndex = startIndex; ((coarseIndex < size) && !success);
				coarseIndex++) {
		fineIndex = 0;
		dataByte = goodSpadArray[coarseIndex];

		if (coarseIndex == startIndex) {
			/* locate the bit position of the provided current
			 * spad bit before iterating
			 */
			dataByte >>= fineOffset;
			fineIndex = fineOffset;
		}

		while (fineIndex < cSpadsPerByte) {
			if ((dataByte & 0x1) == 1) {
				success = 1;
				*next = coarseIndex * cSpadsPerByte + fineIndex;
				break;
			}
			dataByte >>= 1;
			fineIndex++;
		}
	}
}


uint8_t is_aperture(uint32_t spadIndex)
{
	/*
	 * This function reports if a given spad index is an aperture SPAD by
	 * deriving the quadrant.
	 */
	uint32_t quadrant;
	uint8_t isAperture = 1;

	quadrant = spadIndex >> 6;
	if (refArrayQuadrants[quadrant] == REF_ARRAY_SPAD_0)
		isAperture = 0;

	return isAperture;
}


VL53L0X_Error enable_spad_bit(uint8_t spadArray[], uint32_t size,
	uint32_t spadIndex)
{
	VL53L0X_Error status = VL53L0X_ERROR_NONE;
	uint32_t cSpadsPerByte = 8;
	uint32_t coarseIndex;
	uint32_t fineIndex;

	coarseIndex = spadIndex / cSpadsPerByte;
	fineIndex = spadIndex % cSpadsPerByte;
	if (coarseIndex >= size)
		status = VL53L0X_ERROR_REF_SPAD_INIT;
	else
		spadArray[coarseIndex] |= (1 << fineIndex);

	return status;
}

VL53L0X_Error count_enabled_spads(uint8_t spadArray[],
		uint32_t byteCount, uint32_t maxSpads,
		uint32_t *pTotalSpadsEnabled, uint8_t *pIsAperture)
{
	VL53L0X_Error status = VL53L0X_ERROR_NONE;
	uint32_t cSpaÛ}ü¶‰žËkºwµç]¥±°‰”ÕÍ•Ñ¼4($€¨É•ÁÉ•Í•¹ÐÍÁ…‘Ì¸4($€¨¼4(%™½È€¡¥¹‘•à€ô€Àì¥¹‘•à€ðÍÁ…‘ÉÉ…åM¥é”ì¥¹‘•à¬¬¤4($%•Ø´ù…Ñ„¹MÁ…‘…Ñ„¹I•™MÁ…‘¹…‰±•Ím¥¹‘•át€ô€Àì4(4(4(%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}]É	åÑ”¡•Ø°€Áá°€ÁàÀÄ¤ì4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}]É	åÑ”¡•Ø°4($$%Y0ÔÍ0Áa}I}e95%}MA}I}9}MQIQ}=MP°€ÁàÀÀ¤ì4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}]É	åÑ”¡•Ø°4($$%Y0ÔÍ0Áa}I}e95%}MA}9U5}IEUMQ}I}MA°€ÁàÉ¤ì4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}]É	åÑ”¡•Ø°€Áá°€ÁàÀÀ¤ì4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}]É	åÑ”¡•Ø°4($$%Y0ÔÍ0Áa}I}1=	1}=9%}I}9}MQIQ}M1P°4($$%ÍÑ…ÉÑM•±•Ð¤ì4(4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}]É	åÑ”¡•Ø°4($$%Y0ÔÍ0Áa}I}A=]I}5959Q}<Å}A=]I}=I°€À¤ì4(4($¼¨A•É™½É´É•˜…±¥‰É…Ñ¥½¸€¨¼4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}Á•É™½Éµ}É•™}…±¥‰É…Ñ¥½¸¡•Ø°€™Y¡ÙM•ÑÑ¥¹Ì°4($$$™A¡…Í•…°°€À¤ì4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤ì4($$¼¨¹…‰±”5¥¹¥µÕ´9=8µAIQUIMÁ…‘Ì€¨¼4($%ÕÉÉ•¹ÑMÁ…‘%¹‘•à€ô€Àì4($%±…ÍÑMÁ…‘%¹‘•à€ôÕÉÉ•¹ÑMÁ…‘%¹‘•àì4($%¹••‘ÁÑMÁ…‘Ì€ô€Àì4($%MÑ…ÑÕÌ€ô•¹…‰±•}É•™}ÍÁ…‘Ì¡•Ø°4($$$$%¹••‘ÁÑMÁ…‘Ì°4($$$$%•Ø´ù…Ñ„¹MÁ…‘…Ñ„¹I•™½½‘MÁ…‘5…À°4($$$$%•Ø´ù…Ñ„¹MÁ…‘…Ñ„¹I•™MÁ…‘¹…‰±•Ì°4($$$$%ÍÁ…‘ÉÉ…åM¥é”°4($$$$%ÍÑ…ÉÑM•±•Ð°4($$$$%ÕÉÉ•¹ÑMÁ…‘%¹‘•à°4($$$$%µ¥¹¥µÕµMÁ…‘½Õ¹Ð°4($$$$$™±…ÍÑMÁ…‘%¹‘•à¤ì4(%ô4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤ì4($%ÕÉÉ•¹ÑMÁ…‘%¹‘•à€ô±…ÍÑMÁ…‘%¹‘•àì4(4($%MÑ…ÑÕÌ€ôÁ•É™½Éµ}É•™}Í¥¹…±}µ•…ÍÕÉ•µ•¹Ð¡•Ø°4($$$™Á•…­M¥¹…±I…Ñ•I•˜¤ì4($%¥˜€ ¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤€˜˜4($$$¡Á•…­M¥¹…±I…Ñ•I•˜€øÑ…É•ÑI•™I…Ñ”¤¤ì4($$$¼¨M¥¹…°É…Ñ”µ•…ÍÕÉ•µ•¹ÐÑ½¼¡¥ °4($$$€¨ÍÝ¥Ñ Ñ¼AIQUIMAÌ4($$$€¨¼4(4($$%™½È€¡¥¹‘•à€ô€Àì¥¹‘•à€ðÍÁ…‘ÉÉ…åM¥é”ì¥¹‘•à¬¬¤4($$$%•Ø´ù…Ñ„¹MÁ…‘…Ñ„¹I•™MÁ…‘¹…‰±•Ím¥¹‘•át€ô€Àì4(4(4($$$¼¨%¹É•µ•¹ÐÑ¼Ñ¡”™¥ÉÍÐAIQUIÍÁ…€¨¼4($$%Ý¡¥±”€ ¡¥Í}…Á•ÉÑÕÉ”¡ÍÑ…ÉÑM•±•Ð€¬ÕÉÉ•¹ÑMÁ…‘%¹‘•à¤4($$$$ôô€À¤€˜˜€¡ÕÉÉ•¹ÑMÁ…‘%¹‘•à€ðµ…áMÁ…‘½Õ¹Ð¤¤ì4($$$%ÕÉÉ•¹ÑMÁ…‘%¹‘•à¬¬ì4($$%ô4(4($$%¹••‘ÁÑMÁ…‘Ì€ô€Äì4(4($$%MÑ…ÑÕÌ€ô•¹…‰±•}É•™}ÍÁ…‘Ì¡•Ø°4($$$$%¹••‘ÁÑMÁ…‘Ì°4($$$$%•Ø´ù…Ñ„¹MÁ…‘…Ñ„¹I•™½½‘MÁ…‘5…À°4($$$$%•Ø´ù…Ñ„¹MÁ…‘…Ñ„¹I•™MÁ…‘¹…‰±•Ì°4($$$$%ÍÁ…‘ÉÉ…åM¥é”°4($$$$%ÍÑ…ÉÑM•±•Ð°4($$$$%ÕÉÉ•¹ÑMÁ…‘%¹‘•à°4($$$$%µ¥¹¥µÕµMÁ…‘½Õ¹Ð°4($$$$$™±…ÍÑMÁ…‘%¹‘•à¤ì4(4($$%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤ì4($$$%ÕÉÉ•¹ÑMÁ…‘%¹‘•à€ô±…ÍÑMÁ…‘%¹‘•àì4($$$%MÑ…ÑÕÌ€ôÁ•É™½Éµ}É•™}Í¥¹…±}µ•…ÍÕÉ•µ•¹Ð¡•Ø°4($$$$$$™Á•…­M¥¹…±I…Ñ•I•˜¤ì4(4($$$%¥˜€ ¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤€˜˜4($$$$$¡Á•…­M¥¹…±I…Ñ•I•˜€øÑ…É•ÑI•™I…Ñ”¤¤ì4($$$$$¼¨M¥¹…°É…Ñ”ÍÑ¥±°Ñ½¼¡¥ …™Ñ•È4($$$$$€¨Í•ÑÑ¥¹œÑ¡”µ¥¹¥µÕ´¹Õµ‰•È½˜4($$$$$€¨AIQUIÍÁ…‘Ì¸…¸‘¼¹¼µ½É”4($$$$$€¨Ñ¡•É•™½É”Í•ÐÑ¡”µ¥¸¹Õµ‰•È½˜4($$$$$€¨…Á•ÉÑÕÉ”ÍÁ…‘Ì…ÌÑ¡”É•ÍÕ±Ð¸4($$$$$€¨¼4($$$$%¥ÍÁ•ÉÑÕÉ•MÁ…‘Í}¥¹Ð€ô€Äì4($$$$%É•™MÁ…‘½Õ¹Ñ}¥¹Ð€ôµ¥¹¥µÕµMÁ…‘½Õ¹Ðì4($$$%ô4($$%ô4($%ô•±Í”ì4($$%¹••‘ÁÑMÁ…‘Ì€ô€Àì4($%ô4(%ô4(4(%¥˜€ ¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤€˜˜4($$¡Á•…­M¥¹…±I…Ñ•I•˜€ðÑ…É•ÑI•™I…Ñ”¤¤ì4($$¼¨ÐÑ¡¥ÌÁ½¥¹Ð°Ñ¡”µ¥¹¥µÕ´¹Õµ‰•È½˜•¥Ñ¡•È…Á•ÉÑÕÉ”4($$€¨½È¹½¸µ…Á•ÉÑÕÉ”ÍÁ…‘Ì¡…Ù”‰••¸Í•Ð¸AÉ½••Ñ¼…‘4($$€¨ÍÁ…‘Ì…¹Á•É™½É´µ•…ÍÕÉ•µ•¹ÑÌÕ¹Ñ¥°Ñ¡”Ñ…É•Ð4($$€¨É•™•É•¹”¥ÌÉ•…¡•¸4($$€¨¼4($%¥ÍÁ•ÉÑÕÉ•MÁ…‘Í}¥¹Ð€ô¹••‘ÁÑMÁ…‘Ìì4($%É•™MÁ…‘½Õ¹Ñ}¥¹Ð$ôµ¥¹¥µÕµMÁ…‘½Õ¹Ðì4(4($%µ•µÁä¡±…ÍÑMÁ…‘ÉÉ…ä°•Ø´ù…Ñ„¹MÁ…‘…Ñ„¹I•™MÁ…‘¹…‰±•Ì°4($$$%ÍÁ…‘ÉÉ…åM¥é”¤ì4($%±…ÍÑM¥¹…±I…Ñ•¥™˜€ô…‰Ì¡Á•…­M¥¹…±I…Ñ•I•˜€´4($$%Ñ…É•ÑI•™I…Ñ”¤ì4($%½µÁ±•Ñ”€ô€Àì4(4($%Ý¡¥±”€ …½µÁ±•Ñ”¤ì4($$%•Ñ}¹•áÑ}½½‘}ÍÁ… 4($$$%•Ø´ù…Ñ„¹MÁ…‘…Ñ„¹I•™½½‘MÁ…‘5…À°4($$$%ÍÁ…‘ÉÉ…åM¥é”°ÕÉÉ•¹ÑMÁ…‘%¹‘•à°4($$$$™¹•áÑ½½‘MÁ…¤ì4(4($$%¥˜€¡¹•áÑ½½‘MÁ…€ôô€´Ä¤ì4($$$%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}II=I}I}MA}%9%Pì4($$$%‰É•…¬ì4($$%ô4(4($$$¼¨…¹¹½Ð½µ‰¥¹”Á•ÉÑÕÉ”…¹9½¸µÁ•ÉÑÕÉ”ÍÁ…‘Ì°Í¼4($$$€¨•¹ÍÕÉ”Ñ¡”ÕÉÉ•¹ÐÍÁ…¥Ì½˜Ñ¡”½ÉÉ•ÐÑåÁ”¸4($$$€¨¼4($$%¥˜€¡¥Í}…Á•ÉÑÕÉ” ¡Õ¥¹ÐÌÉ}Ð¥ÍÑ…ÉÑM•±•Ð€¬¹•áÑ½½‘MÁ…¤€„ô4($$$$%¹••‘ÁÑMÁ…‘Ì¤ì4($$$$¼¨ÐÑ¡¥ÌÁ½¥¹ÐÝ”¡…Ù”•¹…‰±•Ñ¡”µ…á¥µÕ´4($$$$€¨¹Õµ‰•È½˜Á•ÉÑÕÉ”ÍÁ…‘Ì¸4($$$$€¨¼4($$$%½µÁ±•Ñ”€ô€Äì4($$$%‰É•…¬ì4($$%ô4(4($$$¡É•™MÁ…‘½Õ¹Ñ}¥¹Ð¤¬¬ì4(4($$%ÕÉÉ•¹ÑMÁ…‘%¹‘•à€ô¹•áÑ½½‘MÁ…ì4($$%MÑ…ÑÕÌ€ô•¹…‰±•}ÍÁ…‘}‰¥Ð 4($$$$%•Ø´ù…Ñ„¹MÁ…‘…Ñ„¹I•™MÁ…‘¹…‰±•Ì°4($$$$%ÍÁ…‘ÉÉ…åM¥é”°ÕÉÉ•¹ÑMÁ…‘%¹‘•à¤ì4(4($$%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤ì4($$$%ÕÉÉ•¹ÑMÁ…‘%¹‘•à¬¬ì4($$$$¼¨AÉ½••Ñ¼…ÁÁ±äÑ¡”…‘‘¥Ñ¥½¹…°ÍÁ……¹4($$$$€¨Á•É™½É´µ•…ÍÕÉ•µ•¹Ð¸4($$$$€¨¼4($$$%MÑ…ÑÕÌ€ôÍ•Ñ}É•™}ÍÁ…‘}µ…À¡•Ø°4($$$$%•Ø´ù…Ñ„¹MÁ…‘…Ñ„¹I•™MÁ…‘¹…‰±•Ì¤ì4($$%ô4(4($$%¥˜€¡MÑ…ÑÕÌ€„ôY0ÔÍ0Áa}II=I}9=9¤4($$$%‰É•…¬ì4(4($$%MÑ…ÑÕÌ€ôÁ•É™½Éµ}É•™}Í¥¹…±}µ•…ÍÕÉ•µ•¹Ð¡•Ø°4($$$$$™Á•…­M¥¹…±I…Ñ•I•˜¤ì4(4($$%¥˜€¡MÑ…ÑÕÌ€„ôY0ÔÍ0Áa}II=I}9=9¤4($$$%‰É•…¬ì4(4($$%Í¥¹…±I…Ñ•¥™˜€ô…‰Ì¡Á•…­M¥¹…±I…Ñ•I•˜€´Ñ…É•ÑI•™I…Ñ”¤ì4(4($$%¥˜€¡Á•…­M¥¹…±I…Ñ•I•˜€øÑ…É•ÑI•™I…Ñ”¤ì4($$$$¼¨M•±•ÐÑ¡”ÍÁ…µ…ÀÑ¡…ÐÁÉ½Ù¥‘•ÌÑ¡”4($$$$€¨µ•…ÍÕÉ•µ•¹Ð±½Í•ÍÐÑ¼Ñ¡”Ñ…É•ÐÉ…Ñ”°4($$$$€¨•¥Ñ¡•È…‰½Ù”½È‰•±½Ü¥Ð¸4($$$$€¨¼4($$$%¥˜€¡Í¥¹…±I…Ñ•¥™˜€ø±…ÍÑM¥¹…±I…Ñ•¥™˜¤ì4($$$$$¼¨AÉ•Ù¥½ÕÌÍÁ…µ…ÀÁÉ½‘Õ•„±½Í•È4($$$$$€¨µ•…ÍÕÉ•µ•¹Ð°Í¼¡½½Í”Ñ¡¥Ì¸4($$$$$€¨¼4($$$$%MÑ…ÑÕÌ€ôÍ•Ñ}É•™}ÍÁ…‘}µ…À¡•Ø°4($$$$$$%±…ÍÑMÁ…‘ÉÉ…ä¤ì4($$$$%µ•µÁä 4($$$$%•Ø´ù…Ñ„¹MÁ…‘…Ñ„¹I•™MÁ…‘¹…‰±•Ì°4($$$$%±…ÍÑMÁ…‘ÉÉ…ä°ÍÁ…‘ÉÉ…åM¥é”¤ì4(4($$$$$¡É•™MÁ…‘½Õ¹Ñ}¥¹Ð¤´´ì4($$$%ô4($$$%½µÁ±•Ñ”€ô€Äì4($$%ô•±Í”ì4($$$$¼¨½¹Ñ¥¹Õ”Ñ¼…‘ÍÁ…‘Ì€¨¼4($$$%±…ÍÑM¥¹…±I…Ñ•¥™˜€ôÍ¥¹…±I…Ñ•¥™˜ì4($$$%µ•µÁä¡±…ÍÑMÁ…‘ÉÉ…ä°4($$$$%•Ø´ù…Ñ„¹MÁ…‘…Ñ„¹I•™MÁ…‘¹…‰±•Ì°4($$$$%ÍÁ…‘ÉÉ…åM¥é”¤ì4($$%ô4(4($%ô€¼¨Ý¡¥±”€¨¼4(%ô4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤ì4($$©É•™MÁ…‘½Õ¹Ð€ôÉ•™MÁ…‘½Õ¹Ñ}¥¹Ðì4($$©¥ÍÁ•ÉÑÕÉ•MÁ…‘Ì€ô¥ÍÁ•ÉÑÕÉ•MÁ…‘Í}¥¹Ðì4(4($%Y0ÔÍ0Áa}MQY%MA%%AI5QH¡•Ø°I•™MÁ…‘Í%¹¥Ñ¥…±¥Í•°€Ä¤ì4($%Y0ÔÍ0Áa}MQY%MA%%AI5QH¡•Ø°4($$%I•™•É•¹•MÁ…‘½Õ¹Ð°€¡Õ¥¹Ðá}Ð¤ ©É•™MÁ…‘½Õ¹Ð¤¤ì4($%Y0ÔÍ0Áa}MQY%MA%%AI5QH¡•Ø°4($$%I•™•É•¹•MÁ…‘QåÁ”°€©¥ÍÁ•ÉÑÕÉ•MÁ…‘Ì¤ì4(%ô4(4(%É•ÑÕÉ¸MÑ…ÑÕÌì4)ô4(4)Y0ÔÍ0Áa}ÉÉ½ÈY0ÔÍ0Áa}Í•Ñ}É•™•É•¹•}ÍÁ…‘Ì¡Y0ÔÍ0Áa}X•Ø°4($$$$Õ¥¹ÐÌÉ}Ð½Õ¹Ð°Õ¥¹Ðá}Ð¥ÍÁ•ÉÑÕÉ•MÁ…‘Ì¤4)ì4(%Y0ÔÍ0Áa}ÉÉ½ÈMÑ…ÑÕÌ€ôY0ÔÍ0Áa}II=I}9=9ì4(%Õ¥¹ÐÌÉ}ÐÕÉÉ•¹ÑMÁ…‘%¹‘•à€ô€Àì4(%Õ¥¹Ðá}ÐÍÑ…ÉÑM•±•Ð€ô€ÁáÐì4(%Õ¥¹ÐÌÉ}ÐÍÁ…‘ÉÉ…åM¥é”€ô€Øì4(%Õ¥¹ÐÌÉ}Ðµ…áMÁ…‘½Õ¹Ð€ô€ÐÐì4(%Õ¥¹ÐÌÉ}Ð±…ÍÑMÁ…‘%¹‘•àì4(%Õ¥¹ÐÌÉ}Ð¥¹‘•àì4(4($¼¨4($€¨Q¡¥Ì™Õ¹Ñ¥½¸…ÁÁ±¥•Ì„É•ÅÕ•ÍÑ•¹Õµ‰•È½˜É•™•É•¹”ÍÁ…‘Ì°•¥Ñ¡•È4($€¨…Á•ÉÑÕÉ”½È4($€¨¹½¸µ…Á•ÉÑÕÉ”°…ÌÉ•ÅÕ•ÍÑ•¸4($€¨Q¡”½½ÍÁ…µ…ÀÝ¥±°‰”…ÁÁ±¥•¸4($€¨¼4(4(%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}]É	åÑ”¡•Ø°€Áá°€ÁàÀÄ¤ì4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}]É	åÑ”¡•Ø°4($$%Y0ÔÍ0Áa}I}e95%}MA}I}9}MQIQ}=MP°€ÁàÀÀ¤ì4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}]É	åÑ”¡•Ø°4($$%Y0ÔÍ0Áa}I}e95%}MA}9U5}IEUMQ}I}MA°€ÁàÉ¤ì4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}]É	åÑ”¡•Ø°€Áá°€ÁàÀÀ¤ì4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}]É	åÑ”¡•Ø°4($$%Y0ÔÍ0Áa}I}1=	1}=9%}I}9}MQIQ}M1P°4($$%ÍÑ…ÉÑM•±•Ð¤ì4(4(%™½È€¡¥¹‘•à€ô€Àì¥¹‘•à€ðÍÁ…‘ÉÉ…åM¥é”ì¥¹‘•à¬¬¤4($%•Ø´ù…Ñ„¹MÁ…‘…Ñ„¹I•™MÁ…‘¹…‰±•Ím¥¹‘•át€ô€Àì4(4(%¥˜€¡¥ÍÁ•ÉÑÕÉ•MÁ…‘Ì¤ì4($$¼¨%¹É•µ•¹ÐÑ¼Ñ¡”™¥ÉÍÐAIQUIÍÁ…€¨¼4($%Ý¡¥±”€ ¡¥Í}…Á•ÉÑÕÉ”¡ÍÑ…ÉÑM•±•Ð€¬ÕÉÉ•¹ÑMÁ…‘%¹‘•à¤€ôô€À¤€˜˜4($$$€€¡ÕÉÉ•¹ÑMÁ…‘%¹‘•à€ðµ…áMÁ…‘½Õ¹Ð¤¤ì4($$%ÕÉÉ•¹ÑMÁ…‘%¹‘•à¬¬ì4($%ô4(%ô4(%MÑ…ÑÕÌ€ô•¹…‰±•}É•™}ÍÁ…‘Ì¡•Ø°4($$$%¥ÍÁ•ÉÑÕÉ•MÁ…‘Ì°4($$$%•Ø´ù…Ñ„¹MÁ…‘…Ñ„¹I•™½½‘MÁ…‘5…À°4($$$%•Ø´ù…Ñ„¹MÁ…‘…Ñ„¹I•™MÁ…‘¹…‰±•Ì°4($$$%ÍÁ…‘ÉÉ…åM¥é”°4($$$%ÍÑ…ÉÑM•±•Ð°4($$$%ÕÉÉ•¹ÑMÁ…‘%¹‘•à°4($$$%½Õ¹Ð°4($$$$™±…ÍÑMÁ…‘%¹‘•à¤ì4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤ì4($%Y0ÔÍ0Áa}MQY%MA%%AI5QH¡•Ø°I•™MÁ…‘Í%¹¥Ñ¥…±¥Í•°€Ä¤ì4($%Y0ÔÍ0Áa}MQY%MA%%AI5QH¡•Ø°4($$%I•™•É•¹•MÁ…‘½Õ¹Ð°€¡Õ¥¹Ðá}Ð¤¡½Õ¹Ð¤¤ì4($%Y0ÔÍ0Áa}MQY%MA%%AI5QH¡•Ø°4($$%I•™•É•¹•MÁ…‘QåÁ”°¥ÍÁ•ÉÑÕÉ•MÁ…‘Ì¤ì4(%ô4(4(%É•ÑÕÉ¸MÑ…ÑÕÌì4)ô4(4)Y0ÔÍ0Áa}ÉÉ½ÈY0ÔÍ0Áa}•Ñ}É•™•É•¹•}ÍÁ…‘Ì¡Y0ÔÍ0Áa}X•Ø°4($$%Õ¥¹ÐÌÉ}Ð€©ÁMÁ…‘½Õ¹Ð°Õ¥¹Ðá}Ð€©Á%ÍÁ•ÉÑÕÉ•MÁ…‘Ì¤4)ì4(%Y0ÔÍ0Áa}ÉÉ½ÈMÑ…ÑÕÌ€ôY0ÔÍ0Áa}II=I}9=9ì4(%Õ¥¹Ðá}ÐÉ•™MÁ…‘Í%¹¥Ñ¥…±¥Í•ì4(%Õ¥¹Ðá}ÐÉ•™MÁ…‘ÉÉ…ålÙtì4(%Õ¥¹ÐÌÉ}Ð5…áMÁ…‘½Õ¹Ð€ô€ÐÐì4(%Õ¥¹ÐÌÉ}ÐMÁ…‘ÉÉ…åM¥é”€ô€Øì4(%Õ¥¹ÐÌÉ}ÐÍÁ…‘Í¹…‰±•ì4(%Õ¥¹Ðá}Ð¥ÍÁ•ÉÑÕÉ•MÁ…‘Ì€ô€Àì4(4(%É•™MÁ…‘Í%¹¥Ñ¥…±¥Í•€ôY0ÔÍ0Áa}QY%MA%%AI5QH¡•Ø°4($$$$%I•™MÁ…‘Í%¹¥Ñ¥…±¥Í•¤ì4(4(%¥˜€¡É•™MÁ…‘Í%¹¥Ñ¥…±¥Í•€ôô€Ä¤ì4(4($$©ÁMÁ…‘½Õ¹Ð€ô€¡Õ¥¹ÐÌÉ}Ð¥Y0ÔÍ0Áa}QY%MA%%AI5QH¡•Ø°4($$%I•™•É•¹•MÁ…‘½Õ¹Ð¤ì4($$©Á%ÍÁ•ÉÑÕÉ•MÁ…‘Ì€ôY0ÔÍ0Áa}QY%MA%%AI5QH¡•Ø°4($$%I•™•É•¹•MÁ…‘QåÁ”¤ì4(%ô•±Í”ì4(4($$¼¨½‰Ñ…¥¸ÍÁ…¥¹™¼™É½´‘•Ù¥”¸¨¼4($%MÑ…ÑÕÌ€ô•Ñ}É•™}ÍÁ…‘}µ…À¡•Ø°É•™MÁ…‘ÉÉ…ä¤ì4(4($%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤ì4($$$¼¨½Õ¹Ð•¹…‰±•ÍÁ…‘ÌÝ¥Ñ¡¥¸ÍÁ…µ…À…ÉÉ…ä…¹4($$$€¨‘•Ñ•Éµ¥¹”¥˜Á•ÉÑÕÉ”½È9½¸µÁ•ÉÑÕÉ”¸4($$$€¨¼4($$%MÑ…ÑÕÌ€ô½Õ¹Ñ}•¹…‰±•‘}ÍÁ…‘Ì¡É•™MÁ…‘ÉÉ…ä°4($$$$$$%MÁ…‘ÉÉ…åM¥é”°4($$$$$$%5…áMÁ…‘½Õ¹Ð°4($$$$$$$™ÍÁ…‘Í¹…‰±•°4($$$$$$$™¥ÍÁ•ÉÑÕÉ•MÁ…‘Ì¤ì4(4($$%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤ì4(4($$$$©ÁMÁ…‘½Õ¹Ð€ôÍÁ…‘Í¹…‰±•ì4($$$$©Á%ÍÁ•ÉÑÕÉ•MÁ…‘Ì€ô¥ÍÁ•ÉÑÕÉ•MÁ…‘Ìì4(4($$$%Y0ÔÍ0Áa}MQY%MA%%AI5QH¡•Ø°4($$$$%I•™MÁ…‘Í%¹¥Ñ¥…±¥Í•°€Ä¤ì4($$$%Y0ÔÍ0Áa}MQY%MA%%AI5QH¡•Ø°4($$$$%I•™•É•¹•MÁ…‘½Õ¹Ð°4($$$$$¡Õ¥¹Ðá}Ð¥ÍÁ…‘Í¹…‰±•¤ì4($$$%Y0ÔÍ0Áa}MQY%MA%%AI5QH¡•Ø°4($$$$%I•™•É•¹•MÁ…‘QåÁ”°¥ÍÁ•ÉÑÕÉ•MÁ…‘Ì¤ì4($$%ô4($%ô4(%ô4(4(%É•ÑÕÉ¸MÑ…ÑÕÌì4)ô4(4(4)Y0ÔÍ0Áa}ÉÉ½ÈY0ÔÍ0Áa}Á•É™½Éµ}Í¥¹±•}É•™}…±¥‰É…Ñ¥½¸¡Y0ÔÍ0Áa}X•Ø°4($%Õ¥¹Ðá}ÐÙ¡Ù}¥¹¥Ñ}‰åÑ”¤4)ì4(%Y0ÔÍ0Áa}ÉÉ½ÈMÑ…ÑÕÌ€ôY0ÔÍ0Áa}II=I}9=9ì4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}]É	åÑ”¡•Ø°Y0ÔÍ0Áa}I}MeMI9}MQIP°4($$$%Y0ÔÍ0Áa}I}MeMI9}5=}MQIQ}MQ=@ð4($$$%Ù¡Ù}¥¹¥Ñ}‰åÑ”¤ì4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}µ•…ÍÕÉ•µ•¹Ñ}Á½±±}™½É}½µÁ±•Ñ¥½¸¡•Ø¤ì4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}±•…É%¹Ñ•ÉÉÕÁÑ5…Í¬¡•Ø°€À¤ì4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}]É	åÑ”¡•Ø°Y0ÔÍ0Áa}I}MeMI9}MQIP°€ÁàÀÀ¤ì4(4(%É•ÑÕÉ¸MÑ…ÑÕÌì4)ô4(4(4)Y0ÔÍ0Áa}ÉÉ½ÈY0ÔÍ0Áa}É•™}…±¥‰É…Ñ¥½¹}¥¼¡Y0ÔÍ0Áa}X•Ø°4(%Õ¥¹Ðá}ÐÉ•…‘}¹½Ñ}ÝÉ¥Ñ”°4(%Õ¥¹Ðá}ÐY¡ÙM•ÑÑ¥¹Ì°Õ¥¹Ðá}ÐA¡…Í•…°°4(%Õ¥¹Ðá}Ð€©ÁY¡ÙM•ÑÑ¥¹Ì°Õ¥¹Ðá}Ð€©ÁA¡…Í•…°°4(%½¹ÍÐÕ¥¹Ðá}ÐÙ¡Ù}•¹…‰±”°½¹ÍÐÕ¥¹Ðá}ÐÁ¡…Í•}•¹…‰±”¤4)ì4(%Y0ÔÍ0Áa}ÉÉ½ÈMÑ…ÑÕÌ€ôY0ÔÍ0Áa}II=I}9=9ì4(%Õ¥¹Ðá}ÐA¡…Í•…±¥¹Ð€ô€Àì4(4($¼¨I•…Y!X™É½´‘•Ù¥”€¨¼4(%MÑ…ÑÕÌðôY0ÔÍ0Áa}]É	åÑ”¡•Ø°€Áá°€ÁàÀÄ¤ì4(%MÑ…ÑÕÌðôY0ÔÍ0Áa}]É	åÑ”¡•Ø°€ÁàÀÀ°€ÁàÀÀ¤ì4(%MÑ…ÑÕÌðôY0ÔÍ0Áa}]É	åÑ”¡•Ø°€Áá°€ÁàÀÀ¤ì4(4(%¥˜€¡É•…‘}¹½Ñ}ÝÉ¥Ñ”¤ì4($%¥˜€¡Ù¡Ù}•¹…‰±”¤4($$%MÑ…ÑÕÌðôY0ÔÍ0Áa}I‘	åÑ”¡•Ø°€Áá°ÁY¡ÙM•ÑÑ¥¹Ì¤ì4($%¥˜€¡Á¡…Í•}•¹…‰±”¤4($$%MÑ…ÑÕÌðôY0ÔÍ0Áa}I‘	åÑ”¡•Ø°€Áá°€™A¡…Í•…±¥¹Ð¤ì4(%ô•±Í”ì4($%¥˜€¡Ù¡Ù}•¹…‰±”¤4($$%MÑ…ÑÕÌðôY0ÔÍ0Áa}]É	åÑ”¡•Ø°€Áá°Y¡ÙM•ÑÑ¥¹Ì¤ì4($%¥˜€¡Á¡…Í•}•¹…‰±”¤4($$%MÑ…ÑÕÌðôY0ÔÍ0Áa}UÁ‘…Ñ•	åÑ”¡•Ø°€Áá°€ÁààÀ°A¡…Í•…°¤ì4(%ô4(4(%MÑ…ÑÕÌðôY0ÔÍ0Áa}]É	åÑ”¡•Ø°€Áá°€ÁàÀÄ¤ì4(%MÑ…ÑÕÌðôY0ÔÍ0Áa}]É	åÑ”¡•Ø°€ÁàÀÀ°€ÁàÀÄ¤ì4(%MÑ…ÑÕÌðôY0ÔÍ0Áa}]É	åÑ”¡•Ø°€Áá°€ÁàÀÀ¤ì4(4($©ÁA¡…Í•…°€ô€¡Õ¥¹Ðá}Ð¤¡A¡…Í•…±¥¹Ð˜Áá¤ì4(4(%É•ÑÕÉ¸MÑ…ÑÕÌì4)ô4(4(4)Y0ÔÍ0Áa}ÉÉ½ÈY0ÔÍ0Áa}Á•É™½Éµ}Ù¡Ù}…±¥‰É…Ñ¥½¸¡Y0ÔÍ0Áa}X•Ø°4(%Õ¥¹Ðá}Ð€©ÁY¡ÙM•ÑÑ¥¹Ì°½¹ÍÐÕ¥¹Ðá}Ð•Ñ}‘…Ñ…}•¹…‰±”°4(%½¹ÍÐÕ¥¹Ðá}ÐÉ•ÍÑ½É•}½¹™¥œ¤4)ì4(%Y0ÔÍ0Áa}ÉÉ½ÈMÑ…ÑÕÌ€ôY0ÔÍ0Áa}II=I}9=9ì4(%Õ¥¹Ðá}ÐM•ÅÕ•¹•½¹™¥œ€ô€Àì4(%Õ¥¹Ðá}ÐY¡ÙM•ÑÑ¥¹Ì€ô€Àì4(%Õ¥¹Ðá}ÐA¡…Í•…°€ô€Àì4(%Õ¥¹Ðá}ÐA¡…Í•…±%¹Ð€ô€Àì4(4($¼¨ÍÑ½É”Ñ¡”Ù…±Õ”½˜Ñ¡”Í•ÅÕ•¹”½¹™¥œ°4($€¨Ñ¡¥ÌÝ¥±°‰”É•Í•Ð‰•™½É”Ñ¡”•¹½˜Ñ¡”™Õ¹Ñ¥½¸4($€¨¼4(4(%¥˜€¡É•ÍÑ½É•}½¹™¥œ¤4($%M•ÅÕ•¹•½¹™¥œ€ôA1•Ù…Ñ…•Ð¡•Ø°M•ÅÕ•¹•½¹™¥œ¤ì4(4($¼¨IÕ¸Y!X€¨¼4(%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}]É	åÑ”¡•Ø°Y0ÔÍ0Áa}I}MeMQ5}MEU9}=9%°€ÁàÀÄ¤ì4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}Á•É™½Éµ}Í¥¹±•}É•™}…±¥‰É…Ñ¥½¸¡•Ø°€ÁàÐÀ¤ì4(4($¼¨I•…Y!X™É½´‘•Ù¥”€¨¼4(%¥˜€ ¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤€˜˜€¡•Ñ}‘…Ñ…}•¹…‰±”€ôô€Ä¤¤ì4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}É•™}…±¥‰É…Ñ¥½¹}¥¼¡•Ø°€Ä°4($$%Y¡ÙM•ÑÑ¥¹Ì°A¡…Í•…°°€¼¨9½ÐÕÍ•¡•É”€¨¼4($$%ÁY¡ÙM•ÑÑ¥¹Ì°€™A¡…Í•…±%¹Ð°4($$$Ä°€À¤ì4(%ô•±Í”4($$©ÁY¡ÙM•ÑÑ¥¹Ì€ô€Àì4(4(4(%¥˜€ ¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤€˜˜É•ÍÑ½É•}½¹™¥œ¤ì4($$¼¨É•ÍÑ½É”Ñ¡”ÁÉ•Ù¥½ÕÌM•ÅÕ•¹”½¹™¥œ€¨¼4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}]É	åÑ”¡•Ø°Y0ÔÍ0Áa}I}MeMQ5}MEU9}=9%°4($$$%M•ÅÕ•¹•½¹™¥œ¤ì4($%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($$%A1•Ù…Ñ…M•Ð¡•Ø°M•ÅÕ•¹•½¹™¥œ°M•ÅÕ•¹•½¹™¥œ¤ì4(4(%ô4(4(%É•ÑÕÉ¸MÑ…ÑÕÌì4)ô4(4)Y0ÔÍ0Áa}ÉÉ½ÈY0ÔÍ0Áa}Á•É™½Éµ}Á¡…Í•}…±¥‰É…Ñ¥½¸¡Y0ÔÍ0Áa}X•Ø°4(%Õ¥¹Ðá}Ð€©ÁA¡…Í•…°°½¹ÍÐÕ¥¹Ðá}Ð•Ñ}‘…Ñ…}•¹…‰±”°4(%½¹ÍÐÕ¥¹Ðá}ÐÉ•ÍÑ½É•}½¹™¥œ¤4)ì4(%Y0ÔÍ0Áa}ÉÉ½ÈMÑ…ÑÕÌ€ôY0ÔÍ0Áa}II=I}9=9ì4(%Õ¥¹Ðá}ÐM•ÅÕ•¹•½¹™¥œ€ô€Àì4(%Õ¥¹Ðá}ÐY¡ÙM•ÑÑ¥¹Ì€ô€Àì4(%Õ¥¹Ðá}ÐA¡…Í•…°€ô€Àì4(%Õ¥¹Ðá}ÐY¡ÙM•ÑÑ¥¹Í¥¹Ðì4(4($¼¨ÍÑ½É”Ñ¡”Ù…±Õ”½˜Ñ¡”Í•ÅÕ•¹”½¹™¥œ°4($€¨Ñ¡¥ÌÝ¥±°‰”É•Í•Ð‰•™½É”Ñ¡”•¹½˜Ñ¡”™Õ¹Ñ¥½¸4($€¨¼4(4(%¥˜€¡É•ÍÑ½É•}½¹™¥œ¤4($%M•ÅÕ•¹•½¹™¥œ€ôA1•Ù…Ñ…•Ð¡•Ø°M•ÅÕ•¹•½¹™¥œ¤ì4(4($¼¨IÕ¸A¡…Í•…°€¨¼4(%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}]É	åÑ”¡•Ø°Y0ÔÍ0Áa}I}MeMQ5}MEU9}=9%°€ÁàÀÈ¤ì4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}Á•É™½Éµ}Í¥¹±•}É•™}…±¥‰É…Ñ¥½¸¡•Ø°€ÁàÀ¤ì4(4($¼¨I•…A¡…Í•…°™É½´‘•Ù¥”€¨¼4(%¥˜€ ¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤€˜˜€¡•Ñ}‘…Ñ…}•¹…‰±”€ôô€Ä¤¤ì4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}É•™}…±¥‰É…Ñ¥½¹}¥¼¡•Ø°€Ä°4($$%Y¡ÙM•ÑÑ¥¹Ì°A¡…Í•…°°€¼¨9½ÐÕÍ•¡•É”€¨¼4($$$™Y¡ÙM•ÑÑ¥¹Í¥¹Ð°ÁA¡…Í•…°°4($$$À°€Ä¤ì4(%ô•±Í”4($$©ÁA¡…Í•…°€ô€Àì4(4(4(%¥˜€ ¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤€˜˜É•ÍÑ½É•}½¹™¥œ¤ì4($$¼¨É•ÍÑ½É”Ñ¡”ÁÉ•Ù¥½ÕÌM•ÅÕ•¹”½¹™¥œ€¨¼4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}]É	åÑ”¡•Ø°Y0ÔÍ0Áa}I}MeMQ5}MEU9}=9%°4($$$%M•ÅÕ•¹•½¹™¥œ¤ì4($%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($$%A1•Ù…Ñ…M•Ð¡•Ø°M•ÅÕ•¹•½¹™¥œ°M•ÅÕ•¹•½¹™¥œ¤ì4(4(%ô4(4(%É•ÑÕÉ¸MÑ…ÑÕÌì4)ô4(4)Y0ÔÍ0Áa}ÉÉ½ÈY0ÔÍ0Áa}Á•É™½Éµ}É•™}…±¥‰É…Ñ¥½¸¡Y0ÔÍ0Áa}X•Ø°4(%Õ¥¹Ðá}Ð€©ÁY¡ÙM•ÑÑ¥¹Ì°Õ¥¹Ðá}Ð€©ÁA¡…Í•…°°Õ¥¹Ðá}Ð•Ñ}‘…Ñ…}•¹…‰±”¤4)ì4(%Y0ÔÍ0Áa}ÉÉ½ÈMÑ…ÑÕÌ€ôY0ÔÍ0Áa}II=I}9=9ì4(%Õ¥¹Ðá}ÐM•ÅÕ•¹•½¹™¥œ€ô€Àì4(4($¼¨ÍÑ½É”Ñ¡”Ù…±Õ”½˜Ñ¡”Í•ÅÕ•¹”½¹™¥œ°4($€¨Ñ¡¥ÌÝ¥±°‰”É•Í•Ð‰•™½É”Ñ¡”•¹½˜Ñ¡”™Õ¹Ñ¥½¸4($€¨¼4(4(%M•ÅÕ•¹•½¹™¥œ€ôA1•Ù…Ñ…•Ð¡•Ø°M•ÅÕ•¹•½¹™¥œ¤ì4(4($¼¨%¸Ñ¡”™½±±½Ý¥¹œ™Õ¹Ñ¥½¸Ý”‘½¸ÐÍ…Ù”Ñ¡”½¹™¥œÑ¼½ÁÑ¥µ¥é”4($€¨ÝÉ¥Ñ•Ì½¸‘•Ù¥”¸½¹™¥œ¥ÌÍ…Ù•…¹É•ÍÑ½É•½¹±ä½¹”¸4($€¨¼4(%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}Á•É™½Éµ}Ù¡Ù}…±¥‰É…Ñ¥½¸ 4($$%•Ø°ÁY¡ÙM•ÑÑ¥¹Ì°•Ñ}‘…Ñ…}•¹…‰±”°€À¤ì4(4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}Á•É™½Éµ}Á¡…Í•}…±¥‰É…Ñ¥½¸ 4($$%•Ø°ÁA¡…Í•…°°•Ñ}‘…Ñ…}•¹…‰±”°€À¤ì4(4(4(%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤ì4($$¼¨É•ÍÑ½É”Ñ¡”ÁÉ•Ù¥½ÕÌM•ÅÕ•¹”½¹™¥œ€¨¼4($%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}]É	åÑ”¡•Ø°Y0ÔÍ0Áa}I}MeMQ5}MEU9}=9%°4($$$%M•ÅÕ•¹•½¹™¥œ¤ì4($%¥˜€¡MÑ…ÑÕÌ€ôôY0ÔÍ0Áa}II=I}9=9¤4($$%A1•Ù…Ñ…M•Ð¡•Ø°M•ÅÕ•¹•½¹™¥œ°M•ÅÕ•¹•½¹™¥œ¤ì4(4(%ô4(4(%É•ÑÕÉ¸MÑ…ÑÕÌì4)ô4(4)Y0ÔÍ0Áa}ÉÉ½ÈY0ÔÍ0Áa}Í•Ñ}É•™}…±¥‰É…Ñ¥½¸¡Y0ÔÍ0Áa}X•Ø°4($%Õ¥¹Ðá}ÐY¡ÙM•ÑÑ¥¹Ì°Õ¥¹Ðá}ÐA¡…Í•…°¤4)ì4(%Y0ÔÍ0Áa}ÉÉ½ÈMÑ…ÑÕÌ€ôY0ÔÍ0Áa}II=I}9=9ì4(%Õ¥¹Ðá}ÐÁY¡ÙM•ÑÑ¥¹Ìì4(%Õ¥¹Ðá}ÐÁA¡…Í•…°ì4(4(%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}É•™}…±¥‰É…Ñ¥½¹}¥¼¡•Ø°€À°4($%Y¡ÙM•ÑÑ¥¹Ì°A¡…Í•…°°4($$™ÁY¡ÙM•ÑÑ¥¹Ì°€™ÁA¡…Í•…°°4($$Ä°€Ä¤ì4(4(%É•ÑÕÉ¸MÑ…ÑÕÌì4)ô4(4)Y0ÔÍ0Áa}ÉÉ½ÈY0ÔÍ0Áa}•Ñ}É•™}…±¥‰É…Ñ¥½¸¡Y0ÔÍ0Áa}X•Ø°4($%Õ¥¹Ðá}Ð€©ÁY¡ÙM•ÑÑ¥¹Ì°Õ¥¹Ðá}Ð€©ÁA¡…Í•…°¤4)ì4(%Y0ÔÍ0Áa}ÉÉ½ÈMÑ…ÑÕÌ€ôY0ÔÍ0Áa}II=I}9=9ì4(%Õ¥¹Ðá}ÐY¡ÙM•ÑÑ¥¹Ì€ô€Àì4(%Õ¥¹Ðá}ÐA¡…Í•…°€ô€Àì4(4(%MÑ…ÑÕÌ€ôY0ÔÍ0Áa}É•™}…±¥‰É…Ñ¥½¹}¥¼¡•Ø°€Ä°4($%Y¡ÙM•ÑÑ¥¹Ì°A¡…Í•…°°4($%ÁY¡ÙM•ÑÑ¥¹Ì°ÁA¡…Í•…°°4($$Ä°€Ä¤ì4(4(%É•ÑÕÉ¸MÑ…ÑÕÌì4)ô4(