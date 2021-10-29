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
struct bcmMsgsingleFrame{
    struct bcm_msg_head msg_head;
    struct can_frame frame[1];
};

/**
 * Struct for a BCM message with multiple frames.
 */
 struct bcmMsgMultipleFrames{
     struct bcm_msg_head msg_head;
     struct can_frame frames[MAXFRAMES];
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
int shutdownHandler(int retCode, int socketFD){

    // Close the socket
    if(socketFD != -1){
        close(socketFD);
    }

    exit(retCode);
}

/**
 * Sends a CAN/CANFD frame
 *
 * @param socketFD - The socket file descriptor
 * @param canID    - The CAN ID of the frame
 * @param frame    - The CAN/CANFD frame that should be send
 * @param isCANFD  - Flag for CANFD frames
 */
void create_TX_SEND(int socketFD, uint32_t canID, void *frame, int isCANFD){

    // BCM message with a single frame
    struct bcmMsgsingleFrame msg;

    msg.msg_head.opcode  = TX_SEND;
    msg.msg_head.can_id  = canID;
    msg.msg_head.nframes = 1;

    // Check if it is a CANFD frame
    if(isCANFD) {
        msg.msg_head.flags = CAN_FD_FRAME;
    }

    // Because of the bcm_msg_head we always need to cast to a can_frame
    msg.msg_head.frames[0] = *((struct can_frame*) frame);

    // Send the TX_SEND configuration message
    if(send(socketFD, &msg, sizeof(struct bcmMsgsingleFrame), 0) < 0){
        printf("Error could not write TX_SEND message \n");
        shutdownHandler(ERR_TX_SEND_FAILED, socketFD);
    }

}

/**
 * Create a cyclic transmission task for a CAN/CANFD frame
 *
 * @param socketFD - The socket file descriptor
 * @param canID    - The CAN ID of the frame
 * @param frame    - The CAN/CANFD frame that should be send cyclic
 * @param isCANFD  - Flag for CANFD frames
 */
void create_TX_SETUP(int socketFD, uint32_t canID, void *frame, uint32_t count,
                     struct bcm_timeval ival1, struct bcm_timeval ival2, int isCANFD){

    // BCM message with a single frame
    struct bcmMsgsingleFrame msg;

    msg.msg_head.opcode  = TX_SETUP;
    msg.msg_head.can_id  = canID;
    msg.msg_head.nframes = 1;
    msg.msg_head.count   = count;
    msg.msg_head.ival1   = ival1;
    msg.msg_head.ival2   = ival2;

    // Check if it is a CANFD frame
    if(isCANFD) {
        msg.msg_head.flags = CAN_FD_FRAME;
    }

    // Combine the flag values to immediately start the cyclic transmission task
    msg.msg_head.flags = msg.msg_head.flags | SETTIMER | STARTTIMER;

    // Because of the bcm_msg_head we always need to cast to a can_frame
    msg.msg_head.frames[0] = *((struct can_frame*) frame);

    // Send the TX_SEND configuration message
    if(send(socketFD, &msg, sizeof(struct bcmMsgsingleFrame), 0) < 0){
        printf("Error could not write TX_SEND message \n");
        shutdownHandler(ERR_TX_SEND_FAILED, socketFD);
    }

}

int main(){

    struct sigaction sigAction;                     // Signal action for CTRL + F

    int socketFD = -1;                              // Socket file descriptor
    struct sockaddr_can socketAddr;                 // Socket address


    // Process termination signal for CTRL + F
    sigAction.sa_handler = handleTerminationSignal;

    if(sigaction(SIGINT, &sigAction, NULL) < 0){
        printf("Setting signal handler for SIGINT failed \n");
        shutdownHandler(ERR_SIGACTION_FAILED, socketFD);
    }

    // Set up the socket
    if(setupSocket(&socketFD, &socketAddr) != 0){
        printf("Error could not setup the socket \n");
        shutdownHandler(ERR_SETUP_FAILED, socketFD);
    }

    printf("Setup the socket on the interface %s\n", INTERFACE);


    // Test Frame
    struct can_frame frame;

    frame.can_id  = 0x123;
    frame.can_dlc = 4;
    frame.data[0] = 0xDE;
    frame.data[1] = 0xAD;
    frame.data[2] = 0xBE;
    frame.data[3] = 0xEF;

    // Test intervalls
    struct bcm_timeval ival1;
    ival1.tv_sec  = 0;
    ival1.tv_usec = 500;

    struct bcm_timeval ival2;
    ival2.tv_sec  = 1;
    ival2.tv_usec = 0;

    // TX_SEND Test
    create_TX_SEND(socketFD, 0x123, &frame, 0);

    // TX_SETUP Test
    create_TX_SETUP(socketFD, 0x123, &frame, 10, ival1, ival2, 0);

    while(keepRunning){
        // Run until stopped
    }

    // Call the shutdown handler
    shutdownHandler(RET_E_OK, socketFD);
    return RET_E_OK;
}


/*******************************************************************************
 * END OF FILE
 ******************************************************************************/