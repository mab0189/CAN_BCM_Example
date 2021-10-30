/*******************************************************************************
 \project   INFM_HIL_Interface
 \file      CANFD_BCM_Example.c
 \brief     This dummy application is an example of a CAN/CANFD BCM socket.
 \author    Matthias Bank
 \version   1.0.0
 \date      28.10.2021
 ******************************************************************************/


/*******************************************************************************
 * INCLUDES
 ******************************************************************************/
#include "CANFD_BCM_Error.h"
#include "CANFD_BCM_Config.h"
#include "CANFD_BCM_Socket.h"
#include <linux/can.h>
#include <linux/can/bcm.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>


/*******************************************************************************
 * DEFINES
 ******************************************************************************/

/**
 * Defines how many frames can be put in a bcmMsgMultipleFrames operation.
 * The socketCAN BCM can send up to 256 CAN frames in a sequence in the case
 * of a cyclic TX task configuration (=> check socketCAN documentation).
 */
#define MAXFRAMES 256


/*******************************************************************************
 * STRUCTS
 ******************************************************************************/

/**
 * Struct for a BCM message with a single frame.
 */
struct bcmMsgSingleFrame{
    struct bcm_msg_head msg_head;
    struct can_frame canFrame[1];
};

/**
 * Struct for a BCM message with multiple frames.
 */
 struct bcmMsgMultipleFrames{
     struct bcm_msg_head msg_head;
     struct can_frame canFrames[MAXFRAMES];
 };


/*******************************************************************************
 * VARIABLES
 ******************************************************************************/
volatile int keepRunning = 1; // Keep running till CTRL + F is pressed


/*******************************************************************************
 * FUNCTION DEFINITIONS
 ******************************************************************************/

/**
 * Process termination signal
 *
 * @param signumber Signal number which occurred
 */
static void handleTerminationSignal(int signumber){

    // Stop the application
    keepRunning = 0;
}

/**
 * Handles the shutdown procedure
 *
 * @param retCode  - The return code
 * @param socketFD - The socket file descriptor
 */
int shutdownHandler(int retCode, int const *const socketFD){

    // Close the socket
    if(*socketFD != -1){
        close(*socketFD);
    }

    exit(retCode);
}

/**
 * Create a non cyclic transmission task for CAN/CANFD frames
 *
 * @param socketFD - The socket file descriptor
 * @param frames   - The array of CAN/CANFD frames that should be send
 * @param nframes  - The number of CAN/CANFD frames that should be send
 * @param isCANFD  - Flag for CANFD frames
 */
void create_TX_SEND(int const *const socketFD, void *const frames, int nframes, int isCANFD){

    // BCM message with a single frame
    struct bcmMsgSingleFrame msg;

    // Note: Always initialize the whole struct with 0.
    // Random values in the memory can cause weird bugs!
    memset(&msg, 0, sizeof(msg));

    msg.msg_head.opcode          = TX_SEND;
    msg.msg_head.nframes         = 1;

    // Check if it is a CANFD frame
    if(isCANFD) {
        msg.msg_head.flags = CAN_FD_FRAME;
    }

    // Note: TX_SEND can only send one frame at a time unlike TX_SETUP!
    // This is the reason why we must use a loop instead of a struct that
    // can contain multiple frames.
    for(int index = 0; index < nframes; index++){

        // Because of the bcm_msg_head we always need to cast to a can_frame
        msg.canFrame[0] = ((struct can_frame*) frames)[index];

        // Send the TX_SEND configuration message.
        if(send(*socketFD, &msg, sizeof(msg), 0) < 0){
            printf("Error could not write TX_SEND message \n");
            shutdownHandler(ERR_TX_SEND_FAILED, socketFD);
        }

    }

}

/**
 * Create a cyclic transmission task for CAN/CANFD frames
 *
 * @param socketFD - The socket file descriptor
 * @param frames   - The array of CAN/CANFD frames that should be send cyclic
 * @param nframes  - The number of CAN/CANFD frames that should be send cyclic
 * @param isCANFD  - Flag for CANFD frames
 */
void create_TX_SETUP(int const *const socketFD, void *const frames, int nframes, uint32_t count,
                     struct bcm_timeval ival1, struct bcm_timeval ival2, int isCANFD){

    // BCM message with multiple frames
    struct bcmMsgMultipleFrames msg;

    // Note: Always initialize the whole struct with 0.
    // Random values in the memory can cause weird bugs!
    memset(&msg, 0, sizeof(msg));

    msg.msg_head.opcode          = TX_SETUP;
    msg.msg_head.count           = count;
    msg.msg_head.ival1           = ival1;
    msg.msg_head.ival2           = ival2;
    msg.msg_head.nframes         = nframes;

    // Check if it is a CANFD frame
    if(isCANFD) {
        msg.msg_head.flags = CAN_FD_FRAME;
    }

    // Combine the flag values to immediately start the cyclic transmission task
    msg.msg_head.flags = msg.msg_head.flags | SETTIMER | STARTTIMER;

    for(int index = 0; index < nframes; index++){
        // Because of the bcm_msg_head we always need to cast to a can_frame
        msg.canFrames[index] = ((struct can_frame*) frames)[index];
    }

    // Send the TX_SETUP configuration message
    if(send(*socketFD, &msg, sizeof(msg), 0) < 0){
        printf("Error could not send TX_SETUP message \n");
        shutdownHandler(ERR_TX_SETUP_FAILED, socketFD);
    }

}

/**
 * Creates a RX filter for the CAN ID
 *
 * @param socketFD - The socket file descriptor
 * @param canID    - The CAN ID that should be added to the RX filter
 */
void create_RX_SETUP_CANID(int const *const socketFD, int canID){

    // BCM message with a single frame
    struct bcmMsgSingleFrame msg;

    // Note: Always initialize the whole struct with 0.
    // Random values in the memory can cause weird bugs!
    memset(&msg, 0, sizeof(msg));

    msg.msg_head.opcode          = RX_SETUP;
    msg.msg_head.flags           = RX_FILTER_ID;
    msg.msg_head.can_id          = canID;

    // Send the RX_SETUP configuration message
    if(send(*socketFD, &msg, sizeof(msg), 0) < 0){
        printf("Error could not send RX_SETUP message \n");
        shutdownHandler(ERR_RX_SETUP_FAILED, socketFD);
    }

}

/**
 * Processes the next operation of the queue from the simulation
 *
 * @param socketFD - The socket file descriptor
 */
void process_operation(int const* const socketFD){

    // Get operation from queue

    // Check what we need to do: send, send cyclic, add CAN ID to RX filter etc...

    // Process operation

    printf("Processed operation task from the simulation \n");
}

/**
 * Receive CAN/CANFD frame and put the extracted data in the queue to the simulation
 *
 * @param socketFD - The socket file descriptor
 */
void process_receive(int const* const socketFD){

    // Receive

    // Check if there was sth to receive

    // Check operation value

    // Process operation

    printf("Put operation in the queue for the simulation to process \n");
}

int main(){

    struct sigaction sigAction;                     // Signal action for CTRL + F

    int socketFD = -1;                              // Socket file descriptor
    struct sockaddr_can socketAddr;                 // Socket address


    // Process termination signal for CTRL + F
    sigAction.sa_handler = handleTerminationSignal;

    if(sigaction(SIGINT, &sigAction, NULL) < 0){
        printf("Setting signal handler for SIGINT failed \n");
        shutdownHandler(ERR_SIGACTION_FAILED, &socketFD);
    }

    // Set up the socket
    if(setupSocket(&socketFD, &socketAddr, 0) != 0){
        printf("Error could not setup the socket \n");
        shutdownHandler(ERR_SETUP_FAILED, &socketFD);
    }

    printf("Setup the socket on the interface %s\n", INTERFACE);


    // Test Frame
    struct can_frame frame1;

    frame1.can_id  = 0x123;
    frame1.can_dlc = 4;
    frame1.data[0] = 0xDE;
    frame1.data[1] = 0xAD;
    frame1.data[2] = 0xBE;
    frame1.data[3] = 0xEF;

    struct can_frame frame2;

    frame2.can_id  = 0x345;
    frame2.can_dlc = 3;
    frame2.data[0] = 0xC0;
    frame2.data[1] = 0xFF;
    frame2.data[2] = 0xEE;

    struct can_frame frameArr[2];
    frameArr[0] = frame1;
    frameArr[1] = frame2;

    // Test intervals
    struct bcm_timeval ival1;
    ival1.tv_sec  = 0;
    ival1.tv_usec = 500;

    struct bcm_timeval ival2;
    ival2.tv_sec  = 1;
    ival2.tv_usec = 0;

    // Test RX
    size_t numbytes = 0;
    struct bcmMsgSingleFrame rxMsg;


    // TX_SEND Test
    create_TX_SEND(&socketFD, frameArr, 2, 0);

    // TX_SETUP Test
    create_TX_SETUP(&socketFD, frameArr, 2, 10, ival1, ival2, 0);

    // RX_SETUP Test
    create_RX_SETUP_CANID(&socketFD, 0x222);

    // Keep running until stopped
    while(keepRunning){

        // Process operation message from the queue
        process_operation(&socketFD);

        // Receive on the socket
        process_receive(&socketFD);
    }

    // Call the shutdown handler
    shutdownHandler(RET_E_OK, &socketFD);
    return RET_E_OK;
}


/*******************************************************************************
 * END OF FILE
 ******************************************************************************/