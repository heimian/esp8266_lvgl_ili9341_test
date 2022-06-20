/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/hw_timer.h"
#include "cJSON.h"

#include "esp_spiffs.h"

#include "lvgl.h"
#include "lvgl_helpers.h"
#include "lv_log.h"

#include "lv_demo.h"

static const char *TAG = "app_main";

#define LV_TICK_PERIOD_MS           (10)

#define BOARD_TYPE_ESP01S			(0)
#define BOARD_TYPE_ESP12E			(1)
#define TARGET_BOARD_TYPE			BOARD_TYPE_ESP12E

#if (TARGET_BOARD_TYPE == BOARD_TYPE_ESP01S)
#define BOARD_LED_PIN				(0)
#define BOARD_SWITCH_PIN			(2)
#define BOARD_SWITCH_ON_STATE		(0)
#define BOARD_DEFAULT_ID			(123456)
#elif (TARGET_BOARD_TYPE == BOARD_TYPE_ESP12E)
#define BOARD_LED_PIN				(16)
#define BOARD_SWITCH_PIN			(2)
#define BOARD_SWITCH_ON_STATE		(1)
#define BOARD_DEFAULT_ID			(654321)
#endif

#define DEV_PROPERTY_MAX			(12)
#define SERVER_TOPIC				"manager"

#define MSG_KEYWORD_PROPERTY		"property"
#define MSG_KEYWORD_ADD				"add"
#define MSG_KEYWORD_DELETE			"delete"

#define GET_TYPE_STR(x)				((x) == DEV_TYPE_MANAGER ? "manager" : \
									(x) == DEV_TYPE_SWITCH ? "switch" : "unknow")

/*
#define LOGD(fmt, ...)              ESP_LOGD(TAG, "[%s:%d]:" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(fmt, ...)              ESP_LOGI(TAG, "[%s]:" fmt, __func__, ##__VA_ARGS__)
#define LOGE(fmt, ...)              ESP_LOGE(TAG, "[%s:%d]:" fmt, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(fmt, ...)              ESP_LOGW(TAG, "[%s:%d]:" fmt, __func__, __LINE__, ##__VA_ARGS__)
*/

typedef enum
{
	DEV_TYPE_MANAGER = 0x00,
	DEV_TYPE_SWITCH = 0x01,
	DEV_TYPE_UNKNOW = 0xff,
}DEV_TYPE_E;

typedef enum
{
	DEV_PROP_CONTROL = 0x00,
	DEV_PROP_DELAY_ON,
	DEV_PROP_DELAY_OFF,
	DEV_PROP_SCHEDULE_ON,
	DEV_PROP_SCHEDULE_OFF,
	DEV_PROP_SET_TIME,
	DEV_PROP_SET_NAME,
	DEV_PROP_UNKNOW = 0xff,
}DEV_PROP_TYPE_E;

typedef enum
{
    DEV_CMD_ADD = 0x00,
    DEV_CMD_DELETE,
    DEV_CMD_UNKNOW = 0xff,
}DEV_CMD_TYPE_E;

typedef struct
{
	char* pcKeyWord;
	DEV_PROP_TYPE_E eType;
}DEV_PROP_KW_T;

typedef struct
{
	char* pcKeyWord;
	DEV_CMD_TYPE_E eType;
}DEV_CMD_KW_T;

#if 0
struct tm 
{
	int tm_sec;    /* Seconds (0-60) */
	int tm_min;    /* Minutes (0-59) */
	int tm_hour;   /* Hours (0-23) */
	int tm_mday;   /* Day of the month (1-31) */
	int tm_mon;    /* Month (0-11) */
	int tm_year;   /* Year - 1900 */
	int tm_wday;   /* Day of the week (0-6, Sunday = 0) */
	int tm_yday;   /* Day in the year (0-365, 1 Jan = 0) */
	int tm_isdst;  /* Daylight saving time */
 };
#endif

typedef struct
{
	DEV_TYPE_E eDevType;
	union
	{
		struct
		{
			DEV_PROP_TYPE_E eType;
			union
			{
				bool bControl;
				unsigned int uiDelaySec;
				struct tm tSchedule;
			};
		}tSwitch;
	};
}DEV_PROPERTY_T;

#define MQTT_CLIENT_STATE_CONNECTED         (0x01 << 0)
#define MQTT_CLIENT_STATE_SUBSCRIBED        (0x01 << 1)

static uint64_t			        g_ullDevID = BOARD_DEFAULT_ID;
static char				        g_pcDevIDStr[20] = {0};
static char 			        g_cDevName[128] = {0};
static char 			        g_cDevTopic[128] = {0};
static const DEV_TYPE_E         g_eDevType = DEV_TYPE_SWITCH;
static int                      g_iMqttClientState = 0;

static unsigned char g_ucState;
/* Creates a semaphore to handle concurrent call to lvgl stuff
 * If you wish to call *any* lvgl function from other threads/tasks
 * you should lock on the very same semaphore! */
SemaphoreHandle_t xGuiSemaphore;
lv_obj_t * label1 = NULL;

static int xGPIOInit(unsigned int uiGPIOIndex)
{
	if (uiGPIOIndex >= GPIO_NUM_MAX)
	{
		return -1;
	}
	
	gpio_config_t tConfig;
	//disable interrupt
	tConfig.intr_type = GPIO_INTR_DISABLE;
	//set as output mode
	tConfig.mode = GPIO_MODE_OUTPUT;
	//bit mask of the pins that you want to set,e.g.GPIO15/16
	tConfig.pin_bit_mask = 1ULL << uiGPIOIndex;
	//disable pull-down mode
	tConfig.pull_down_en = 0;
	//disable pull-up mode
	tConfig.pull_up_en = 0;
	//configure GPIO with the given settings
	return gpio_config(&tConfig);
}

static int xGPIOCtrl(unsigned int uiGPIOIndex, unsigned char ucValue)
{
	if (uiGPIOIndex >= GPIO_NUM_MAX || (ucValue > 1))
	{
		return -1;
	}

	return gpio_set_level((gpio_num_t)uiGPIOIndex, ucValue);
}

static const char* xGetMacStr(void)
{
	uint8_t ucMac[6] = {0};
	static char cTemp[64] = {0};
	if (ESP_OK != esp_efuse_mac_get_default(ucMac))
	{
		LOGE("esp_efuse_mac_get_default fail!!");
		return NULL;
	}
	else
	{	
		int iLen = 0;
		for (int i = 0; i < 6; i++)
		{
			iLen += snprintf(cTemp + iLen, 63 - iLen, "%02x", ucMac[i]);
		}
		//LOGI("LocalMac%s", cTemp);
		return cTemp;
	}
}

static uint64_t xGetMacHex(void)
{
	uint8_t ucMac[6] = {0};
	uint64_t ullMacHex = 0;
	if (ESP_OK != esp_efuse_mac_get_default(ucMac))
	{
		LOGE("esp_efuse_mac_get_default fail!!");
		return 0xffffffffffff;
	}
	else
	{	
		for (int i = 0; i < 6; i++)
		{
			ullMacHex += (uint64_t)ucMac[i] << (8 * (5 - i));
		}

		//LOGI("MacHex:%#lx", ulMacHex);
		return ullMacHex;
	}
}

static int xSetTime(struct tm tTime)
{
    struct timeval tv;

    tTime.tm_year -= 1900;
    tTime.tm_mon -= 1;

    tv.tv_sec = mktime(&tTime);
    tv.tv_usec = 0;
    return settimeofday(&tv, NULL);
}

static const char* _xGetTimeStr(bool bType)
{
    static char cTemp[128] = {0};
    struct tm tLocal;
    time_t tTime;
    time(&tTime);
    memcpy(&tLocal, localtime(&tTime), sizeof(tLocal));

	if (bType)
	{
	    snprintf(cTemp, sizeof(cTemp) - 1, "%04u-%02u-%02u %02u:%02u:%02u",
	            (tLocal.tm_year + 1900), tLocal.tm_mon + 1, tLocal.tm_mday,
	            tLocal.tm_hour, tLocal.tm_min, tLocal.tm_sec);
	}
	else
	{
		snprintf(cTemp, sizeof(cTemp) - 1, "%04u%02u%02u_%02u_%02u_%02u",
	            (tLocal.tm_year + 1900), tLocal.tm_mon + 1, tLocal.tm_mday,
	            tLocal.tm_hour, tLocal.tm_min, tLocal.tm_sec);
	}
	
    return cTemp;
}

static int xReadConfigFromFlash(cJSON** ppJObj)
{
    if (ppJObj == NULL)
    {
        return -1;
    }
    int iRet = 0;
    
    esp_vfs_spiffs_conf_t config;
    config.base_path = "/spiffs";
    config.partition_label = NULL;
    config.max_files = 5;
    config.format_if_mount_failed = true;

    esp_err_t tRet = esp_vfs_spiffs_register(&config);
    if (tRet != ESP_OK)
    {
        LOGE("esp_vfs_spiffs_register() fail!! ret:%s", esp_err_to_name(tRet));
        return -1;
    }
    else
    {
        FILE* pFile = fopen("/spiffs/config.txt", "r");
        if(pFile == NULL) 
        {
            LOGE("open file error!!");
            iRet = -1;
            goto End_Func;
        }
        else
        {
            char cBuf[512] = {0};
            size_t tSize = fread(cBuf, 1, sizeof(cBuf), pFile);
            LOGI("tSize:%d read:%s", tSize, cBuf);
            if (tSize <= 0)
            {
                iRet = -1;
                goto End_Func;
            }
            else
            {
                *ppJObj = cJSON_Parse(cBuf);
                if (*ppJObj == NULL)
                {
                    LOGE("cJSON_Parse fail!!(%s)", cJSON_GetErrorPtr());
                    iRet = -1;
                    goto End_Func;
                }
                char* pcTemp = cJSON_Print(*ppJObj);

                if (pcTemp)
                {
                    LOGI("got JSON object:%s.", pcTemp);
                }
            }
        }

End_Func:
        if (pFile)
        {
            fclose(pFile);
        }
        esp_vfs_spiffs_unregister(NULL);
    }
    return iRet;
}

static int xWriteConfigToFlash(cJSON* pJObj)
{
    if (pJObj == NULL)
    {
        return -1;
    }
    
    int iRet = 0;

    char* pcTemp = cJSON_Print(pJObj);

    if (pcTemp == NULL)
    {
        LOGE("Parameter error!! cJSON_GetStringValue not available.");
        return -1;
    }
    
    esp_vfs_spiffs_conf_t config;
    config.base_path = "/spiffs";
    config.partition_label = NULL;
    config.max_files = 5;
    config.format_if_mount_failed = true;

    esp_err_t tRet = esp_vfs_spiffs_register(&config);
    if (tRet != ESP_OK)
    {
        LOGE("esp_vfs_spiffs_register() fail!! ret:%s", esp_err_to_name(tRet));
        return -1;
    }
    else
    {
        FILE* pFile = fopen("/spiffs/config.txt", "w+");
        if(pFile == NULL) 
        {
            LOGE("open file error!!");
            iRet = -1;
            goto End_Func;
        }
        else
        {
            size_t tSize = fwrite(pcTemp, 1, strlen(pcTemp), pFile);
            LOGI("1 tSize:%d write:%s\n", tSize, pcTemp);
            if (tSize != strlen(pcTemp))
            {
                LOGE("fwrite size error!! strlen(pcTemp):%d", strlen(pcTemp));
                iRet = -1;
                goto End_Func;
            }
            else
            {
                fflush(pFile);
                rewind(pFile);
                char cTemp[512] = {0};
                tSize = fread(cTemp, 1, sizeof(cTemp), pFile);
                LOGI("2 tSize:%d read:%s\n", tSize, cTemp);
                if (tSize != strlen(pcTemp))
                {
                    LOGE("fread size error!! tSize:%d", tSize);
                    iRet = -1;
                    //goto End_Func;
                }
                else
                {
                    if (strcmp(cTemp, pcTemp))
                    {
                        LOGE("fread not same with fwrite!!");
                        iRet = -1;
                        goto End_Func;
                    }
                }
            }
        }
        
End_Func:
        if (pFile)
        {
            fclose(pFile);
        }
        esp_vfs_spiffs_unregister(NULL);
    }
    return iRet;
}


static void _xSimpleTestTask(void* pParam)
{
	unsigned char ucState = 0;
    uint32_t uiCnt = 0x11223344;
    char cTmp[64] = {0};
    
	while (1)
	{
		vTaskDelay(1000);
		xGPIOCtrl(BOARD_LED_PIN, ucState);
		//xGPIOCtrl(BOARD_SWITCH_PIN, ucState);
		ucState = !ucState;
        //lvgl_spi_transmit(SPI_SEND, 15, (uint8_t*)&uiCnt, 4);

	}
}

static uint32_t g_uiCnt = 0;

static void lv_tick_task(void* arg) 
{
#if 0
    g_uiCnt++;
    if ((g_uiCnt % 200) == 0)
    {
        LOGI("g_uiCnt:%u", g_uiCnt);
    }
#endif
    (void) arg;
    lv_tick_inc(LV_TICK_PERIOD_MS);
}

static void lv_log_print(const char* pcLog)
{
    ESP_LOGI(TAG, "%s", pcLog);
}

void lvgl_init(void)
{
    lv_log_register_print_cb(lv_log_print);
    
    lv_init();

    /* Initialize SPI or I2C bus used by the drivers */
    lvgl_driver_init();
}

static void event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if(code == LV_EVENT_CLICKED) {
        LOGI("Clicked");
    }
    else if(code == LV_EVENT_VALUE_CHANGED) {
        LOGI("Toggled");
    }
}

static void guiTask(void *pvParameter) 
{
    LOGI("Enter >>");
    (void) pvParameter;
    xGuiSemaphore = xSemaphoreCreateMutex();

    LOGI("DISP_BUF_SIZE:%u", DISP_BUF_SIZE);
#if 1
    //lv_color_t* buf1 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    lv_color_t* buf1 = (lv_color_t*)malloc(DISP_BUF_SIZE * sizeof(lv_color_t));
    assert(buf1 != NULL);

    /* Use double buffered when not working with monochrome displays */
#ifndef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    //lv_color_t* buf2 = heap_caps_malloc(DISP_BUF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    lv_color_t* buf2 = (lv_color_t*)malloc(DISP_BUF_SIZE * sizeof(lv_color_t));

    assert(buf2 != NULL);
#else
    static lv_color_t *buf2 = NULL;
#endif

    static lv_disp_draw_buf_t disp_buf;

    uint32_t size_in_px = DISP_BUF_SIZE;

#if defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_IL3820         \
    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_JD79653A    \
    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_UC8151D     \
    || defined CONFIG_LV_TFT_DISPLAY_CONTROLLER_SSD1306

    /* Actual size in pixels, not bytes. */
    size_in_px *= 8;
#endif

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, size_in_px);

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;
    
    disp_drv.flush_cb = disp_driver_flush;

    /* When using a monochrome display we need to register the callbacks:
     * - rounder_cb
     * - set_px_cb */
#ifdef CONFIG_LV_TFT_DISPLAY_MONOCHROME
    disp_drv.rounder_cb = disp_driver_rounder;
    disp_drv.set_px_cb = disp_driver_set_px;
#endif

    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);

#if 1
#ifdef ESP32
    /* Create and start a periodic timer interrupt to call lv_tick_inc */
    const esp_timer_create_args_t periodic_timer_args = {
        .callback = &lv_tick_task,
        .name = "periodic_gui"
    };
    esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, LV_TICK_PERIOD_MS * 1000));
    
#else   // ESP8266

    LOGI("Initialize hw_timer for callback1");
    if (ESP_OK != hw_timer_init(lv_tick_task, NULL))
    {
        LOGE("hw_timer_init fail!!");
    }
    LOGI("Set hw_timer timing time 1000us with reload");
    if (ESP_OK != hw_timer_alarm_us(LV_TICK_PERIOD_MS * 1000, true))
    {
        LOGE("hw_timer_alarm_us fail!!");
    }    
#endif
#else
    LOGI("Initialize timer for lv_tick_task");
    TimerHandle_t tTimer = xTimerCreate("lv_tick_task", pdMS_TO_TICKS(LV_TICK_PERIOD_MS), pdTRUE, NULL, lv_tick_task);
    if (tTimer)
    {
        LOGI("xTimerCreate timer:%p success.", tTimer);
    }
    else
    {        
        LOGE("xTimerCreate fail!!");
    }
    
    if (tTimer && pdPASS != xTimerStart(tTimer, 0))
    {
        LOGE("xTimerStart timer:%p fail!!", tTimer);
    }
#endif
#endif

#if 0
    /* use a pretty small demo for monochrome displays */
    /* Get the current screen  */
    lv_obj_t * scr = lv_disp_get_scr_act(NULL);
    LOGI("lv_disp_get_scr_act scr:%p", scr);

    if (scr)
    {
        /*Create a Label on the currently active screen*/
        label1 =  lv_label_create(scr);
        LOGI("lv_label_create label1:%p", label1);

        if (label1)
        {
            /*Modify the Label's text*/
            lv_label_set_text(label1, "Hello\nworld");
            LOGI("lv_label_set_text");

            /* Align the Label to the center
             * NULL means align on parent (which is the screen now)
             * 0, 0 at the end means an x, y offset after alignment*/
            lv_obj_align(label1, LV_ALIGN_CENTER, 0, 0);
            LOGI("lv_obj_align");
        }
    }
#else

#endif

    //lv_demo_stress();
   //LOGI("lv_demo_stress");
    //lv_demo_widgets();
    lv_demo_benchmark();

    /*
    lv_obj_t * label;
    lv_obj_t * btn1 = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn1, event_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(btn1, LV_ALIGN_CENTER, 0, -40);

    label = lv_label_create(btn1);
    lv_label_set_text(label, "Button");
    lv_obj_center(label);

    lv_obj_t * btn2 = lv_btn_create(lv_scr_act());
    lv_obj_add_event_cb(btn2, event_handler, LV_EVENT_ALL, NULL);
    lv_obj_align(btn2, LV_ALIGN_CENTER, 0, 40);
    lv_obj_add_flag(btn2, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_height(btn2, LV_SIZE_CONTENT);

    label = lv_label_create(btn2);
    lv_label_set_text(label, "Toggle");
    lv_obj_center(label);
*/
    uint32_t uiCnt = 0;
    while (1)
    {
        /* Delay 1 tick (assumes FreeRTOS tick is 10ms */
        vTaskDelay(pdMS_TO_TICKS(10));      
#if 1
        /* Try to take the semaphore, call lvgl related function on success */
        if (pdTRUE == xSemaphoreTake(xGuiSemaphore, portMAX_DELAY)) 
        {
            if ((++uiCnt % 100) == 0)
            {
                LOGI("uiCnt:%u", uiCnt);
            }
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
#endif
    }
    LOGI("End <<");

    vTaskDelete(NULL);
}

void app_main(void)
{
    LOGI("[APP] Startup..");
    LOGI("[APP] Free memory: %d bytes", esp_get_free_heap_size());
    LOGI("[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
    
    ESP_ERROR_CHECK(nvs_flash_init());     
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
	xGPIOInit(BOARD_SWITCH_PIN);
	xGPIOInit(BOARD_LED_PIN);
	
	xGPIOCtrl(BOARD_SWITCH_PIN, !BOARD_SWITCH_ON_STATE);
    g_ucState = 0;

	g_ullDevID = xGetMacHex();
	strcpy(g_pcDevIDStr, xGetMacStr());

    strcpy(g_cDevName, GET_TYPE_STR(g_eDevType));
    strcat(g_cDevName, "_");
    strncat(g_cDevName, g_pcDevIDStr, 8);

    cJSON* pJObj = NULL;
    
    if (xReadConfigFromFlash(&pJObj) != 0)
    {
        pJObj = cJSON_CreateObject();
        if (pJObj)
        {
            cJSON_AddItemToObject(pJObj, "name", cJSON_CreateString(strlen(g_cDevName) > 0 ? g_cDevName : "none"));
            cJSON_AddItemToObject(pJObj, "id", cJSON_CreateString(g_pcDevIDStr));
            cJSON_AddItemToObject(pJObj, "type", cJSON_CreateString(GET_TYPE_STR(g_eDevType)));
            xWriteConfigToFlash(pJObj);            
        }
    }
    else
    {
        cJSON* pJTemp = cJSON_GetObjectItem(pJObj, "name");
        char* pcTemp = NULL;
        if (pJTemp)
        {
            pcTemp = cJSON_GetStringValue(pJTemp);
            if (pcTemp && strlen(pcTemp))
            {
                strcpy(g_cDevName, pcTemp);
            }
        }
        pJTemp = cJSON_GetObjectItem(pJObj, "id");
        pcTemp = NULL;
        if (pJTemp)
        {
            pcTemp = cJSON_GetStringValue(pJTemp);
            if (pcTemp && strlen(pcTemp))
            {
                strcpy(g_pcDevIDStr, pcTemp);
            }
        }

        pJTemp = cJSON_GetObjectItem(pJObj, "type");
        pcTemp = NULL;
        if (pJTemp)
        {
            pcTemp = cJSON_GetStringValue(pJTemp);
            if (pcTemp && strlen(pcTemp))
            {
                if (strncmp(pcTemp, GET_TYPE_STR(g_eDevType), strlen(GET_TYPE_STR(g_eDevType))))
                {
                    LOGE("Saved device type(%s) wrong!!", pcTemp);
                }
            }
        }
    }

    if (pJObj)
    {
        cJSON_Delete(pJObj);
        pJObj = NULL;
    }
    
	LOGI("name:%s id:%s type:%s", g_cDevName, g_pcDevIDStr, GET_TYPE_STR(g_eDevType));
    
    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
    * Read "Establishing Wi-Fi or Ethernet Connection" section in
    * examples/protocols/README.md for more information about this function.
    */
    //ESP_ERROR_CHECK(example_connect());
    
	struct tm tTime;

	tTime.tm_year = 2021;
    tTime.tm_mon = 4;
    tTime.tm_mday = 18;
    tTime.tm_hour = 21;
    tTime.tm_min = 13;
    tTime.tm_sec = 30;

	xSetTime(tTime);
    
    lvgl_init();

    LOGI("configMINIMAL_STACK_SIZE:%u", configMINIMAL_STACK_SIZE);
    xTaskCreate(_xSimpleTestTask, "_xSimpleTestTask", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY + 2, NULL);
    /* If you want to use a task to create the graphic, you NEED to create a Pinned task
     * Otherwise there can be problem such as memory corruption and so on.
     * NOTE: When not using Wi-Fi nor Bluetooth you can pin the guiTask to core 0 */
    //xTaskCreatePinnedToCore(guiTask, "gui", 4096*2, NULL, 0, NULL, 1);

    BaseType_t iRet = xTaskCreate(guiTask, "gui", 4096*2, NULL, tskIDLE_PRIORITY + 4, NULL);
    if (iRet != pdPASS)
    {
        LOGE("xTaskCreate gui fail!! iRet:%d", iRet);
    }

	//xTaskCreate(_xSimpleTestTask, "_xSimpleTestTask", configMINIMAL_STACK_SIZE * 2, NULL, tskIDLE_PRIORITY + 2, NULL);
}
