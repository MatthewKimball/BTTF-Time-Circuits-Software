/*
 * imu_bno055.c
 *
 *  Created on: Jul 23, 2025
 *      Author: Matthew Kimball
 * 
 * Description:
 * 
 */

#include "imu.h"
#include "bno055.h"
#include "stm32f4xx_hal.h"
#include "gpio.h"
#include <stdbool.h>
#include <stdint.h>

#define ENABLED 1
#define DISABLED 0

extern struct bno055_t bno055;
extern I2C_HandleTypeDef hi2c1;
extern s8 BNO055_I2C_bus_write(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt);
extern s8 BNO055_I2C_bus_read(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt);
extern void BNO055_delay_msek(u32 msek);


static uint32_t first_hit_time = 0;
static bool waiting_for_second_hit = false;
static const uint32_t double_hit_window_ms = 1000;

volatile bool gGlitchDoubleHit = false;


void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == IMU_INTERRUPT_Pin)
      {
          uint32_t now = HAL_GetTick();

          if (!waiting_for_second_hit)
          {
              // First hit detected
              first_hit_time = now;
              waiting_for_second_hit = true;
              gGlitchDoubleHit = false;
          }
          else if ((now - first_hit_time) <= double_hit_window_ms)
          {
              // Second hit detected within time window → Double Hit!
              waiting_for_second_hit = false;
              gGlitchDoubleHit = true;
          }
          else
          {
              // Too late — treat as new first hit
              first_hit_time = now;
          }

          // Clear interrupt
          bno055_set_intr_rst(ENABLED);
      }
}

s8 BNO055_I2C_bus_write(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
    if (HAL_I2C_Mem_Write(&hi2c1, dev_addr << 1, reg_addr,
                          I2C_MEMADD_SIZE_8BIT, reg_data, cnt, 100) == HAL_OK)
        return 0;
    else
        return -1;
}

s8 BNO055_I2C_bus_read(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt)
{
    if (HAL_I2C_Mem_Read(&hi2c1, dev_addr << 1, reg_addr,
                         I2C_MEMADD_SIZE_8BIT, reg_data, cnt, 100) == HAL_OK)
        return 0;
    else
        return -1;
}

void BNO055_delay_msek(u32 msek)
{
  HAL_Delay(msek);
}


IMU_BNO055_Status_t imu_bno055_init(void)
{
  // Assign platform-specific read/write/delay functions
  bno055.bus_read     = BNO055_I2C_bus_read;
  bno055.bus_write    = BNO055_I2C_bus_write;
  bno055.delay_msec   = BNO055_delay_msek;
  bno055.dev_addr     = BNO055_I2C_ADDR1;  // 0x28 << 1 if needed

  // Initialize sensor
  if (bno055_init(&bno055) != BNO055_SUCCESS)
      return IMU_BNO055_ERROR;
  bno055.delay_msec(1500);

  bno055_set_operation_mode(BNO055_OPERATION_MODE_CONFIG);
  bno055.delay_msec(25);

  bno055_set_power_mode(BNO055_POWER_MODE_NORMAL);

  // reset all previous int signal
  bno055_set_intr_rst(ENABLED);

  bno055_set_accel_unit(BNO055_ACCEL_UNIT_MSQ);
  bno055_set_gyro_unit(BNO055_GYRO_UNIT_DPS);
  bno055_set_euler_unit(BNO055_EULER_UNIT_DEG);
  bno055_set_temp_unit(BNO055_TEMP_UNIT_CELSIUS);

  bno055_set_accel_any_motion_no_motion_axis_enable(0, ENABLED);
  bno055_set_accel_any_motion_no_motion_axis_enable(1, ENABLED);
  bno055_set_accel_any_motion_no_motion_axis_enable(2, ENABLED);

  bno055_set_accel_any_motion_thres(35);
  bno055_set_accel_any_motion_durn(1);
  bno055_set_intr_accel_any_motion(ENABLED);
  bno055_set_intr_mask_accel_any_motion(ENABLED);

  bno055_set_operation_mode(BNO055_OPERATION_MODE_AMG);
  bno055.delay_msec(25);

  return IMU_BNO055_OK;

}

//
//
//#define DOUBLE_TAP_INTERVAL_MS 400  // Max time between taps in ms
//
////static uint32_t last_tap_time = 0;
//
//bool imu_bno055_check_motion_interrupt(void);
////bool imu_bno055_read_euler(uint16_t* heading, uint16_t* roll, uint16_t* pitch);
//
//s8 bno055_set_sensor_offset(const bno055_offsets_t *offsets);
//
//IMU_BNO055_Status_t imu_bno055_init(void)
//{
//    bno055.bus_write = BNO055_I2C_bus_write;
//    bno055.bus_read  = BNO055_I2C_bus_read;
//    bno055.dev_addr  = BNO055_I2C_ADDR1;  // 0x28 << 1
//    bno055.delay_msec = BNO055_delay_msek;
//
//    if (bno055_init(&bno055) != BNO055_SUCCESS)
//        return IMU_BNO055_ERROR;
//
//    // Soft reset
//    u8 sys_trigger = 0x20;
//    bno055_write_register(0x3F, &sys_trigger, 1);
//    bno055.delay_msec(650);
//
//    if (bno055_init(&bno055) != BNO055_SUCCESS)
//        return IMU_BNO055_ERROR;
//
//    // CONFIG_MODE before any config
//    u8 config_mode = 0x00;
//    bno055_write_register(BNO055_OPR_MODE_ADDR, &config_mode, 1);
//    bno055.delay_msec(25);
//
//    // Load calibration offsets
//    bno055_offsets_t offsets = {
//        .accel_offset_x = -13,
//        .accel_offset_y = 8,
//        .accel_offset_z = 6,
//        .mag_offset_x   = 147,
//        .mag_offset_y   = -231,
//        .mag_offset_z   = -61,
//        .gyro_offset_x  = -1,
//        .gyro_offset_y  = -2,
//        .gyro_offset_z  = 0,
//        .accel_radius   = 1000,
//        .mag_radius     = 833
//    };
//    if (bno055_set_sensor_offset(&offsets) != BNO055_SUCCESS)
//        return IMU_BNO055_ERROR;
//
//    // Optional: Verify offsets were written
//    uint8_t offset_buf[22] = {0};
//    bno055_read_register(BNO055_ACCEL_OFFSET_X_LSB_ADDR, offset_buf, 22);
//
//    // Set units (m/s^2, Celsius, degrees)
//    uint8_t unit_sel = 0x00;
//    bno055_write_register(0x3B, &unit_sel, 1);
//
//    // Configure interrupts on Page 1
//    if (bno055_write_page_id(1) != BNO055_SUCCESS)
//        return IMU_BNO055_ERROR;
//
//    // Set motion sensitivity (threshold), lower = more sensitive
//    uint8_t acc_nm_thres = 0x01;
//    bno055_write_register(0x11, &acc_nm_thres, 1);  // ACC_NM_THRES
//
//    // Duration = 1 sample, threshold = 1 (very sensitive)
//    uint8_t acc_nm_set = 0x01;
//    bno055_write_register(0x12, &acc_nm_set, 1);  // ACC_NM_SET
//
//    // Orientation (optional, not used here)
//    uint8_t int_settings = 0x00;
//    bno055_write_register(0x0E, &int_settings, 1);  // INT_SETTINGS
//
//    // Enable only ACC_NM (disable ORI)
//    uint8_t int_en  = 0x01;  // Bit 0: ACC_NM
//    uint8_t int_msk = 0x01;  // Bit 0: ACC_NM routed to INT
//    bno055_write_register(0x10, &int_en, 1);   // INT_EN
//    bno055_write_register(0x0F, &int_msk, 1);  // INT_MSK
//
//    // Back to Page 0
//    bno055_write_page_id(0);
//
//    // Set final mode to AMG (raw accel/gyro/mag, no fusion)
//    u8 op_mode = 0x07;
//    bno055_write_register(BNO055_OPR_MODE_ADDR, &op_mode, 1);
//    bno055.delay_msec(25);
//
//    // Optional: verify CHIP_ID
//    uint8_t chip_id = 0;
//    bno055_read_register(0x00, &chip_id, 1);
//    if (chip_id != 0xA0)
//        return IMU_BNO055_ERROR;
//
//    // Optional: verify mode
//    bno055_read_register(0x3D, &op_mode, 1);
//    if (op_mode != 0x07)
//        return IMU_BNO055_ERROR;
//
//    return IMU_BNO055_OK;
//
//}
//
//void imu_bno055_poll_and_toggle_LED(void)
//{
//    uint8_t int_status = 0;
//
//    // Read interrupt status register (INT_STA = 0x37)
//    if (bno055_read_register(0x37, &int_status, 1) == BNO055_SUCCESS)
//    {
//        // Bit 0 = ACC_NM interrupt, Bit 4 = ORI interrupt
//        if ((int_status & 0x01) || (int_status & 0x10))
//        {
//            HAL_GPIO_TogglePin(GPIOA, WHITE_LED_Pin);
//        }
//    }
//}
//
//
//
//
//
//
//
//void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
//{
//    if (GPIO_Pin == IMU_INTERRUPT_Pin) {
//
//        if (imu_bno055_check_motion_interrupt()) {
//            // Motion detected and interrupt cleared
//            HAL_GPIO_TogglePin(GPIOA, WHITE_LED_Pin);
//        }
//    }
//
//}
//bool imu_bno055_check_motion_interrupt(void)
//{
//    uint8_t int_status = 0;
//
//    // Make sure we're on Page 0
//    if (bno055_write_page_id(0) != BNO055_SUCCESS)
//        return false;
//
//    // Read the interrupt status register (0x37)
//    if (bno055_read_register(0x37, &int_status, 1) != BNO055_SUCCESS)
//        return false;
//
//    uint8_t zero = 0x00;
//    bno055_write_register(0x37, &zero, 1);
//
////    // Check if bit 0 (ACC_NM interrupt) is set
////    if (int_status & 0x01) {
////        // Reading INT_STA clears it, so no extra action needed
////        return true;
////    }
//
//    return true;
//}
////
////uint8_t imu_bno055_poll_interrupt_status(void)
////{
////  uint8_t status = 0;
////
////  uint16_t heading = 0, roll = 0, pitch = 0;
////
////  if (imu_bno055_read_euler(&heading, &roll, &pitch)) {
////      // Values are in 1/16 degree units, convert if needed
////      float h = heading / 16.0f;
////      float r = roll / 16.0f;
////      float p = pitch / 16.0f;
////
////
////  }
////
////    return status;  // return full status byte for inspection
////}
////
////bool imu_bno055_read_euler(uint16_t* heading, uint16_t* roll, uint16_t* pitch)
////{
////    uint8_t buffer[6] = {0};
////
////    // Ensure we're on page 0
////    if (bno055_write_page_id(0) != BNO055_SUCCESS)
////        return false;
////
////    if (bno055_read_register(0x1A, buffer, 6) != BNO055_SUCCESS)
////        return false;
////
////    *heading = ((uint16_t)buffer[1] << 8) | buffer[0];
////    *roll    = ((uint16_t)buffer[3] << 8) | buffer[2];
////    *pitch   = ((uint16_t)buffer[5] << 8) | buffer[4];
////
////    return true;
////}
//
//s8 bno055_set_sensor_offset(const bno055_offsets_t *offsets) {
//    u8 buffer[22];
//
//    buffer[0]  = offsets->accel_offset_x & 0xFF;
//    buffer[1]  = (offsets->accel_offset_x >> 8) & 0xFF;
//    buffer[2]  = offsets->accel_offset_y & 0xFF;
//    buffer[3]  = (offsets->accel_offset_y >> 8) & 0xFF;
//    buffer[4]  = offsets->accel_offset_z & 0xFF;
//    buffer[5]  = (offsets->accel_offset_z >> 8) & 0xFF;
//
//    buffer[6]  = offsets->mag_offset_x & 0xFF;
//    buffer[7]  = (offsets->mag_offset_x >> 8) & 0xFF;
//    buffer[8]  = offsets->mag_offset_y & 0xFF;
//    buffer[9]  = (offsets->mag_offset_y >> 8) & 0xFF;
//    buffer[10] = offsets->mag_offset_z & 0xFF;
//    buffer[11] = (offsets->mag_offset_z >> 8) & 0xFF;
//
//    buffer[12] = offsets->gyro_offset_x & 0xFF;
//    buffer[13] = (offsets->gyro_offset_x >> 8) & 0xFF;
//    buffer[14] = offsets->gyro_offset_y & 0xFF;
//    buffer[15] = (offsets->gyro_offset_y >> 8) & 0xFF;
//    buffer[16] = offsets->gyro_offset_z & 0xFF;
//    buffer[17] = (offsets->gyro_offset_z >> 8) & 0xFF;
//
//    buffer[18] = offsets->accel_radius & 0xFF;
//    buffer[19] = (offsets->accel_radius >> 8) & 0xFF;
//    buffer[20] = offsets->mag_radius & 0xFF;
//    buffer[21] = (offsets->mag_radius >> 8) & 0xFF;
//
//    // Must be in CONFIG_MODE and PAGE 0
//    return bno055_write_register(BNO055_ACCEL_OFFSET_X_LSB_ADDR, buffer, 22);
//
//}
