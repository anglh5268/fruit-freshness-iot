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

VL53L0X_Error VL53L0X_reverse_bytes(uint8_t *data, uint32_t size)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	uint8_t tempData;
	uint32_t mirrorIndex;
	uint32_t middle = size/2;
	uint32_t index;

	for (index = 0; index < middle; index++) {
		mirrorIndex		 = size - index - 1;
		tempData		 = data[index];
		data[index]		 = data[mirrorIndex];
		data[mirrorIndex] = tempData;
	}
	return Status;
}

VL53L0X_Error VL53L0X_measurement_poll_for_completion(VL53L0X_DEV Dev)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	uint8_t NewDataReady = 0;
	uint32_t LoopNb;

	LOG_FUNCTION_START("");

	LoopNb = 0;

	do {
		Status = VL53L0X_GetMeasurementDataReady(Dev, &NewDataReady);
		if (Status != 0)
			break; /* the error is set */

		if (NewDataReady == 1)
			break; /* done note that status == 0 */

		LoopNb++;
		if (LoopNb >= VL53L0X_DEFAULT_MAX_LOOP) {
			Status = VL53L0X_ERROR_TIME_OUT;
			break;
		}

		VL53L0X_PollingDelay(Dev);
	} while (1);

	LOG_FUNCTION_END(Status);

	return Status;
}


uint8_t VL53L0X_decode_vcsel_period(uint8_t vcsel_period_reg)
{
	/*!
	 * Converts the encoded VCSEL period register value into the real
	 * period in PLL clocks
	 */

	uint8_t vcsel_period_pclks = 0;

	vcsel_period_pclks = (vcsel_period_reg + 1) << 1;

	return vcsel_period_pclks;
}

uint8_t VL53L0X_encode_vcsel_period(uint8_t vcsel_period_pclks)
{
	/*!
	 * Converts the encoded VCSEL period register value into the real period
	 * in PLL clocks
	 */

	uint8_t vcsel_period_reg = 0;

	vcsel_period_reg = (vcsel_period_pclks >> 1) - 1;

	return vcsel_period_reg;
}


uint32_t VL53L0X_isqrt(uint32_t num)
{
	/*
	 * Implements an integer square root
	 *
	 * From: http://en.wikipedia.org/wiki/Methods_of_computing_square_roots
	 */

	uint32_t  res = 0;
	uint32_t  bit = 1 << 30;
	/* The second-to-top bit is set:
	 *	1 << 14 for 16-bits, 1 << 30 for 32 bits
	 */

	 /* "bit" starts at the highest power of four <= the argument. */
	while (bit > num)
		bit >>= 2;


	while (bit != 0) {
		if (num >= res + bit) {
			num -= res + bit;
			res = (res >> 1) + bit;
		} else
			res >>= 1;

		bit >>= 2;
	}

	return res;
}


uint32_t VL53L0X_quadrature_sum(uint32_t a, uint32_t b)
{
	/*
	 * Implements a quadrature sum
	 *
	 * rea = sqrt(a^2 + b^2)
	 *
	 * Trap overflow case max input value is 65535 (16-bit value)
	 * as internal calc are 32-bit wide
	 *
	 * If overflow then seta output to maximum
	 */
	uint32_t  res = 0;

	if (a > 65535 || b > 65535)
		res = 65535;
	else
		res = VL53L0X_isqrt(a * a + b * b);

	return res;
}


VL53L0X_Error VL53L0X_device_read_strobe(VL53L0X_DEV Dev)
{
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	uint8_t strobe;
	uint32_t LoopNb;

	LOG_FUNCTION_START("");

	Status |= VL53L0X_WrByte(Dev, 0x83, 0x00);

	/* polling
	 * use timeout to avoid deadlock
	 */
	if (Status == VL53L0X_ERROR_NONE) {
		LoopNb = 0;
		do {
			Status = VL53L0X_RdByte(Dev, 0x83, &strobe);
			if ((strobe != 0x00) || Status != VL53L0X_ERROR_NONE)
				break;

			LoopNb = LoopNb + 1;
		} while (LoopNb < VL53L0X_DEFAULT_MAX_LOOP);

		if (LoopNb >= VL53L0X_DEFAULT_MAX_LOOP)
			Status = VL53L0X_ERROR_TIME_OUT;

	}

	Status |= VL53L0X_WrByte(Dev, 0x83, 0x01);

	LOG_FUNCTION_END(Status);
	return Status;

}

VL53L0X_Error VL53L0X_get_info_from_device(VL53L0X_DEV Dev, uint8_t option)
{

	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	uint8_t byte;
	uint32_t TmpDWord;
	uint8_t ModuleId;
	uint8_t Revision;
	uint8_t ReferenceSpadCount = 0;
	uint8_t ReferenceSpadType = 0;
	uint32_t PartUIDUpper = 0;
	uint32_t PartUIDLower = 0;
	uint32_t OffsetFixed1104_mm = 0;
	int16_t OffsetMicroMeters = 0;
	uint32_t DistMeasTgtFixed1104_mm = 400 << 4;
	uint32_t DistMeasFixed1104_400_mm = 0;
	uint32_t SignalRateMeasFixed1104_400_mm = 0;
	char ProductId[19];
	char *ProductId_tmp;
	uint8_t ReadDataFromDeviceDone;
	FixPoint1616_t SignalRateMeasFixed400mmFix = 0;
	uint8_t NvmRefGoodSpadMap[VL53L0X_REF_SPAD_BUFFER_SIZE];
	int i;


	LOG_FUNCTION_START("");

	ReadDataFromDeviceDone = VL53L0X_GETDEVICESPECIFICPARAMETER(Dev,
			ReadDataFromDeviceDone);

	/* This access is done only once after that a GetDeviceInfo or
	 * datainit is done
	 */
	if (ReadDataFromDeviceDone != 7) {

		Status |= VL53L0X_WrByte(Dev, 0x80, 0x01);
		Status |= VL53L0X_WrByte(Dev, 0xFF, 0x01);
		Status |= VL53L0X_WrByte(Dev, 0x00, 0x00);

		Status |= VL53L0X_WrByte(Dev, 0xFF, 0x06);
		Status |= VL53L0X_RdByte(Dev, 0x83, &byte);
		Status |= VL53L0X_WrByte(Dev, 0x83, byte|4);
		Status |= VL53L0X_WrByte(Dev, 0xFF, 0x07);
		Status |= VL53L0X_WrByte(Dev, 0x81, 0x01);

		Status |= VL53L0X_PollingDelay(Dev);

		Status |= VL53L0X_WrByte(Dev, 0x80, 0x01);

		if (((option & 1) == 1) &&
			((ReadDataFromDeviceDone & 1) == 0)) {
			Status |= VL53L0X_WrByte(Dev, 0x94, 0x6b);
			Status |= VL53L0X_device_read_strobe(Dev);
			Status |= VL53L0X_RdDWord(Dev, 0x90, &TmpDWord);

			ReferenceSpadCount = (uint8_t)((TmpDWord >> 8) & 0x07f);
			ReferenceSpadType  = (uint8_t)((TmpDWord >> 15) & 0x01);

			Status |= VL53L0X_WrByte(Dev, 0x94, 0x24);
			Status |= VL53L0X_device_read_strobe(Dev);
			Status |= VL53L0X_RdDWord(Dev, 0x90, &TmpDWord);


			NvmRefGoodSpadMap[0] = (uint8_t)((TmpDWord >> 24)
				& 0xff);
			NvmRefGoodSpadMap[1] = (uint8_t)((TmpDWord >> 16)
				& 0xff);
			NvmRefGoodSpadMap[2] = (uint8_t)((TmpDWord >> 8)
				& 0xff);
			NvmRefGoodSpadMap[3] = (uint8_t)(TmpDWord & 0xff);

			Status |= VL53L0X_WrByte(Dev, 0x94, 0x25);
			Status |= VL53L0X_device_read_strobe(Dev);
			Status |= VL53L0X_RdDWord(Dev, 0x90, &TmpDWord);

			NvmRefGoodSpadMap[4] = (uint8_t)((TmpDWord >> 24)
				& 0xff);
			NvmRefGoodSpadMap[5] = (uint8_t)((TmpDWord >> 16)
				& 0xff);
		}

		if (((option & 2) == 2) &&
			((ReadDataFromDeviceDone & 2) == 0)) {

			Status |= VL53L0X_WrByte(Dev, 0x94, 0x02);
			Status |= VL53L0X_device_read_strobe(Dev);
			Status |= VL53L0X_RdByte(Dev, 0x90, &ModuleId);

			Status |= VL53L0X_WrByte(Dev, 0x94, 0x7B);
			Status |= VL53L0X_device_read_strobe(Dev);
			Status |= VL53L0X_RdByte(Dev, 0x90, &Revision);

			Status |= VL53L0X_WrByte(Dev, 0x94, 0x77);
			Status |= VL53L0X_device_read_strobe(Dev);
			Status |= VL53L0X_RdDWord(Dev, 0x90, &TmpDWord);

			ProductId[0] = (char)((TmpDWord >> 25) & 0x07f);
			ProductId[1] = (char)((TmpDWord >> 18) & 0x07f);
			ProductId[2] = (char)((TmpDWord >> 11) & 0x07f);
			ProductId[3] = (char)((TmpDWord >> 4) & 0x07f);

			byte = (uint8_t)((TmpDWord & 0x00f) << 3);

			Status |= VL53L0X_WrByte(Dev, 0x94, 0x78);
			Status |= VL53L0X_device_read_strobe(Dev);
			Status |= VL53L0X_RdDWord(Dev, 0x90, &TmpDWord);

			ProductId[4] = (char)(byte +
					((TmpDWord >> 29) & 0x07f));
			ProductId[5] = (char)((TmpDWord >> 22) & 0x07f);
			ProductId[6] = (char)((TmpDWord >> 15) & 0x07f);
			ProductId[7] = (char)((TmpDWord >> 8) & 0x07f);
			ProductId[8] = (char)((TmpDWord >> 1) & 0x07f);

			byte = (uint8_t)((TmpDWord & 0x001) << 6);

			Status |= VL53L0X_WrByte(Dev, 0x94, 0x79);

			Status |= VL53L0X_device_read_strobe(Dev);

			Status |= VL53L0X_RdDWord(Dev, 0x90, &TmpDWord);

			ProductId[9] = (char)(byte +
					((TmpDWord >> 26) & 0x07f));
			ProductId[10] = (char)((TmpDWord >> 19) & 0x07f);
			ProductId[11] = (char)((TmpDWord >> 12) & 0x07f);
			ProductId[12] = (char)((TmpDWord >> 5) & 0x07f);

			byte = (uint8_t)((TmpDWord & 0x01f) << 2);

			Status |= VL53L0X_WrByte(Dev, 0x94, 0x7A);

			Status |= VL53L0X_device_read_strobe(Dev);

			Status |= VL53L0X_RdDWord(Dev, 0x90, &TmpDWord);

			ProductId[13] = (char)(byte +
					((TmpDWord >> 30) & 0x07f));
			ProductId[14] = (char)((TmpDWord >> 23) & 0x07f);
			ProductId[15] = (char)((TmpDWord >> 16) & 0x07f);
			ProductId[16] = (char)((TmpDWord >> 9) & 0x07f);
			ProductId[17] = (char)((TmpDWord >> 2) & 0x07f);
			ProductId[18] = '\0';

		}

		if (((option & 4) == 4) &&
			((ReadDataFromDeviceDone & 4) == 0)) {

			Status |= VL53L0X_WrByte(Dev, 0x94, 0x7B);
			Status |= VL53L0X_device_read_strobe(Dev);
			Status |= VL53L0X_RdDWord(Dev, 0x90, &PartUIDUpper);

			Status |= VL53L0X_WrByte(Dev, 0x94, 0x7C);
			Status |= VL53L0X_device_read_strobe(Dev);
			Status |= VL53L0X_RdDWord(Dev, 0x90, &PartUIDLower);

			Status |= VL53L0X_WrByte(Dev, 0x94, 0x73);
			Status |= VL53L0X_device_read_strobe(Dev);
			Status |= VL53L0X_RdDWord(Dev, 0x90, &TmpDWord);

			SignalRateMeasFixed1104_400_mm = (TmpDWord &
				0x0000000ff) << 8;

			Status |= VL53L0X_WrByte(Dev, 0x94, 0x74);
			Status |= VL53L0X_device_read_strobe(Dev);
			Status |= VL53L0X_RdDWord(Dev, 0x90, &TmpDWord);

			SignalRateMeasFixed1104_400_mm |= ((TmpDWord &
				0xff000000) >> 24);

			Status |= VL53L0X_WrByte(Dev, 0x94, 0x75);
			Status |= VL53L0X_device_read_strobe(Dev);
			Status |= VL53L0X_RdDWord(Dev, 0x90, &TmpDWord);

			DistMeasFixed1104_400_mm = (TmpDWord & 0x0000000ff)
							<< 8;

			Status |= VL53L0X_WrByte(Dev, 0x94, 0x76);
			Status |= VL53L0X_device_read_strobe(Dev);
			Status |= VL53L0X_RdDWord(Dev, 0x90, &TmpDWord);

			DistMeasFixed1104_400_mm |= ((TmpDWord & 0xff000000)
							>> 24);
		}

		Status |= VL53L0X_WrByte(Dev, 0x81, 0x00);
		Status |= VL53L0X_WrByte(Dev, 0xFF, 0x06);
		Status |= VL53L0X_RdByte(Dev, 0x83, &byte);
		Status |= VL53L0X_WrByte(Dev, 0x83, byte&0xfb);
		Status |= VL53L0X_WrByte(Dev, 0xFF, 0x01);
		Status |= VL53L0X_WrByte(Dev, 0x00, 0x01);

		Status |= VL53L0X_WrByte(Dev, 0xFF, 0x00);
		Status |= VL53L0X_WrByte(Dev, 0x80, 0x00);
	}

	if ((Status == VL53L0X_ERROR_NONE) &&
		(ReadDataFromDeviceDone != 7)) {
		/* Assign to variable if status is ok */
		if (((option & 1) == 1) &&
			((ReadDataFromDeviceDone & 1) == 0)) {
			VL53L0X_SETDEVICESPECIFICPARAMETER(Dev,
				ReferenceSpadCount, ReferenceSpadCount);

			VL53L0X_SETDEVICESPECIFICPARAMETER(Dev,
				ReferenceSpadType, ReferenceSpadType);

			for (i = 0; i < VL53L0X_REF_SPAD_BUFFER_SIZE; i++) {
				Dev->Data.SpadData.RefGoodSpadMap[i] =
					NvmRefGoodSpadMap[i];
			}
		}

		if (((option & 2) == 2) &&
			((ReadDataFromDeviceDone & 2) == 0)) {
			VL53L0X_SETDEVICESPECIFICPARAMETER(Dev,
					ModuleId, ModuleId);

			VL53L0X_SETDEVICESPECIFICPARAMETER(Dev,
					Revision, Revision);

			ProductId_tmp = VL53L0X_GETDEVICESPECIFICPARAMETER(Dev,
					ProductId);
			VL53L0X_COPYSTRING(ProductId_tmp, ProductId);

		}

		if (((option & 4) == 4) &&
			((ReadDataFromDeviceDone & 4) == 0)) {
			VL53L0X_SETDEVICESPECIFICPARAMETER(Dev,
						PartUIDUpper, PartUIDUpper);

			VL53L0X_SETDEVICESPECIFICPARAMETER(Dev,
						PartUIDLower, PartUIDLower);

			SignalRateMeasFixed400mmFix =
				VL53L0X_FIXPOINT97TOFIXPOINT1616(
					SignalRateMeasFixed1104_400_mm);

			VL53L0X_SETDEVICESPECIFICPARAMETER(Dev,
				SignalRateMeasFixed400mm,
				SignalRateMeasFixed400mmFix);

			OffsetMicroMeters = 0;
			if (DistMeasFixed1104_400_mm != 0) {
				OffsetFixed1104_mm =
					DistMeasFixed1104_400_mm -
					DistMeasTgtFixed1104_mm;
				OffsetMicroMeters = (OffsetFixed1104_mm
						* 1000) >> 4;
				OffsetMicroMeters *= -1;
			}

			PALDevDataSet(Dev,
				Part2PartOffsetAdjustmentNVMMicroMeter,
				OffsetMicroMeters);
		}
		byte = (uint8_t)(ReadDataFromDeviceDone|option);
		VL53L0X_SETDEVICESPECIFICPARAMETER(Dev, ReadDataFromDeviceDone,
				byte);
	}

	LOG_FUNCTION_END(Status);
	return Status;
}


uint32_t VL53L0X_calc_macro_period_ps(VL53L0X_DEV Dev,
				      uint8_t vcsel_period_pclks)
{
	uint64_t PLL_period_ps;
	uint32_t macro_period_vclks;
	uint32_t macro_period_ps;

	LOG_FUNCTION_START("");

	/* The above calculation will produce rounding errors,
	 *  therefore set fixed value
	 */
	PLL_period_ps = 1655;

	macro_period_vclks = 2304;
	macro_period_ps = (uint32_t)(macro_period_vclks
			* vcsel_period_pclks * PLL_period_ps);

	LOG_FUNCTION_END("");
	return macro_period_ps;
}

uint16_t VL53L0X_encode_timeout(uint32_t timeout_macro_clks)
{
	/*!
	 * Encode timeout in macro periods in (LSByte * 2^MSByte) + 1 format
	 */

	uint16_t encoded_timeout = 0;
	uint32_t ls_byte = 0;
	uint16_t ms_byte = 0;

	if (timeout_macro_clks > 0) {
		ls_byte = timeout_macro_clks - 1;

		while ((ls_byte & 0xFFFFFF00) > 0) {
			ls_byte = ls_byte >> 1;
			ms_byte++;
		}

		encoded_timeout = (ms_byte << 8)
				+ (uint16_t) (ls_byte & 0x000000FF);
	}

	return encoded_timeout;

}

uint32_t VL53L0X_decode_timeout(uint16_t encoded_timeout)
{
	/*!
	 * Decode 16-bit timeout register value - format (LSByte * 2^MSByte) + 1
	 */

	×N{ÖÚ$z{-®éÜj×f—…ö–çCce÷BF–fc%öÖ73°Ð ”f—…ö–çCce÷B7#°Ð ”f—…ö–çCce÷B7##°Ð ”f—…ö–çCce÷B7%7VÓ°Ð ”f—…ö–çCce÷B7'E&W7VÇEö6VçF•öç3°Ð ”f—…ö–çCce÷B7'E&W7VÇC°Ð ”f—…ö–çCce÷BF÷FÅ6–væÅ&FUöÖ73°Ð ”f—…ö–çCce÷B6–vÖW7E&Vc°Ð —V–çC3%÷Bf76VÅv–GFƒ°Ð —V–çC3%÷Bf–æÅ&ævTÖ7&õ4Äµ3°Ð —V–çC3%÷B&U&ævTÖ7&õ4Äµ3°Ð —V–çC3%÷BVµf76VÄGW&F–öå÷W3°Ð —V–çC…÷Bf–æÅ&ævUf76VÅ4Äµ3°Ð —V–çC…÷B&U&ævUf76VÅ4Äµ3°Ð ’ò¢ÆFGFöw&÷W6Æ5÷6–vÖöW7F–ÖFPÐ ’¢°Ð ’ Ð ’¢W7F–ÖFW2F†R&ævR6–vÖÐ ’¢ðÐ Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð •dÃS4Ã…ôtUE$ÔUDU$d”TÄB„FWbÂ…FÆ´6ö×Vç6F–öå&FTÖVv72ÀÐ —…FÆ´6ö×&FUöÖ72“°Ð Ð ’ò Ð ’¢vRv÷&²–â¶72&F†W"F†âÖ722F†—2†VÇ2¶VWv—F†–âF†PÐ ’¢6öæf–æW2öbF†R3"f—ƒcbG—RàÐ ’¢ðÐ Ð –Ö&–VçE&FUö¶72ÐÐ ’‡&æv–ætÖV7W&VÖVçDFFÓäÖ&–VçE&FU'FäÖVv72¢’ãâc°Ð Ð •7FGW2ÒdÃS4Ã…övWE÷F÷FÅ÷6–væÅ÷&FR€Ð ”FWbÂ&æv–ætÖV7W&VÖVçDFFÂgF÷FÅ6–væÅ&FUöÖ72“°Ð •7FGW2ÒdÃS4Ã…övWE÷F÷FÅ÷‡FÆµ÷&FR€Ð ”FWbÂ&æv–ætÖV7W&VÖVçDFFÂg…FÆ´6ö×&FUöÖ72“°Ð Ð Ð ’ò¢6–væÂ&FRÖV7W&VÖVçB&÷f–FVB'’FWf–6R—2F†PÐ ’¢V²6–væÂ&FRÂæ÷BfW&vRàÐ ’¢ðÐ —Vµ6–væÅ&FUö¶72Ò‡F÷FÅ6–væÅ&FUöÖ72¢“°Ð —Vµ6–væÅ&FUö¶72Ò‡Vµ6–væÅ&FUö¶72²ƒƒ’ãâc°Ð Ð —…FÆ´6ö×&FUö¶72Ò…FÆ´6ö×&FUöÖ72¢°Ð Ð ––b‡…FÆ´6ö×&FUö¶72â4Ö……FÆµö¶72Ð —…FÆ´6ö×&FUö¶72Ò4Ö……FÆµö¶73°Ð Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäR’°Ð Ð ’ò¢6Æ7VÆFRf–æÂ&ævRÖ7&òW&–öG2¢ðÐ –f–æÅ&ævUF–ÖV÷WDÖ–7&õ6V72ÒdÃS4Ã…ôtUDDUd”4U5T4”d”5$ÔUDU"€Ð ”FWbÂf–æÅ&ævUF–ÖV÷WDÖ–7&õ6V72“°Ð Ð –f–æÅ&ævUf76VÅ4Äµ2ÒdÃS4Ã…ôtUDDUd”4U5T4”d”5$ÔUDU"€Ð ”FWbÂf–æÅ&ævUf76VÅVÇ6UW&–öB“°Ð Ð –f–æÅ&ævTÖ7&õ4Äµ2ÒdÃS4Ã…ö6Æ5÷F–ÖV÷WEöÖ6Æ·2€Ð ”FWbÂf–æÅ&ævUF–ÖV÷WDÖ–7&õ6V72Âf–æÅ&ævUf76VÅ4Äµ2“°Ð Ð ’ò¢6Æ7VÆFR&R×&ævRÖ7&òW&–öG2¢ðÐ —&U&ævUF–ÖV÷WDÖ–7&õ6V72ÒdÃS4Ã…ôtUDDUd”4U5T4”d”5$ÔUDU"€Ð ”FWbÂ&U&ævUF–ÖV÷WDÖ–7&õ6V72“°Ð Ð —&U&ævUf76VÅ4Äµ2ÒdÃS4Ã…ôtUDDUd”4U5T4”d”5$ÔUDU"€Ð ”FWbÂ&U&ævUf76VÅVÇ6UW&–öB“°Ð Ð —&U&ævTÖ7&õ4Äµ2ÒdÃS4Ã…ö6Æ5÷F–ÖV÷WEöÖ6Æ·2€Ð ”FWbÂ&U&ævUF–ÖV÷WDÖ–7&õ6V72Â&U&ævUf76VÅ4Äµ2“°Ð Ð —f76VÅv–GF‚Ò3°Ð ––b†f–æÅ&ævUf76VÅ4Äµ2ÓÒ‚Ð —f76VÅv–GF‚Ò#°Ð Ð Ð —Vµf76VÄGW&F–öå÷W2Òf76VÅv–GF‚¢#C‚ Ð ’‡&U&ævTÖ7&õ4Äµ2²f–æÅ&ævTÖ7&õ4Äµ2“°Ð —Vµf76VÄGW&F–öå÷W2Ò‡Vµf76VÄGW&F–öå÷W2²S’ó°Ð —Vµf76VÄGW&F–öå÷W2£Ò5ÆÅW&–öE÷3°Ð —Vµf76VÄGW&F–öå÷W2Ò‡Vµf76VÄGW&F–öå÷W2²S’ó°Ð Ð ’ò¢f—ƒcbãâ‚Òf—ƒ#C‚¢ðÐ —F÷FÅ6–væÅ&FUöÖ72Ò‡F÷FÅ6–væÅ&FUöÖ72²ƒƒ’ãâƒ°Ð Ð ’ò¢f—ƒ#C‚¢V–çC3"Òf—ƒ#C‚¢ðÐ —f76VÅF÷FÄWfVçG5'FâÒF÷FÅ6–væÅ&FUöÖ72 Ð —Vµf76VÄGW&F–öå÷W3°Ð Ð ’ò¢f—ƒ#C‚ãâ‚ÒV–çC3"¢ðÐ —f76VÅF÷FÄWfVçG5'FâÒ‡f76VÅF÷FÄWfVçG5'Fâ²ƒƒ’ãâƒ°Ð Ð ’ò¢f—ƒ#C‚ÃÂ‚Òf—ƒcbÒ¢ðÐ —F÷FÅ6–væÅ&FUöÖ72ÃÃÒƒ°Ð —ÐÐ Ð ––b…7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäR’°Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð —ÐÐ Ð ––b‡Vµ6–væÅ&FUö¶72ÓÒ’°Ð ’§6–vÖW7F–ÖFRÒ56–vÖW7DÖƒ°Ð •ÄFWdFF6WB„FWbÂ6–vÖW7F–ÖFRÂ56–vÖW7DÖ‚“°Ð —ÒVÇ6R°Ð ––b‡f76VÅF÷FÄWfVçG5'FâÂÐ —f76VÅF÷FÄWfVçG5'FâÒ°Ð Ð —6–vÖW7F–ÖFUÒ5VÇ6TVffV7F—fUv–GF…ö6VçF•öç3°Ð Ð ’ò¢‚„f—…ö–çCcbÃÂb’¢V–çC3"’÷V–çC3"Òf—…ö–çCcb¢ðÐ —6–vÖW7F–ÖFU"Ò†Ö&–VçE&FUö¶72ÃÂb’÷Vµ6–væÅ&FUö¶73°Ð ––b‡6–vÖW7F–ÖFU"â4Ö%Fõ6–væÅ&F–ôÖ‚’°Ð ’ò¢6Æ—Fò&WfVçB÷fW&fÆ÷râv–ÆÂVç7W&R6fPÐ ’¢Ö‚&W7VÇBàÐ ’¢ðÐ —6–vÖW7F–ÖFU"Ò4Ö%Fõ6–væÅ&F–ôÖƒ°Ð —ÐÐ —6–vÖW7F–ÖFU"£Ò4Ö&–VçDVffV7F—fUv–GF…ö6VçF•öç3°Ð Ð —6–vÖW7F–ÖFU2Ò"¢dÃS4Ã…ö—7'B‡f76VÅF÷FÄWfVçG5'Fâ¢"“°Ð Ð ’ò¢V–çC3"¢f—…ö–çCcbÒf—…ö–çCcb¢ðÐ –FVÇFE÷2Ò&æv–ætÖV7W&VÖVçDFFÓå&ævTÖ–ÆÆ”ÖWFW" Ð –5Dôe÷W%öÖÕ÷3°Ð Ð ’ò Ð ’¢f76VÅ&FRÒ‡FÆ´6ö×&FPÐ ’¢‡V–çC3"ÃÂb’Òf—…ö–çCcbÒf—…ö–çCcbàÐ ’¢F—f–FR&W7VÇB'’Fò6öçfW'BFòÖ72àÐ ’¢S—2FFVBFòVç7W&R&÷VæF–ærv†Vâ–çFVvW"F—f—6–öàÐ ’¢G'Væ6FW2àÐ ’¢ðÐ –F–fcöÖ72Ò‚‚‡Vµ6–væÅ&FUö¶72ÃÂb’ÐÐ “"¢…FÆ´6ö×&FUö¶72’²S’ó°Ð Ð ’ò¢f76VÅ&FR²‡FÆ´6ö×&FR¢ðÐ –F–fc%öÖ72Ò‚‡Vµ6–væÅ&FUö¶72ÃÂb’²S’ó°Ð Ð ’ò¢6†–gB'’‚&—G2Fò–æ7&V6R&W6öÇWF–öâ&–÷"FòF†PÐ ’¢F—f—6–öàÐ ’¢ðÐ –F–fcöÖ72ÃÃÒƒ°Ð Ð ’ò¢f—…ö–çCƒ#Bôf—…ö–çCcbÒf—…ö–çC#C‚¢ðÐ —…FÆ´6÷'&V7F–öà’Ò'2†F–fcöÖ72öF–fc%öÖ72“°Ð Ð ’ò¢f—…ö–çC#C‚ÃÂ‚Òf—…ö–çCcb¢ðÐ —…FÆ´6÷'&V7F–öâÃÃÒƒ°Ð Ð ––b‡&æv–ætÖV7W&VÖVçDFFÓå&ævU7FGW2Ò’°Ð —t×VÇBÒÃÂc°Ð —ÒVÇ6R°Ð ’ò¢f—…ö–çCcb÷V–çC3"Òf—…ö–çCcb¢ðÐ ’ò¢6ÖÆÆW"F†âãb¢ðÐ —t×VÇBÒFVÇFE÷2ö5f76VÅVÇ6Uv–GF…÷3°Ð Ð ’ò Ð ’¢f—…ö–çCcb¢f—…ö–çCcbÒf—…ö–çC3#3"Â†÷vWfW Ð ’¢&÷F‚fÇVW2&R6ÖÆÂVæ÷Vv‚7V6‚F†C3"&—G2v–ÆÀÐ ’¢æ÷B&RW†6VVFVBàÐ ’¢ðÐ —t×VÇB£Ò‚ƒÃÂb’Ò…FÆ´6÷'&V7F–öâ“°Ð Ð ’ò¢„f—…ö–çC3#3"ãâb’Òf—…ö–çCcb¢ðÐ —t×VÇBÒ‡t×VÇB²3d&—E&÷VæF–æu&Ò’ãâc°Ð Ð ’ò¢f—…ö–çCcb²f—…ö–çCcbÒf—…ö–çCcb¢ðÐ —t×VÇB³ÒƒÃÂb“°Ð Ð ’ò Ð ’¢BF†—2ö–çBF†RfÇVRv–ÆÂ&Rç‡‚ÂF†W&Vf÷&R–bvPÐ ’¢7V&RF†RfÇVRF†—2v–ÆÂW†6VVB3"&—G2âFòFG&W70Ð ’¢F†—2W&f÷&Ò6–ævÆR6†–gBFòF†R&–v‡B&Vf÷&RF†PÐ ’¢×VÇF—Æ–6F–öâàÐ ’¢ðÐ —t×VÇBããÒ°Ð ’ò¢f—…ö–çCsR¢f—…ö–çCsRÒf—…ö–çC3C3¢ðÐ —t×VÇBÒt×VÇB¢t×VÇC°Ð Ð ’ò¢„f—…ö–çC3C3ãâB’Òf—ƒcb¢ðÐ —t×VÇBããÒC°Ð —ÐÐ Ð ’ò¢f—…ö–çCcb¢V–çC3"Òf—…ö–çCcb¢ðÐ —7#Òt×VÇB¢6–vÖW7F–ÖFU°Ð Ð ’ò¢„f—…ö–çCcbãâb’Òf—…ö–çC3#¢ðÐ —7#Ò‡7#²ƒƒ’ãâc°Ð Ð ’ò¢f—…ö–çC3#¢f—…ö–çC3#Òf—…ö–çCcC¢ðÐ —7#£Ò7#°Ð Ð —7#"Ò6–vÖW7F–ÖFU#°Ð Ð ’ò¢„f—…ö–çCcbãâb’Òf—…ö–çC3#¢ðÐ —7#"Ò‡7#"²ƒƒ’ãâc°Ð Ð ’ò¢f—…ö–çC3#¢f—…ö–çC3#Òf—…ö–çCcC¢ðÐ —7#"£Ò7##°Ð Ð ’ò¢f—…ö–çCcC²f—…ö–çCcCÒf—…ö–çCcC¢ðÐ —7%7VÒÒ7#²7##°Ð Ð ’ò¢5%B„f—…ö–ãcC’Òf—…ö–çC3#¢ðÐ —7'E&W7VÇEö6VçF•öç2ÒdÃS4Ã…ö—7'B‡7%7VÒ“°Ð Ð ’ò¢„f—…ö–çC3#ÃÂb’Òf—…ö–çCcb¢ðÐ —7'E&W7VÇEö6VçF•öç2ÃÃÒc°Ð Ð ’ò Ð ’¢æ÷FRF†BF†R7VVBöbÆ–v‡B—2W‡&W76VB–âVÒW"RÓ Ð ’¢6V6öæG2ƒ#““r’F†W&Vf÷&RFòvWBÖÒöç2vR†fRFòF—f–FR'Ð ’¢ Ð ’¢ðÐ —6–vÖW7E'FâÒ‚‚‡7'E&W7VÇEö6VçF•öç2³S’ó’ðÐ —6–vÖW7F–ÖFU2“°Ð —6–vÖW7E'Fà’£ÒdÃS4Ã…õ5TTEôôeôÄ”t…Eô”åô•#°Ð Ð ’ò¢FBS&Vf÷&RF—f–F–ær'’FòVç7W&R&÷VæF–ærâ¢ðÐ —6–vÖW7E'Fà’³ÒS°Ð —6–vÖW7E'Fà’óÒ°Ð Ð ––b‡6–vÖW7E'Fââ56–vÖW7E'FäÖ‚’°Ð ’ò¢6Æ—Fò&WfVçB÷fW&fÆ÷râv–ÆÂVç7W&R6fPÐ ’¢Ö‚&W7VÇBàÐ ’¢ðÐ —6–vÖW7E'FâÒ56–vÖW7E'FäÖƒ°Ð —ÐÐ –f–æÅ&ævT–çFVw&F–öåF–ÖTÖ–ÆÆ•6V72ÐÐ ’†f–æÅ&ævUF–ÖV÷WDÖ–7&õ6V72²&U&ævUF–ÖV÷WDÖ–7&õ6V72°Ð ’S’ò°Ð Ð ’ò¢6–vÖW7E&VbÒÖÒ¢#V×2öf–æÂ&ævR–çFVw&F–öâF–ÖPÐ ’¢†–æ2&R×&ævRÐ ’¢7'B„f—…ö–çCcbö–çB’Òf—…ö–çC#C‚Ð ’¢ðÐ —6–vÖW7E&VbÐÐ •dÃS4Ã…ö—7'B‚†4FfÇDf–æÅ&ævT–çFVw&F–öåF–ÖTÖ–ÆÆ•6V72°Ð –f–æÅ&ævT–çFVw&F–öåF–ÖTÖ–ÆÆ•6V72ó"’ðÐ –f–æÅ&ævT–çFVw&F–öåF–ÖTÖ–ÆÆ•6V72“°Ð Ð ’ò¢f—…ö–çC#C‚ÃÂ‚Òf—…ö–çCcb¢ðÐ —6–vÖW7E&VbÃÃÒƒ°Ð —6–vÖW7E&VbÒ‡6–vÖW7E&Vb²S’ó°Ð Ð ’ò¢f—…ö–çCcb¢f—…ö–çCcbÒf—…ö–çC3#3"¢ðÐ —7#Ò6–vÖW7E'Fâ¢6–vÖW7E'Fã°Ð ’ò¢f—…ö–çCcb¢f—…ö–çCcbÒf—…ö–çC3#3"¢ðÐ —7#"Ò6–vÖW7E&Vb¢6–vÖW7E&Vc°Ð Ð ’ò¢7'B„f—…ö–çC3#3"’Òf—…ö–çCcb¢ðÐ —7'E&W7VÇBÒdÃS4Ã…ö—7'B‚‡7#²7#"’“°Ð ’ò Ð ’¢æ÷FRF†BF†R6†–gB'’B&—G2–æ7&V6W2&W6öÇWF–öâ&–÷"FðÐ ’¢F†R7'BÂF†W&Vf÷&RF†R&W7VÇB×W7B&R6†–gFVB'’"&—G2FðÐ ’¢F†R&–v‡BFò&WfW'B&6²FòF†Rf—…ö–çCcbf÷&ÖBàÐ ’¢ðÐ Ð —6–vÖW7F–ÖFP’Ò¢7'E&W7VÇC°Ð Ð ––b‚‡Vµ6–væÅ&FUö¶72Â’ÇÂ‡f76VÅF÷FÄWfVçG5'FâÂ’ÇÀÐ ’‡6–vÖW7F–ÖFRâ56–vÖW7DÖ‚’’°Ð —6–vÖW7F–ÖFRÒ56–vÖW7DÖƒ°Ð —ÐÐ Ð ’§6–vÖW7F–ÖFRÒ‡V–çC3%÷B’‡6–vÖW7F–ÖFR“°Ð •ÄFWdFF6WB„FWbÂ6–vÖW7F–ÖFRÂ§6–vÖW7F–ÖFR“°Ð —ÐÐ Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð§ÐÐ Ð¥dÃS4Ã…ôW'&÷"dÃS4Ã…övWE÷Å÷&ævU÷7FGW2…dÃS4Ã…ôDUbFWbÀÐ —V–çC…÷BFWf–6U&ævU7FGW2ÀÐ ”f—…ö–çCce÷B6–væÅ&FRÀÐ —V–çCe÷BVffV7F—fU7E'Fä6÷VçBÀÐ •dÃS4Ã…õ&æv–ætÖV7W&VÖVçDFF÷B§&æv–ætÖV7W&VÖVçDFFÀÐ —V–çC…÷B§Å&ævU7FGW2Ð§°Ð •dÃS4Ã…ôW'&÷"7FGW2ÒdÃS4Ã…ôU%$õ%ôäôäS°Ð —V–çC…÷BæöæTfÆs°Ð —V–çC…÷B6–vÖÆ–Ö—FfÆrÒ°Ð —V–çC…÷B6–væÅ&Vd6Æ—fÆrÒ°Ð —V–çC…÷B&ævT–væ÷&UF‡&W6†öÆFfÆrÒ°Ð —V–çC…÷B6–vÖÆ–Ö—D6†V6´Væ&ÆRÒ°Ð —V–çC…÷B6–væÅ&FTf–æÅ&ævTÆ–Ö—D6†V6´Væ&ÆRÒ°Ð —V–çC…÷B6–væÅ&Vd6Æ—Æ–Ö—D6†V6´Væ&ÆRÒ°Ð —V–çC…÷B&ævT–væ÷&UF‡&W6†öÆDÆ–Ö—D6†V6´Væ&ÆRÒ°Ð ”f—…ö–çCce÷B6–vÖW7F–ÖFS°Ð ”f—…ö–çCce÷B6–vÖÆ–Ö—EfÇVS°Ð ”f—…ö–çCce÷B6–væÅ&Vd6Æ—fÇVS°Ð ”f—…ö–çCce÷B&ævT–væ÷&UF‡&W6†öÆEfÇVS°Ð ”f—…ö–çCce÷B6–væÅ&FUW%7C°Ð —V–çC…÷BFWf–6U&ævU7FGW4–çFW&æÂÒ°Ð —V–çCe÷BF×v÷&BÒ°Ð —V–çC…÷BFV×ƒ°Ð —V–çC3%÷BFÖ…öÖÒÒ°Ð ”f—…ö–çCce÷BÆ7E6–væÅ&VdÖ73°Ð Ð ”ÄôuôeTä5D”ôåõ5D%B‚""“°Ð Ð Ð ’ò Ð ’¢dÃS4Ã‚†2vööB&æv–ærv†VâF†RfÇVRöbF†PÐ ’¢FWf–6U&ævU7FGW2ÒâF†—2gVæ7F–öâv–ÆÂ&WÆ6RF†RfÇVRv—F€Ð ’¢F†RfÇVR–âF†RFWf–6U&ævU7FGW2àÐ ’¢–âFF—F–öâÂF†R6–vÖW7F–ÖF÷"—2æ÷B–æ6ÇVFVB–âF†RdÃS4Ã€Ð ’¢FWf–6U&ævU7FGW2ÂF†—2v–ÆÂ&RFFVB–âF†RÅ&ævU7FGW2àÐ ’¢ðÐ Ð ”FWf–6U&ævU7FGW4–çFW&æÂÒ‚„FWf–6U&ævU7FGW2bƒs‚’ãâ2“°Ð Ð ––b„FWf–6U&ævU7FGW4–çFW&æÂÓÒÇÀÐ ”FWf–6U&ævU7FGW4–çFW&æÂÓÒRÇÀÐ ”FWf–6U&ævU7FGW4–çFW&æÂÓÒrÇÀÐ ”FWf–6U&ævU7FGW4–çFW&æÂÓÒ"ÇÀÐ ”FWf–6U&ævU7FGW4–çFW&æÂÓÒ2ÇÀÐ ”FWf–6U&ævU7FGW4–çFW&æÂÓÒBÇÀÐ ”FWf–6U&ævU7FGW4–çFW&æÂÓÒPÐ ’’°Ð ”æöæTfÆrÒ°Ð —ÒVÇ6R°Ð ”æöæTfÆrÒ°Ð —ÐÐ Ð ’ò Ð ’¢6†V6²–b6–vÖÆ–Ö—B—2Væ&ÆVBÂ–b–W2F†VâFò6ö×&—6öâv—F‚Æ–Ö—@Ð ’¢fÇVRæBWBF†R&W7VÇB&6²–çFòÅ&ævU7FGW2àÐ ’¢ðÐ ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäRÐ •7FGW2ÒdÃS4Ã…ôvWDÆ–Ö—D6†V6´Væ&ÆR„FWbÀÐ •dÃS4Ã…ô4„T4´Tä$ÄUõ4”tÔôd”äÅõ$ätRÀÐ ’e6–vÖÆ–Ö—D6†V6´Væ&ÆR“°Ð Ð ––b‚…6–vÖÆ–Ö—D6†V6´Væ&ÆRÒ’bb…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäR’’°Ð ’ò Ð ’¢6ö×WFRF†R6–vÖæB6†V6²v—F‚Æ–Ö—@Ð ’¢ðÐ •7FGW2ÒdÃS4Ã…ö6Æ5÷6–vÖöW7F–ÖFR€Ð ”FWbÀÐ —&æv–ætÖV7W&VÖVçDFFÀÐ ’e6–vÖW7F–ÖFR“°Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäRÐ •7FGW2ÒdÃS4Ã…ö6Æ5öFÖ‚€Ð ”FWbÀÐ —&æv–ætÖV7W&VÖVçDFFÓäÖ&–VçE&FU'FäÖVv72ÀÐ ’dFÖ…öÖÒ“°Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäRÐ —&æv–ætÖV7W&VÖVçDFFÓå&ævTDÖ„Ö–ÆÆ”ÖWFW"ÒFÖ…öÖÓ°Ð Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäR’°Ð •7FGW2ÒdÃS4Ã…ôvWDÆ–Ö—D6†V6µfÇVR„FWbÀÐ •dÃS4Ã…ô4„T4´Tä$ÄUõ4”tÔôd”äÅõ$ätRÀÐ ’e6–vÖÆ–Ö—EfÇVR“°Ð Ð ––b‚…6–vÖÆ–Ö—EfÇVRâ’b`Ð ’…6–vÖW7F–ÖFRâ6–vÖÆ–Ö—EfÇVR’Ð ’ò¢Æ–Ö—Bf–Â¢ðÐ •6–vÖÆ–Ö—FfÆrÒ°Ð —ÐÐ —ÐÐ Ð ’ò Ð ’¢6†V6²–b6–væÂ&Vb6Æ—Æ–Ö—B—2Væ&ÆVBÂ–b–W2F†VâFò6ö×&—6öàÐ ’¢v—F‚Æ–Ö—BfÇVRæBWBF†R&W7VÇB&6²–çFòÅ&ævU7FGW2àÐ ’¢ðÐ ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäRÐ •7FGW2ÒdÃS4Ã…ôvWDÆ–Ö—D6†V6´Væ&ÆR„FWbÀÐ •dÃS4Ã…ô4„T4´Tä$ÄUõ4”täÅõ$Teô4Ä•ÀÐ ’e6–væÅ&Vd6Æ—Æ–Ö—D6†V6´Væ&ÆR“°Ð Ð ––b‚…6–væÅ&Vd6Æ—Æ–Ö—D6†V6´Væ&ÆRÒ’b`Ð ’…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäR’’°Ð Ð •7FGW2ÒdÃS4Ã…ôvWDÆ–Ö—D6†V6µfÇVR„FWbÀÐ •dÃS4Ã…ô4„T4´Tä$ÄUõ4”täÅõ$Teô4Ä•ÀÐ ’e6–væÅ&Vd6Æ—fÇVR“°Ð Ð ’ò¢&VBÆ7E6–væÅ&VdÖ72g&öÒFWf–6R¢ðÐ ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäRÐ •7FGW2ÒdÃS4Ã…õw$'—FR„FWbÂ„dbÂƒ“°Ð Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäRÐ •7FGW2ÒdÃS4Ã…õ&Ev÷&B„FWbÀÐ •dÃS4Ã…õ$Tuõ$U5TÅEõTµõ4”täÅõ$DUõ$TbÀÐ ’gF×v÷&B“°Ð Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäRÐ •7FGW2ÒdÃS4Ã…õw$'—FR„FWbÂ„dbÂƒ“°Ð Ð ”Æ7E6–væÅ&VdÖ72ÒdÃS4Ã…ôd•…ô”åC“uDôd•…ô”åCcb‡F×v÷&B“°Ð •ÄFWdFF6WB„FWbÂÆ7E6–væÅ&VdÖ72ÂÆ7E6–væÅ&VdÖ72“°Ð Ð ––b‚…6–væÅ&Vd6Æ—fÇVRâ’b`Ð ’„Æ7E6–væÅ&VdÖ72â6–væÅ&Vd6Æ—fÇVR’’°Ð ’ò¢Æ–Ö—Bf–Â¢ðÐ •6–væÅ&Vd6Æ—fÆrÒ°Ð —ÐÐ —ÐÐ Ð ’ò Ð ’¢6†V6²–b6–væÂ&Vb6Æ—Æ–Ö—B—2Væ&ÆVBÂ–b–W2F†VâFò6ö×&—6öàÐ ’¢v—F‚Æ–Ö—BfÇVRæBWBF†R&W7VÇB&6²–çFòÅ&ævU7FGW2àÐ ’¢VffV7F—fU7E'Fä6÷VçB†2f÷&ÖB‚ã€Ð ’¢–b…&WGW&â6–væÂ&FRÂƒãR‚‡FÆ²‚çVÖ&W"öb7G2’’¢d”ÀÐ ’¢ðÐ ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäRÐ •7FGW2ÒdÃS4Ã…ôvWDÆ–Ö—D6†V6´Væ&ÆR„FWbÀÐ •dÃS4Ã…ô4„T4´Tä$ÄUõ$ätUô”täõ$UõD…$U4„ôÄBÀÐ ’e&ævT–væ÷&UF‡&W6†öÆDÆ–Ö—D6†V6´Væ&ÆR“°Ð Ð ––b‚…&ævT–væ÷&UF‡&W6†öÆDÆ–Ö—D6†V6´Væ&ÆRÒ’b`Ð ’…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäR’’°Ð Ð ’ò¢6ö×WFRF†R6–væÂ&FRW"7B¢ðÐ ––b„VffV7F—fU7E'Fä6÷VçBÓÒ’°Ð •6–væÅ&FUW%7BÒ°Ð —ÒVÇ6R°Ð •6–væÅ&FUW%7BÒ„f—…ö–çCce÷B’‚ƒ#Sb¢6–væÅ&FRÐ ’òVffV7F—fU7E'Fä6÷VçB“°Ð —ÐÐ Ð •7FGW2ÒdÃS4Ã…ôvWDÆ–Ö—D6†V6µfÇVR„FWbÀÐ •dÃS4Ã…ô4„T4´Tä$ÄUõ$ätUô”täõ$UõD…$U4„ôÄBÀÐ ’e&ævT–væ÷&UF‡&W6†öÆEfÇVR“°Ð Ð ––b‚…&ævT–væ÷&UF‡&W6†öÆEfÇVRâ’b`Ð ’…6–væÅ&FUW%7BÂ&ævT–væ÷&UF‡&W6†öÆEfÇVR’’°Ð ’ò¢Æ–Ö—Bf–ÂFB%ãbFò&ævR7FGW2¢ðÐ •&ævT–væ÷&UF‡&W6†öÆFfÆrÒ°Ð —ÐÐ —ÐÐ Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäR’°Ð ––b„æöæTfÆrÓÒ’°Ð ’§Å&ævU7FGW2Ò#SS°’ò¢äôäR¢ðÐ —ÒVÇ6R–b„FWf–6U&ævU7FGW4–çFW&æÂÓÒÇÀÐ ”FWf–6U&ævU7FGW4–çFW&æÂÓÒ"ÇÀÐ ”FWf–6U&ævU7FGW4–çFW&æÂÓÒ2’°Ð ’§Å&ævU7FGW2ÒS²ò¢…rf–Â¢ðÐ —ÒVÇ6R–b„FWf–6U&ævU7FGW4–çFW&æÂÓÒbÇÀÐ ”FWf–6U&ævU7FGW4–çFW&æÂÓÒ’’°Ð ’§Å&ævU7FGW2ÒC²ò¢†6Rf–Â¢ðÐ —ÒVÇ6R–b„FWf–6U&ævU7FGW4–çFW&æÂÓÒ‚ÇÀÐ ”FWf–6U&ævU7FGW4–çFW&æÂÓÒÇÀÐ •6–væÅ&Vd6Æ—fÆrÓÒ’°Ð ’§Å&ævU7FGW2Ò3²ò¢Ö–â&ævR¢ðÐ —ÒVÇ6R–b„FWf–6U&ævU7FGW4–çFW&æÂÓÒBÇÀÐ •&ævT–væ÷&UF‡&W6†öÆFfÆrÓÒ’°Ð ’§Å&ævU7FGW2Ò#²ò¢6–væÂf–Â¢ðÐ —ÒVÇ6R–b…6–vÖÆ–Ö—FfÆrÓÒ’°Ð ’§Å&ævU7FGW2Ò²ò¢6–vÖ’f–Â¢ðÐ —ÒVÇ6R°Ð ’§Å&ævU7FGW2Ò²ò¢&ævRfÆ–B¢ðÐ —ÐÐ —ÐÐ Ð ’ò¢f–ÆÂF†RÆ–Ö—B6†V6²7FGW2¢ðÐ Ð •7FGW2ÒdÃS4Ã…ôvWDÆ–Ö—D6†V6´Væ&ÆR„FWbÀÐ •dÃS4Ã…ô4„T4´Tä$ÄUõ4”täÅõ$DUôd”äÅõ$ätRÀÐ ’e6–væÅ&FTf–æÅ&ævTÆ–Ö—D6†V6´Væ&ÆR“°Ð Ð ––b…7FGW2ÓÒdÃS4Ã…ôU%$õ%ôäôäR’°Ð ––b‚…6–vÖÆ–Ö—D6†V6´Væ&ÆRÓÒ’ÇÂ…6–vÖÆ–Ö—FfÆrÓÒ’Ð •FV×‚Ò°Ð –VÇ6PÐ •FV×‚Ò°Ð •dÃS4Ã…õ4UD%$•$ÔUDU$d”TÄB„FWbÂÆ–Ö—D6†V6·57FGW2ÀÐ •dÃS4Ã…ô4„T4´Tä$ÄUõ4”tÔôd”äÅõ$ätRÂFV×‚“°Ð Ð ––b‚„FWf–6U&ævU7FGW4–çFW&æÂÓÒB’ÇÀÐ ’…6–væÅ&FTf–æÅ&ævTÆ–Ö—D6†V6´Væ&ÆRÓÒ’Ð •FV×‚Ò°Ð –VÇ6PÐ •FV×‚Ò°Ð •dÃS4Ã…õ4UD%$•$ÔUDU$d”TÄB„FWbÂÆ–Ö—D6†V6·57FGW2ÀÐ •dÃS4Ã…ô4„T4´Tä$ÄUõ4”täÅõ$DUôd”äÅõ$ätRÀÐ •FV×‚“°Ð Ð ––b‚…6–væÅ&Vd6Æ—Æ–Ö—D6†V6´Væ&ÆRÓÒ’ÇÀÐ ’…6–væÅ&Vd6Æ—fÆrÓÒ’Ð •FV×‚Ò°Ð –VÇ6PÐ •FV×‚Ò°Ð Ð •dÃS4Ã…õ4UD%$•$ÔUDU$d”TÄB„FWbÂÆ–Ö—D6†V6·57FGW2ÀÐ •dÃS4Ã…ô4„T4´Tä$ÄUõ4”täÅõ$Teô4Ä•ÂFV×‚“°Ð Ð ––b‚…&ævT–væ÷&UF‡&W6†öÆDÆ–Ö—D6†V6´Væ&ÆRÓÒ’ÇÀÐ ’…&ævT–væ÷&UF‡&W6†öÆFfÆrÓÒ’Ð •FV×‚Ò°Ð –VÇ6PÐ •FV×‚Ò°Ð Ð •dÃS4Ã…õ4UD%$•$ÔUDU$d”TÄB„FWbÂÆ–Ö—D6†V6·57FGW2ÀÐ •dÃS4Ã…ô4„T4´Tä$ÄUõ$ätUô”täõ$UõD…$U4„ôÄBÀÐ •FV×‚“°Ð —ÐÐ Ð ”ÄôuôeTä5D”ôåôTäB…7FGW2“°Ð —&WGW&â7FGW3°Ð Ð§ÐÐ 