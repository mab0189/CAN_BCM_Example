/*******************************************************************************
 \project   INFM_HIL_Interface
 \file      CANFD_BCM_Socket.c
 \brief     Provides functions for the setup of a CAN/CANFD BCM socket.
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
#include <errno.h>
#include <linux/can.h>
#include <net/if.h>
#include <string.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <unistd.h>


/*******************************************************************************
 * FUNCTION DEFINITIONS
 ******************************************************************************/

int setupSocket(int *const socketFD, struct sockaddr_can *const addr){

    // Get the socket file descriptor for ioctl
    *socketFD = socket(PF_CAN, SOCK_DGRAM, CAN_BCM);

    // Error handling
    if(*socketFD == -1){
        perror("Error getting socket file descriptor failed");
        return ERR_SOCKET_FAILED;
    }

    // Set the interface name in the ifr
    struct ifreq ifr;
    snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", INTERFACE);

    // Get the ifrindex of the interface name
    if(ioctl(*socketFD, SIOCGIFINDEX, &ifr) < 0){
        printf("Error could not get ifrindex: %s\n", strerror(errno));
        return ERR_IF_NOT_FOUND;
    }

    // Fill in the family and ifrindex
    addr->can_family  = AF_CAN;
    addr->can_ifindex = ifr.ifr_ifindex;

    // Connect to the socket
    if(connect(*socketFD, (struct sockaddr *) addr, sizeof(struct sockaddr_can)) != 0){
        perror("Error could not connect to the socket");
        close(*socketFD);
        return ERR_SETUP_FAILED;
    }

    return RET_E_OK;
}


/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
