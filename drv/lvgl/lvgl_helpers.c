/**
 * @file lvgl_helpers.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "sdkconfig.h"
#include "lvgl_helpers.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "lvgl_tft/disp_spi.h"

#include "lvgl_spi_conf.h"

#include "lvgl.h"

/*********************
 *      DEFINES
 *********************/

#define TAG "lvgl_helpers"

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/
static SemaphoreHandle_t semphor = NULL;

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/
     
void lvgl_spi_transmit(spi_master_mode_t tMode, const uint8_t* pucData, uint32_t uiLen)
{
    if (tMode == SPI_RECV)
    {
        return;
    }
    //LOGI( "SPI_SEND len:%u", len);

    if (uiLen == 0)
    {
        return;
    }

    if (!semphor)
        xSemaphoreTake(semphor, portMAX_DELAY);
    
    uint32_t uiDoneLen = 0, uiTmpLen = 0;
    
    spi_trans_t trans;
    uint32_t addr = 0x0;
    //memset(&trans, 0x0, sizeof(trans));
    trans.bits.val = 0;                     // clear all bit
    trans.addr = &addr;

    uint32_t uiBuf[16] = {0};
    trans.mosi = uiBuf;
    
    do
    {        
        uiTmpLen = (uiLen - uiDoneLen) > 64 ? 64 : (uiLen - uiDoneLen);
        
        if (uiTmpLen < 64)
        {
            if ((uiTmpLen % 4) > 0)
            {
                trans.bits.addr = (uiTmpLen % 4) * 8;
                trans.bits.mosi = (uiTmpLen / 4) * 32;
                memcpy(&addr, pucData + uiDoneLen, uiTmpLen % 4);
                uint32_t uiTmp = addr;
                addr = (uiTmp & 0xff) << 24;
                addr += (uiTmp & 0xff00) << 8;
                addr += (uiTmp & 0xff0000) >> 8;
                addr += uiTmp >> 24;
                //LOGI("bits.addr:%u bits.mosi:%u trans.addr:%#x", trans.bits.addr, trans.bits.mosi, *trans.addr);

                if (uiTmpLen / 4)
                {
                    if (((uint32_t)(pucData + (uiDoneLen + uiTmpLen % 4)) / 4) == 0)
                    {
                        trans.mosi = (uint32_t*)(pucData + (uiDoneLen + uiTmpLen % 4));
                    }
                    else
                    {
                        memcpy(uiBuf, pucData + (uiDoneLen + (uiTmpLen % 4)), (uiTmpLen / 4) * 4);
                    }
                    //LOGI("trans.mosi[0]:%#x", trans.mosi[0]);
                }
                else
                {
                    trans.mosi = NULL;
                }
            }
            else
            {
                trans.bits.addr = 0;
                trans.bits.mosi = uiTmpLen * 8;
                if ((((uint32_t)(pucData + uiDoneLen)) / 4) == 0)
                {
                    trans.mosi = (uint32_t*)(pucData + uiDoneLen);
                }
                else
                {
                    memcpy(uiBuf, pucData + uiDoneLen, uiTmpLen);
                }
                
            }
        }
        else
        {
            trans.bits.addr = 0;
            trans.bits.mosi = (64 * 8);
            if ((((uint32_t)(pucData + uiDoneLen)) / 4) == 0)
            {
                trans.mosi = (uint32_t*)(pucData + uiDoneLen);
            }
            else
            {
                memcpy(uiBuf, pucData + uiDoneLen, 64);
            }
        }
        spi_trans(HSPI_HOST, &trans);
        uiDoneLen += uiTmpLen;

    }while (uiLen > uiDoneLen);
    
    if (!semphor)
    {
        //xSemaphoreTake(semphor, portMAX_DELAY);
        xSemaphoreGive(semphor);
    }    
}

static void IRAM_ATTR spi_event_callback(int event, void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    switch (event) {
        case SPI_INIT_EVENT: {

        }
        break;

        case SPI_TRANS_START_EVENT: {
            //xSemaphoreTakeFromISR(semphor, &xHigherPriorityTaskWoken);
        }
        break;

        case SPI_TRANS_DONE_EVENT: {
            //if (!semphor)
                //xSemaphoreGiveFromISR(semphor, &xHigherPriorityTaskWoken);
        }
        break;

        case SPI_DEINIT_EVENT: {
        }
        break;
    }
    
    if (xHigherPriorityTaskWoken == pdTRUE) 
    {
        taskYIELD();
    }
}

void lvgl_spi_init()
{
    LOGI("Enter >>");
    
    semphor = xSemaphoreCreateBinary();
    spi_config_t spi_config;
    // Load default interface parameters
    // CS_EN:1, MISO_EN:1, MOSI_EN:1, BYTE_TX_ORDER:1, BYTE_TX_ORDER:1, BIT_RX_ORDER:0, BIT_TX_ORDER:0, CPHA:0, CPOL:0
    spi_config.interface.val = SPI_DEFAULT_INTERFACE;

    // Load default interrupt enable
    // TRANS_DONE: true, WRITE_STATUS: false, READ_STATUS: false, WRITE_BUFFER: false, READ_BUFFER: false
    spi_config.intr_enable.val = SPI_MASTER_DEFAULT_INTR_ENABLE;

    /*
    spi_config.interface.cs_en = 0;    
    spi_config.interface.mosi_en = 1;
    spi_config.interface.miso_en = 1;
    */
    // CPOL: 1, CPHA: 1
    spi_config.interface.cpol = 1;
    spi_config.interface.cpha = 1;
    // Set SPI to master mode
    // ESP8266 Only support half-duplex
    spi_config.mode = SPI_MASTER_MODE;
    // Set the SPI clock frequency division factor
    spi_config.clk_div = SPI_40MHz_DIV;
    spi_config.event_cb = spi_event_callback;
    LOGI("init spi");
    spi_init(HSPI_HOST, &spi_config);
    LOGI("End <<");
}

/* Interface and driver initialization */
void lvgl_driver_init(void)
{
    /* Since LVGL v8 LV_HOR_RES_MAX and LV_VER_RES_MAX are not defined, so
     * print it only if they are defined. */
#if (LVGL_VERSION_MAJOR < 8)
    LOGI("Display hor size: %d, ver size: %d", LV_HOR_RES_MAX, LV_VER_RES_MAX);
#endif

    LOGI("Display buffer size: %d", DISP_BUF_SIZE);

/* Display controller initialization */
#if defined CONFIG_LV_TFT_DISPLAY_PROTOCOL_SPI
    LOGI("Initializing SPI master for display");

    lvgl_spi_init();

    disp_driver_init();
#elif defined (CONFIG_LV_I2C_DISPLAY)
    disp_driver_init();
#else
#error "No protocol defined for display controller"
#endif
}

