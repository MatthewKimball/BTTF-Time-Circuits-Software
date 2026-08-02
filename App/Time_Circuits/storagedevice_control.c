/*
 * storagedevice_control.c
 *
 *  Created on: Nov 17, 2024
 *      Author: Professor Gizmo
 */
#include <storagedevice_control.h>
#include "fatfs.h"



struct StorageDevice_Config_Tag
{
  //StorageDevice_Config_t * pStorageDeviceConfig;
  SPI_HandleTypeDef * hspi;

  FATFS   FatFs;    //Fatfs handle
  FIL     fil;      //File handle used for general (non-audio) file access, e.g. saved date/time
  FIL     filSound; //Dedicated file handle for the sound task's WAV streaming, kept separate so a
                     //concurrent save on `fil` from another task can never clobber an in-progress
                     //audio file object
  FRESULT fres;   //Result after operation
  DWORD   free_clusters;
  DWORD   free_sectors;
  DWORD   total_sectors;
  FATFS*  getFreeFs;

} StorageDevice_Config;

//Private Function Definitions
StorageDevice_Status_t storageDevice_closeFile(StorageDevice_Config_t * pConfig);
static StorageDevice_Status_t storageDevice_openSoundFile(StorageDevice_Config_t * pConfig, TCHAR* const filename, BYTE mode);


StorageDevice_Config_t* storageDevice_init(SPI_HandleTypeDef * hspi)
{
  StorageDevice_Config_t* pConfig = calloc(1, sizeof(StorageDevice_Config_t));
  pConfig->hspi = hspi;

//  storageDevice_demountDrive(pConfig);

  storageDevice_mountDrive(pConfig);

  return pConfig;
}

StorageDevice_Status_t storageDevice_deInit(StorageDevice_Config_t * pConfig)
{
  StorageDevice_Status_t isSuccess = false;

  isSuccess = storageDevice_demountDrive(pConfig);
  free(pConfig);

  return isSuccess;

}

StorageDevice_Status_t storageDevice_mountDrive(StorageDevice_Config_t * pConfig)
{
  StorageDevice_Status_t isSuccess = true;

  //Recommended delay while SD Card Boots
  osDelay(100);

  pConfig->fres = f_mount(&pConfig->FatFs, "", 1); //1=mount now
  if (pConfig->fres != FR_OK) {
    isSuccess = 0;
  //while(1);
  }

  pConfig->fres = f_getfree("", &pConfig->free_clusters, &pConfig->getFreeFs);
  if (pConfig->fres != FR_OK) {
    isSuccess = 0;
  //while(1);
  }

  pConfig->total_sectors = (pConfig->getFreeFs->n_fatent - 2) * pConfig->getFreeFs->csize;
  pConfig->free_sectors = pConfig->free_clusters * pConfig->getFreeFs->csize;

  return isSuccess;
}

StorageDevice_Status_t storageDevice_demountDrive(StorageDevice_Config_t * pConfig)
{
  StorageDevice_Status_t isSuccess = false;

  storageDevice_closeFile(pConfig);
  storageDevice_closeSoundFile(pConfig);

  f_mount(NULL, "", 0);

  if (pConfig->fres == FR_OK)
  {
    isSuccess = true;
  }
  return isSuccess;
}

StorageDevice_Status_t storageDevice_openFile(StorageDevice_Config_t * pConfig, TCHAR* const filename, BYTE mode)
{
  StorageDevice_Status_t isSuccess = true;

  pConfig->fres = f_open(&pConfig->fil, filename, mode);
  if (pConfig->fres != FR_OK) {
    isSuccess = false;
  //while(1);
  }

  return isSuccess;
}

StorageDevice_Status_t storageDevice_closeFile(StorageDevice_Config_t * pConfig)
{
  StorageDevice_Status_t isSuccess = true;

  pConfig->fres = f_close(&pConfig->fil);

  return isSuccess;
}

volatile uint32_t gDbgPlaySoundCallCount = 0;
volatile FRESULT  gDbgOpenFres  = 0;
volatile FRESULT  gDbgLseekFres = 0;
volatile FRESULT  gDbgReadFres  = 0;
volatile FRESULT  gDbgCloseFres = 0;

static StorageDevice_Status_t storageDevice_openSoundFile(StorageDevice_Config_t * pConfig, TCHAR* const filename, BYTE mode)
{
  StorageDevice_Status_t isSuccess = true;

  gDbgPlaySoundCallCount++;
  pConfig->fres = f_open(&pConfig->filSound, filename, mode);
  gDbgOpenFres = pConfig->fres;
  if (pConfig->fres != FR_OK) {
    isSuccess = false;
  }

  return isSuccess;
}

StorageDevice_Status_t storageDevice_closeSoundFile(StorageDevice_Config_t * pConfig)
{
  StorageDevice_Status_t isSuccess = true;

  pConfig->fres = f_close(&pConfig->filSound);
  gDbgCloseFres = pConfig->fres;

  return isSuccess;
}

StorageDevice_Status_t storageDevice_readFile(StorageDevice_Config_t * pConfig, char* readBuf,
    const uint8_t bufferSize, const char* const filename)
{
  StorageDevice_Status_t isSuccess = false;

  isSuccess = storageDevice_openFile(pConfig, (TCHAR*)filename, FA_READ);

  TCHAR* rres = f_gets((TCHAR*)readBuf, bufferSize, &pConfig->fil);
  if(rres != 0) {
    isSuccess = true;

  } else {
    isSuccess = false;
  }

  return isSuccess;
}

StorageDevice_Status_t storageDevice_readWavDataSize(StorageDevice_Config_t * pConfig, const char* const filename,
    uint32_t* pRecordingSize, uint32_t* pPlayedSize)
{
  StorageDevice_Status_t isSuccess = false;

  isSuccess = storageDevice_openSoundFile(pConfig, (TCHAR*)filename, FA_READ);

  pConfig->fres = f_lseek(&pConfig->filSound, 40);
  gDbgLseekFres = pConfig->fres;

  pConfig->fres = f_read(&pConfig->filSound, pRecordingSize, 4, (UINT *) pPlayedSize);
  gDbgReadFres = pConfig->fres;

  return isSuccess;
}

StorageDevice_Status_t storageDevice_readFileData(StorageDevice_Config_t * pConfig, uint16_t* dataBuffer,
    uint32_t bytesToRead, uint32_t* pBytesRead)
{
  StorageDevice_Status_t isSuccess = false;

  pConfig->fres = f_read(&pConfig->filSound, dataBuffer, (UINT) bytesToRead, (UINT *) pBytesRead);

  return isSuccess;
}

StorageDevice_Status_t storageDevice_writeFile(StorageDevice_Config_t * pConfig, char* readBuf,
    const uint8_t bufferSize, const char* const filename)
{
  StorageDevice_Status_t isSuccess = false;
  UINT bytesWrote;

  isSuccess = storageDevice_openFile(pConfig, (TCHAR*)filename, FA_WRITE | FA_OPEN_ALWAYS | FA_CREATE_ALWAYS);

  pConfig->fres = f_write(&pConfig->fil, readBuf, bufferSize, &bytesWrote);
  if(pConfig->fres == FR_OK) {
    isSuccess = true;
  } else {
    isSuccess = false;
  }

  //isSuccess &= storageDevice_closeFile(pConfig);

  return isSuccess;
}
