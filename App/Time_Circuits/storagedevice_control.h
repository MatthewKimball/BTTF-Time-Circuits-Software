/*
 * storagedevice_control.h
 *
 *  Created on: Nov 17, 2024
 *      Author: Professor Gizmo
 */

#ifndef STORAGEDEVICE_H_
#define STORAGEDEVICE_H_

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct  StorageDevice_Config_Tag StorageDevice_Config_t;
typedef bool    StorageDevice_Status_t;

StorageDevice_Config_t* storageDevice_init(SPI_HandleTypeDef * hspi);
StorageDevice_Status_t  storageDevice_mountDrive(StorageDevice_Config_t * pConfig);
StorageDevice_Status_t  storageDevice_demountDrive(StorageDevice_Config_t * pConfig);
StorageDevice_Status_t storageDevice_readFile(StorageDevice_Config_t * pConfig, char* readBuf,
    const uint8_t bufferSize, const char* const filename);
StorageDevice_Status_t storageDevice_writeFile(StorageDevice_Config_t * pConfig, char* readBuf,
    const uint8_t bufferSize, const char* const filename);
StorageDevice_Status_t storageDevice_readWavDataSize(StorageDevice_Config_t * pConfig, const char* const filename,
    uint32_t* pRecordingSize, uint32_t* pPlayedSize);
StorageDevice_Status_t storageDevice_readFileData(StorageDevice_Config_t * pConfig, uint16_t* dataBuffer,
    uint32_t bytesToRead, uint32_t* pBytesRead);
StorageDevice_Status_t storageDevice_closeFile(StorageDevice_Config_t * pConfig);

StorageDevice_Status_t storageDevice_deInit();



#endif /* STORAGEDEVICE_H_ */
