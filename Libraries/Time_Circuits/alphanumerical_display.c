/*
 * alphanumerical_display.c
 *
 *  Created on: 26 Feb 2023
 *      Author: Matthew Kimball
 */

#include <alphanumerical_display.h>

#define  CORRECT_WIRING 0

struct Alpha_Display_Config_Tag
{
  I2C_HandleTypeDef*        hi2c;
  uint8_t                   i2cAddrs;
  Ht16k33_Config_t*         hDisplayDriver;

} Alpha_Display_Config;


Alpha_Display_Config_t* alpha_display_init(I2C_HandleTypeDef* const hi2c, const uint8_t addrs)
{
  Alpha_Display_Config_t* pConfig = malloc(sizeof(Alpha_Display_Config_t));
  pConfig->i2cAddrs = addrs;
  pConfig->hi2c = hi2c;
  pConfig->hDisplayDriver = ht16k33_init(hi2c, addrs);

  ht16k33_setSystemSetup(pConfig->hDisplayDriver, Ht16k33_SystemOscillator_On);
  ht16k33_setDisplaySetup(pConfig->hDisplayDriver, Ht16k33_DisplayStatus_On,
          Ht16k33_BlinkingFrequency_Off);

  return pConfig;
}

Alpha_Display_Status_t clearDisplay(Alpha_Display_Config_t* const pConfig)
{
  uint8_t* clearBuffer = malloc (TOTAL_NUMBER_OF_ROWS);
  Alpha_Display_Status_t isSuccess   = 0;

  for (int buffCount = 0; buffCount < TOTAL_NUMBER_OF_ROWS; buffCount++)
  {
    clearBuffer[buffCount] = 0x00;
  }

  isSuccess = ht16k33_updateDisplayData (pConfig->hDisplayDriver,DISPLAY_DATA_REG_ADDRESS, clearBuffer,
      TOTAL_NUMBER_OF_ROWS);

  if (isSuccess)
  {
    isSuccess = ht16k33_setDisplaySetup(pConfig->hDisplayDriver, Ht16k33_DisplayStatus_On,
        Ht16k33_BlinkingFrequency_Off);
  }

  free(clearBuffer);

  return isSuccess;
}

#if CORRECT_WIRING
Alpha_Display_Status_t charDisplay(Alpha_Display_Config_t* const pConfig, const uint8_t segmentNum,
    const char dispDigit)
{
  uint8_t* displayBuffer = malloc (1);

  displayBuffer[0] = sevenSegmentChars[(uint8_t)dispDigit];
  ht16k33_updateDisplayData (pConfig->hDisplayDriver, segmentNum, displayBuffer, 1);

  return 0;
}
#endif

//Hacky way of fixing wiring issue of LED numerical digits
Alpha_Display_Status_t digitDisplay(Alpha_Display_Config_t* const pConfig, const uint8_t segmentNum,
    const char dispDigit)
{
  uint8_t* displayBuffer = malloc (2);
  //uint8_t* pRamLocation = malloc (1);
  uint8_t test[] = {0,0};
  uint8_t boo[] = {0x00};

   if (segmentNum % 2)
  {
    boo[0] = segmentNum - 0x01;
    HAL_I2C_Master_Transmit(pConfig->hi2c, (pConfig->i2cAddrs)<<1, boo,  sizeof(boo), HAL_MAX_DELAY);
    HAL_I2C_Master_Receive(pConfig->hi2c, ((pConfig->i2cAddrs)<<1)|0x01, test, 1, HAL_MAX_DELAY);
    test[0] = 0x7F & test[0];
    test[0] |= (sevenSegmentChars[(uint8_t)dispDigit] & 0x01 )<< 7;
    test[1] |= sevenSegmentChars[(uint8_t)dispDigit] >> 1;
    ht16k33_updateDisplayData (pConfig->hDisplayDriver, boo[0], test, 2);
  }
  else
  {
    boo[0] = segmentNum;
    HAL_I2C_Master_Transmit(pConfig->hi2c, (pConfig->i2cAddrs)<<1, boo,  1, HAL_MAX_DELAY);
    HAL_I2C_Master_Receive(pConfig->hi2c, ((pConfig->i2cAddrs)<<1)|0x01, test, 1, HAL_MAX_DELAY);
    test[0] = 0x80 & test[0];
    displayBuffer[0] = test[0] | sevenSegmentChars[(uint8_t)dispDigit];
    ht16k33_updateDisplayData (pConfig->hDisplayDriver, segmentNum, displayBuffer, 1);
  }


   free(displayBuffer);
  return 0;
}

Alpha_Display_Status_t alphaDisplay(Alpha_Display_Config_t* const pConfig, const uint8_t segmentNum,
    const char dispAlpha)
{
  uint8_t displayBuffer[2] = {0,0};

  displayBuffer[0] = (uint8_t)((fourteenSegmentChars[(uint8_t)(dispAlpha-0x41)]>>0) & 0xFF);
  displayBuffer[1] = (uint8_t)((fourteenSegmentChars[(uint8_t)(dispAlpha-0x41)]>>8) & 0xFF);
  ht16k33_updateDisplayData (pConfig->hDisplayDriver, (10 + (2*segmentNum)), displayBuffer, 2);

  //free(displayBuffer);
  return 0;
}


