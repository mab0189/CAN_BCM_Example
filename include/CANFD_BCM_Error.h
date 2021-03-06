/*******************************************************************************
 \project   INFM_HIL_Interface
 \file      CANFD_BCM_Error.h
 \brief     Defines the error and return codes.
 \author    Matthias Bank
 \version   1.0.0
 \date      28.10.2021
 ******************************************************************************/
#ifndef CANFD_BCM_ERROR_H
#define CANFD_BCM_ERROR_H


/*******************************************************************************
 * DEFINES
 ******************************************************************************/
#define RET_E_OK                     0
#define ERR_SIGACTION_FAILED        -1
#define ERR_IF_NOT_FOUND            -2
#define ERR_SOCKET_FAILED           -3
#define ERR_FCNTL_FAILED            -4
#define ERR_SETUP_FAILED            -5
#define ERR_TX_SEND_FAILED          -6
#define ERR_TX_SETUP_FAILED         -7
#define ERR_RX_SETUP_FAILED         -8
#define ERR_RECV_FAILED             -9
#define ERR_MALLOC_FAILED          -10

#endif //CANFD_BCM_ERROR_H


/*******************************************************************************
 * END OF FILE
 ******************************************************************************/