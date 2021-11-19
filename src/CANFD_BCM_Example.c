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
#include <errno.h>
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
 * of a cyclic TX task configuration. Check the socketCAN documentation.
 */
#define MAXFRAMES 256


/*******************************************************************************
 * STRUCTS
 ******************************************************************************/

/**
 * Struct for a BCM message with a single CAN frame.
 */
struct bcmMsgSingleFrameCan{
    struct bcm_msg_head msg_head;
    struct can_frame canFrame[1];
};

/**
 * Struct for a BCM message with a single CANFD frame.
 */
struct bcmMsgSingleFrameCanFD{
    struct bcm_msg_head msg_head;
    struct canfd_frame canfdFrame[1];
};

/**
 * Struct for a BCM message with multiple CAN frames.
 */
 struct bcmMsgMultipleFramesCan{
     struct bcm_msg_head msg_head;
     struct can_frame canFrames[MAXFRAMES];
 };

/**
* Struct for a BCM message with multiple CANFD frames.
*/
struct bcmMsgMultipleFramesCanFD{
    struct bcm_msg_head msg_head;
    struct canfd_frame canfdFrames[MAXFRAMES];
};


/*******************************************************************************
 * VARIABLES
 ******************************************************************************/
volatile int keepRunning = 1; // Keep running till CTRL + F is pressed


/*******************************************************************************
 * FUNCTION DEFINITIONS
 ******************************************************************************/

/**
 * Process termination signal.
 *
 * @param signumber The number of the signal that occurred.
 */
static void handleTerminationSignal(int signumber){

    // Stop the application
    keepRunning = 0;
}

/**
 * Handles the shutdown procedure.
 *
 * @param retCode  - The return code.
 * @param socketFD - The socket file descriptor.
 */
void shutdownHandler(int retCode, int const *const socketFD){

    // Close the socket
    if(*socketFD != -1){
        close(*socketFD);
    }

    exit(retCode);
}

/**
 * Create a non cyclic transmission task for multiple CAN/CANFD frames.
 *
 * @param socketFD - The socket file descriptor.
 * @param frames   - The array of CAN/CANFD frames that should be send.
 * @param nframes  - The number of CAN/CANFD frames that should be send.
 * @param isCANFD  - Flag for CANFD frames.
 */
void createTxSend(int const *const socketFD, struct canfd_frame frames[], int nframes, int isCANFD){

    // BCM message we are sending with a single CAN or CANFD frame
    void* msg      = NULL;
    size_t msgSize = 0;

    // Check if we are sending CAN or CANFD frames.
    if(isCANFD){
        msgSize = sizeof(struct bcmMsgSingleFrameCanFD);
        msg = malloc(msgSize);
    }else {
        msgSize = sizeof(struct bcmMsgSingleFrameCan);
        msg = malloc(msgSize);
    }

    // Error handling
    if(msg == NULL){
        printf("Error could not allocate memory for the message \n");
        shutdownHandler(ERR_MALLOC_FAILED, socketFD);
    }

    // Note: Always initialize the whole struct with 0.
    // Random values in the memory can cause weird bugs!
    memset(msg, 0, msgSize);

    if(isCANFD){
        struct bcmMsgSingleFrameCanFD *msgCANFD = (struct bcmMsgSingleFrameCanFD *) msg;
        msgCANFD->msg_head.opcode  = TX_SEND;
        msgCANFD->msg_head.flags   = CAN_FD_FRAME;
        msgCANFD->msg_head.nframes = 1;
    }else{
        struct bcmMsgSingleFrameCan *msgCAN = (struct bcmMsgSingleFrameCan *) msg;
        msgCAN->msg_head.opcode    = TX_SEND;
        msgCAN->msg_head.nframes   = 1;
    }

    // Note: TX_SEND can only send one frame at a time unlike TX_SETUP!
    // This is the reason why we must use a loop instead of a struct
    // that can contain multiple frames.
    for(int index = 0; index < nframes; index++){

        if(isCANFD){
            struct bcmMsgSingleFrameCanFD *msgCANFD = (struct bcmMsgSingleFrameCanFD *) msg;
            msgCANFD->msg_head.can_id = frames[index].can_id;
            msgCANFD->canfdFrame[0]   = frames[index];
        }else{
            struct bcmMsgSingleFrameCan *msgCAN = (struct bcmMsgSingleFrameCan *) msg;
            struct can_frame* canFrame = (struct can_frame *) &frames[index];
            msgCAN->msg_head.can_id = canFrame->can_id;
            msgCAN->canFrame[0]     = *canFrame;
        }

        // Send the TX_SEND configuration message.
        if(send(*socketFD, msg, msgSize, 0) < 0){
            printf("Error could not write TX_SEND message \n");
            shutdownHandler(ERR_TX_SEND_FAILED, socketFD);
        }

    }

    // Free the allocated memory for the BCM message
    if(msg != NULL){
        free(msg);
    }
}

/**
 * Create a cyclic transmission task for one or multiple CAN/CANFD frames.
 *
 * Note: The frames will not be send as a atomic sequence. For each frame
 * a new cyclic transmission task will be created. There will be a delay
 * between the frames.
 *
 * @param socketFD - The socket file descriptor.
 * @param frames   - The array of CAN/CANFD frames that should be send cyclic.
 * @param nframes  - The number of CAN/CANFD frames that should be send cyclic.
 * @param count    - Number of times the frame is send with the first interval.
 *                   If count is zero only the second interval is being used.
 * @param ival1    - First interval.
 * @param ival2    - Second interval.
 * @param isCANFD  - Flag for CANFD frames.
 */
void createTxSetup(int const *const socketFD, struct canfd_frame frames[], int nframes, const uint32_t count[],
                           struct bcm_timeval ival1[], struct bcm_timeval ival2[], int isCANFD){

    // BCM message we are sending with multiple CAN or CANFD frame
    void* msg      = NULL;
    size_t msgSize = 0;

    // Check if we are sending CAN or CANFD frames.
    if(isCANFD){
        msgSize = sizeof(struct bcmMsgSingleFrameCanFD);
        msg = malloc(msgSize);
    }else{
        msgSize = sizeof(struct bcmMsgSingleFrameCan);
        msg = malloc(msgSize);
    }

    // Error handling
    if(msg == NULL){
        printf("Error could not allocate memory for the message \n");
        shutdownHandler(ERR_MALLOC_FAILED, socketFD);
    }

    // Note: Always initialize the whole struct with 0.
    // Random values in the memory can cause weird bugs!
    memset(msg, 0, msgSize);

    // Note: By combining the flags SETTIMER and STARTTIMER
    // the BCM will start sending the messages immediately
    if(isCANFD){
        struct bcmMsgSingleFrameCanFD *msgCANFD = (struct bcmMsgSingleFrameCanFD *) msg;

        msgCANFD->msg_head.opcode  = TX_SETUP;
        msgCANFD->msg_head.flags   = CAN_FD_FRAME | SETTIMER | STARTTIMER;
        msgCANFD->msg_head.nframes = 1;

    }else{
        struct bcmMsgSingleFrameCan *msgCAN = (struct bcmMsgSingleFrameCan *) msg;

        msgCAN->msg_head.opcode    = TX_SETUP;
        msgCAN->msg_head.flags     = SETTIMER | STARTTIMER;
        msgCAN->msg_head.nframes   = 1;
    }

    // Note: We send for each TX_SETUP a single CAN/CANFD frame with its CAN ID in the
    // bcm_msg_head. This way we do not create a cyclic transmission sequence which can
    // only be removed with the CAN ID that was set in the bcm_msg_head. Another benefit
    // is that each CAN/CANFD frame can have different count, ival1, and ival2 values.
    for(int index = 0; index < nframes; index++){

        // Fill out the CAN ID and frame data
        if(isCANFD){
            struct bcmMsgSingleFrameCanFD *msgCANFD = (struct bcmMsgSingleFrameCanFD *) msg;
            struct canfd_frame *canfdFrame = &frames[index];

            msgCANFD->msg_head.can_id = canfdFrame->can_id;
            msgCANFD->msg_head.count  = count[index];
            msgCANFD->msg_head.ival1  = ival1[index];
            msgCANFD->msg_head.ival2  = ival2[index];
            msgCANFD->canfdFrame[0]   = *canfdFrame;

        }else{
            struct bcmMsgSingleFrameCan *msgCAN = (struct bcmMsgSingleFrameCan *) msg;
            struct can_frame *canFrame = (struct can_frame*) &frames[index];

            msgCAN->msg_head.can_id = canFrame->can_id;
            msgCAN->msg_head.count  = count[index];
            msgCAN->msg_head.ival1  = ival1[index];
            msgCAN->msg_head.ival2  = ival2[index];
            msgCAN->canFrame[0]     = *canFrame;
        }

        // Send the TX_SETUP configuration message
        if(send(*socketFD, msg, msgSize, 0) < 0){
            printf("Error could not send TX_SETUP message \n");
            shutdownHandler(ERR_TX_SETUP_FAILED, socketFD);
        }

    }

    // Free the allocated memory for the BCM message
    if(msg != NULL){
        free(msg);
    }
}


/**
 * Create a cyclic transmission task for one or multiple CAN/CANFD frames.
 * If more than one frame should be send cyclic the provided sequence of
 * the frames is kept by the BCM.
 *
 * Note: The cyclic transmission task for the sequence can only be deleted
 * with the CAN ID that was set in the bcm_msg_head!
 *
 * @param socketFD - The socket file descriptor.
 * @param frames   - The array of CAN/CANFD frames that should be send cyclic.
 * @param nframes  - The number of CAN/CANFD frames that should be send cyclic.
 * @param count    - Number of times the frame is send with the first interval.
 *                   If count is zero only the second interval is being used.
 * @param ival1    - First interval.
 * @param ival2    - Second interval.
 * @param isCANFD  - Flag for CANFD frames.
 */
void createTxSetupSequence(int const *const socketFD, struct canfd_frame frames[], int nframes, uint32_t count,
                           struct bcm_timeval ival1, struct bcm_timeval ival2, int isCANFD){

    // BCM message we are sending with multiple CAN or CANFD frame
    void* msg      = NULL;
    size_t msgSize = 0;

    // Check if we are sending CAN or CANFD frames.
    if(isCANFD){
        msgSize = sizeof(struct bcmMsgMultipleFramesCanFD);
        msg = malloc(msgSize);
    }else{
        msgSize = sizeof(struct bcmMsgMultipleFramesCan);
        msg = malloc(msgSize);
    }

    // Error handling
    if(msg == NULL){
        printf("Error could not allocate memory for the message \n");
        shutdownHandler(ERR_MALLOC_FAILED, socketFD);
    }

    // Note: Always initialize the whole struct with 0.
    // Random values in the memory can cause weird bugs!
    memset(msg, 0, msgSize);

    // Note: By combining the flags SETTIMER and STARTTIMER
    // the BCM will start sending the messages immediately
    if(isCANFD){
        struct bcmMsgMultipleFramesCanFD *msgCANFD = (struct bcmMsgMultipleFramesCanFD *) msg;

        msgCANFD->msg_head.opcode  = TX_SETUP;
        msgCANFD->msg_head.flags   = CAN_FD_FRAME | SETTIMER | STARTTIMER;
        msgCANFD->msg_head.can_id  = frames[0].can_id;
        msgCANFD->msg_head.count   = count;
        msgCANFD->msg_head.ival1   = ival1;
        msgCANFD->msg_head.ival2   = ival2;
        msgCANFD->msg_head.nframes = nframes;

        size_t arrSize = sizeof(struct canfd_frame) * nframes;
        memcpy(msgCANFD->canfdFrames, frames, arrSize);
    }else{
        struct bcmMsgMultipleFramesCan *msgCAN = (struct bcmMsgMultipleFramesCan *) msg;
        struct can_frame *firstCanFrame = (struct can_frame*) &frames[0];

        msgCAN->msg_head.opcode    = TX_SETUP;
        msgCAN->msg_head.flags     = SETTIMER | STARTTIMER;
        msgCAN->msg_head.can_id    = firstCanFrame->can_id;
        msgCAN->msg_head.count     = count;
        msgCAN->msg_head.ival1     = ival1;
        msgCAN->msg_head.ival2     = ival2;
        msgCAN->msg_head.nframes   = nframes;

        for(int index = 0; index < nframes; index++){
            struct can_frame *canFrame = (struct can_frame*) &frames[index];
            msgCAN->canFrames[index] = *canFrame;
        }
    }

    // Send the TX_SETUP configuration message
    if(send(*socketFD, msg, msgSize, 0) < 0){
        printf("Error could not send TX_SETUP message \n");
        shutdownHandler(ERR_TX_SETUP_FAILED, socketFD);
    }

    // Free the allocated memory for the BCM message
    if(msg != NULL){
        free(msg);
    }
}

/**
 * Updates a cyclic transmission task for one or multiple CAN/CANFD frames.
 *
 * @param socketFD - The socket file descriptor.
 * @param frames   - The array of CAN/CANFD frames with the updated data.
 * @param nframes  - The number of CAN/CANFD frames that should be updated.
 * @param isCANFD  - Flag for CANFD frames.
 * @param announce - The cycle is retained but the changed data will be send immediately once.
 */
void createTxSetupUpdate(int const *const socketFD, struct canfd_frame frames[], int nframes, int isCANFD, int announce){

    // BCM message we are sending with multiple CAN or CANFD frame
    void* msg      = NULL;
    size_t msgSize = 0;

    // Check if we are sending CAN or CANFD frames.
    if(isCANFD){
        msgSize = sizeof(struct bcmMsgSingleFrameCanFD);
        msg = malloc(msgSize);
    }else{
        msgSize = sizeof(struct bcmMsgSingleFrameCan);
        msg = malloc(msgSize);
    }

    // Error handling
    if(msg == NULL){
        printf("Error could not allocate memory for the message \n");
        shutdownHandler(ERR_MALLOC_FAILED, socketFD);
    }

    // Note: Always initialize the whole struct with 0.
    // Random values in the memory can cause weird bugs!
    memset(msg, 0, msgSize);

    // Note: By combining the flags SETTIMER and STARTTIMER
    // the BCM will start sending the messages immediately
    if(isCANFD){
        struct bcmMsgSingleFrameCanFD *msgCANFD = (struct bcmMsgSingleFrameCanFD *) msg;

        msgCANFD->msg_head.opcode  = TX_SETUP;
        msgCANFD->msg_head.flags   = CAN_FD_FRAME;
        msgCANFD->msg_head.nframes = 1;

        if(announce){
            msgCANFD->msg_head.flags = msgCANFD->msg_head.flags | TX_ANNOUNCE;
        }

    }else{
        struct bcmMsgSingleFrameCan *msgCAN = (struct bcmMsgSingleFrameCan *) msg;

        msgCAN->msg_head.opcode    = TX_SETUP;
        msgCAN->msg_head.nframes   = 1;

        if(announce){
            msgCAN->msg_head.flags = TX_ANNOUNCE;
        }

    }

    for(int index = 0; index < nframes; index++){

        // Fill out the CAN ID and frame data
        if(isCANFD){
            struct bcmMsgSingleFrameCanFD *msgCANFD = (struct bcmMsgSingleFrameCanFD *) msg;
            struct canfd_frame *canfdFrame = &frames[index];

            msgCANFD->msg_head.can_id = canfdFrame->can_id;
            msgCANFD->canfdFrame[0]   = *canfdFrame;

        }else{
            struct bcmMsgSingleFrameCan *msgCAN = (struct bcmMsgSingleFrameCan *) msg;
            struct can_frame *canFrame = (struct can_frame*) &frames[index];

            msgCAN->msg_head.can_id = canFrame->can_id;
            msgCAN->canFrame[0]     = *canFrame;
        }

        // Send the TX_SETUP configuration message
        if(send(*socketFD, msg, msgSize, 0) < 0){
            printf("Error could not send TX_SETUP message \n");
            shutdownHandler(ERR_TX_SETUP_FAILED, socketFD);
        }

    }

    // Free the allocated memory for the BCM message
    if(msg != NULL){
        free(msg);
    }
}

/**
 * Removes a cyclic transmission task for a CAN ID.
 *
 * Note: If the cyclic transmission task was created with createTxSetupSequence
 * it can only be removed with the CAN ID that was set in the bcm_msg_head even
 * if the CAN IDs of the sequence are different. If you try to delete a cyclic
 * transmission task which CAN ID was not set in the bcm_msg_head the delete
 * will fail with an error! If you delete the transmission task that was set
 * in the bcm_msg_head the cyclic transmission of all frames in the sequence
 * will be stopped.
 *
 * @param socketFD - The socket file descriptor.
 * @param canID    - The CAN ID of the cyclic transmission task that should be removed.
 * @param isCANFD  - Flag for CANFD frames.
 */
void createTxDelete(int const *const socketFD, canid_t canID, int isCANFD){

    struct bcm_msg_head msg;

    // Note: Always initialize the whole struct with 0.
    // Random values in the memory can cause weird bugs!
    memset(&msg, 0, sizeof(msg));

    msg.opcode = TX_DELETE;
    msg.can_id = canID;

    if(isCANFD){
        msg.flags = CAN_FD_FRAME;
    }

    // Send the TX_DELETE configuration message
    if(send(*socketFD, &msg, sizeof(msg), 0) < 0){
        printf("Error could not send TX_DELETE message \n");
        shutdownHandler(ERR_RX_SETUP_FAILED, socketFD);
    }
}

/**
 * Creates a RX filter for the CAN ID.
 * I. e. we get notified on all received frames with this CAN ID!
 *
 * @param socketFD - The socket file descriptor.
 * @param canID    - The CAN ID that should be added to the RX filter.
 * @param isCANFD  - Flag for CANFD frames.
 */
void createRxSetupCanID(int const *const socketFD, canid_t canID, int isCANFD){

    struct bcm_msg_head msg;

    // Note: Always initialize the whole struct with 0.
    // Random values in the memory can cause weird bugs!
    memset(&msg, 0, sizeof(msg));

    msg.opcode = RX_SETUP;
    msg.flags  = RX_FILTER_ID;
    msg.can_id = canID;

    if(isCANFD){
        msg.flags = msg.flags | CAN_FD_FRAME;
    }

    // Send the RX_SETUP configuration message
    if(send(*socketFD, &msg, sizeof(msg), 0) < 0){
        printf("Error could not send RX_SETUP message \n");
        shutdownHandler(ERR_RX_SETUP_FAILED, socketFD);
    }
}

/**
 * Creates a RX filter for the CAN ID and the relevant bits of the frame.
 * I. e. we only get notified on changes for the set bits in the mask.
 *
 * @param socketFD - The socket file descriptor.
 * @param canID    - The CAN ID that should be added to the RX filter.
 * @param mask     - The mask for the relevant bits of the frame.
 * @param isCANFD  - Flag for CANFD frames.
 */
void createRxSetupMask(int const *const socketFD, canid_t canID, struct canfd_frame mask, int isCANFD){

    // BCM message we are sending with a single CAN or CANFD frame
    void* msg      = NULL;
    size_t msgSize = 0;

    // Check if we are sending CAN or CANFD frames.
    if(isCANFD){
        msgSize = sizeof(struct bcmMsgSingleFrameCanFD);
        msg = malloc(msgSize);
    }else {
        msgSize = sizeof(struct bcmMsgSingleFrameCan);
        msg = malloc(msgSize);
    }

    // Error handling
    if(msg == NULL){
        printf("Error could not allocate memory for the message \n");
        shutdownHandler(ERR_MALLOC_FAILED, socketFD);
    }

    // Note: Always initialize the whole struct with 0.
    // Random values in the memory can cause weird bugs!
    memset(msg, 0, msgSize);

    if(isCANFD){
        struct bcmMsgSingleFrameCanFD *msgCANFD = (struct bcmMsgSingleFrameCanFD *) msg;

        msgCANFD->msg_head.opcode  = RX_SETUP;
        msgCANFD->msg_head.flags   = CAN_FD_FRAME;
        msgCANFD->msg_head.can_id  = canID;
        msgCANFD->msg_head.nframes = 1;

        msgCANFD->canfdFrame[0]   = mask;
    }else{
        struct bcmMsgSingleFrameCan *msgCAN = (struct bcmMsgSingleFrameCan *) msg;

        msgCAN->msg_head.opcode    = RX_SETUP;
        msgCAN->msg_head.can_id    = canID;
        msgCAN->msg_head.nframes   = 1;

        msgCAN->canFrame[0]       = *((struct can_frame*) &mask);
    }

    // Send the RX_SETUP configuration message
    if(send(*socketFD, msg, msgSize, 0) < 0){
        printf("Error could not send RX_SETUP message \n");
        shutdownHandler(ERR_TX_SETUP_FAILED, socketFD);
    }
}

/**
 * Removes a RX filter for the CAN ID.
 *
 * @param socketFD - The socket file descriptor.
 * @param canID    - The CAN ID that should be removed from the RX filter.
 * @param isCANFD  - Flag for CANFD frames.
 */
void createRxDelete(int const *const socketFD, canid_t canID, int isCANFD){

    struct bcm_msg_head msg;

    // Note: Always initialize the whole struct with 0.
    // Random values in the memory can cause weird bugs!
    memset(&msg, 0, sizeof(msg));

    msg.opcode = RX_DELETE;
    msg.can_id = canID;

    if(isCANFD){
        msg.flags = CAN_FD_FRAME;
    }

    // Send the RX_DELETE configuration message
    if(send(*socketFD, &msg, sizeof(msg), 0) < 0){
        printf("Error could not send RX_DELETE message \n");
        shutdownHandler(ERR_RX_SETUP_FAILED, socketFD);
    }
}

/**
 * Processes the next operation of the queue from the simulation.
 *
 * @param socketFD - The socket file descriptor.
 */
void processOperation(int const* const socketFD){

    // Get operation from queue

    // Check what we need to do: send, send cyclic, add CAN ID to RX filter etc...

    // Process operation

    printf("Processed operation task from the simulation \n");
}

/**
 * Processes the timeout of a cyclic CAN/CANFD message.
 *
 * @param msg - The received timeout message from the BCM socket.
 */
void processTimeout(struct bcmMsgSingleFrameCanFD const* const msg){

    //TODO: What do we do in this case?

    printf("Timeout occurred! \n");
}

/**
 * Processes the content change of a CAN/CANFD message.
 *
 * @param msg - The received content change message from the BCM socket.
 */
void processContentChange(struct bcmMsgSingleFrameCanFD const* const msg){

    //TODO:
    // 1. Extract needed information
    // 2. Map information to the Event
    // 3. Put the event in the queue

    printf("Content changed! \n");
}

/**
 * Receive CAN/CANFD frame and put the extracted data in the queue to the simulation.
 *
 * @param socketFD - The socket file descriptor.
 */
void processReceive(int const* const socketFD){

    int nbytes = 0;                    // Number of bytes we received
    struct bcmMsgSingleFrameCanFD msg; // The buffer that stores the received message

    // Note: Always initialize the whole struct with 0.
    // Random values in the memory can cause weird bugs!
    memset(&msg, 0, sizeof(msg));

    // Reset errno before calling receive on the socket that sets errno on failure
    errno = 0;

    // Receive on the BCM socket
    nbytes = recv(*socketFD, &msg, sizeof(msg), 0);

    // Check validity of the received message
    if(nbytes < 0){

        // Check if there was an actual error or if there was nothing received on the socket.
        // This can happen when the socket is set to be non-blocking.
        if(errno != EAGAIN && errno != EWOULDBLOCK){
            printf("Error could not receive on the socket \n");
            shutdownHandler(ERR_RECV_FAILED, socketFD);
        }

        // There was nothing to receive so we can exit early
        return;

    }else if(nbytes != sizeof(struct bcmMsgSingleFrameCan) && nbytes != sizeof(struct bcmMsgSingleFrameCanFD)){
        printf("Error received unexpected number of bytes \n");
        shutdownHandler(ERR_RECV_FAILED, socketFD);
    }

    // Check if we got one of the expected operation codes:
    // RX_CHANGED: Simple reception of a CAN/CANFD frame or a content change occurred.
    // RX_TIMEOUT: Cyclic message is detected to be absent.
    if(msg.msg_head.opcode != RX_CHANGED && msg.msg_head.opcode != RX_TIMEOUT){
        printf("Error received returned unexpected operation code \n");
        shutdownHandler(ERR_RECV_FAILED, socketFD);
    }else if(msg.msg_head.opcode == RX_TIMEOUT){
        processTimeout(&msg);
    }else{
        processContentChange(&msg);
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
        shutdownHandler(ERR_SIGACTION_FAILED, &socketFD);
    }

    // Set up the socket
    if(setupSocket(&socketFD, &socketAddr, 0) != 0){
        printf("Error could not setup the socket \n");
        shutdownHandler(ERR_SETUP_FAILED, &socketFD);
    }

    printf("Setup the socket on the interface %s\n", INTERFACE);

    // Test CAN Frame
    struct can_frame canFrame1;
    canFrame1.can_id  = 0x123;
    canFrame1.can_dlc = 4;
    canFrame1.data[0] = 0xDE;
    canFrame1.data[1] = 0xAD;
    canFrame1.data[2] = 0xBE;
    canFrame1.data[3] = 0xEF;

    struct can_frame canFrame2;
    canFrame2.can_id  = 0x345;
    canFrame2.can_dlc = 3;
    canFrame2.data[0] = 0xC0;
    canFrame2.data[1] = 0xFF;
    canFrame2.data[2] = 0xEE;

    struct canfd_frame frameArrCAN[2];
    frameArrCAN[0] = *((struct canfd_frame*) &canFrame1);
    frameArrCAN[1] = *((struct canfd_frame*) &canFrame2);

    struct can_frame canFrame1Modified;
    canFrame1Modified.can_id  = 0x123;
    canFrame1Modified.can_dlc = 4;
    canFrame1Modified.data[0] = 0xBE;
    canFrame1Modified.data[1] = 0xEF;
    canFrame1Modified.data[2] = 0xDE;
    canFrame1Modified.data[3] = 0xAD;

    struct can_frame canFrame2Modified;
    canFrame2Modified.can_id  = 0x345;
    canFrame2Modified.can_dlc = 5;
    canFrame2Modified.data[0] = 0xC0;
    canFrame2Modified.data[1] = 0xFF;
    canFrame2Modified.data[2] = 0xEE;
    canFrame2Modified.data[3] = 0xFF;
    canFrame2Modified.data[4] = 0xEE;

    struct canfd_frame frameArrCANModified[2];
    frameArrCANModified[0] = *((struct canfd_frame*) &canFrame1Modified);
    frameArrCANModified[1] = *((struct canfd_frame*) &canFrame2Modified);

    // Test CANFD Frame
    struct canfd_frame canfdFrame1;
    canfdFrame1.can_id   = 0x567;
    canfdFrame1.len      = 16;
    canfdFrame1.data[0]  = 0xDE;
    canfdFrame1.data[1]  = 0xAD;
    canfdFrame1.data[2]  = 0xBE;
    canfdFrame1.data[3]  = 0xEF;
    canfdFrame1.data[4]  = 0xDE;
    canfdFrame1.data[5]  = 0xAD;
    canfdFrame1.data[6]  = 0xBE;
    canfdFrame1.data[7]  = 0xEF;
    canfdFrame1.data[8]  = 0xDE;
    canfdFrame1.data[9]  = 0xAD;
    canfdFrame1.data[10] = 0xBE;
    canfdFrame1.data[11] = 0xEF;
    canfdFrame1.data[12] = 0xDE;
    canfdFrame1.data[13] = 0xAD;
    canfdFrame1.data[14] = 0xBE;
    canfdFrame1.data[15] = 0xEF;

    struct canfd_frame canfdFrame2;
    canfdFrame2.can_id   = 0x789;
    canfdFrame2.len      = 12;
    canfdFrame2.data[0]  = 0xC0;
    canfdFrame2.data[1]  = 0xFF;
    canfdFrame2.data[2]  = 0xEE;
    canfdFrame2.data[3]  = 0xC0;
    canfdFrame2.data[4]  = 0xFF;
    canfdFrame2.data[5]  = 0xEE;
    canfdFrame2.data[6]  = 0xC0;
    canfdFrame2.data[7]  = 0xFF;
    canfdFrame2.data[8]  = 0xEE;
    canfdFrame2.data[9]  = 0xC0;
    canfdFrame2.data[10] = 0xFF;
    canfdFrame2.data[11] = 0xEE;

    struct canfd_frame frameArrCANFD[2];
    frameArrCANFD[0] = canfdFrame1;
    frameArrCANFD[1] = canfdFrame2;

    // Test intervals
    struct bcm_timeval ival1;
    ival1.tv_sec  = 0;
    ival1.tv_usec = 500;

    struct bcm_timeval ival2;
    ival2.tv_sec  = 3;
    ival2.tv_usec = 0;

    struct bcm_timeval ivalZero;
    ivalZero.tv_sec  = 0;
    ivalZero.tv_usec = 0;

    struct bcm_timeval ivalArr1[2];
    ivalArr1[0] = ival1;
    ivalArr1[1] = ival1;

    struct bcm_timeval ivalArr1Zero[2];
    ivalArr1Zero[0] = ivalZero;
    ivalArr1Zero[1] = ivalZero;

    struct bcm_timeval ivalArr2[2];
    ivalArr2[0] = ival2;
    ivalArr2[1] = ival2;

    // Test counts
    uint32_t countArr[2];
    countArr[0] = 10;
    countArr[1] = 5;

    uint32_t countArrZero[2];
    countArr[0] = 0;
    countArr[1] = 0;

    // Test Mask
    struct canfd_frame mask;
    mask.len     = 1;
    mask.data[0] = 0xFF;

    // TX_SEND Test
    //createTxSend(&socketFD, frameArrCAN, 2, 0);
    //createTxSend(&socketFD, frameArrCANFD, 2, 1);

    // TX_SETUP Test
    //createTxSetup(&socketFD, frameArrCAN, 2, countArr, ivalArr1, ivalArr2,0);
    //sleep(10);

    //createTxSetup(&socketFD, frameArrCANFD, 2, countArr, ivalArr1, ivalArr2, 1);
    //sleep(10);

    // TX_SETUP Sequence Test
    //createTxSetupSequence(&socketFD, frameArrCAN, 2, 10, ival1, ival2, 0);
    //sleep(10);

    //createTxSetupSequence(&socketFD, frameArrCANFD, 2, 10, ival1, ival2, 1);
    //sleep(10);

    // TX_SETUP Announce Test without announce
    //createTxSetup(&socketFD, frameArrCAN, 2, countArrZero, ivalArr1Zero, ivalArr2,0);
    //sleep(10);
    //createTxSetupUpdate(&socketFD, frameArrCANModified, 2, 0, 0);
    //sleep(10);

    // TX_SETUP Announce Test with announce
    //createTxSetup(&socketFD, frameArrCAN, 2, countArrZero, ivalArr1Zero, ivalArr2,0);
    //sleep(10);
    //createTxSetupUpdate(&socketFD, frameArrCANModified, 2, 0, 1);
    //sleep(10);

    // TX_DELETE Test
    //createTxSetupSequence(&socketFD, &canfdFrame1, 1, 10, ival1, ival2, 1);
    //sleep(5);
    //createTxDelete(&socketFD, canfdFrame1.can_id, 1);
    //sleep(10);

    //createTxSetupSequence(&socketFD, frameArrCANFD, 2, 10, ival1, ival2, 1);
    //sleep(5);
    //createTxDelete(&socketFD, canfdFrame1.can_id, 1);
    //sleep(10);

    // RX_SETUP CAN ID Test
    //createRxSetupCanID(&socketFD, 0x222, 0);
    //createRxSetupCanID(&socketFD, 0x333, 1);

    // RX_SETUP CAN ID + Mask Test
    //createRxSetupMask(&socketFD, 0x222, mask, 0);
    //createRxSetupMask(&socketFD, 0x333, mask, 1);

    // RX_DELETE Test
    //createRxDelete(&socketFD, 0x222, 0);
    //createTxDelete(&socketFD, 0x333, 1);

    // Keep running until stopped
    //while(keepRunning){

        // Process operation message from the queue
        //processOperation(&socketFD);

        // Receive on the socket
        //processReceive(&socketFD);
    //}

    // Call the shutdown handler
    shutdownHandler(RET_E_OK, &socketFD);
    return RET_E_OK;
}


/*******************************************************************************
 * END OF FILE
 ******************************************************************************/