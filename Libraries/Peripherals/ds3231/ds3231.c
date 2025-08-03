/******************************************************************************
 * @file    ds3231.c
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
#include "stm32f4xx_hal.h"
#include <stdint.h>

#define DS3231_I2C_ADDR   (0x68 << 1) // HAL expects 8-bit address

static uint8_t dec2bcd(uint8_t val) { return (val / 10 * 16) + (val % 10); }
static uint8_t bcd2dec(uint8_t val) { return (val / 16 * 10) + (val % 16); }

// Initialize DS3231 (check if device is present)
HAL_StatusTypeDef DS3231_Init(I2C_HandleTypeDef *hi2c)
{
    return HAL_I2C_IsDeviceReady(hi2c, DS3231_I2C_ADDR, 3, 100);
}

// Set DS3231 time and date using HAL RTC typedefs
HAL_StatusTypeDef DS3231_SetDateTime(I2C_HandleTypeDef *hi2c,
                                     RTC_TimeTypeDef *time,
                                     RTC_DateTypeDef *date)
{
    uint8_t buf[7];

    buf[0] = dec2bcd(time->Seconds);
    buf[1] = dec2bcd(time->Minutes);

    // ---- Hours ----
    uint8_t hour_bcd = dec2bcd(time->Hours % 12);
    if(hour_bcd == 0) hour_bcd = 0x12;     // 12-hour mode uses 12 instead of 0
    hour_bcd |= 0x40;                      // 12-hour mode
    if(time->TimeFormat) hour_bcd |= 0x20; // PM if TimeFormat = 1
    buf[2] = hour_bcd;

    buf[3] = dec2bcd(date->WeekDay);
    buf[4] = dec2bcd(date->Date);
    buf[5] = dec2bcd(date->Month);
    buf[6] = dec2bcd(date->Year);

    return HAL_I2C_Mem_Write(hi2c, DS3231_I2C_ADDR, 0x00,
                             I2C_MEMADD_SIZE_8BIT, buf, 7, 100);
}

// Get DS3231 time and date into HAL RTC typedefs
HAL_StatusTypeDef DS3231_GetDateTime(I2C_HandleTypeDef *hi2c,
                                     RTC_TimeTypeDef *time,
                                     RTC_DateTypeDef *date)
{
    uint8_t buf[7];
    HAL_StatusTypeDef status;

    status = HAL_I2C_Mem_Read(hi2c, DS3231_I2C_ADDR, 0x00,
                              I2C_MEMADD_SIZE_8BIT, buf, 7, 100);
    if(status != HAL_OK) return status;

    time->Seconds = bcd2dec(buf[0] & 0x7F);
    time->Minutes = bcd2dec(buf[1] & 0x7F);

    if(buf[2] & 0x40) // 12-hour mode
    {
        time->Hours = bcd2dec(buf[2] & 0x1F);
        if(time->Hours == 0) time->Hours = 12;
        time->TimeFormat = (buf[2] & 0x20) ? 1 : 0; // 0=AM,1=PM
    }
    else // 24-hour fallback
    {
        uint8_t hours_24 = bcd2dec(buf[2] & 0x3F);
        time->TimeFormat = (hours_24 >= 12) ? 1 : 0;
        time->Hours = (hours_24 % 12 == 0) ? 12 : (hours_24 % 12);
    }

    date->WeekDay = bcd2dec(buf[3]);
    date->Date    = bcd2dec(buf[4]);
    date->Month   = bcd2dec(buf[5] & 0x1F);
    date->Year    = bcd2dec(buf[6]); // last 2 digits

    return HAL_OK;
}
