/*
 * imu_bno055.h
 *
 *  Created on: Jul 23, 2025
 *      Author: Matthew Kimball
 * 
 * Description:
 * 
 */

#ifndef IMU_H_
#define IMU_H_
#include <stdbool.h>
#include "bno055.h"
#include <stdint.h>

typedef struct  IMU_BNO055_Config_Tag IMU_BNO055_Config_t;
typedef bool    IMU_BNO055_Status_t;

typedef enum {
    IMU_BNO055_OK = 0,
    IMU_BNO055_ERROR = 1,
} IMU_BNO055_Status_e;

extern volatile bool gGlitchDoubleHit;

typedef struct {
    int16_t accel_offset_x;
    int16_t accel_offset_y;
    int16_t accel_offset_z;

    int16_t mag_offset_x;
    int16_t mag_offset_y;
    int16_t mag_offset_z;

    int16_t gyro_offset_x;
    int16_t gyro_offset_y;
    int16_t gyro_offset_z;

    int16_t accel_radius;
    int16_t mag_radius;
} bno055_offsets_t;


IMU_BNO055_Status_t imu_bno055_init(void);

s8 BNO055_I2C_bus_write(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt);
s8 BNO055_I2C_bus_read(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt);
void BNO055_delay_msek(u32 msek);
uint8_t imu_bno055_poll_interrupt_status(void);
void imu_bno055_poll_and_toggle_LED(void);


#endif /* IMU_H_ */
