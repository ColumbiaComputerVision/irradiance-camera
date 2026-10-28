/*
 * Copyright (c) 2017-2018, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== rfEasyLinkTx.c ========
 */
 /* Standard C Libraries */
#include <stdlib.h>

/* XDCtools Header files */
#include <xdc/std.h>
#include <xdc/runtime/Assert.h>
#include <xdc/runtime/Error.h>
#include <xdc/runtime/System.h>

/* BIOS Header files */
#include <ti/sysbios/BIOS.h>
#include <ti/sysbios/knl/Task.h>
#include <ti/sysbios/knl/Semaphore.h>
#include <ti/sysbios/knl/Clock.h>

/* TI-RTOS Header files */
#include <ti/drivers/pin/PINCC26XX.h>
#include <ti/drivers/SPI.h>
#include <ti/drivers/Power.h>
#include <ti/drivers/power/PowerCC26XX.h>

/* Board Header files */
#include "Board.h"

/* EasyLink API Header files */
#include "easylink/EasyLink.h"

/* Application header files */
#include "smartrf_settings/smartrf_settings.h"



/* Undefine to not use async mode */
#define RFEASYLINKTX_ASYNC

#define RFEASYLINKTX_TASK_STACK_SIZE    1024
#define RFEASYLINKTX_TASK_PRIORITY      2

Task_Struct txTask;    /* not static so you can see in ROV */
static Task_Params txTaskParams;
static uint8_t txTaskStack[RFEASYLINKTX_TASK_STACK_SIZE];


// #define DEVKIT
// #define USE_LEDS

#ifdef USE_LEDS
/* Pin driver handle */
static PIN_Handle ledPinHandle;
static PIN_State ledPinState;
/*
 * Application LED pin configuration table:
 *   - All LEDs board LEDs are off.
 */
PIN_Config ledPinTable[] = {
    Board_PIN_LED1 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    Board_PIN_LED2 | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};
#endif


/* Wakeup Pin */
#define WAKEUP_PIN Board_DIO1
static PIN_Handle wakeupPinHandle;
static PIN_State wakeupPinState;
PIN_Config wakeupPinTable[] = {
    WAKEUP_PIN | PIN_INPUT_EN | PIN_NOPULL | PIN_IRQ_POSEDGE,
    PIN_TERMINATE
};

#define SPI_READY_PIN IOID_3 // Was used for uart
static PIN_Handle spiReadyPinHandle;
static PIN_State spiReadyPinState;
PIN_Config spiReadyPinTable[] = {
    SPI_READY_PIN | PIN_GPIO_OUTPUT_EN | PIN_GPIO_LOW | PIN_PUSHPULL | PIN_DRVSTR_MAX,
    PIN_TERMINATE
};

#ifdef RFEASYLINKTX_ASYNC
static Semaphore_Handle txDoneSem;
#endif //RFEASYLINKTX_ASYNC

#define SPI_PACKET_SIZE      98
#define RF_MAX_PAYLOAD       98

static SPI_Params spiParams;
static SPI_Handle spi;
static uint8_t spiRxBuf[SPI_PACKET_SIZE];
static SPI_Transaction transaction;
static uint16_t seq_number = 0;

static Semaphore_Handle wakeSem;
static Semaphore_Handle spiDoneSem;

void transmit_data(uint8_t *payload, size_t payload_len);


#ifdef RFEASYLINKTX_ASYNC
void txDoneCb(EasyLink_Status status) {
    #ifdef USE_LEDS
    if (status == EasyLink_Status_Success) {
        /* Toggle LED1 to indicate TX */
        PIN_setOutputValue(ledPinHandle, Board_PIN_LED1,!PIN_getOutputValue(Board_PIN_LED1));
    } else if (status == EasyLink_Status_Aborted) {
        /* Toggle LED2 to indicate command aborted */
        PIN_setOutputValue(ledPinHandle, Board_PIN_LED2,!PIN_getOutputValue(Board_PIN_LED2));
    } else {
        /* Toggle LED1 and LED2 to indicate error */
        PIN_setOutputValue(ledPinHandle, Board_PIN_LED1,!PIN_getOutputValue(Board_PIN_LED1));
        PIN_setOutputValue(ledPinHandle, Board_PIN_LED2,!PIN_getOutputValue(Board_PIN_LED2));
    }
    #endif

    Semaphore_post(txDoneSem);
}
#endif //RFEASYLINKTX_ASYNC


void wakeupCb(PIN_Handle handle, PIN_Id pinId) {
    Semaphore_post(wakeSem);
}

void spiCallback(SPI_Handle handle, SPI_Transaction *transaction) {
    // Reset ready signal
    Semaphore_post(spiDoneSem);
}

void init_spi(void) {
    SPI_init();
    SPI_Params_init(&spiParams);

    spiParams.mode = SPI_SLAVE;
    spiParams.frameFormat = SPI_POL1_PHA1;   // Must match STM32
    spiParams.transferMode = SPI_MODE_CALLBACK;
    spiParams.transferCallbackFxn = spiCallback;  // Added
    spiParams.dataSize = 8; // 8 bit packets

    spi = SPI_open(Board_SPI0, &spiParams);
    if (spi == NULL) {
        while (1); // error
    }

    transaction.count = SPI_PACKET_SIZE;
    transaction.txBuf = NULL;        // we only receive
    transaction.rxBuf = spiRxBuf;

    Semaphore_Params semParams;
    Error_Block eb;
    
    Semaphore_Params_init(&semParams);
    Error_init(&eb);

    spiDoneSem = Semaphore_create(0, &semParams, &eb);
    if(spiDoneSem == NULL) {
        System_abort("Semaphore creation failed");
    }

}

void transmit_data(uint8_t *payload, size_t payload_len) {
    EasyLink_TxPacket txPacket =  { {0}, 0, 0, {0} };

    txPacket.payload[0] = seq_number & 0xFF;
    txPacket.payload[1] = (seq_number >> 8) & 0xFF;

    size_t i;
    for (i = 0; i < payload_len; i++) {
        txPacket.payload[i + 2] = payload[i];
    }

    txPacket.len = payload_len + 2;

    /*
    * Address filtering is enabled by default on the Rx device with the
    * an address of 0xAA. This device must set the dstAddr accordingly.
    */
    txPacket.dstAddr[0] = 0xaa;

    txPacket.absTime = 0; // Send packet immediately

    EasyLink_transmitAsync(&txPacket, txDoneCb);

    seq_number++;
}


void init_rf() {
    /* Create a semaphore for Async */
    Semaphore_Params params;
    Error_Block eb;

    /* Init params */
    Semaphore_Params_init(&params);
    Error_init(&eb);

    /* Create semaphore instance */
    txDoneSem = Semaphore_create(0, &params, &eb);
    if(txDoneSem == NULL)
    {
        System_abort("Semaphore creation failed");
    }

    // Initialize the EasyLink parameters to their default values
    EasyLink_Params easyLink_params;
    EasyLink_Params_init(&easyLink_params);

    /*
     * Initialize EasyLink with the settings found in easylink_config.h
     * Modify EASYLINK_PARAM_CONFIG in easylink_config.h to change the default
     * PHY
     */
    if(EasyLink_init(&easyLink_params) != EasyLink_Status_Success)
    {
        System_abort("EasyLink_init failed");
    }

    /*
     * If you wish to use a frequency other than the default, use
     * the following API:
     * EasyLink_setFrequency(868000000);
     */

    EasyLink_setCtrl(EasyLink_Ctrl_Idle_TimeOut, 0);
}


void init_pins() {
#ifdef USE_LEDS
    /* Open LED pins */
    ledPinHandle = PIN_open(&ledPinState, ledPinTable);
    Assert_isTrue(ledPinHandle != NULL, NULL);

    /* Clear LED pins */
    PIN_setOutputValue(ledPinHandle, Board_PIN_LED1, 0);
    PIN_setOutputValue(ledPinHandle, Board_PIN_LED2, 0);
#endif

    spiReadyPinHandle = PIN_open(&spiReadyPinState, spiReadyPinTable);
    if (spiReadyPinHandle == NULL) {
        System_abort("SPI ready pin open failed");
    }
    PIN_setOutputValue(spiReadyPinHandle, SPI_READY_PIN, 0);
}

void init_wakeup() {
    Semaphore_Params semParams;
    Error_Block eb;

    Semaphore_Params_init(&semParams);
    Error_init(&eb);

    wakeSem = Semaphore_create(0, &semParams, &eb);
    if(wakeSem == NULL) {
        System_abort("Semaphore creation failed");
    }

    wakeupPinHandle = PIN_open(&wakeupPinState, wakeupPinTable);
    PIN_registerIntCb(wakeupPinHandle, wakeupCb);
}


static void rfEasyLinkTxFnx(UArg arg0, UArg arg1) {
    init_pins();
    init_wakeup();    
    init_spi();
    init_rf();
        
    while(1) {
        Semaphore_pend(wakeSem, BIOS_WAIT_FOREVER);
        // Disable further wake interrupts
        PIN_setConfig(wakeupPinHandle, PIN_BM_IRQ, WAKEUP_PIN | PIN_IRQ_DIS);

        // Queue SPI transaction
        SPI_transfer(spi, &transaction);

        // Ready for SPI
        PIN_setOutputValue(spiReadyPinHandle, SPI_READY_PIN, 1);
        
        // Wait for SPI transaction
        Semaphore_pend(spiDoneSem, BIOS_WAIT_FOREVER);

        PIN_setOutputValue(spiReadyPinHandle, SPI_READY_PIN, 0);

        if (transaction.status == SPI_STATUS_SUCCESS) {
            // #ifdef USE_LEDS
            // PIN_setOutputValue(ledPinHandle, Board_PIN_LED1,!PIN_getOutputValue(Board_PIN_LED1));
            // #endif

            // Queue tx
            transmit_data(spiRxBuf, SPI_PACKET_SIZE);

            // Wait for tx to complete
            Semaphore_pend(txDoneSem, BIOS_WAIT_FOREVER);
        }

        PIN_setConfig(wakeupPinHandle, PIN_BM_IRQ, WAKEUP_PIN | PIN_IRQ_POSEDGE);
    }
}

void txTask_init() {
    Task_Params_init(&txTaskParams);
    txTaskParams.stackSize = RFEASYLINKTX_TASK_STACK_SIZE;
    txTaskParams.priority = RFEASYLINKTX_TASK_PRIORITY;
    txTaskParams.stack = &txTaskStack;
    txTaskParams.arg0 = (UInt) 1000000;

    Task_construct(&txTask, rfEasyLinkTxFnx, &txTaskParams, NULL);
}

/*
 *  ======== main ========
 */
int main(void)
{
    /* Call driver init functions. */

    Board_initGeneral(); // Power_init() and flash disable called in here

    txTask_init();

    /* Start BIOS */
    BIOS_start();

    return (0);
}
