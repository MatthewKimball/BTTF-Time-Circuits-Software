/*
 * keypad3x4w.c
 *
 *  Created on: Nov 5, 2024
 *      Author: Professor Gizmo
 */

#include <keypad3x4w.h>

struct Keypad3x4w_Config_Tag
{
  const Keypad3x4w_PinConfig_t* pkeypadPinConfig;
  uint8_t*                      pActivatedKey;

} Keypad3x4w_Config;


Keypad3x4w_Config_t * keypad3x4w_init(const Keypad3x4w_PinConfig_t * const pPinConfig)
{
  Keypad3x4w_Config_t * pConfig = malloc(sizeof(Keypad3x4w_Config_t));

  pConfig->pkeypadPinConfig = pPinConfig;

  return pConfig;
}

Keypad3x4w_Status_t keypad3x4w_setColumnGpio(Keypad3x4w_Config_t * pConfig, keypad3x4w_ColumnScanType_e columnScanType)
{

  for (uint8_t columnCount = 0; columnCount < KEYPAD_COLUMN_NUMBER; columnCount++)
  {
    HAL_GPIO_WritePin(pConfig->pkeypadPinConfig->Column_Port[columnCount],
        pConfig->pkeypadPinConfig->Column_Pin[columnCount], columnScanConfig[columnScanType][columnCount]);
  }
  return 0;
}


Keypad3x4w_Status_t keypad3x4w_readKeypad(Keypad3x4w_Config_t * pConfig, uint8_t* pKey)
{
  Keypad3x4w_Status_t isPressed = false;

  //Turn on column GPIO outputs
  keypad3x4w_setColumnGpio(pConfig, keypad3x4w_ColumnScanType_Full);

  //Check if a row has been activated
  for (uint8_t rowCount = 0; rowCount < KEYPAD_ROW_NUMBER; rowCount++)
  {
    if (HAL_GPIO_ReadPin (pConfig->pkeypadPinConfig->Row_Port[rowCount], pConfig->pkeypadPinConfig->Row_Pin[rowCount]))
    {
      //Determine Column Pressed
      for (uint8_t columnCount = 0; columnCount < KEYPAD_COLUMN_NUMBER; columnCount++)
      {
        keypad3x4w_setColumnGpio(pConfig, keypad3x4w_ColumnScanType_Col1 + columnCount);

        for (uint8_t rowCount = 0; rowCount < KEYPAD_ROW_NUMBER; rowCount++)
        {
          if (HAL_GPIO_ReadPin (pConfig->pkeypadPinConfig->Row_Port[rowCount], pConfig->pkeypadPinConfig->Row_Pin[rowCount]))
          {
            isPressed = true;
            *pKey = keypadNumberOrder[rowCount][columnCount];
          }
        }
      }
    }
  }

  return isPressed;
 }
