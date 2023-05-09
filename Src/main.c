/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "crc.h"
#include "dma2d.h"
#include "fatfs.h"
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "sdcard.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int _write(int file, char *ptr, int len) // for the "printf()" function
{
  HAL_UART_Transmit(&huart1, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY);
  return len;
}

static FRESULT ReadLongFile(void)
{
  uint32_t bytesRead;
  uint32_t i1;
  uint32_t f_size = f_size(&USERFile);
  printf("file size: %lu\r\n", f_size);
//  uint32_t ind = 0;
//  do
//  {
//    uint8_t sect[512];
//    if (f_size < sizeof(sect))
//    {
//      i1 = f_size;
//    }
//    else
//    {
//      i1 = sizeof(sect);
//    }
//    f_size -= i1;
//    f_lseek(&USERFile, ind);
//    f_read(&USERFile, sect, i1, (UINT *)&bytesRead);
//    for (uint32_t i = 0; i < bytesRead; i++)
//    {
//      putchar(*(sect + i));
//    }
//    ind += i1;
//  } while (f_size > 0);

  uint8_t buf[2048];
  f_read(&USERFile, buf, f_size, (UINT *)&bytesRead);
  for (uint32_t i = 0; i < bytesRead; i++)
  {
    putchar(*(buf + i));
  }

  printf("\r\n");
  return FR_OK;
}

static void Test1_WriteBlock(void)
{
  uint8_t buffer[1024] = "22255678911 know what are you looking for and that's why probably you are here. So I am not going to waste your time with the facts that what is an SD card? How does it work and all these... In this tutorial we will interface a SD card with stm32 micro controller using SPI mode.I am using STM32F103C8 controller and SD card size is 1 GB.Also we will do some basic file handling operations such as creating a file, writing, reading, deleting etc. Some advanced operations, i.e.directory related operations, wil\r\n55555678911 know what are you looking for and that's why probably you are here. So I am not going to waste your time with the facts that what is an SD card? How does it work and all these... In this tutorial we will interface a SD card with stm32 micro controller using SPI mode.I am using STM32F103C8 controller and SD card size is 1 GB.Also we will do some basic file handling operations such as creating a file, writing, reading, deleting etc. Some advanced operations, i.e.directory related operations, wil\r\n"; //Буфер данных для записи/чтения
  DRESULT res2 = SDCARD_Write(USERFatFS.drv, (uint8_t*)buffer, 0x0400, 2);
  printf("Write_res=%u\r\n", res2);
  printf("Write block...%s\r\n", (res2 == RES_OK) ? "OK" : "ERROR");
}

static void Test2_ReadBlock(void)
{
  uint8_t buffer[1024];
  for (uint32_t i = 0; i < sizeof(buffer); i++)
  {
    buffer[i] = '\0';
  }
  DRESULT res2 = SDCARD_Read(USERFatFS.drv, buffer, 0x0400, 2);
  printf("Read_res=%u\r\n", res2);
  if (res2 == RES_OK)
  {
    for (uint32_t i = 0; i < sizeof(buffer); i++)
    {
      putchar(*(buffer + i));
    }
    printf("Read block...OK\r\n");
  }
  else
  {
    printf("Read block...ERROR\r\n");
  }
}

static void Test3_WriteFile(void)
{
  if (f_mount(&USERFatFS, (TCHAR const*)USERPath, 0) != FR_OK)
  {
    Error_Handler();
  }
  else
  {
    if (f_open(&USERFile, "STM32_Write2.txt", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
    {
      Error_Handler();
    }
    else
    {
      const uint8_t wtext[] = "22255678911 know what are you looking for and that's why probably you are here. So I am not going to waste your time with the facts that what is an SD card? How does it work and all these... In this tutorial we will interface a SD card with stm32 micro controller using SPI mode.I am using STM32F103C8 controller and SD card size is 1 GB.Also we will do some basic file handling operations such as creating a file, writing, reading, deleting etc. Some advanced operations, i.e.directory related operations, wil\r\n55555678911 know what are you looking for and that's why probably you are here. So I am not going to waste your time with the facts that what is an SD card? How does it work and all these... In this tutorial we will interface a SD card with stm32 micro controller using SPI mode.I am using STM32F103C8 controller and SD card size is 1 GB.Also we will do some basic file handling operations such as creating a file, writing, reading, deleting etc. Some advanced operations, i.e.directory related operations, wil\r\n";
      //const uint8_t wtext[] = "Hello from STM32!!!\r\n";
      uint32_t bytesWritten;
      FRESULT res = f_write(&USERFile, wtext, sizeof(wtext), (void*)&bytesWritten);
      if ((bytesWritten == 0) || (res != FR_OK))
      {
        Error_Handler();
      }
      f_close(&USERFile);
    }
  }
}

static void Test4_ReadFile(void)
{
  if (f_mount(&USERFatFS, (TCHAR const*)USERPath, 0) != FR_OK)
  {
    Error_Handler();
  }
  else
  {
    if (f_open(&USERFile, "STM32.TXT", FA_READ) != FR_OK)
    {
      Error_Handler();
    }
    else
    {
      ReadLongFile();
      f_close(&USERFile);
    }
  }
}

static void Test5_ReadDirectory(void)
{
  FATFS *fs;
  DIR dir;
  DWORD fre_clust;
  DWORD fre_sect;
  DWORD tot_sect;

  if (f_mount(&USERFatFS, (TCHAR const*)USERPath, 1) != FR_OK)
  {
    Error_Handler();
  }
  else
  {
    FILINFO fileInfo;
    FRESULT result = f_opendir(&dir, "/");
    if (result == FR_OK)
    {
      while (1)
      {
        result = f_readdir(&dir, &fileInfo);
        if (result == FR_OK && fileInfo.fname[0])
        {
          printf("%s", fileInfo.fname);
          if (fileInfo.fattrib & AM_DIR)
          {
            printf(" [DIR]");
          }
          if (fileInfo.fattrib & AM_HID)
          {
            printf(" [HID]");
          }
        }
        else
        {
          break;
        }
        printf("\r\n");
      }

      f_getfree("/", &fre_clust, &fs);
      printf("fre_clust: %lu\r\n", fre_clust);
      printf("n_fatent: %lu\r\n", fs->n_fatent);
      printf("fs_csize: %d\r\n", fs->csize);

      tot_sect = (fs->n_fatent - 2) * fs->csize;
      printf("tot_sect: %lu\r\n", tot_sect);

      fre_sect = fre_clust * fs->csize;
      printf("fre_sect: %lu\r\n", fre_sect);
      printf("%lu KB total drive space.\r\n%lu KB available.\r\n", tot_sect / 2, fre_sect / 2);

      f_closedir(&dir);
    }
  }

  if (f_mount(NULL, USERPath, 0) == FR_OK)
  {
    FATFS_UnLinkDriver(USERPath);
  }
}

void Test6_FatFs(void)
{
  FRESULT res; /* FatFs function common result code */
  uint32_t byteswritten, bytesread; /* File write/read counts */
  uint8_t wtext[] = "This is STM32 working with FatFs"; /* File write buffer */
  uint8_t rtext[100]; /* File read buffer */
  uint8_t workBuffer[_MAX_SS];
  
  /* Configure LED1  */
//  HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_RESET);
  
  /*##-1- Link the micro SD disk I/O driver ##################################*/
  if (retUSER == 0)
  {
    /*##-2- Register the file system object to the FatFs module ##############*/
    res = f_mount(&USERFatFS, (TCHAR const*)USERPath, 1);
    if (res != FR_OK)
    {
      /* FatFs Initialization Error */
      Error_Handler();
    }
    else
    {
      /*##-3- Create a FAT file system (format) on the logical drive #########*/
      /* WARNING: Formatting the uSD card will delete all content on the device */
      res = f_mkfs((TCHAR const*)USERPath, FM_ANY, 0, workBuffer, sizeof(workBuffer));
      if (res != FR_OK)
      {
        /* FatFs Format Error */
        Error_Handler();
      }
      else
      {       
        /*##-4- Create and Open a new text file object with write access #####*/
        res = f_open(&USERFile, "STM32.TXT", FA_CREATE_ALWAYS | FA_WRITE);
        if (res != FR_OK)
        {
          /* 'STM32.TXT' file Open for write Error */
          Error_Handler();
        }
        else
        {
          /*##-5- Write data to the text file ################################*/
          res = f_write(&USERFile, wtext, sizeof(wtext), (void *)&byteswritten);
          
          if ((byteswritten == 0) || (res != FR_OK))
          {
            /* 'STM32.TXT' file Write or EOF Error */
            Error_Handler();
          }
          else
          {
            /*##-6- Close the open text file #################################*/
            res = f_close(&USERFile);
            if (res != FR_OK)
            {
              Error_Handler();
            }
            
            /*##-7- Open the text file object with read access ###############*/
            res = f_open(&USERFile, "STM32.TXT", FA_READ);
            if (res != FR_OK)
            {
              /* 'STM32.TXT' file Open for read Error */
              Error_Handler();
            }
            else
            {
              /*##-8- Read data from the text file ###########################*/
              res = f_read(&USERFile, rtext, sizeof(rtext), (UINT*)&bytesread);
              
              if ((bytesread == 0) || (res != FR_OK))
              {
                /* 'STM32.TXT' file Read or EOF Error */
                Error_Handler();
              }
              else
              {
                /*##-9- Close the open text file #############################*/
                res = f_close(&USERFile);
                if (res != FR_OK)
                {
                  Error_Handler();
                }
                
                /*##-10- Compare read data with the expected data ############*/
                if ((bytesread != byteswritten))
                {                
                  /* Read data is different from the expected data */
                  Error_Handler();
                }
                else
                {
                  /* Success of the demo: no error occurrence */
                  //HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
                }
              }
            }
          }
        }
      }
    }
  }
  
  /*##-11- Unlink the micro SD disk I/O driver ###############################*/
  FATFS_UnLinkDriver(USERPath);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA2D_Init();
  MX_CRC_Init();
  MX_USART1_UART_Init();
  MX_SPI2_Init();
  MX_FATFS_Init();
  /* USER CODE BEGIN 2 */
//  Test1_WriteBlock();
//  Test2_ReadBlock();
//  Test3_WriteFile();
//  Test4_ReadFile();
//  Test5_ReadDirectory();
  Test6_FatFs();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 15;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
