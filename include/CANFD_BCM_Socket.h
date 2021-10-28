/*******************************************************************************
 \project   INFM_HIL_Interface
 \file      CANFD_BCM_Socket.h
 \brief     Provides functions for the setup of a CAN/CANFD BCM socket.
 \author    Matthias Bank
 \version   1.0.0
 \date      28.10.2021
 ******************************************************************************/
#ifndef CANFD_BCM_SOCKET_H
#define CANFD_BCM_SOCKET_H


/*******************************************************************************
 * INCLUDES
 ******************************************************************************/
#include <linux/can.h>


/*******************************************************************************
 * FUNCTION DECLARATIONS
 ******************************************************************************/

/**
 * Creates a CAN/CANFD BCM socket on an interface.
 * Support for CANFD frames can be enabled if needed.
 *
 * @param socketFD  - Storage for the created socket descriptor
 * @param addr     -  Storage for the sockaddr_can of the socket
 */
extern int setupSocket(int *socketFD, struct sockaddr_can *addr);


#endif //CANFD_BCM_SOCKET_H


/*******************************************************************************
 * END OF FILE
 ******************************************************************************/
