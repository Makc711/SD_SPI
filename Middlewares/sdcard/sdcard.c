/* Includes ------------------------------------------------------------------*/
#include "sdcard.h"
#include "main.h"
#include <stdbool.h>

/* Private defines ----------------------------------------------------------*/
// Definitions for MMC/SDC command
#define CMD0  (0x40+0)  // GO_IDLE_STATE
#define CMD1  (0x40+1)  // SEND_OP_COND (MMC)
#define ACMD41 (0xC0+41) // SEND_OP_COND (SDC)
#define CMD8  (0x40+8)  // SEND_IF_COND
#define CMD9  (0x40+9)  // SEND_CSD
#define CMD10 (0x40+10) // SEND_CID
#define CMD12 (0x40+12) // STOP_TRANSMISSION
#define CMD16 (0x40+16) // SET_BLOCKLEN
#define CMD17 (0x40+17) // READ_BLOCK
#define CMD18 (0x40+18) // READ_MULTIPLE_BLOCK
#define ACMD23 (0xC0+23) // SET_BLOCK_COUNT
#define CMD24 (0x40+24) // WRITE_BLOCK
#define CMD25 (0x40+25) // WRITE_MULTIPLE_BLOCK
#define CMD55 (0x40+55) // APP_CMD
#define CMD58 (0x40+58) // READ_OCR

#define CRC_FOR_CMD0  ((0x4A << 1) | 1)
#define CRC_FOR_CMD8  ((0x43 << 1) | 1)

#define BLOCK_SIZE 512
#define CSD_SIZE 16
#define NUMBER_OF_ATTEMPTS_R1 10
#define NUMBER_OF_ATTEMPTS_DATA_TOKEN UINT16_MAX
#define NUMBER_OF_ATTEMPTS_NOT_BUSY UINT16_MAX
#define NUMBER_OF_ATTEMPTS_ACMD41 12000
#define NUMBER_OF_ATTEMPTS_CMD1 25000
#define NUMBER_OF_ATTEMPTS_RECEIVE_RESPONSE 64

/* Private typedef -----------------------------------------------------------*/
typedef enum
{
  DATA_TOKEN_CMD9        = 0xFE,
  DATA_TOKEN_CMD10       = 0xFE,
  DATA_TOKEN_CMD17       = 0xFE,
  DATA_TOKEN_CMD18       = 0xFE,
  DATA_TOKEN_CMD24       = 0xFE,
  DATA_TOKEN_CMD25       = 0xFC,
  STOP_TRANSACTION_TOKEN = 0xFD
} DataToken;

typedef enum
{
  SDCARD_NO_ERROR = 0x00,
  SDCARD_IN_IDLE_STATE = 0x01,
  SDCARD_TIMEOUT_ERROR = 0xFF
} r1Result;
/*
r1Result: 0abcdefg
           ||||||`- 1th bit (g): card is in idle state
           |||||`-- 2th bit (f): erase sequence cleared
           ||||`--- 3th bit (e): illegal command detected
           |||`---- 4th bit (d): crc check error
           ||`----- 5th bit (c): error in the sequence of erase commands
           |`------ 6th bit (b): misaligned address used in command
           `------- 7th bit (a): command argument outside allowed range
                   (8th bit is always zero)
*/

/* Variables -----------------------------------------------------------------*/
extern SPI_HandleTypeDef SDCARD_HSPI;
static volatile DSTATUS diskStatus_ = STA_NOINIT;
static uint8_t cardType_;
static bool isPowerOn_ = false;

/* Functions -----------------------------------------------------------------*/
static uint8_t SPI_TransmitReceiveByte(uint8_t byte)
{
  uint8_t receivedByte = 0;
  while (!__HAL_SPI_GET_FLAG(&SDCARD_HSPI, SPI_FLAG_TXE)) {}
  if (HAL_SPI_TransmitReceive(&SDCARD_HSPI, &byte,
    &receivedByte, sizeof(byte), SPI_TIMEOUT_MS) != HAL_OK)
  {
    Error_Handler();
  }
  return receivedByte;
}

static void SPI_TransmitByte(uint8_t byte)
{
  while (!__HAL_SPI_GET_FLAG(&SDCARD_HSPI, SPI_FLAG_TXE)) {}
  if (HAL_SPI_Transmit(&SDCARD_HSPI, &byte, sizeof(byte), SPI_TIMEOUT_MS) != HAL_OK)
  {
    Error_Handler();
  }
}

static void SPI_TransmitBuffer(uint8_t *buff, uint16_t size)
{
  while (!__HAL_SPI_GET_FLAG(&SDCARD_HSPI, SPI_FLAG_TXE)) {}
  if (HAL_SPI_Transmit(&SDCARD_HSPI, buff, size, SPI_TIMEOUT_MS) != HAL_OK)
  {
    Error_Handler();
  }
}

static void SDCARD_SkipByte(void)
{
  SPI_TransmitReceiveByte(0xFF);
}

static uint8_t SDCARD_ReadByte(void)
{
  return SPI_TransmitReceiveByte(0xFF);
}

static void SDCARD_ReadBuffer(uint8_t* buff, size_t size)
{
  for (size_t i = 0; i < size; i++)
  {
    buff[i] = SDCARD_ReadByte();
  }
}

static void SDCARD_Select(void)
{
  HAL_GPIO_WritePin(SDCARD_CS_GPIO_Port, SDCARD_CS_Pin, GPIO_PIN_RESET);
  HAL_Delay(1);
}

static void SDCARD_Deselect(void)
{
  HAL_GPIO_WritePin(SDCARD_CS_GPIO_Port, SDCARD_CS_Pin, GPIO_PIN_SET);
  HAL_Delay(1);
}

static r1Result SDCARD_ReadR1(void)
{ // Wait for a valid response in timeout of n attempts
  uint16_t n = NUMBER_OF_ATTEMPTS_R1;
  r1Result result = SDCARD_TIMEOUT_ERROR;
  do
  {
    const uint8_t r1 = SDCARD_ReadByte();
    if ((r1 & 0x80) == 0)
    {
      result = r1;
      break;
    }
  } while (--n != 0);
  return result;
}

static DRESULT SDCARD_WaitDataToken(DataToken token)
{ // Wait for a valid response in timeout of n attempts
  uint16_t n = NUMBER_OF_ATTEMPTS_DATA_TOKEN;
  DRESULT result = RES_NOTRDY;
  do
  {
    const uint8_t fb = SDCARD_ReadByte();
    if(fb == token)
    {
      result = RES_OK;
      break;
    }
    if(fb != 0xFF)
    {
      result = RES_ERROR;
      break;
    }
  } while (--n != 0);
  return result;
}

static r1Result SDCARD_WaitNotBusy(void)
{ // Wait for a valid response in timeout of n attempts
  uint16_t n = NUMBER_OF_ATTEMPTS_NOT_BUSY;
  r1Result result = SDCARD_TIMEOUT_ERROR;
  do 
  {
    const uint8_t busy = SDCARD_ReadByte();
    if (busy == 0xFF)
    {
      result = SDCARD_NO_ERROR;
      break;
    }
  } while (--n != 0);
  return result;
}

static r1Result SDCARD_SendCmd(uint8_t cmd, uint32_t arg)
{
  r1Result result;

  // ACMD<n> is the command sequence of CMD55-CMD<n>
  if ((cmd & 0x80) != 0)
  {
    cmd &= 0x7F;
    result = SDCARD_SendCmd(CMD55, 0);
    if ((result != SDCARD_NO_ERROR) && (result != SDCARD_IN_IDLE_STATE))
    {
      return result;
    }
  }

  result = SDCARD_WaitNotBusy();
  if (result != SDCARD_NO_ERROR)
  {
    return result;
  }

  // Send a command packet
  uint8_t buf[] = {
    cmd, // Start + Command index
    (uint8_t)(arg >> 24), // Argument[31..24]
    (uint8_t)(arg >> 16), // Argument[23..16]
    (uint8_t)(arg >> 8), // Argument[15..8]
    (uint8_t)arg // Argument[7..0]
  };
  SPI_TransmitBuffer(buf, sizeof(buf));

  uint8_t crc = 0x01; // Dummy CRC + Stop
  if (cmd == CMD0)
  {
    crc = CRC_FOR_CMD0;
  }
  else if (cmd == CMD8)
  {
    crc = CRC_FOR_CMD8;
  }
  SPI_TransmitByte(crc);

  /*
  The received byte immediately following CMD12 is a stuff byte, it should be
  discarded before receive the response of the CMD12
  */
  if (cmd == CMD12)
  {
    SDCARD_SkipByte();
  }

  result = SDCARD_ReadR1();
  return result;
}

static void SDCARD_SwitchToSpiMode(void)
{
  /*
  Set DI and CS high and apply 74 or more clock pulses to SCLK. Without this
  step under certain circumstances SD-card will not work. For instance, when
  multiple SPI devices are sharing the same bus (i.e. MISO, MOSI, CS).
  */
  SDCARD_Deselect();

  const uint32_t temp = SDCARD_HSPI.Init.BaudRatePrescaler;
  SDCARD_HSPI.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_100_400KHZ;
  HAL_SPI_Init(&SDCARD_HSPI);

  for (int i = 0; i < 10; i++) // At least 74 SCLK pulses
  {
    SDCARD_SkipByte();
  }

  SDCARD_HSPI.Init.BaudRatePrescaler = temp; // The clock frequency should be up to 20 MHz
  HAL_SPI_Init(&SDCARD_HSPI);

  SDCARD_Select();
}

static DSTATUS SDCARD_PowerOn(void) 
{
  SDCARD_SwitchToSpiMode();

  // Reset the SD card
  if (SDCARD_SendCmd(CMD0, 0) != SDCARD_IN_IDLE_STATE)
  {
    diskStatus_ |= STA_NODISK;
    SDCARD_Deselect();
    return RES_ERROR;
  }

  SDCARD_Deselect();
  SDCARD_SkipByte();

  diskStatus_ &= ~STA_NODISK;
  isPowerOn_ = true;

  return RES_OK;
}

static void SDCARD_PowerOff(void) 
{
  isPowerOn_ = false;
}

static uint8_t SDCARD_CheckPower(void) 
{
  return isPowerOn_;
}

/**
  * @brief  Initializes a Drive
  * @param  pdrv - Physical drive number (0..)
  * @retval DSTATUS - Operation status
  */
DSTATUS SDCARD_Init(BYTE pdrv)
{
  /* Supports only one type of drive */ 
  if (pdrv)
  {
    return STA_NOINIT; 
  }

  if (diskStatus_ & STA_NODISK)
  {
    return diskStatus_;
  }

  HAL_Delay(DELAY_AFTER_POWER_ON_MS);

  if (SDCARD_PowerOn() != RES_OK)
  {
    SDCARD_Deselect();
    return diskStatus_;
  }

  SDCARD_Select();

  /*
  After the card enters idle state with a CMD0, send a CMD8 with argument of
  0x000001AA and correct CRC prior to initialization process. If the CMD8 is
  rejected with illegal command error (0x05), the card is SDC version 1 or
  MMC version 3. If accepted, R7 response (R1(0x01) + 32-bit return value)
  will be returned. The lower 12 bits in the return value 0x1AA means that
  the card is SDC version 2 and it can work at voltage range of 2.7 to 3.6
  volts. If not the case, the card should be rejected.
  */

  // ReSharper disable once CppInitializedValueIsAlwaysRewritten
  uint8_t sdType = 0;
  if (SDCARD_SendCmd(CMD8, 0x01AA) == SDCARD_IN_IDLE_STATE) // SDv2
  {
    uint8_t responseR7[4];
    SDCARD_ReadBuffer(responseR7, sizeof(responseR7)); 

    if ((responseR7[2] != 0x01) || (responseR7[3] != 0xAA))
    { // The card can't work at vdd range of 2.7-3.6V
      SDCARD_Deselect();
      return diskStatus_;
    }

    /*
    And then initiate initialization with ACMD41 with HCS flag (bit 30).
    */
    uint16_t n = NUMBER_OF_ATTEMPTS_ACMD41; // Wait for a valid response in timeout of n attempts
    do
    {
      const r1Result responseR1 = SDCARD_SendCmd(ACMD41, 1UL << 30);
      if (responseR1 == SDCARD_NO_ERROR)
      {
        break;
      }
      if (responseR1 != SDCARD_IN_IDLE_STATE)
      {
        SDCARD_Deselect();
        return diskStatus_;
      }
    } while (--n != 0);

    /*
    After the initialization completed, read OCR register with CMD58 and check
    CCS flag (bit 30). When it is set, the card is a high-capacity card known
    as SDHC/SDXC.
    */
    if ((n == 0) || (SDCARD_SendCmd(CMD58, 0) != SDCARD_NO_ERROR))
    {
      SDCARD_Deselect();
      return diskStatus_;
    }

    uint8_t responseR3[4];
    SDCARD_ReadBuffer(responseR3, sizeof(responseR3));
    if (responseR3[0] & 0x40)
    {
      sdType = CT_SD2 | CT_BLOCK; // SDv2 (SDHC/SDXC)
    }
    else
    {
      sdType = CT_SD2; // SDv2 (SDSC)
      if (SDCARD_SendCmd(CMD16, BLOCK_SIZE) != SDCARD_NO_ERROR) // Set R/W block length to BLOCK_SIZE
      {
        SDCARD_Deselect();
        return diskStatus_;
      }
    }
  }
  else // SDv1 or MMCv3
  {
    uint8_t cmd;
    if (SDCARD_SendCmd(ACMD41, 0) <= 1)
    {
      sdType = CT_SD1;
      cmd = ACMD41;
    }
    else
    {
      sdType = CT_MMC;
      cmd = CMD1;
    }
    uint16_t n = NUMBER_OF_ATTEMPTS_CMD1; // Wait for a valid response in timeout of n attempts
    do
    { // Wait for leaving idle state
      const r1Result responseR1 = SDCARD_SendCmd(cmd, 0);
      if (responseR1 == SDCARD_NO_ERROR)
      {
        break;
      }
      if (responseR1 != SDCARD_IN_IDLE_STATE)
      {
        SDCARD_Deselect();
        return diskStatus_;
      }
    } while (--n != 0);

    if ((n == 0) || SDCARD_SendCmd(CMD16, BLOCK_SIZE) != SDCARD_NO_ERROR) // Set R/W block length to BLOCK_SIZE
    {
      SDCARD_Deselect();
      return diskStatus_;
    }
  }

  cardType_ = sdType;
  SDCARD_Deselect();
  SDCARD_SkipByte();

  if (sdType)
  {
    diskStatus_ &= ~STA_NOINIT;
  }
  else
  {
    /* Initialization failed */
    SDCARD_PowerOff();
  }
  return diskStatus_;
}

/**
  * @brief  Gets Disk Status
  * @param  pdrv - Physical drive number (0..)
  * @retval DSTATUS - Operation status
  */
DSTATUS SDCARD_DiskStatus(BYTE pdrv) 
{
  if (pdrv)
  {
    return STA_NOINIT;
  }
  return diskStatus_;
}

static DRESULT SDCARD_ReadData(uint8_t* buff, uint16_t size, DataToken dataToken)
{
  const DRESULT result = SDCARD_WaitDataToken(dataToken);
  if (result != RES_OK) 
  {
    return result;
  }

  SDCARD_ReadBuffer(buff, size);

  uint8_t crc[2];
  SDCARD_ReadBuffer(crc, sizeof(crc));

  return RES_OK;
}

/**
  * @brief  Reads Sector(s)
  * @param  pdrv - Physical drive number (0..) to identify the drive
  * @param  buff - Data buffer to store read data
  * @param  sector - Sector address (LBA)
  * @param  count - Number of sectors to read (1..128)
  * @retval DRESULT - Operation result
  */
DRESULT SDCARD_Read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
  /* Supports only one type of drive */ 
  if (pdrv || (count == 0))
  {
    return RES_PARERR; 
  }

  /* no disk */
  if (diskStatus_ & STA_NOINIT)
  {
    return RES_NOTRDY;
  }

  /* convert to byte address */
  if ((cardType_ & CT_SD2) == 0)
  {
    sector *= BLOCK_SIZE;
  }

  SDCARD_Select();

  if (count == 1)
  { /* READ_SINGLE_BLOCK */
    if (SDCARD_SendCmd(CMD17, sector) != SDCARD_NO_ERROR) 
    {
      SDCARD_Deselect();
      return RES_ERROR;
    }
    const DRESULT result = SDCARD_ReadData(buff, BLOCK_SIZE, DATA_TOKEN_CMD17);
    if (result != RES_OK)
    {
      SDCARD_Deselect();
      return result;
    }
  }
  else
  { /* READ_MULTIPLE_BLOCK */
    if (SDCARD_SendCmd(CMD18, sector) != SDCARD_NO_ERROR) 
    {
      SDCARD_Deselect();
      return RES_ERROR;
    }

    do 
    {
      const DRESULT result = SDCARD_ReadData(buff, BLOCK_SIZE, DATA_TOKEN_CMD18);
      if (result != RES_OK) 
      {
        SDCARD_Deselect();
        return result;
      }
      buff += BLOCK_SIZE;
    } while (--count);

    /* STOP_TRANSMISSION */
    if (SDCARD_SendCmd(CMD12, 0) != SDCARD_NO_ERROR) 
    {
      SDCARD_Deselect();
      return RES_ERROR;
    }
  }

  SDCARD_Deselect();
  SDCARD_SkipByte();

  return RES_OK;
}

#if _USE_WRITE
static DRESULT SDCARD_WriteBlock(uint8_t* buff, DataToken dataToken) // sizeof(buff) == BLOCK_SIZE
{
  if (SDCARD_WaitNotBusy() != SDCARD_NO_ERROR)
  {
    return RES_NOTRDY;
  }

  SPI_TransmitByte((uint8_t)dataToken);

  SPI_TransmitBuffer(buff, BLOCK_SIZE);
  uint8_t crc[2] = { 0xFF, 0xFF };
  SPI_TransmitBuffer(crc, sizeof(crc));

  uint16_t n = NUMBER_OF_ATTEMPTS_RECEIVE_RESPONSE;
  do
  {
    /*
      dataResp:
      xxx0abc1
          010 - Data accepted
          101 - Data rejected due to CRC error
          110 - Data rejected due to write error
    */
    const uint8_t dataResponse = SDCARD_ReadByte();
    if ((dataResponse & 0x1F) == 0x05)
    { 
      break;
    }
  } while (--n != 0);

  if ((n == 0) || (SDCARD_WaitNotBusy() != SDCARD_NO_ERROR))
  {
    return RES_NOTRDY;
  }

  return RES_OK;
}

static DRESULT SDCARD_WriteStop(void)
{
  SPI_TransmitByte(STOP_TRANSACTION_TOKEN);

  // skip one byte before reading "busy"
  // this is required by the spec and is necessary for some real SD-cards!
  SDCARD_SkipByte();

  return (SDCARD_WaitNotBusy() != SDCARD_NO_ERROR) ? RES_NOTRDY : RES_OK;
}

/**
  * @brief  Writes Sector(s)
  * @param  pdrv - Physical drive number (0..) to identify the drive
  * @param  buff - Data to be written
  * @param  sector - Sector address (LBA)
  * @param  count - Number of sectors to write (1..128)
  * @retval DRESULT - Operation result
  */
DRESULT SDCARD_Write(BYTE pdrv, BYTE * buff, DWORD sector, UINT count)
{
  /* Supports only one type of drive */ 
  if (pdrv || (count == 0))
  {
    return RES_PARERR; 
  }

  /* no disk */
  if (diskStatus_ & STA_NOINIT)
  {
    return RES_NOTRDY;
  }

  /* write protection */
  if (diskStatus_ & STA_PROTECT)
  {
    return RES_WRPRT;
  }

  /* convert to byte address */
  if ((cardType_ & CT_SD2) == 0)
  {
    sector *= BLOCK_SIZE;
  }

  SDCARD_Select();

  if (count == 1)
  {
    /* WRITE_SINGLE_BLOCK */
    if (SDCARD_SendCmd(CMD24, sector) != SDCARD_NO_ERROR) 
    {
      SDCARD_Deselect();
      return RES_ERROR;
    }
    const DRESULT result = SDCARD_WriteBlock(buff, DATA_TOKEN_CMD24);
    if (result != RES_OK)
    {
      SDCARD_Deselect();
      return result;
    }
  }
  else
  {
    /* WRITE_MULTIPLE_BLOCK */
    if (cardType_ & CT_SD1)
    {
      if (SDCARD_SendCmd(ACMD23, count) != SDCARD_NO_ERROR) 
      {
        SDCARD_Deselect();
        return RES_ERROR;
      }
    }

    if (SDCARD_SendCmd(CMD25, sector) != SDCARD_NO_ERROR) 
    {
      SDCARD_Deselect();
      return RES_ERROR;
    }

    do 
    {
      const DRESULT result = SDCARD_WriteBlock(buff, DATA_TOKEN_CMD25);
      if (result != RES_OK) 
      {
        SDCARD_Deselect();
        return result;
      }
      buff += BLOCK_SIZE;
    } while (--count);

    const DRESULT result = SDCARD_WriteStop();
    if (result != RES_OK) 
    {
      SDCARD_Deselect();
      return result;
    }
  }

  SDCARD_Deselect();
  SDCARD_SkipByte();

  return RES_OK;
}
#endif  // _USE_WRITE

#if _USE_IOCTL
static DRESULT SDCARD_GetSectorCount(uint32_t* num)
{
  if (SDCARD_SendCmd(CMD9, 0) != SDCARD_NO_ERROR) 
  {
    return RES_ERROR;
  }
  uint8_t csd[CSD_SIZE];
  const DRESULT result = SDCARD_ReadData(csd, sizeof(csd), DATA_TOKEN_CMD9);
  if (result != RES_OK)
  {
    SDCARD_Deselect();
    return result;
  }

  if ((csd[0] >> 6) == 1) // SDv2
  {
    uint32_t tmp = csd[7] & 0x3F; // two bits are reserved
    tmp = (tmp << 8) | csd[8];
    tmp = (tmp << 8) | csd[9];
    // Full volume: (C_SIZE+1)*512KByte == (C_SIZE+1)<<19
    // Block size: 512Byte == 1<<9
    // Blocks number: CARD_SIZE/BLOCK_SIZE = (C_SIZE+1)*(1<<19) / (1<<9) = (C_SIZE+1)*(1<<10)
    tmp = (tmp + 1) << 10;
    *num = tmp;
  }
  else // SDv1 or MMCv3
  {
    const uint8_t n = (uint8_t)((csd[5] & 0x0F) + ((csd[10] & 0x80) >> 7) + ((csd[9] & 0x03) << 1) + 0x02);
    const uint16_t csize = (uint16_t)((csd[8] >> 6) + ((uint16_t)csd[7] << 2) + ((uint16_t)(csd[6] & 3) << 10) + 0x01);
    *num = (uint32_t) csize << (n - 9);
  }

  return RES_OK;
}

/**
  * @brief  I/O control operation
  * @param  pdrv - Physical drive number (0..)
  * @param  ctrl - Control code
  * @param  buff - Buffer to send/receive control data
  * @retval DRESULT - Operation result
  */
DRESULT SDCARD_DiskIoctl(BYTE pdrv, BYTE ctrl, void *buff) 
{
  uint8_t *ptr = buff;

  /* pdrv should be 0 */
  if (pdrv)
  {
    return RES_PARERR;
  }

  DRESULT result = RES_ERROR;

  if (ctrl == CTRL_POWER)
  {
    switch (*ptr)
    {
    case 0:
      SDCARD_PowerOff();
      result = RES_OK;
      break;
    case 1:
      if (SDCARD_PowerOn() == RES_OK)
      {
        result = RES_OK;
      }
      break;
    case 2:
      *(ptr + 1) = SDCARD_CheckPower();
      result = RES_OK;
      break;
    default:
      result = RES_PARERR;
    }
  }
  else
  {
    /* no disk */
    if (diskStatus_ & STA_NOINIT)
    {
      return RES_NOTRDY;
    }

    SDCARD_Select();

    switch (ctrl)
    {
    case GET_SECTOR_COUNT:
      result = SDCARD_GetSectorCount(buff);
      break;
    case GET_SECTOR_SIZE:
      *(WORD*) buff = BLOCK_SIZE;
      result = RES_OK;
      break;
    case CTRL_SYNC:
      if (SDCARD_WaitNotBusy() == SDCARD_NO_ERROR)
      {
        result = RES_OK;
      }
      break;
    case MMC_GET_CSD:
      /* SEND_CSD */
      if (SDCARD_SendCmd(CMD9, 0) == SDCARD_NO_ERROR && 
        SDCARD_ReadData(ptr, CSD_SIZE, DATA_TOKEN_CMD9) == RES_OK)
      {
        result = RES_OK;
      }
      break;
    case MMC_GET_CID:
      /* SEND_CID */
      if (SDCARD_SendCmd(CMD10, 0) == SDCARD_NO_ERROR && 
        SDCARD_ReadData(ptr, CSD_SIZE, DATA_TOKEN_CMD10) == RES_OK)
      {
        result = RES_OK;
      }
      break;
    case MMC_GET_OCR:
      /* READ_OCR */
      if (SDCARD_SendCmd(CMD58, 0) == SDCARD_NO_ERROR)
      {
        SDCARD_ReadBuffer(ptr, 4);
        result = RES_OK;
      }
      break;
    default:
      result = RES_PARERR;
    }

    SDCARD_Deselect();
    SDCARD_SkipByte();
  }

  return result;
}
#endif // _USE_IOCTL
