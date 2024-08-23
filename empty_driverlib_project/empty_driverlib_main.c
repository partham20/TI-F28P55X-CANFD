#include "driverlib.h"
#include "device.h"
#include "inc/stw_types.h"
#include "inc/stw_dataTypes.h"
#include <string.h>

static void MCANConfig(void);
static void MCANIntrConfig(void);

typedef uint8_t i8;
//
// Defines.
//
#define NUM_OF_MSG                      (1U)
#define MCAN_STD_ID_FILTER_NUM          (1U)
#define MCAN_EXT_ID_FILTER_NUM          (0U)
#define MCAN_FIFO_0_NUM                 (0U)
#define MCAN_FIFO_0_ELEM_SIZE           (MCAN_ELEM_SIZE_64BYTES)
#define MCAN_FIFO_1_NUM                 (0U)
#define MCAN_FIFO_1_ELEM_SIZE           (MCAN_ELEM_SIZE_64BYTES)
#define MCAN_RX_BUFF_NUM                (10U)
#define MCAN_RX_BUFF_ELEM_SIZE          (MCAN_ELEM_SIZE_64BYTES)
#define MCAN_TX_BUFF_SIZE               (1U)
#define MCAN_TX_FQ_SIZE                 (0U)
#define MCAN_TX_BUFF_ELEM_SIZE          (MCAN_ELEM_SIZE_64BYTES)
#define MCAN_TX_EVENT_SIZE              (0U)

//
//  Defining Starting Addresses for Message RAM Sections,
//  (Calculated from Macros based on User defined configuration above)
//
#define MCAN_STD_ID_FILT_START_ADDR     (0x0U)
#define MCAN_EXT_ID_FILT_START_ADDR     (MCAN_STD_ID_FILT_START_ADDR + ((MCAN_STD_ID_FILTER_NUM * MCANSS_STD_ID_FILTER_SIZE_WORDS * 4U)))
#define MCAN_FIFO_0_START_ADDR          (MCAN_EXT_ID_FILT_START_ADDR + ((MCAN_EXT_ID_FILTER_NUM * MCANSS_EXT_ID_FILTER_SIZE_WORDS * 4U)))
#define MCAN_FIFO_1_START_ADDR          (MCAN_FIFO_0_START_ADDR + (MCAN_getMsgObjSize(MCAN_FIFO_0_ELEM_SIZE) * 4U * MCAN_FIFO_0_NUM))
#define MCAN_RX_BUFF_START_ADDR         (MCAN_FIFO_1_START_ADDR + (MCAN_getMsgObjSize(MCAN_FIFO_1_ELEM_SIZE) * 4U * MCAN_FIFO_1_NUM))
#define MCAN_TX_BUFF_START_ADDR         (MCAN_RX_BUFF_START_ADDR + (MCAN_getMsgObjSize(MCAN_RX_BUFF_ELEM_SIZE) * 4U * MCAN_RX_BUFF_NUM))
#define MCAN_TX_EVENT_START_ADDR        (MCAN_TX_BUFF_START_ADDR + (MCAN_getMsgObjSize(MCAN_TX_BUFF_ELEM_SIZE) * 4U * (MCAN_TX_BUFF_SIZE + MCAN_TX_FQ_SIZE)))



volatile uint32_t ledState = 0; // LED
volatile uint32_t txledState = 0; // LED
int32_t loopCnt = 0U;
int32_t     error = 0;
MCAN_RxBufElement rxMsg[NUM_OF_MSG], rxMsg1;

//Function prototype
__interrupt void MCANIntr1ISR(void);
void rx(MCAN_RxBufElement* rxMessage, uint32_t* receivedMsgId, uint32_t* receivedDlc, int32_t* error);
int sendCANMessage(uint32_t msgId, uint8_t* data, uint32_t dataLength);

//Rx Wrapper Function
void rx(MCAN_RxBufElement* rxMessage, uint32_t* receivedMsgId, uint32_t* receivedDlc, int32_t* error)
{


    // Configure the MCAN Module
    MCANConfig();

    // Enable Interrupts and configure them
    MCANIntrConfig();

    // Enable MCAN module interrupts
    MCAN_enableIntr(MCANA_DRIVER_BASE, MCAN_INTR_MASK_ALL, 1U);

    // Select Interrupt Line
    MCAN_selectIntrLine(MCANA_DRIVER_BASE, MCAN_INTR_MASK_ALL, MCAN_INTR_LINE_NUM_1);

    // Enable Interrupt Line
    MCAN_enableIntrLine(MCANA_DRIVER_BASE, MCAN_INTR_LINE_NUM_1, 1U);

    // Initialize variables
    *receivedMsgId = 0;
    *receivedDlc = 0;
    *error = 0;

    while (1)
    {
        // Adding delay of 1 second
        DEVICE_DELAY_US(1000000);

        uint32_t intrStatus = MCAN_getIntrStatus(MCANA_DRIVER_BASE);

        // Clear the interrupt Status
        MCAN_clearIntrStatus(MCANA_DRIVER_BASE, intrStatus);

        // Check to see if the interrupt is caused by a message being received in dedicated RX Buffers
        if ((MCAN_INTR_SRC_DEDICATED_RX_BUFF_MSG & intrStatus) == MCAN_INTR_SRC_DEDICATED_RX_BUFF_MSG)
        {
            MCAN_RxNewDataStatus newData;
            MCAN_getNewDataStatus(MCANA_DRIVER_BASE, &newData);

            // If message is received in buffer element 0
            if ((newData.statusLow & (1UL << 0U)) != 0)
            {
                MCAN_readMsgRam(MCANA_DRIVER_BASE, MCAN_MEM_TYPE_BUF, 0U, 0, rxMessage);

                // Store received message ID and DLC
                *receivedMsgId = rxMessage->id;
                *receivedDlc = rxMessage->dlc;

                // Toggle LED2
                ledState = !ledState;
                GPIO_writePin(DEVICE_GPIO_PIN_LED2, ledState);
            }

            // Clear the NewData registers
            MCAN_clearNewDataStatus(MCANA_DRIVER_BASE, &newData);
        }
        else
        {
            (*error)++;
        }
    }
}

// Main function
void main()
{  MCAN_RxBufElement rxMessage;
    uint32_t receivedMsgId;
    uint32_t receivedDlc;
    int32_t error;

    i8 data[64] = {0x1, 0x2, 0x3, 0x4}; // Sample data

    while(1)
    {
       // Send a CAN message with standard ID 0x4
    sendCANMessage(0x4, data, 4);
    rx(&rxMessage, &receivedMsgId, &receivedDlc, &error);
    }
    // Stop the application
    asm("   ESTOP0");
}

static void MCANConfig(void)
{
    // Initialize device clock and peripherals
      Device_init();

      // Initialize GPIO and unlock the GPIO configuration registers
      Device_initGPIO();
      GPIO_setPadConfig(DEVICE_GPIO_PIN_LED2, GPIO_PIN_TYPE_STD);
      GPIO_setDirectionMode(DEVICE_GPIO_PIN_LED2, GPIO_DIR_MODE_OUT);
      GPIO_setPadConfig(DEVICE_GPIO_PIN_LED1, GPIO_PIN_TYPE_STD);
      GPIO_setDirectionMode(DEVICE_GPIO_PIN_LED1, GPIO_DIR_MODE_OUT);

      // Configure the divisor for the MCAN bit-clock
      SysCtl_setMCANClk(SYSCTL_MCANA, SYSCTL_MCANCLK_DIV_5);

      // Initialize the interrupt module and vector table
      Interrupt_initModule();
      Interrupt_initVectorTable();

      // Enable Global Interrupt (INTM) and realtime interrupt (DBGM)
      EINT;
      ERTM;

      // Configure GPIO pins for MCANTX/MCANRX operation
      GPIO_setPinConfig(DEVICE_GPIO_CFG_MCANRXA);
      GPIO_setPinConfig(DEVICE_GPIO_CFG_MCANTXA);




    MCAN_InitParams initParams;
    MCAN_MsgRAMConfigParams    msgRAMConfigParams;
    MCAN_StdMsgIDFilterElement stdFiltelem;
    MCAN_BitTimingParams       bitTimes;

    //
    //  Initializing all structs to zero to prevent stray values
    //
    memset(&initParams, 0, sizeof(initParams));
    memset(&msgRAMConfigParams, 0, sizeof(msgRAMConfigParams));
    memset(&stdFiltelem, 0, sizeof(stdFiltelem));
    memset(&bitTimes, 0, sizeof(bitTimes));

    //
    // Configure MCAN initialization parameters
    //
    initParams.fdMode            = 0x1U; // FD operation enabled.
    initParams.brsEnable         = 0x1U; // Bit rate switching enabled

    //
    // Initialize Message RAM Sections Configuration Parameters
    //
    msgRAMConfigParams.flssa                = MCAN_STD_ID_FILT_START_ADDR;
    // Standard ID Filter List Start Address.
    msgRAMConfigParams.lss                  = MCAN_STD_ID_FILTER_NUM;
    // List Size: Standard ID.
    msgRAMConfigParams.rxBufStartAddr       = MCAN_RX_BUFF_START_ADDR;
    // Rx Buffer Start Address.
    msgRAMConfigParams.rxBufElemSize        = MCAN_RX_BUFF_ELEM_SIZE;
    // Rx Buffer Element Size.

    //
    // Initialize Rx Buffer Configuration parameters.
    //
    stdFiltelem.sfid2              = 0x0U; // Standard Filter ID 2.
    // Configuring received frame to be stored in buffer element 0
    stdFiltelem.sfid1              = 0x4U; // Standard Filter ID 1.
    // Confifuring frames with msg ID = 0x4U to be accepted by filter element
    stdFiltelem.sfec               = 0x7U; // Store into Rx Buffer
                                           // configuration of SFT[1:0] ignored


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
    // Configure Standard ID filter element
    //
    MCAN_addStdMsgIDFilter(MCANA_DRIVER_BASE, 0U, &stdFiltelem);

    //
    // Take MCAN out of the SW initialization mode
    //
    MCAN_setOpMode(MCANA_DRIVER_BASE, MCAN_OPERATION_MODE_NORMAL);

    while (MCAN_OPERATION_MODE_NORMAL != MCAN_getOpMode(MCANA_DRIVER_BASE))
    {

    }
}

//
// This function will configure X-BAR for MCAN interrupts.
//
static void MCANIntrConfig(void)
{

    Interrupt_initModule();
    Interrupt_initVectorTable();

    Interrupt_register(INT_MCANA_1,&MCANIntr1ISR);
    Interrupt_enable(INT_MCANA_1);

    Interrupt_enableGlobal();

}

//
// This is Interrupt Service Routine for MCAN interrupt 1.
//
__interrupt void MCANIntr1ISR(void)
{
    uint32_t intrStatus;
    MCAN_RxNewDataStatus newData;

    intrStatus = MCAN_getIntrStatus(MCANA_DRIVER_BASE);

    //
    // Clear the interrupt Status.
    //
    MCAN_clearIntrStatus(MCANA_DRIVER_BASE, intrStatus);

    //
    //  Clearing the interrupt lineNum
    //
    MCAN_clearInterrupt(MCANA_DRIVER_BASE, 0x2);

    //
    //  Check to see if the interrupt is caused by a message being
    //  received in dedicated RX Buffers
    //
    if((MCAN_INTR_SRC_DEDICATED_RX_BUFF_MSG & intrStatus) == MCAN_INTR_SRC_DEDICATED_RX_BUFF_MSG)
    {
        //
        // Read the NewData registers
        //

        MCAN_getNewDataStatus(MCANA_DRIVER_BASE, &newData);

        //  If message is received in buffer element 0
        if((newData.statusLow & (1UL << 0U)) != 0)
        {
            MCAN_readMsgRam(MCANA_DRIVER_BASE, MCAN_MEM_TYPE_BUF, 0U,
                          0, &rxMsg1);

            rxMsg[loopCnt] = rxMsg1;
            // Toggle LED2
                  ledState = !ledState;
                  GPIO_writePin(DEVICE_GPIO_PIN_LED2, ledState);
        }

        //
        //  Clearing the NewData registers
        //
        MCAN_clearNewDataStatus(MCANA_DRIVER_BASE, &newData);


    }
    else
    {
        error++;

        //
        //  Interrupt handling for other interrupt sources goes here
        //

    }

    //
    // Acknowledge this interrupt located in group 9
    //
    Interrupt_clearACKGroup(INTERRUPT_ACK_GROUP9);
}

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
    txledState = !txledState;
    GPIO_writePin(DEVICE_GPIO_PIN_LED1, txledState);
    return 0; // Return success
}

