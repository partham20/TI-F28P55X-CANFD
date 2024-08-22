//
// Include Files
//
#include "driverlib.h"
#include "device.h"
#include "inc/stw_types.h"
#include "inc/stw_dataTypes.h"
#include <string.h>

typedef uint8_t i8;

//
// Defines.
//
#define MCAN_TX_BUFF_SIZE               (1U) // Single message buffer
#define MCAN_TX_BUFF_ELEM_SIZE          (MCAN_ELEM_SIZE_64BYTES)

//
// Defining Starting Addresses for Message RAM Sections.
//
#define MCAN_TX_BUFF_START_ADDR         (0x0U)

//
// Global Variables.
//
int32_t error = 0;

//
// Function Prototypes.
//
static void MCANConfig(void);
int sendCANMessage(uint32_t msgId, uint8_t* data, uint32_t dataLength);

//
// Main
//
int main(void)
{
    i8 data[64] = {0x1, 0x2, 0x3, 0x4}; // Sample data

    // Send a CAN message with standard ID 0x4
    if (sendCANMessage(0x4, data, 4) == 0)
    {
        // Success
    }
    else
    {
        // Error handling
    }

    // Other code...

    return 0;
}

//
// Wrapper function to configure MCAN and send a CAN message
//
int sendCANMessage(uint32_t msgId, uint8_t* data, uint32_t dataLength)
{
    MCAN_TxBufElement txMsg;
    int i;

    if (dataLength > 64)
    {
        // The maximum data length for CAN FD is 64 bytes
        return -1; // Return error if dataLength exceeds 64 bytes
    }

    // Configure the MCAN Module.
    MCANConfig();

    // Initialize message to transmit
    txMsg.id = (msgId << 18U); // Convert 11-bit standard ID to internal format
    txMsg.rtr = 0U;  // Data frame
    txMsg.xtd = 0U;  // Standard identifier (11-bit)
    txMsg.esi = 0U;
    txMsg.dlc = dataLength; // Calculate the appropriate DLC value
    txMsg.brs = 1U;  // Bit-rate switching enabled
    txMsg.fdf = 1U;  // Frame transmitted in CAN FD format
    txMsg.efc = 1U;  // Store TX events
    txMsg.mm = 0xAAU;

    for (i = 0; i < dataLength; i++)
    {
        txMsg.data[i] = data[i];
    }



    // Write message to Message RAM.
    MCAN_writeMsgRam(MCANA_DRIVER_BASE, MCAN_MEM_TYPE_BUF, 0U, &txMsg);

    // Add transmission request for Tx buffer 0
    MCAN_txBufAddReq(MCANA_DRIVER_BASE, 0U);

    // Wait until the frame is successfully transmitted
    while (MCAN_getTxBufReqPend(MCANA_DRIVER_BASE))
    {
    }

    return 0; // Return success
}

//
// MCAN Configuration Function
//
static void MCANConfig(void)
{


    // Initialize the device and MCAN module if needed
    // You can also assume initialization is done elsewhere and remove this
    Device_init();
    Device_initGPIO();
    SysCtl_setMCANClk(SYSCTL_MCANA, SYSCTL_MCANCLK_DIV_5);
    Interrupt_initModule();
    Interrupt_initVectorTable();
    EINT;
    ERTM;

    GPIO_setPinConfig(DEVICE_GPIO_CFG_MCANRXA);
    GPIO_setPinConfig(DEVICE_GPIO_CFG_MCANTXA);


    MCAN_InitParams initParams;
    MCAN_MsgRAMConfigParams    msgRAMConfigParams;
    MCAN_BitTimingParams       bitTimes;

    //
    //  Initializing all structs to zero to prevent stray values
    //
    memset(&initParams, 0, sizeof(initParams));
    memset(&msgRAMConfigParams, 0, sizeof(msgRAMConfigParams));
    memset(&bitTimes, 0, sizeof(bitTimes));

    //
    // Initialize MCAN Init parameters.
    //
    initParams.fdMode            = 0x1U; // FD operation enabled.
    initParams.brsEnable         = 0x1U; // Bit rate switching for
                                         // transmissions enabled.

    //
    // Transmitter Delay Compensation parameters.
    //
    initParams.tdcConfig.tdcf    = 0xAU;
    initParams.tdcConfig.tdco    = 0x6U;

    //
    // Initialize Message RAM Sections Configuration Parameters
    //
    msgRAMConfigParams.txStartAddr          = MCAN_TX_BUFF_START_ADDR;
    // Tx Buffers Start Address.

    msgRAMConfigParams.txBufNum             = MCAN_TX_BUFF_SIZE;
    // Number of Dedicated Transmit Buffers.

    msgRAMConfigParams.txBufMode            = 0U;

    msgRAMConfigParams.txFIFOSize           = 0U;
    // Number of Tx FIFO or Tx Queue Elements

    msgRAMConfigParams.txBufElemSize        = MCAN_TX_BUFF_ELEM_SIZE;
    // Tx Buffer Element Size.

    //
    // Initialize bit timings.
    //
    bitTimes.nomRatePrescalar   = 0xBU; // Nominal Baud Rate Pre-scaler
    bitTimes.nomTimeSeg1        = 0x2U; // Nominal Time segment before SP
    bitTimes.nomTimeSeg2        = 0x0U; // Nominal Time segment after SP
    bitTimes.nomSynchJumpWidth  = 0x0U; // Nominal SJW

    bitTimes.dataRatePrescalar  = 0x2U; // Data Baud Rate Pre-scaler
    bitTimes.dataTimeSeg1       = 0x2U; // Data Time segment before SP
    bitTimes.dataTimeSeg2       = 0x0U; // Data Time segment after SP
    bitTimes.dataSynchJumpWidth = 0x0U; // Data SJW
    //
    // Wait for memory initialization to happen.
    //
    while(FALSE == MCAN_isMemInitDone(MCANA_DRIVER_BASE))
    {
    }

    //
    // Put MCAN in SW initialization mode.
    //
    MCAN_setOpMode(MCANA_DRIVER_BASE, MCAN_OPERATION_MODE_SW_INIT);

    //
    // Wait till MCAN is not initialized.
    //
    while (MCAN_OPERATION_MODE_SW_INIT != MCAN_getOpMode(MCANA_DRIVER_BASE))
    {}

    //
    // Initialize MCAN module.
    //
    MCAN_init(MCANA_DRIVER_BASE, &initParams);

    //
    // Configure Bit timings.
    //
    MCAN_setBitTime(MCANA_DRIVER_BASE, &bitTimes);

    //
    // Configure Message RAM Sections
    //
    MCAN_msgRAMConfig(MCANA_DRIVER_BASE, &msgRAMConfigParams);

    //
    // Take MCAN out of the SW initialization mode
    //
    MCAN_setOpMode(MCANA_DRIVER_BASE, MCAN_OPERATION_MODE_NORMAL);

    while (MCAN_OPERATION_MODE_NORMAL != MCAN_getOpMode(MCANA_DRIVER_BASE))
    {

    }
}
