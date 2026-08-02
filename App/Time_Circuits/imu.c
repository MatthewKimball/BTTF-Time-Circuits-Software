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
#include "cmsis_os.h"
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

// Set from the EXTI ISR, serviced from task context by imu_bno055_service().
// The BNO055 INT pin is edge-latched: once asserted it stays high until the
// host clears it, and the STM32 side only reacts to a rising *edge* - so a
// single lost/failed clear permanently stops all future interrupts (looks
// exactly like the IMU "going to sleep", recoverable only by a power cycle).
static volatile bool sImuIntPending = false;

// Guards every multi-step I2C transaction to the BNO055 (hi2c1). Needed
// because bno055_set_intr_rst() and imu_bno055_updateAnyMotionSettings()
// both issue several blocking HAL_I2C_Mem_* calls in sequence, and nothing
// about the ST HAL I2C driver is reentrant - without this, an EXTI-context
// clear used to race with a task-context settings update on the same
// handle, which could wedge hi2c1 or silently drop the interrupt clear.
static osMutexId_t sImuI2cMutexHandle = NULL;
static const osMutexAttr_t sImuI2cMutexAttr = { .name = "ImuI2cMutex" };


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

          // Defer the interrupt-status clear to task context - see
          // imu_bno055_service(). Doing the I2C transaction here, in a
          // hardware ISR, is what let it race with other I2C1 users.
          sImuIntPending = true;
      }
}

// Call periodically from task context (StartMainTask, every 20ms). Clears
// the BNO055 interrupt latch after a hit, and separately watches for the
// INT pin being stuck high with nothing pending - that means an earlier
// clear attempt was lost to a genuine I2C error rather than the ISR race,
// and without this watchdog it would otherwise never recover.
void imu_bno055_service(void)
{
    if (sImuIntPending)
    {
        sImuIntPending = false;

        if (osMutexAcquire(sImuI2cMutexHandle, 50) == osOK)
        {
            bno055_set_intr_rst(ENABLED);
            osMutexRelease(sImuI2cMutexHandle);
        }
        else
        {
            sImuIntPending = true; // couldn't get the bus - retry next tick
        }
        return;
    }

    if (HAL_GPIO_ReadPin(IMU_INTERRUPT_GPIO_Port, IMU_INTERRUPT_Pin) == GPIO_PIN_SET)
    {
        static uint32_t lastWatchdogClearTick = 0;
        uint32_t now = HAL_GetTick();

        if ((now - lastWatchdogClearTick) > 2000)
        {
            lastWatchdogClearTick = now;
            if (osMutexAcquire(sImuI2cMutexHandle, 50) == osOK)
            {
                bno055_set_intr_rst(ENABLED);
                osMutexRelease(sImuI2cMutexHandle);
            }
        }
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
  sImuI2cMutexHandle = osMutexNew(&sImuI2cMutexAttr);

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

IMU_BNO055_Status_t imu_bno055_updateAnyMotionSettings(u8 threshold, u8 duration)
{
    s8 rslt = BNO055_SUCCESS;
    u8 prev_mode = 0;

    // Clamp to sensible ranges
    if (threshold == 0) threshold = 1;
    if (duration > 3) duration = 3;  // duration is a tiny sample count

    if (osMutexAcquire(sImuI2cMutexHandle, osWaitForever) != osOK)
        return IMU_BNO055_ERROR;

    // Remember current mode
    rslt |= bno055_get_operation_mode(&prev_mode);

    // Go to CONFIG
    rslt |= bno055_set_operation_mode(BNO055_OPERATION_MODE_CONFIG);
    bno055.delay_msec(25);

    // Update any-motion params
    rslt |= bno055_set_accel_any_motion_thres(threshold);
    rslt |= bno055_set_accel_any_motion_durn(duration);

    // (Optional) Re-enable / re-mask interrupts if needed
    rslt |= bno055_set_intr_accel_any_motion(ENABLED);
    rslt |= bno055_set_intr_mask_accel_any_motion(ENABLED);

    // Restore prior mode (AMG or whatever you were using)
    rslt |= bno055_set_operation_mode(prev_mode);
    bno055.delay_msec(25);

    osMutexRelease(sImuI2cMutexHandle);

    return (rslt == BNO055_SUCCESS) ? IMU_BNO055_OK : IMU_BNO055_ERROR;
}
