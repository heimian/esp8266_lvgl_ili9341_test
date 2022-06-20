/**
 * @file lvgl_helpers.h
 */

#ifndef LVGL_HELPERS_H
#define LVGL_HELPERS_H

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <stdbool.h>

#include "lvgl_spi_conf.h"
#include "lvgl_tft/disp_driver.h"

/*********************
 *      DEFINES
 *********************/
//#define CONFIG_LV_TFT_DISPLAY_MONOCHROME
#define CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9341
#define CONFIG_LV_TOUCH_CONTROLLER_XPT2046

#define CONFIG_LV_TFT_DISPLAY_PROTOCOL_SPI
#define LV_HOR_RES_MAX                                  (240)
#define LV_VER_RES_MAX                                  (320)

#define CONFIG_LV_DISP_SPI_PIN_CS                       (15)    // gpio_15
#define CONFIG_LV_DISP_PIN_DC                           (4)     // gpio_4
#define CONFIG_LV_DISP_PIN_RST                          (5)     // gpio_5
#define CONFIG_LV_DISP_USE_RST                          (1)
#define CONFIG_LV_DISPLAY_ORIENTATION                   (0)
#define CONFIG_LV_PREDEFINED_DISPLAY_NONE               (1)
#define CONFIG_LV_INVERT_COLORS                         (0)

/*********  touch  ************/
// XPT2046
#define CONFIG_LV_TOUCH_PIN_IRQ                         (9)     // gpio_9
#define CONFIG_LV_TOUCH_SPI_PIN_CS                      (2)    // gpio_2
#define CONFIG_LV_TOUCH_X_MIN                           (0)
#define CONFIG_LV_TOUCH_Y_MIN                           (0)
#define CONFIG_LV_TOUCH_X_MAX                           (240)
#define CONFIG_LV_TOUCH_Y_MAX                           (320)
#define CONFIG_LV_TOUCH_INVERT_X                        (0)
#define CONFIG_LV_TOUCH_INVERT_Y                        (0)
#define CONFIG_LV_TOUCH_XY_SWAP                         (0)
#define CONFIG_LV_TOUCH_DETECT_IRQ                      (1)
#define CONFIG_LV_TOUCH_DETECT_IRQ_PRESSURE             (1)

/********* input keys  *********/
#define CONFIG_LV_INPUT_KEY_PIN_1                       (1)
#define CONFIG_LV_INPUT_KEY_PIN_2                       (3)

/* DISP_BUF_SIZE value doesn't have an special meaning, but it's the size
 * of the buffer(s) passed to LVGL as display buffers. The default values used
 * were the values working for the contributor of the display controller.
 *
 * As LVGL supports partial display updates the DISP_BUF_SIZE doesn't
 * necessarily need to be equal to the display size.
 *
 * When using RGB displays the display buffer size will also depends on the
 * color format being used, for RGB565 each pixel needs 2 bytes.
 * When using the mono theme, the display pixels can be represented in one bit,
 * so the buffer size can be divided by 8, e.g. see SSD1306 display size. */
#if defined (CONFIG_CUSTOM_DISPLAY_BUFFER_SIZE)
#define DISP_BUF_SIZE   CONFIG_CUSTOM_DISPLAY_BUFFER_BYTES
#else
#if defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7789)
#define DISP_BUF_SIZE  (LV_HOR_RES_MAX * 40)
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7735S
#define DISP_BUF_SIZE  (LV_HOR_RES_MAX * 40)
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ST7796S
#define DISP_BUF_SIZE  (LV_HOR_RES_MAX * 40)
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_HX8357
#define DISP_BUF_SIZE  (LV_HOR_RES_MAX * 40)
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SH1107
#define DISP_BUF_SIZE  (LV_HOR_RES_MAX * LV_VER_RES_MAX)
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9481
#define DISP_BUF_SIZE  (LV_HOR_RES_MAX * 40)
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9486
#define DISP_BUF_SIZE  (LV_HOR_RES_MAX * 40)
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9488
#define DISP_BUF_SIZE  (LV_HOR_RES_MAX * 40)
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9341
#define DISP_BUF_SIZE  (LV_HOR_RES_MAX * 30)
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SSD1306
#if defined (CONFIG_LV_THEME_MONO)
#define DISP_BUF_SIZE  (LV_HOR_RES_MAX * (LV_VER_RES_MAX / 8))
#else
#define DISP_BUF_SIZE  (LV_HOR_RES_MAX * LV_VER_RES_MAX)
#endif
#elif defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_FT81X)
#define DISP_BUF_LINES  40
#define DISP_BUF_SIZE  (LV_HOR_RES_MAX * DISP_BUF_LINES)
#elif defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_IL3820)
#define DISP_BUF_SIZE (LV_VER_RES_MAX * IL3820_COLUMNS)
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_RA8875
#define DISP_BUF_SIZE  (LV_HOR_RES_MAX * 40)
#elif defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_GC9A01)
#define DISP_BUF_SIZE  (LV_HOR_RES_MAX * 40)
#elif defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_JD79653A)
#define DISP_BUF_SIZE ((LV_VER_RES_MAX * LV_VER_RES_MAX) / 8) // 5KB
#elif defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_UC8151D)
#define DISP_BUF_SIZE ((LV_VER_RES_MAX * LV_VER_RES_MAX) / 8) // 2888 bytes
#elif defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_ILI9163C
#define DISP_BUF_SIZE (LV_HOR_RES_MAX * 40)
#elif defined (CONFIG_LV_TFT_DISPLAY_CONTROLLER_PCD8544)
#define DISP_BUF_SIZE  (LV_HOR_RES_MAX * (LV_VER_RES_MAX / 8))
#else
#error "No display controller selected"
#endif
#endif


/**********************
 *      TYPEDEFS
 **********************/
typedef enum 
{
    SPI_SEND = 0,
    SPI_RECV
}spi_master_mode_t;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

void lvgl_i2c_locking(void* leader);

/* Initialize detected SPI and I2C bus and devices */
void lvgl_driver_init(void);

void lvgl_spi_transmit(spi_master_mode_t tMode, const uint8_t* pucData, uint32_t uiLen);

/**********************
 *      MACROS
 **********************/


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LVGL_HELPERS_H */
