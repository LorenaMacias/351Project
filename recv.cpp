#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "msg.h"    /* For the message struct */

/* The size of the shared memory chunk */
#define SHARED_MEMORY_CHUNK_SIZE 1000

/* The ids for the shared memory segment and the message queue */
int shmid, msqid;
int ipc = IPC_CREAT | 0666;
/* The pointer to the shared memory */
void *sharedMemPtr;

/* The name of the received file */
const char recvFileName[] = "recvfile";

/**
 * Sets up the shared memory segment and message queue
 * @param shmid - the id of the allocated shared memory 
 * @param msqid - the id of the shared memory
 * @param sharedMemPtr - the pointer to the shared memory
 */

void init(int& shmid, int& msqid, void*& sharedMemPtr) {
    /* Store the IDs and the pointer to the shared memory region in the
     * corresponding parameters */

    //created file "keyfile.txt" manually

    //generate key 
    key_t key = ftok("keyfile.txt", 'a');
    if (key == (key_t)-1) {
        perror("Error: ftok");
        exit(1);
    }

    //create message que
    if ((shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, ipc)) < 0) {
        perror("Error: shmget");
        exit(1);
    }
    else{
	(void) fprintf(stderr, "shmget returned %d\n", shmid);
    }

    //attach sharedMemPtr to shared memory location 
    if ((sharedMemPtr = shmat(shmid, NULL, 0)) == (char*)-1) {
        perror("Error: shmat");
        exit(1);
    }

    //allocated shared memory
    if ((msqid = msgget(key, IPC_CREAT | 0642)) < 0) {
        perror("Error: msgget");
        exit(1);
    }
}

/**
 * The main loop
 */
void mainLoop() {
    struct message sndMsg;
    sndMsg.mtype = RECV_DONE_TYPE;

    struct message rcvMsg;
    rcvMsg.mtype = SENDER_DATA_TYPE;

    const int eachMsgSize = sizeof(struct message) - sizeof(long);

    /* The size of the mesage */
    int msgSize = 0;
    
    /* Open the file for writing */
    FILE* fp = fopen(recvFileName, "w");
    /* Error checks */
    if(!fp)
    {
	perror("fopen");
	exit(-1);
    }
    int size = msgrcv(msqid, &rcvMsg, eachMsgSize, rcvMsg.mtype, 0);

    /* Keep receiving until the sender set the size to 0, indicating that
       there is no more data to send */
    msgSize = rcvMsg.size; 
    while(msgSize != 0)
    {
	/* If the sender is not telling us that we are done, then get to work */
	if(msgSize != 0)
	{
		/* Save the shared memory to file */
		if(fwrite(sharedMemPtr, sizeof(char), msgSize, fp) < 0) { 
			perror("Error: fwrite");
		}
		else{
			printf("Memory has been written to file \n");
		}

		size = msgsnd(msqid, &sndMsg, eachMsgSize, 0);
 		if (size == -1){ 
			perror("Error: msgsnd");
			exit(1);
		}
		else{
			printf("Notified sender we are ready for next chunk \n");
		}
	}
	/* We are done */
	else { fclose(fp); }/* Close the file */

	size = msgrcv(msqid, &rcvMsg, eachMsgSize, rcvMsg.mtype, 0);
        if (msgSize < 0){
		perror("Error: msgrcv");
		exit(1);
	}
	msgSize = rcvMsg.size;

     }

}

/**
 * Perfoms the cleanup functions
 *
 * @param sharedMemPtr - the pointer to the shared memory
 * @param shmid - the id of the shared memory segment
 * @param msqid - the id of the message queue
 */
void cleanUp(const int shmid, const int msqid, void *sharedMemPtr) {
    //detach from shared mem
    printf("Detaching from shared memory\n");
    shmdt(sharedMemPtr);

    //deallocate the shared memory
    printf("Deallocating the shared memory\n");
    shmctl(shmid, IPC_RMID, NULL);

    //deallocate the message queue
    printf("Deallocate the message queue\n");
    msgctl(msqid, IPC_RMID, NULL);
}

/**
 * Handles the exit signal
 * @param signal - the signal type
 */
void ctrlCSignal(int signal) {
    /* Free system V resources */
    cleanUp(shmid, msqid, sharedMemPtr);
}

// we're not putting the params here because we're not using them
int main() {
    //Install a signal handler
    signal(SIGINT, ctrlCSignal);

    /* Initialize */
    init(shmid, msqid, sharedMemPtr);

    /* Go to the main loop */
    mainLoop();

    //detach from shared memory segment and deallocate shared memory and message queue
    cleanUp(shmid, msqid, sharedMemPtr);

    return 0;
}

