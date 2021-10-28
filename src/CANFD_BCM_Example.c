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
#include <sys/socket.h>
#include <unistd.h>


/*******************************************************************************
 * STRUCTS
 ******************************************************************************/

/**
 * Struct for a single TX message.
 */
struct singleTxMsg{
    struct bcm_msg_head msg_head;
    struct can_frame frame[1];
};


/*******************************************************************************
 * FUNCTION DEFINITIONS
 ******************************************************************************/

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

void create_TX_SEND(int socketFD, struct singleTxMsg *const msg){

    // Write the TX_SEND configuration message
    if(write(socketFD, msg, sizeof(struct singleTxMsg)) < 0){
        printf("Error could not write TX_SEND message \n");
        shutdownHandler(ERR_TX_SEND_FAILED, socketFD);
    }

}

int main(){

    int socketFD = -1;                              // Socket file descriptor
    struct sockaddr_can socketAddr;                 // Socket address

    // Set up the socket
    if(setupSocket(&socketFD, &socketAddr) != 0){
        printf("Error could not setup the socket \n");
        shutdownHandler(ERR_SETUP_FAILED, socketFD);
    }

    struct can_frame frame1;
    struct singleTxMsg msg1;

    frame1.can_id  = 0x123;
    frame1.can_dlc = 4;
    frame1.data[0] = 0xDE;
    frame1.data[1] = 0xAD;
    frame1.data[2] = 0xBE;
    frame1.data[3] = 0xEF;

    msg1.msg_head.opcode  = TX_SEND;
    msg1.msg_head.can_id  = 0x123;
    msg1.msg_head.flags   = 0;
    msg1.msg_head.nframes = 1;
    msg1.frame[0]         = frame1;

    printf("Setup the socket on the interface %s\n", INTERFACE);

    create_TX_SEND(socketFD, &msg1);

    shutdownHandler(RET_E_OK, socketFD);
    return RET_E_OK;
}


/*******************************************************************************
 * END OF FILE
 ******************************************************************************/