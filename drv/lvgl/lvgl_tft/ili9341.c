/**
 * @file ili9341.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "ili9341.h"
#include "disp_spi.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*********************
 *      DEFINES
 *********************/
 #define TAG "ILI9341"

/**********************
 *      TYPEDEFS
 **********************/

/*The LCD needs a bunch of command/argument values to be initialized. They are stored in this struct. */
typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; //No of data in data; bit 7 = delay after set; 0xFF = end of cmds.
} lcd_init_cmd_t;

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void ili9341_set_orientation(uint8_t orientation);

static void ili9341_send_cmd(uint8_t cmd);
static void ili9341_send_data(void * data, uint16_t length);
static void ili9341_send_color(void * data, uint16_t length);

/**********************
 *  STATIC VARIABLES
 **********************/
 #if 1
lcd_init_cmd_t ili_init_cmds[]={
	{0xCF, {0x00, 0x83, 0X30}, 3},
	{0xED, {0x64, 0x03, 0X12, 0X81}, 4},
	{0xE8, {0x85, 0x01, 0x79}, 3},
	{0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
	{0xF7, {0x20}, 1},
	{0xEA, {0x00, 0x00}, 2},
	{0xC0, {0x26}, 1},          /*Power control*/
	{0xC1, {0x11}, 1},          /*Power control */
	{0xC5, {0x35, 0x3E}, 2},    /*VCOM control*/
	{0xC7, {0xBE}, 1},          /*VCOM control*/
	{0x36, {0x28}, 1},          /*Memory Access Control*/
	{0x3A, {0x55}, 1},			/*Pixel Format Set*/
	{0xB1, {0x00, 0x1B}, 2},
	{0xF2, {0x08}, 1},
	{0x26, {0x01}, 1},
	{0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0X87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},
	{0XE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},
	{0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
	{0x2B, {0x00, 0x00, 0x01, 0x3f}, 4},
	{0x2C, {0}, 0},
	{0xB7, {0x07}, 1},
	{0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
	{0x11, {0}, 0x80},
	{0x29, {0}, 0x80},
	{0, {0}, 0xff},
}; 
#else
lcd_init_cmd_t ili_init_cmds[]={
	{0xCF, {0x00, 0xC1, 0X30}, 3},
	{0xED, {0x64, 0x03, 0X12, 0X81}, 4},
	{0xE8, {0x85, 0x01, 0x7A}, 3},
	{0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
	{0xF7, {0x20}, 1},
	{0xEA, {0x00, 0x00}, 2},
	{0xC0, {0x21}, 1},          /*Power control*/
	{0xC1, {0x11}, 1},          /*Power control */
	{0xC5, {0x31, 0x3C}, 2},    /*VCOM control*/
	{0xC7, {0x9F}, 1},          /*VCOM control*/
	{0x36, {0x08}, 1},          /*Memory Access Control*/
	{0x3A, {0x55}, 1},			/*Pixel Format Set*/
	{0xB1, {0x00, 0x1B}, 2},
    {0xB6, {0x0A, 0xA2}, 2},    // Display Function Control
	{0xF2, {0x00}, 1},          // 3Gamma Function Disable
	{0x26, {0x01}, 1},          //Gamma curve selected
	{0xE0, {0x0F, 0x20, 0x1d, 0x0b, 0x10, 0x0a, 0x49, 0Xa9, 0x3b, 0x0A, 0x15, 0x06, 0x0c, 0x06, 0x00}, 15},
	{0XE1, {0x00, 0x1f, 0x22, 0x04, 0x0f, 0x05, 0x36, 0x46, 0x46, 0x05, 0x0b, 0x09, 0x33, 0x39, 0x0F}, 15},
	//{0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
	//{0x2B, {0x00, 0x00, 0x01, 0x3f}, 4},
	//{0x2C, {0}, 0},
	//{0xB7, {0x07}, 1},
	//{0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
	{0x11, {0}, 0x80},
	{0x29, {0}, 0},
    {0x2C, {0}, 0},
	{0, {0}, 0xff},
}; 

#endif
/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void ili9341_init(void)
{
    LOGI("Enter >>");
	//Initialize non-SPI GPIOs
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO15/16
    io_conf.pin_bit_mask = (1ULL << ILI9341_DC);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

#if ILI9341_USE_RST
    io_conf.pull_up_en = 0;
    io_conf.pin_bit_mask = (1ULL << ILI9341_RST);
    gpio_config(&io_conf);

	//Reset the display
	gpio_set_level(ILI9341_RST, 0);
	vTaskDelay(100 / portTICK_RATE_MS);
	gpio_set_level(ILI9341_RST, 1);
	vTaskDelay(120 / portTICK_RATE_MS);
#endif

	LOGI("Initialization.");

	//Send all the commands
	uint16_t cmd = 0;
	while (ili_init_cmds[cmd].databytes != 0xff) 
    {
        //LOGI("cmd:%u", cmd);
		ili9341_send_cmd(ili_init_cmds[cmd].cmd);
        if (ili_init_cmds[cmd].databytes & 0x1F)
        {
    		ili9341_send_data(ili_init_cmds[cmd].data, ili_init_cmds[cmd].databytes & 0x1F);
        }
		if (ili_init_cmds[cmd].databytes & 0x80) 
        {
			vTaskDelay(100 / portTICK_RATE_MS);
		}
		cmd++;
	}

    ili9341_set_orientation(CONFIG_LV_DISPLAY_ORIENTATION);

#if ILI9341_INVERT_COLORS == 1
	ili9341_send_cmd(0x21);
#else
	ili9341_send_cmd(0x20);
#endif
    LOGI("End <<");
}


void ili9341_flush(lv_disp_drv_t * drv, const lv_area_t * area, lv_color_t * color_map)
{
    //LOGI("Enter >>");
	uint8_t data[4];

	/*Column addresses*/
	ili9341_send_cmd(0x2A);
	data[0] = (area->x1 >> 8) & 0xFF;
	data[1] = area->x1 & 0xFF;
	data[2] = (area->x2 >> 8) & 0xFF;
	data[3] = area->x2 & 0xFF;
	ili9341_send_data(data, 4);

	/*Page addresses*/
	ili9341_send_cmd(0x2B);
	data[0] = (area->y1 >> 8) & 0xFF;
	data[1] = area->y1 & 0xFF;
	data[2] = (area->y2 >> 8) & 0xFF;
	data[3] = area->y2 & 0xFF;
	ili9341_send_data(data, 4);

	/*Memory write*/
	ili9341_send_cmd(0x2C);
	uint32_t size = lv_area_get_width(area) * lv_area_get_height(area);
	ili9341_send_color((void*)color_map, size * 2);
    
    //LOGI("End <<");
}

void ili9341_sleep_in()
{
    LOGI("Enter >>");
	uint8_t data[] = {0x08};
	ili9341_send_cmd(0x10);
	ili9341_send_data(&data, 1);
    
    LOGI("End <<");
}

void ili9341_sleep_out()
{
    LOGI("Enter >>");
	uint8_t data[] = {0x08};
	ili9341_send_cmd(0x11);
	ili9341_send_data(&data, 1);
    
    LOGI("End <<");
}

/**********************
 *   STATIC FUNCTIONS
 **********************/


static void ili9341_send_cmd(uint8_t cmd)
{
    disp_wait_for_pending_transactions();
    gpio_set_level(ILI9341_DC, 0);	 /*Command mode*/    
    disp_spi_send_data(&cmd, 1);
}

static void ili9341_send_data(void * data, uint16_t length)
{
    disp_wait_for_pending_transactions();
    gpio_set_level(ILI9341_DC, 1);	 /*Data mode*/
    disp_spi_send_data(data, length);
}

static void ili9341_send_color(void * data, uint16_t length)
{
    disp_wait_for_pending_transactions();
    gpio_set_level(ILI9341_DC, 1);   /*Data mode*/
    disp_spi_send_colors(data, length);
}

static void ili9341_set_orientation(uint8_t orientation)
{
    LOGI("Enter >>");
    // ESP_ASSERT(orientation < 4);

    const char *orientation_str[] = {
        "PORTRAIT", "PORTRAIT_INVERTED", "LANDSCAPE", "LANDSCAPE_INVERTED"
    };

    LOGI("Display orientation: %s", orientation_str[orientation]);

#if defined CONFIG_LV_PREDEFINED_DISPLAY_M5STACK
    uint8_t data[] = {0x68, 0x68, 0x08, 0x08};
#elif defined (CONFIG_LV_PREDEFINED_DISPLAY_M5CORE2)
	uint8_t data[] = {0x08, 0x88, 0x28, 0xE8};
#elif defined (CONFIG_LV_PREDEFINED_DISPLAY_WROVER4)
    uint8_t data[] = {0x6C, 0xEC, 0xCC, 0x4C};
#elif defined (CONFIG_LV_PREDEFINED_DISPLAY_NONE)
    uint8_t data[] = {0x48, 0x88, 0x28, 0xE8};
#endif

    LOGI("0x36 command value: 0x%02X", data[orientation]);

    ili9341_send_cmd(0x36);
    ili9341_send_data((void *) &data[orientation], 1);
    
    LOGI("End <<");
}
