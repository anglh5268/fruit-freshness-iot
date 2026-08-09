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
 *****************************************************************************/

#ifndef _VL53L0X_API_H_
#define _VL53L0X_API_H_

#include "vl53l0x_api_strings.h"
#include "vl53l0x_def.h"
#include "vl53l0x_platform.h"

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef _MSC_VER
#   ifdef VL53L0X_API_EXPORTS
#       define VL53L0X_API  __declspec(dllexport)
#   else
#       define VL53L0X_API
#   endif
#else
#   define VL53L0X_API
#endif

/** @defgroup VL53L0X_cut11_group VL53L0X cut1.1 Function Definition
 *  @brief    VL53L0X cut1.1 Function Definition
 *  @{
 */

/** @defgroup VL53L0X_general_group VL53L0X General Functions
 *  @brief    General functions and definitions
 *  @{
 */

/**
 * @brief Return the VL53L0X PAL Implementation Version
 *
 * @note This function doesn't access to the device
 *
 * @param   pVersion              Pointer to current PAL Implementation Version
 * @return  VL53L0X_ERROR_NONE     Success
 * @return  "Other error code"    See ::VL53L0X_Error
 */
VL53L0X_API VL53L0X_Error VL53L0X_GetVersion(VL53L0X_Version_t *pVersion);

/**
 * @brief Return the PAL Specification Version used for the current
 * implementation.
 *
 * @note This function doesn't access to the device
 *
 * @param   pPalSpecVersion       Pointer to current PAL Specification Version
 * @return  VL53L0X_ERROR_NONE        Success
 * @return  "Other error code"    See ::VL53L0X_Error
 */
VL53L0X_API VL53L0X_Error VL53L0X_GetPalSpecVersion(
	VL53L0X_Version_t *pPalSpecVersion);

/**
 * @brief Reads the Product Revision for a for given Device
 * This function can be used to distinguish cut1.0 from cut1.1.
 *
 * @note This function Access to the device
 *
 * @param   Dev                 Device Handle
 * @param   pProductRevisionMajor  Pointer to Product Revision Major
 * for a given Device
 * @param   pProductRevisionMinor  Pointer to Product Revision Minor
 * for a given Device
 * @return  VL53L0X_ERROR_NONE      Success
 * @return  "Other error code"  See ::VL53L0X_Error
 */
VL53L0X_API VL53L0X_Error VL53L0X_GetProductRevision(VL53L0X_DEV Dev,
	uint8_t *pProductRevisionMajor, uint8_t *pProductRevisionMinor);

/**
 * @brief Reads the Device information for given Device
 *
 * @note This function Access to the device
 *
 * @param   Dev                 Device Handle
 * @param   pVL53L0X_DeviceInfo  Pointer to current device info for a given
 *  Device
 * @return  VL53L0X_ERROR_NONE   Success
 * @return  "Other error code"  See ::VL53L0X_Error
 */
VL53L0X_API VL53L0X_Error VL53L0X_GetDeviceInfo(VL53L0X_DEV Dev,
	VL53L0X_DeviceInfo_t *pVL53L0X_DeviceInfo);

/**
 * @brief Read current status of the error register for the selected device
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   pDeviceErrorStatus    Pointer to current error code of the device
 * @return  VL53L0X_ERROR_NONE     Success
 * @return  "Other error code"    See ::VL53L0X_Error
 */
VL53L0X_API VL53L0X_Error VL53L0X_GetDeviceErrorStatus(VL53L0X_DEV Dev,
	VL53L0X_DeviceError * pDeviceErrorStatus);

/**
 * @brief Human readable Range Status string for a given RangeStatus
 *
 * @note This function doesn't access to the device
 *
 * @param   RangeStatus         The RangeStatus code as stored on
 * @a VL53L0X_RangingMeasurementData_t
 * @param   pRangeStatusString  The returned RangeStatus string.
 * @return  VL53L0X_ERROR_NONE   Success
 * @return  "Other error code"  See ::VL53L0X_Error
 */
VL53L0X_API VL53L0X_Error VL53L0X_GetRangeStatusString(uint8_t RangeStatus,
	char *pRangeStatusString);

/**
 * @brief Human readable error string for a given Error Code
 *
 * @note This function doesn't access to the device
 *
 * @param  ErrorCode           The error code as stored on ::VL53L0X_DeviceError
 * @param  pDeviceErrorString  The error string corresponding to the ErrorCode
 * @return VL53L0X_ERROR_NONE   Success
 * @return "Other error code"  See ::VL53L0X_Error
 */
VL53L0X_API VL53L0X_Error VL53L0X_GetDeviceErrorString(
	VL53L0X_DeviceError ErrorCode, char *pDeviceErrorString);

/**
 * @brief Human readable error string for current PAL error status
 *
 * @note This function doesn't access to the device
 *
 * @param   PalErrorCode       The error code as stored on @a VL53L0X_Error
 * @param   pPalErrorString    The error string corresponding to the
 * PalErrorCode
 * @return  VL53L0X_ERROR_NONE  Success
 * @return  "Other error code" See ::VL53L0X_Error
 */
VL53L0X_API VL53L0X_Error VL53L0X_GetPalErrorString(VL53L0X_Error PalErrorCode,
	char *pPalErrorString);

/**
 * @brief Human readable PAL State string
 *
 * @note This function doesn't access to the device
 *
 * @param   PalStateCode          The State code as stored on @a VL53L0X_State
 * @param   pPalStateString       The State string corresponding to the
 * PalStateCode
 * @return  VL53L0X_ERROR_NONE     Success
 * @return  "Other error code"    See ::VL53L0X_Error
 */
VL53L0X_API VL53L0X_Error VL53L0X_GetPalStateString(VL53L0X_State PalStateCode,
	char *pPalStateString);

/**
 * @brief Reads the internal state of the PAL for a given Device
 *
 * @note This function doesn't access to the device
 *
 * @param   Dev                   Device Handle
 * @param   pPalState             Pointer to current state of the PAL for a
 * given Device
 * @return  VL53L0X_ERROR_NONE     Success
 * @return  "Other error code"    See ::VL53L0X_Error
 */
VL53L0X_API VL53L0X_Error VL53L0X_GetPalState(VL53L0X_DEV Dev,
	VL53L0X_State * pPalState);

/**
 * @brief Set the power mode for a given Device
 * The power mode can be Standby or Idle. Different level of both Standby and
 * Idle can exists.
 * This function should not be used when device is in Ranging state.
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   PowerMode             The value of the power mode to set.
 * see ::VL53L0X_PowerModes
 *                                Valid values are:
 *                                VL53L0X_POWERMODE_STANDBY_LEVEL1,
 *                                VL53L0X_POWERMODE_IDLE_LEVEL1
 * @return  VL53L0X_ERROR_NONE                  Success
 * @return  VL53L0X_ERROR_MODE_NOT_SUPPORTED    This error occurs when PowerMode
 * is not in the supported list
 * @return  "Other error code"    See ::VL53L0X_Error
 */
VL53L0X_API VL53L0X_Error VL53L0X_SetPowerMode(VL53L0X_DEV Dev,
	VL53L0X_PowerModes PowerMode);

/**
 * @brief Get the power mode for a given Device
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   pPowerMode            Pointer to the current value of the power
 * mode. see ::VL53L0X_PowerModes
 *                                Valid values are:
 *                                VL53L0X_POWERMODE_STANDBY_LEVEL1,
 *                                VL53L0X_POWERMODE_IDLE_LEVEL1
 * @return  VL53L0X_ERROR_NONE     Success
 * @return  "Other error code"    See ::VL53L0X_Error
 */
VL53L0X_API VL53L0X_Error VL53L0X_GetPowerMode(VL53L0X_DEV Dev,
	VL53L0X_PowerModes * pPowerMode);

/**
 * Set or over-hide part to part calibration offset
 * \sa VL53L0X_DataInit()   VL53L0X_GetOffsetCalibrationDataMicroMeter()
 *
 * @note This function Access to the device
 *
 * @param   Dev                                Device Handle
 * @param   OffsetCalibrationDataMicroMeter    Offset (microns)
 * @return  VL53L0X_ERROR_NONE                  Success
 * @return  "Other error code"                 See ::VL53L0X_Error
 */
VL53L0X_API VL53L0X_Error VL53L0X_SetOffsetCalibrationDataMicroMeter(
	VL53L0X_DEV Dev, int32_t OffsetCalibrationDataMicroMeter);

/**
 * @brief Get part to part calibration offset
 *
 * @par Function Description
 * Should only be used after a successful call to @a VL53L0X_DataInit to backup
 * device NVM value
 *
 * @note This function Access to the device
 *
 * @param   Dev                                Device Handle
 * @param   pOffsetCalibrationDataMicroMeter   Return part to part
 * calibration offset from device (microns)
 * @return  VL53L0X_ERROR_NONE                  Success
 * @return  "Other error code"                 See ::VL53L0X_Error
 */
VL53L0X_API VL53L0X_Error VL53L0X_GetOffsetCalibrationDataMicroMeter(
	VL53L0X_DEV Dev, int32_t *pOffsetCalibrationDataMicroMeter);

/**
 * Set the linearity corrective gain
 *
 * @note This function Access to the device
 *
 * @param   Dev                                Device Handle
 * @param   LinearityCorrectiveGain            Linearity corrective
 * gain in x1000
 * if value is 1000 then no modification is applied.
 * @return  VL53L0X_ERROR_NONE                  Success
 * @return  "Other error code"                 See ::VL53L0X_Error
 */
VL53L0X_API VL53L0X_Error VL53L0X_SetLinearityCorrectiveGain(VL53L0X_DEV Dev,
	int16_t LinearityCorrectiveGain);

/**
 * @brief Get the linearity corrective gain
 *
 * @par Function Description
 * Should only be used after a successful call to @a VL53L0X_DataInit to backup
 * device NVM value
 *
 * @note This function Access to the device
 *
 * @param   Dev                                Device Handle
 * @param   pLinearityCorrectiveGain           Pointer to the linearity
 * corrective gain in x1000
 * if value is 1000 then no modification is applied.
 * @return  VL53L0X_ERROR_NONE                  Success
 * @return  "Other error code"                 See ::VL53L0X_Error
 */
VL53L0X_API VL53L0X_Error VL53L0X_GetLinearityCorrectiveGain(VL53L0X_DEV Dev,
	uint16_t *pLinearityCorrectiveGain);

/**
 * Set Group parameter Hold state
 *
 * @par Function Description
 * Set or remove device internal group parameter hold
 *
 * @note This function is not Implemented
 *
 * @param   Dev      Device Handle
 * @param   GroupParamHold   Group parameter Hold state to be set (on/off)
 * @return  VL53L0X_ERROR_NOT_IMPLEMENTED        Not implemented
 */
VL53L0X_API VL53L0X_Error VL53L0X_SetGroupParamHold(VL53L0X_DEV Dev,
	uint8_t GroupParamHold);

/**
 * @brief Get the maximal distance for actual setup
 * @par Function Description
 * Device must be initialized through @a VL53L0X_SetParameters() prior calling
 * this function.
 *
 * Any range value more than the value returned is to be considered as
 * "no target detected" or
 * "no target in detectable range"\n
 * @warning The maximal distance depends on the setup
 *
 * @note This function is not Implemented
 *
 * @param   Dev      Device Handle
 * @param   pUpperLimitMilliMeter   The maximal range limit for actual setup
 * (in millimeter)
 * @return  VL53L0X_ERROR_NOT_IMPLEMENTED        Not implemented
 */
VL53L0X_API VL53L0X_Error VL53L0X_GetUpperLimitMilliMeter(VL53L0X_DEV Dev,
	uint16_t *pUpperLimitMilliMeter);


/**
 * @brief Get the Total Signal Rate
 * @par Function Description
 * This function will return the Total Signal Rate after a good ranging is done.
 *
 * @note This function access to Device
 *
 * @param   Dev      Device Handle
 * @param   pTotalSignalRate   Total Signal Rate value in Mega count per second
 * @return  VL53L0X_ERROR_NONE     Success
 * @return  "Other error code"    See ::VL53L0X_Error
 */
VL53L0X_Error VL53L0X_GetTotalSignalRate(VL53L0X_DEV Dev,
	FixPoint1616_t *pTotalSignalRate);

/** @} VL53L0X_general_group */

/** @defgroup VL53L0X_init_group VL53L0X Init Functions
 *  @brief    VL53L0X Init Functions
 *  @{
 */

/**
 * @brief Set new device address
 *
 * After completion the device will answer to the new address programmed.
 * This function should be called when several devices are used in parallel
 * before start programming the sensor.
 * When a single device us used, there is no need to call this function.
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle
 * @param   DeviceAddress         The new Device address
 * @return  VL53L0X_ERROR_NONE     Success
 * @return  "Other error code"    See ::VL53L0X_Error
 */
VL53L0X_API VL53L0X_Error VL53L0X_SetDeviceAddress(VL53L0X_DEV Dev,
	uint8_t DeviceAddress);

/**
 *
 * @brief One time device initialization
 *
 * To be called once and only once after device is brought out of reset
 * (Chip enable) and booted see @a VL53L0X_WaitDeviceBooted()
 *
 * @par Function Description
 * When not used after a fresh device "power up" or reset, it may return
 * @a #VL53L0X_ERROR_CALIBRATION_WARNING meaning wrong calibration data
 * may have been fetched from device that can result in ranging offset error\n
 * If application cannot execute device reset or need to run VL53L0X_DataInit
 * multiple time then it  must ensure proper offset calibration saving and
 * restore on its own by using @a VL53L0X_GetOffsetCalibrationData() on first
 * power up and then @a VL53L0X_SetOffsetCalibrationData() in all subsequent
 * init.
 * This function will change the VL53L0X_State from VL53L0X_STATE_POWERDOWN to
 * VL53L0X_STATE_WAIT_STATICINIT.
 *
 * @note This function Access to the device
 *
 * @param   Dev                   Device Handle×xÒÚ$z{-®éÜj×F–öàĞ¢¢6WBF†RçVÖ&W"öb$ô’¦öæW2Fò&RW6VBf÷"7V6–f–2FWf–6RàĞ¢¢F†R&öw&ÖÖVBfÇVR6†÷VÆB&RÆW72F†âF†RÖ‚çVÖ&W"öb$ô’¦öæW2v—fVàĞ¢¢v—F‚dÃS4Ã…ôvWDÖ„çVÖ&W$öe$ô•¦öæW2‚’àĞ¢¢F†—2fW'6–öâöb’ÖævRöæÇ’öæR¦öæRàĞ¢ Ğ¢¢&ÒFWbFWf–6R†æFÆPĞ¢¢&ÒçVÖ&W$öe$ô•¦öæW2çVÖ&W"öb$ô’¦öæW2Fò&RW6VBf÷"Ğ¢¢7V6–f–2FWf–6RàĞ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôäôäR7V66W70Ğ¢¢&WGW&âdÃS4Ã…ôU%$õ%ô”ådÄ”Eõ$Õ2F†—2W'&÷"—2&WGW&æVB–`Ğ¢¢çVÖ&W$öe$ô•¦öæW2ÒĞ¢¢ğĞ¥dÃS4Ã…ô’dÃS4Ã…ôW'&÷"dÃS4Ã…õ6WDçVÖ&W$öe$ô•¦öæW2…dÃS4Ã…ôDUbFWbÀĞ —V–çC…÷BçVÖ&W$öe$ô•¦öæW2“°Ğ Ğ¢ò¢ Ğ¢¢'&–VbvWBF†RçVÖ&W"öb$ô’¦öæW2ÖævVB'’F†RFWf–6PĞ¢ Ğ¢¢"gVæ7F–öâFW67&—F–öàĞ¢¢vWBçVÖ&W"öb$ô’¦öæW2ÖævVB'’F†RFWf–6PĞ¢¢U4U"6†÷VÆBF¶R6&R&÷WBdÃS4Ã…ôvWDçVÖ&W$öe$ô•¦öæW2‚Ğ¢¢&Vf÷&RvWBFFgFW"W&f÷&ÒÖV7W&VÖVçBàĞ¢¢Âv–ÆÂf–ÆÂçVÖ&W$öe$ô•¦öæW2F–ÖW2F†R6÷'&W7öæF–ærFFĞ¢¢7G'V7GW&RW6VB–âF†RÖV7W&VÖVçBgVæ7F–öâàĞ¢ Ğ¢¢æ÷FRF†—2gVæ7F–öâFöW6âwB66W72FòF†RFWf–6PĞ¢ Ğ¢¢&ÒFWbFWf–6R†æFÆPĞ¢¢&ÒçVÖ&W$öe$ô•¦öæW2ö–çFW"FòF†RçVÖ&W"öb$ô’¦öæW2fÇVRàĞ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôäôäR7V66W70Ğ¢¢ğĞ¥dÃS4Ã…ô’dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWDçVÖ&W$öe$ô•¦öæW2…dÃS4Ã…ôDUbFWbÀĞ —V–çC…÷B§çVÖ&W$öe$ô•¦öæW2“°Ğ Ğ¢ò¢ Ğ¢¢'&–VbvWBF†RÖ†–×VÒçVÖ&W"öb$ô’¦öæW2ÖævVB'’F†RFWf–6PĞ¢ Ğ¢¢"gVæ7F–öâFW67&—F–öàĞ¢¢vWBÖ†–×VÒçVÖ&W"öb$ô’¦öæW2ÖævVB'’F†RFWf–6RàĞ¢ Ğ¢¢æ÷FRF†—2gVæ7F–öâFöW6âwB66W72FòF†RFWf–6PĞ¢ Ğ¢¢&ÒFWbFWf–6R†æFÆPĞ¢¢&ÒÖ„çVÖ&W$öe$ô•¦öæW2ö–çFW"FòF†RÖ†–×VÒçVÖ&W Ğ¢¢öb$ô’¦öæW2fÇVRàĞ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôäôäR7V66W70Ğ¢¢ğĞ¥dÃS4Ã…ô’dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWDÖ„çVÖ&W$öe$ô•¦öæW2…dÃS4Ã…ôDUbFWbÀĞ —V–çC…÷B§Ö„çVÖ&W$öe$ô•¦öæW2“°Ğ Ğ¢ò¢¢ÒdÃS4Ã…öÖV7W&VÖVçEöw&÷W¢ğĞ Ğ¢ò¢¢FVfw&÷WdÃS4Ã…ö–çFW''WEöw&÷WdÃS4Ã‚–çFW''WBgVæ7F–öç0Ğ¢¢'&–VbgVæ7F–öç2W6VBf÷"–çFW''WBÖævVÖVçG0Ğ¢¢°Ğ¢¢ğĞ Ğ¢ò¢ Ğ¢¢'&–Vb6WBF†R6öæf–wW&F–öâöbu”ò–âf÷"v—fVâFWf–6PĞ¢ Ğ¢¢æ÷FRF†—2gVæ7F–öâ66W72FòF†RFWf–6PĞ¢ Ğ¢¢&ÒFWbFWf–6R†æFÆPĞ¢¢&Ò–â”BöbF†Ru”ò–àĞ¢¢&ÒgVæ7F–öæÆ—G’6VÆV7B–âgVæ7F–öæÆ—G’àĞ¢¢&VfW"Fò£¥dÃS4Ã…ôw–ôgVæ7F–öæÆ—GĞ¢¢&ÒFWf–6TÖöFRFWf–6RÖöFR76ö6–FVBFòF†Rw–òàĞ¢¢&ÒöÆ&—G’6WB–çFW''WBöÆ&—G’â7F—fR†–v€Ğ¢¢÷"7F—fRÆ÷r6VR£¥dÃS4Ã…ô–çFW''WEöÆ&—GĞ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôäôäR7V66W70Ğ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôu”õôäõEôU„•5D”äröæÇ’–ãÓ—266WFV@Ğ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôu”õôeTä5D”ôäÄ•E•ôäõEõ5Uõ%DTBF†—2W'&÷"ö67W'0Ğ¢¢v†VâgVæ7F–öæÆ—G’&öw&ÖÖVB—2æ÷B–âF†R7W÷'FVBÆ—7C Ğ¢¢7W÷'FVBfÇVR&S Ğ¢¢dÃS4Ã…ôu”ôeTä5D”ôäÄ•E•ôôdbÀĞ¢¢dÃS4Ã…ôu”ôeTä5D”ôäÄ•E•õD…$U4„ôÄEô5$õ54TEôÄõrÀĞ¢¢dÃS4Ã…ôu”ôeTä5D”ôäÄ•E•õD…$U4„ôÄEô5$õ54TEô„”t‚ÀĞ¢dÃS4Ã…ôu”ôeTä5D”ôäÄ•E•õD…$U4„ôÄEô5$õ54TEôõUBÀĞ¢¢dÃS4Ã…ôu”ôeTä5D”ôäÄ•E•ôäUuôÔT5U$Uõ$TEĞ¢¢&WGW&â$÷F†W"W'&÷"6öFR"6VR£¥dÃS4Ã…ôW'&÷ Ğ¢¢ğĞ¥dÃS4Ã…ô’dÃS4Ã…ôW'&÷"dÃS4Ã…õ6WDw–ô6öæf–r…dÃS4Ã…ôDUbFWbÂV–çC…÷B–âÀĞ •dÃS4Ã…ôFWf–6TÖöFW2FWf–6TÖöFRÂdÃS4Ã…ôw–ôgVæ7F–öæÆ—G’gVæ7F–öæÆ—G’ÀĞ •dÃS4Ã…ô–çFW''WEöÆ&—G’öÆ&—G’“°Ğ Ğ¢ò¢ Ğ¢¢'&–VbvWB7W'&VçB6öæf–wW&F–öâf÷"u”ò–âf÷"v—fVâFWf–6PĞ¢ Ğ¢¢æ÷FRF†—2gVæ7F–öâ66W72FòF†RFWf–6PĞ¢ Ğ¢¢&ÒFWbFWf–6R†æFÆPĞ¢¢&Ò–â”BöbF†Ru”ò–àĞ¢¢&ÒFWf–6TÖöFRö–çFW"FòFWf–6RÖöFR76ö6–FVBFòF†Rw–òàĞ¢¢&ÒgVæ7F–öæÆ—G’ö–çFW"Fò–âgVæ7F–öæÆ—G’àĞ¢¢&VfW"Fò£¥dÃS4Ã…ôw–ôgVæ7F–öæÆ—GĞ¢¢&ÒöÆ&—G’ö–çFW"Fò–çFW''WBöÆ&—G’àĞ¢¢7F—fR†–v‚÷"7F—fRÆ÷r6VR£¥dÃS4Ã…ô–çFW''WEöÆ&—GĞ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôäôäR7V66W70Ğ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôu”õôäõEôU„•5D”äröæÇ’–ãÓ—266WFV@Ğ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôu”õôeTä5D”ôäÄ•E•ôäõEõ5Uõ%DTBF†—2W'&÷"ö67W'0Ğ¢¢v†VâgVæ7F–öæÆ—G’&öw&ÖÖVB—2æ÷B–âF†R7W÷'FVBÆ—7C Ğ¢¢7W÷'FVBfÇVR&S Ğ¢¢dÃS4Ã…ôu”ôeTä5D”ôäÄ•E•ôôdbÀĞ¢¢dÃS4Ã…ôu”ôeTä5D”ôäÄ•E•õD…$U4„ôÄEô5$õ54TEôÄõrÀĞ¢¢dÃS4Ã…ôu”ôeTä5D”ôäÄ•E•õD…$U4„ôÄEô5$õ54TEô„”t‚ÀĞ¢¢dÃS4Ã…ôu”ôeTä5D”ôäÄ•E•õD…$U4„ôÄEô5$õ54TEôõUBÀĞ¢¢dÃS4Ã…ôu”ôeTä5D”ôäÄ•E•ôäUuôÔT5U$Uõ$TEĞ¢¢&WGW&â$÷F†W"W'&÷"6öFR"6VR£¥dÃS4Ã…ôW'&÷ Ğ¢¢ğĞ¥dÃS4Ã…ô’dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWDw–ô6öæf–r…dÃS4Ã…ôDUbFWbÂV–çC…÷B–âÀĞ •dÃS4Ã…ôFWf–6TÖöFW2¢FWf–6TÖöFRÀĞ •dÃS4Ã…ôw–ôgVæ7F–öæÆ—G’¢gVæ7F–öæÆ—G’ÀĞ •dÃS4Ã…ô–çFW''WEöÆ&—G’¢öÆ&—G’“°Ğ Ğ¢ò¢ Ğ¢¢'&–Vb6WBÆ÷ræB†–v‚–çFW''WBF‡&W6†öÆG2f÷"v—fVâÖöFPĞ¢¢‡&æv–ærÂÅ2Ââââ’f÷"v—fVâFWf–6PĞ¢ Ğ¢¢"gVæ7F–öâFW67&—F–öàĞ¢¢6WBÆ÷ræB†–v‚–çFW''WBF‡&W6†öÆG2f÷"v—fVâÖöFR‡&æv–ærÂÅ2ÂâââĞ¢¢f÷"v—fVâFWf–6PĞ¢ Ğ¢¢æ÷FRF†—2gVæ7F–öâ66W72FòF†RFWf–6PĞ¢ Ğ¢¢æ÷FRFWf–6TÖöFR—2–væ÷&VBf÷"F†R7W'&VçBFWf–6PĞ¢ Ğ¢¢&ÒFWbFWf–6R†æFÆPĞ¢¢&ÒFWf–6TÖöFRFWf–6RÖöFRf÷"v†–6‚6†ævRF‡&W6†öÆG0Ğ¢¢&ÒF‡&W6†öÆDÆ÷rÆ÷rF‡&W6†öÆB†ÖÒÂÇW‚âââÂFWVæF–æröâF†RÖöFRĞ¢¢&ÒF‡&W6†öÆD†–v‚†–v‚F‡&W6†öÆB†ÖÒÂÇW‚âââÂFWVæF–æröâF†RÖöFRĞ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôäôäR7V66W70Ğ¢¢&WGW&â$÷F†W"W'&÷"6öFR"6VR£¥dÃS4Ã…ôW'&÷ Ğ¢¢ğĞ¥dÃS4Ã…ô’dÃS4Ã…ôW'&÷"dÃS4Ã…õ6WD–çFW''WEF‡&W6†öÆG2…dÃS4Ã…ôDUbFWbÀĞ •dÃS4Ã…ôFWf–6TÖöFW2FWf–6TÖöFRÂf—…ö–çCce÷BF‡&W6†öÆDÆ÷rÀĞ ”f—…ö–çCce÷BF‡&W6†öÆD†–v‚“°Ğ Ğ¢ò¢ Ğ¢¢'&–VbvWB†–v‚æBÆ÷r–çFW''WBF‡&W6†öÆG2f÷"v—fVâÖöFPĞ¢¢‡&æv–ærÂÅ2Ââââ’f÷"v—fVâFWf–6PĞ¢ Ğ¢¢"gVæ7F–öâFW67&—F–öàĞ¢¢vWB†–v‚æBÆ÷r–çFW''WBF‡&W6†öÆG2f÷"v—fVâÖöFR‡&æv–ærÂÅ2ÂâââĞ¢¢f÷"v—fVâFWf–6PĞ¢ Ğ¢¢æ÷FRF†—2gVæ7F–öâ66W72FòF†RFWf–6PĞ¢ Ğ¢¢æ÷FRFWf–6TÖöFR—2–væ÷&VBf÷"F†R7W'&VçBFWf–6PĞ¢ Ğ¢¢&ÒFWbFWf–6R†æFÆPĞ¢¢&ÒFWf–6TÖöFRFWf–6RÖöFRg&öÒv†–6‚&VBF‡&W6†öÆG0Ğ¢¢&ÒF‡&W6†öÆDÆ÷rÆ÷rF‡&W6†öÆB†ÖÒÂÇW‚âââÂFWVæF–æröâF†RÖöFRĞ¢¢&ÒF‡&W6†öÆD†–v‚†–v‚F‡&W6†öÆB†ÖÒÂÇW‚âââÂFWVæF–æröâF†RÖöFRĞ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôäôäR7V66W70Ğ¢¢&WGW&â$÷F†W"W'&÷"6öFR"6VR£¥dÃS4Ã…ôW'&÷ Ğ¢¢ğĞ¥dÃS4Ã…ô’dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWD–çFW''WEF‡&W6†öÆG2…dÃS4Ã…ôDUbFWbÀĞ •dÃS4Ã…ôFWf–6TÖöFW2FWf–6TÖöFRÂf—…ö–çCce÷B§F‡&W6†öÆDÆ÷rÀĞ ”f—…ö–çCce÷B§F‡&W6†öÆD†–v‚“°Ğ Ğ¢ò¢ Ğ¢¢'&–Vb&WGW&âFWf–6R7F÷6ö×ÆWF–öâ7FGW0Ğ¢ Ğ¢¢"gVæ7F–öâFW67&—F–öàĞ¢¢&WGW&ç27F÷6ö×ÆWF–ö"7FGW2àĞ¢¢W6W"6†ÆÂ6ÆÂF†—2gVæ7F–öâgFW"7F÷6öÖÖæ@Ğ¢ Ğ¢¢æ÷FRF†—2gVæ7F–öâ66W72FòF†RFWf–6PĞ¢ Ğ¢¢&ÒFWbFWf–6R†æFÆPĞ¢¢&Ò7F÷7FGW2ö–çFW"Fò7FGW2f&–&ÆRFòWFFPĞ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôäôäR7V66W70Ğ¢¢&WGW&â$÷F†W"W'&÷"6öFR"6VR£¥dÃS4Ã…ôW'&÷ Ğ¢¢ğĞ¥dÃS4Ã…ô’dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWE7F÷6ö×ÆWFVE7FGW2…dÃS4Ã…ôDUbFWbÀĞ —V–çC3%÷B§7F÷7FGW2“°Ğ Ğ Ğ¢ò¢ Ğ¢¢'&–Vb6ÆV"v—fVâ7—7FVÒ–çFW''WB6öæF—F–öàĞ¢ Ğ¢¢"gVæ7F–öâFW67&—F–öàĞ¢¢6ÆV"v—fVâ–çFW''WB‡2’àĞ¢ Ğ¢¢æ÷FRF†—2gVæ7F–öâ66W72FòF†RFWf–6PĞ¢ Ğ¢¢&ÒFWbFWf–6R†æFÆPĞ¢¢&Ò–çFW''WDÖ6²Ö6²öb–çFW''WG2Fò6ÆV Ğ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôäôäR7V66W70Ğ¢¢&WGW&âdÃS4Ã…ôU%$õ%ô”åDU%%UEôäõEô4ÄT$TB6ææ÷B6ÆV"–çFW''WG0Ğ¢ Ğ¢¢&WGW&â$÷F†W"W'&÷"6öFR"6VR£¥dÃS4Ã…ôW'&÷ Ğ¢¢ğĞ¥dÃS4Ã…ô’dÃS4Ã…ôW'&÷"dÃS4Ã…ô6ÆV$–çFW''WDÖ6²…dÃS4Ã…ôDUbFWbÀĞ —V–çC3%÷B–çFW''WDÖ6²“°Ğ Ğ¢ò¢ Ğ¢¢'&–Vb&WGW&âFWf–6R–çFW''WB7FGW0Ğ¢ Ğ¢¢"gVæ7F–öâFW67&—F–öàĞ¢¢&WGW&ç27W'&VçFÇ’&—6VB–çFW''WG2'’F†RFWf–6RàĞ¢¢W6W"6†ÆÂ&R&ÆRFò7F—fFRöFV7F—fFR–çFW''WG2F‡&÷Vv€Ğ¢¢dÃS4Ã…õ6WDw–ô6öæf–r‚Ğ¢ Ğ¢¢æ÷FRF†—2gVæ7F–öâ66W72FòF†RFWf–6PĞ¢ Ğ¢¢&ÒFWbFWf–6R†æFÆPĞ¢¢&Ò–çFW''WDÖ6µ7FGW2ö–çFW"Fò7FGW2f&–&ÆRFòWFFPĞ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôäôäR7V66W70Ğ¢¢&WGW&â$÷F†W"W'&÷"6öFR"6VR£¥dÃS4Ã…ôW'&÷ Ğ¢¢ğĞ¥dÃS4Ã…ô’dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWD–çFW''WDÖ6µ7FGW2…dÃS4Ã…ôDUbFWbÀĞ —V–çC3%÷B§–çFW''WDÖ6µ7FGW2“°Ğ Ğ¢ò¢ Ğ¢¢'&–Vb6öæf–wW&R&æv–ær–çFW''WB&W÷'FVBFò7—7FVĞĞ¢ Ğ¢¢æ÷FRF†—2gVæ7F–öâ—2æ÷B–×ÆVÖVçFV@Ğ¢ Ğ¢¢&ÒFWbFWf–6R†æFÆPĞ¢¢&Ò–çFW''WDÖ6²Ö6²öb–çFW''WBFòVæ&ÆRöF—6&ÆPĞ¢¢ƒ¦–çFW''WBF—6&ÆVB÷"¢–çFW''WBVæ&ÆVBĞ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôäõEô”ÕÄTÔTåDTBæ÷B–×ÆVÖVçFV@Ğ¢¢ğĞ¥dÃS4Ã…ô’dÃS4Ã…ôW'&÷"dÃS4Ã…ôVæ&ÆT–çFW''WDÖ6²…dÃS4Ã…ôDUbFWbÀĞ —V–çC3%÷B–çFW''WDÖ6²“°Ğ Ğ¢ò¢¢ÒdÃS4Ã…ö–çFW''WEöw&÷W¢ğĞ Ğ¢ò¢¢FVfw&÷WdÃS4Ã…õ5FgVæ7F–öç5öw&÷WdÃS4Ã‚5BgVæ7F–öç0Ğ¢¢'&–VbgVæ7F–öç2W6VBf÷"5BÖævVÖVçG0Ğ¢¢°Ğ¢¢ğĞ Ğ¢ò¢ Ğ¢¢'&–Vb6WBF†R5BÖ&–VçBF×W"F‡&W6†öÆBfÇVPĞ¢ Ğ¢¢"gVæ7F–öâFW67&—F–öàĞ¢¢F†—2gVæ7F–öâ6WBF†R5BÖ&–VçBF×W"F‡&W6†öÆBfÇVPĞ¢ Ğ¢¢æ÷FRF†—2gVæ7F–öâ66W72FòF†RFWf–6PĞ¢ Ğ¢¢&ÒFWbFWf–6R†æFÆPĞ¢¢&Ò7DÖ&–VçDF×W%F‡&W6†öÆB5BÖ&–VçBF×W"F‡&W6†öÆBfÇVPĞ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôäôäR7V66W70Ğ¢¢&WGW&â$÷F†W"W'&÷"6öFR"6VR£¥dÃS4Ã…ôW'&÷ Ğ¢¢ğĞ¥dÃS4Ã…ô’dÃS4Ã…ôW'&÷"dÃS4Ã…õ6WE7DÖ&–VçDF×W%F‡&W6†öÆB…dÃS4Ã…ôDUbFWbÀĞ —V–çCe÷B7DÖ&–VçDF×W%F‡&W6†öÆB“°Ğ Ğ¢ò¢ Ğ¢¢'&–VbvWBF†R7W'&VçB5BÖ&–VçBF×W"F‡&W6†öÆBfÇVPĞ¢ Ğ¢¢"gVæ7F–öâFW67&—F–öàĞ¢¢F†—2gVæ7F–öâvWBF†R5BÖ&–VçBF×W"F‡&W6†öÆBfÇVPĞ¢ Ğ¢¢æ÷FRF†—2gVæ7F–öâ66W72FòF†RFWf–6PĞ¢ Ğ¢¢&ÒFWbFWf–6R†æFÆPĞ¢¢&Ò7DÖ&–VçDF×W%F‡&W6†öÆBö–çFW"Fò&öw&ÖÖV@Ğ¢¢5BÖ&–VçBF×W"F‡&W6†öÆBfÇVPĞ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôäôäR7V66W70Ğ¢¢&WGW&â$÷F†W"W'&÷"6öFR"6VR£¥dÃS4Ã…ôW'&÷ Ğ¢¢ğĞ¥dÃS4Ã…ô’dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWE7DÖ&–VçDF×W%F‡&W6†öÆB…dÃS4Ã…ôDUbFWbÀĞ —V–çCe÷B§7DÖ&–VçDF×W%F‡&W6†öÆB“°Ğ Ğ¢ò¢ Ğ¢¢'&–Vb6WBF†R5BÖ&–VçBF×W"f7F÷"fÇVPĞ¢ Ğ¢¢"gVæ7F–öâFW67&—F–öàĞ¢¢F†—2gVæ7F–öâ6WBF†R5BÖ&–VçBF×W"f7F÷"fÇVPĞ¢ Ğ¢¢æ÷FRF†—2gVæ7F–öâ66W72FòF†RFWf–6PĞ¢ Ğ¢¢&ÒFWbFWf–6R†æFÆPĞ¢¢&Ò7DÖ&–VçDF×W$f7F÷"5BÖ&–VçBF×W"f7F÷"fÇVPĞ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôäôäR7V66W70Ğ¢¢&WGW&â$÷F†W"W'&÷"6öFR"6VR£¥dÃS4Ã…ôW'&÷ Ğ¢¢ğĞ¥dÃS4Ã…ô’dÃS4Ã…ôW'&÷"dÃS4Ã…õ6WE7DÖ&–VçDF×W$f7F÷"…dÃS4Ã…ôDUbFWbÀĞ —V–çCe÷B7DÖ&–VçDF×W$f7F÷"“°Ğ Ğ¢ò¢ Ğ¢¢'&–VbvWBF†R7W'&VçB5BÖ&–VçBF×W"f7F÷"fÇVPĞ¢ Ğ¢¢"gVæ7F–öâFW67&—F–öàĞ¢¢F†—2gVæ7F–öâvWBF†R5BÖ&–VçBF×W"f7F÷"fÇVPĞ¢ Ğ¢¢æ÷FRF†—2gVæ7F–öâ66W72FòF†RFWf–6PĞ¢ Ğ¢¢&ÒFWbFWf–6R†æFÆPĞ¢¢&Ò7DÖ&–VçDF×W$f7F÷"ö–çFW"Fò&öw&ÖÖVB5BÖ&–Vç@Ğ¢¢F×W"f7F÷"fÇVPĞ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôäôäR7V66W70Ğ¢¢&WGW&â$÷F†W"W'&÷"6öFR"6VR£¥dÃS4Ã…ôW'&÷ Ğ¢¢ğĞ¥dÃS4Ã…ô’dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWE7DÖ&–VçDF×W$f7F÷"…dÃS4Ã…ôDUbFWbÀĞ —V–çCe÷B§7DÖ&–VçDF×W$f7F÷"“°Ğ Ğ¢ò¢ Ğ¢¢'&–VbW&f÷&×2&VfW&Væ6R7BÖævVÖVç@Ğ¢ Ğ¢¢"gVæ7F–öâFW67&—F–öàĞ¢¢F†R&VfW&Væ6R5B–æ—F–Æ—¦F–öâ&ö6VGW&RFWFW&Ö–æW2F†RÖ–æ–×VÒÖ÷Vç@Ğ¢¢öb&VfW&Væ6R7G2Fò&RVæ&ÆW2Fò6†–WfRF&vWB&VfW&Væ6R6–væÂ&FPĞ¢¢æB6†÷VÆB&RW&f÷&ÖVBöæ6RGW&–ær–æ—F–Æ—¦F–öâàĞ¢ Ğ¢¢æ÷FRF†—2gVæ7F–öâ66W72FòF†RFWf–6PĞ¢ Ğ¢¢æ÷FRF†—2gVæ7F–öâ6†ævRF†RFWf–6RÖöFRFğĞ¢¢dÃS4Ã…ôDUd”4TÔôDUõ4”ätÄUõ$ät”äpĞ¢ Ğ¢¢&ÒFWbFWf–6R†æFÆPĞ¢¢&Ò&Ve7D6÷VçB&W÷'G2&Vb7B6÷Vç@Ğ¢¢&Ò—4W'GW&U7G2&W÷'G2–b7G2&RöbG—PĞ¢¢W'GW&R÷"æöâÖW'GW&RàĞ¢¢£ÖW'GW&RÂ£ÔæöâÔW'GW&PĞ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôäôäR7V66W70Ğ¢¢&WGW&âdÃS4Ã…ôU%$õ%õ$Teõ5Eô”ä•BW'&÷"–âF†R&Vb7B&ö6VGW&RàĞ¢¢&WGW&â$÷F†W"W'&÷"6öFR"6VR£¥dÃS4Ã…ôW'&÷ Ğ¢¢ğĞ¥dÃS4Ã…ô’dÃS4Ã…ôW'&÷"dÃS4Ã…õW&f÷&Õ&Ve7DÖævVÖVçB…dÃS4Ã…ôDUbFWbÀĞ —V–çC3%÷B§&Ve7D6÷VçBÂV–çC…÷B¦—4W'GW&U7G2“°Ğ Ğ¢ò¢ Ğ¢¢'&–VbÆ–W2&VfW&Væ6R5B6öæf–wW&F–öàĞ¢ Ğ¢¢"gVæ7F–öâFW67&—F–öàĞ¢¢F†—2gVæ7F–öâÆ–W2v—fVâçVÖ&W"öb&VfW&Væ6R7G2Â–FVçF–f–VB0Ğ¢¢V—F†W"W'GW&R÷"æöâÔW'GW&RàĞ¢¢F†R&WVW7FVB7B6÷VçBæBG—R&R7F÷&VBv—F†–âF†RFWf–6R7V6–f–0Ğ¢¢&ÖWFW'2FFf÷"66W72'’F†R†÷7BàĞ¢ Ğ¢¢æ÷FRF†—2gVæ7F–öâ66W72FòF†RFWf–6PĞ¢ Ğ¢¢&ÒFWbFWf–6R†æFÆPĞ¢¢&Ò&Ve7D6÷VçBçVÖ&W"öb&Vb7G2àĞ¢¢&Ò—4W'GW&U7G2FVf–æW2–b7G2&RöbG—PĞ¢¢W'GW&R÷"æöâÖW'GW&RàĞ¢¢£ÖW'GW&RÂ£ÔæöâÔW'GW&PĞ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôäôäR7V66W70Ğ¢¢&WGW&âdÃS4Ã…ôU%$õ%õ$Teõ5Eô”ä•BW'&÷"–âF†R–âF†R&VfW&Væ6PĞ¢¢7B6öæf–wW&F–öâàĞ¢¢&WGW&â$÷F†W"W'&÷"6öFR"6VR£¥dÃS4Ã…ôW'&÷ Ğ¢¢ğĞ¥dÃS4Ã…ô’dÃS4Ã…ôW'&÷"dÃS4Ã…õ6WE&VfW&Væ6U7G2…dÃS4Ã…ôDUbFWbÀĞ —V–çC3%÷B&Ve7D6÷VçBÂV–çC…÷B—4W'GW&U7G2“°Ğ Ğ¢ò¢ Ğ¢¢'&–Vb&WG&–WfW25B6öæf–wW&F–öàĞ¢ Ğ¢¢"gVæ7F–öâFW67&—F–öàĞ¢¢F†—2gVæ7F–öâ&WG&–WfW2F†R7W'&VçBçVÖ&W"öbÆ–VB&VfW&Væ6R7G0Ğ¢¢æBÇ6òF†V—"G—R¢W'GW&R÷"æöâÔW'GW&RàĞ¢ Ğ¢¢æ÷FRF†—2gVæ7F–öâ66W72FòF†RFWf–6PĞ¢ Ğ¢¢&ÒFWbFWf–6R†æFÆPĞ¢¢&Ò&Ve7D6÷VçBçVÖ&W"&Vb7B6÷Vç@Ğ¢¢&Ò—4W'GW&U7G2&W÷'G2–b7G2&RöbG—PĞ¢¢W'GW&R÷"æöâÖW'GW&RàĞ¢¢£ÖW'GW&RÂ£ÔæöâÔW'GW&PĞ¢¢&WGW&âdÃS4Ã…ôU%$õ%ôäôäR7V66W70Ğ¢¢&WGW&âdÃS4Ã…ôU%$õ%õ$Teõ5Eô”ä•BW'&÷"–âF†R–âF†R&VfW&Væ6PĞ¢¢7B6öæf–wW&F–öâàĞ¢¢&WGW&â$÷F†W"W'&÷"6öFR"6VR£¥dÃS4Ã…ôW'&÷ Ğ¢¢ğĞ¥dÃS4Ã…ô’dÃS4Ã…ôW'&÷"dÃS4Ã…ôvWE&VfW&Væ6U7G2…dÃS4Ã…ôDUbFWbÀĞ —V–çC3%÷B§&Ve7D6÷VçBÂV–çC…÷B¦—4W'GW&U7G2“°Ğ Ğ¢ò¢¢ÒdÃS4Ã…õ5FgVæ7F–öç5öw&÷W¢ğĞ Ğ¢ò¢¢ÒdÃS4Ã…ö7WCöw&÷W¢ğĞ Ğ¢6–fFVbõö7ÇW7ÇW0Ğ§ĞĞ¢6VæF–`Ğ Ğ¢6VæF–bò¢õdÃS4Ã…ô•ô…ò¢ğĞ 