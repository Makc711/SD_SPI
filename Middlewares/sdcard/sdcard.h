/**
  ******************************************************************************
  * @file           : sdcard.h
  * @author         : Rusanov M.N.
  * @version        : V1.0.0
  * @date           : 08-April-2023
  * @brief          : Header for sdcard.c file.
  *                   This file contains functions for working with SD cards
  *                   over the SPI interface.
  *
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef SDCARD_H_
#define SDCARD_H_

/* Includes ------------------------------------------------------------------*/
#include "diskio.h"
#include "stm32f7xx_hal.h"

/* Exported defines ----------------------------------------------------------*/
#define SDCARD_HSPI          hspi2
#define SDCARD_CS_GPIO_Port  GPIOI
#define SDCARD_CS_Pin        GPIO_PIN_3

// The prescaler at which the frequency is between 100 and 400 kHz
#define SPI_BAUDRATEPRESCALER_100_400KHZ   SPI_BAUDRATEPRESCALER_64

#define SPI_TIMEOUT_MS 100
#define DELAY_AFTER_POWER_ON_MS 10

/* Card type flags (CardType) */
#define CT_MMC 0x01 /* MMC ver 3 */
#define CT_SD1 0x02 /* SD ver 1 */
#define CT_SD2 0x04 /* SD ver 2 */
#define CT_SDC (CT_SD1|CT_SD2) /* SD */
#define CT_BLOCK 0x08 /* Block addressing */

/* Exported functions prototypes ---------------------------------------------*/
DSTATUS SDCARD_Init(BYTE pdrv);
DSTATUS SDCARD_DiskStatus(BYTE pdrv);
DRESULT SDCARD_Read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count);
#if _USE_WRITE
DRESULT SDCARD_Write(BYTE pdrv, BYTE* buff, DWORD sector, UINT count);
#endif  // _USE_WRITE
#if _USE_IOCTL
DRESULT SDCARD_DiskIoctl(BYTE pdrv, BYTE ctrl, void *buff);
#endif // _USE_IOCTL

#endif // SDCARD_H_
