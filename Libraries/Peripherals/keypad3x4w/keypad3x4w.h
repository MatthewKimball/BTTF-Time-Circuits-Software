/*
 * keypad3x4w.h
 *
 *  Created on: Nov 5, 2024
 *      Author: Professor Gizmo
 */

#ifndef PERIPHERALS_KEYPAD3X4W_KEYPAD3X4W_H_
#define PERIPHERALS_KEYPAD3X4W_KEYPAD3X4W_H_

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define KEYPAD_ROW_NUMBER         4
#define KEYPAD_COLUMN_NUMBER      3
#define KEYPAD_KEYS_TOTAL         (KEYPAD_ROW_NUMBER * KEYPAD_ROW_NUMBER)
#define KEYPAD_KEY_PRESSED        1
#define KEYPAD_KEY_RELEASED       0

typedef enum
{
  keypad3x4w_ColumnScanType_Full  = 0,
  keypad3x4w_ColumnScanType_Col1  = 1,
  keypad3x4w_ColumnScanType_Col2  = 2,
  keypad3x4w_ColumnScanType_Col3  = 3,
} keypad3x4w_ColumnScanType_e;

typedef struct  Keypad3x4w_Config_Tag Keypad3x4w_Config_t;
typedef bool    Keypad3x4w_Status_t;

typedef struct
{
  GPIO_TypeDef *  Column_Port[KEYPAD_COLUMN_NUMBER];
  uint16_t        Column_Pin[KEYPAD_COLUMN_NUMBER];
  GPIO_TypeDef *  Row_Port[KEYPAD_ROW_NUMBER];
  uint16_t        Row_Pin[KEYPAD_ROW_NUMBER];

}Keypad3x4w_PinConfig_t;

Keypad3x4w_Config_t * keypad3x4w_init(Keypad3x4w_PinConfig_t * pPinConfig);
Keypad3x4w_Status_t keypad3x4w_readKeypad(Keypad3x4w_Config_t * pConfig, uint8_t * pKey);

//Column scan order
static const bool columnScanConfig[4][3] = {
    {1,1,1},  /* Full Column Scan */
    {1,0,0},  /* Column 1 Scan */
    {0,1,0},  /* Column 2 Scan */
    {0,0,1},  /* Column 3 Scan */
};
//Keypad number order
static const uint8_t keypadNumberOrder[4][3] = {
    {1,2,3},  /* Keypad Row 1 Numbers */
    {4,5,6},  /* Keypad Row 2 Numbers */
    {7,8,9},  /* Keypad Row 3 Numbers */
    {0,0,0},  /* Keypad Row 4 Numbers */
};

#endif /* PERIPHERALS_KEYPAD3X4W_KEYPAD3X4W_H_ */
