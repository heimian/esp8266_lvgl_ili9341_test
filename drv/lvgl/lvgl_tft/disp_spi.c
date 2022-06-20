/**
 * @file disp_spi.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include "esp8266/spi_struct.h"
#include "esp8266/gpio_struct.h"
#include "esp_system.h"
#include "esp_log.h"
    
#include "driver/gpio.h"
#include "driver/spi.h"

#define TAG "disp_spi"

#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "lvgl.h"

#include "disp_spi.h"
#include "disp_driver.h"

#include "../lvgl_helpers.h"
#include "../lvgl_spi_conf.h"

/******************************************************************************
 * Notes about DMA spi_transaction_ext_t structure pooling
 * 
 * An xQueue is used to hold a pool of reusable SPI spi_transaction_ext_t 
 * structures that get used for all DMA SPI transactions. While an xQueue may 
 * seem like overkill it is an already built-in RTOS feature that comes at 
 * little cost. xQueues are also ISR safe if it ever becomes necessary to 
 * access the pool in the ISR callback.
 * 
 * When a DMA request is sent, a transaction structure is removed from the 
 * pool, filled out, and passed off to the esp32 SPI driver. Later, when 
 * servicing pending SPI transaction results, the transaction structure is 
 * recycled back into the pool for later reuse. This matches the DMA SPI 
 * transaction life cycle requirements of the esp32 SPI driver.
 * 
 * When polling or synchronously sending SPI requests, and as required by the 
 * esp32 SPI driver, all pending DMA transactions are first serviced. Then the 
 * polling SPI request takes place. 
 * 
 * When sending an asynchronous DMA SPI request, if the pool is empty, some 
 * small percentage of pending transactions are first serviced before sending 
 * any new DMA SPI transactions. Not too many and not too few as this balance 
 * controls DMA transaction latency.
 * 
 * It is therefore not the design that all pending transactions must be 
 * serviced and placed back into the pool with DMA SPI requests - that 
 * will happen eventually. The pool just needs to contain enough to float some 
 * number of in-flight SPI requests to speed up the overall DMA SPI data rate 
 * and reduce transaction latency. If however a display driver uses some 
 * polling SPI requests or calls disp_wait_for_pending_transactions() directly,
 * the pool will reach the full state more often and speed up DMA queuing.
 * 
 *****************************************************************************/

/*********************
 *      DEFINES
 *********************/
#define SPI_TRANSACTION_POOL_SIZE                   50	/* maximum number of DMA transactions simultaneously in-flight */

/* DMA Transactions to reserve before queueing additional DMA transactions. A 1/10th seems to be a good balance. Too many (or all) and it will increase latency. */
#define SPI_TRANSACTION_POOL_RESERVE_PERCENTAGE     10
#if SPI_TRANSACTION_POOL_SIZE >= SPI_TRANSACTION_POOL_RESERVE_PERCENTAGE
#define SPI_TRANSACTION_POOL_RESERVE                (SPI_TRANSACTION_POOL_SIZE / SPI_TRANSACTION_POOL_RESERVE_PERCENTAGE)	
#else
#define SPI_TRANSACTION_POOL_RESERVE                1	/* defines minimum size */
#endif

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

 void disp_spi_transaction(const uint8_t *data, size_t length, disp_spi_send_flag_t flags, 
                                            uint8_t *out, uint64_t addr, uint8_t dummy_bits)
{    
    lvgl_spi_transmit(SPI_SEND, data, length);
}

void disp_wait_for_pending_transactions(void)
{
    return;
}

void disp_spi_acquire(void)
{
    return;
}
void disp_spi_release(void)
{
    return;
}


/**********************
 *   STATIC FUNCTIONS
 **********************/

