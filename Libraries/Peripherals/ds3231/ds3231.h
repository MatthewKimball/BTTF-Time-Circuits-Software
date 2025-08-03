/******************************************************************************
 * @file    ds3231.h
 * @brief   [Short description of this file]
 *
 * @author  Matthew Kimball
 * @date    Aug 3, 2025
 *
 * Copyright (c) 2025 Matthew Kimball.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 ******************************************************************************/
#ifndef __DS3231_H
#define __DS3231_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdint.h>

/*
 * DS3231 I2C 7-bit address is 0x68
 * HAL expects left-shifted 8-bit address (0xD0 for write / 0xD1 for read)
 */
#define DS3231_I2C_ADDR   (0x68 << 1)

/**
 * @brief  Initialize the DS3231 by checking device presence.
 * @param  hi2c : Pointer to the I2C HAL handle
 * @retval HAL_StatusTypeDef (HAL_OK if ACK received)
 */
HAL_StatusTypeDef DS3231_Init(I2C_HandleTypeDef *hi2c);

/**
 * @brief  Set the current date and time in the DS3231.
 *         Uses HAL RTC typedefs for compatibility with STM32 code.
 *         Supports 12-hour mode with AM/PM.
 * @param  hi2c : Pointer to the I2C HAL handle
 * @param  time : Pointer to RTC_TimeTypeDef (Hours, Minutes, Seconds, TimeFormat)
 * @param  date : Pointer to RTC_DateTypeDef (WeekDay, Date, Month, Year [0-99])
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef DS3231_SetDateTime(I2C_HandleTypeDef *hi2c,
                                     RTC_TimeTypeDef *time,
                                     RTC_DateTypeDef *date);

/**
 * @brief  Get the current date and time from the DS3231.
 *         Populates HAL RTC typedefs for direct reuse in existing code.
 * @param  hi2c : Pointer to the I2C HAL handle
 * @param  time : Pointer to RTC_TimeTypeDef (populated by function)
 * @param  date : Pointer to RTC_DateTypeDef (populated by function)
 * @retval HAL_StatusTypeDef
 */
HAL_StatusTypeDef DS3231_GetDateTime(I2C_HandleTypeDef *hi2c,
                                     RTC_TimeTypeDef *time,
                                     RTC_DateTypeDef *date);

#ifdef __cplusplus
}
#endif

#endif /* __DS3231_H */
