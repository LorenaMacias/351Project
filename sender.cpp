#include <sys/shm.h>
#include <sys/msg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "msg.h" /* For the message struct */

/* The size of the shared memory chunk */
#define SHARED_MEMORY_CHUNK_SIZE 1000

/* The ids for the shared memory segment and the message queue */
int shmid, msqid;

key_t key;
/* The pointer to the shared memory */
void* sharedMemPtr;
int ipc = IPC_CREAT | 0666;

/**
 * Sets up the shared memory segment and message queue
 * @param shmid - the id of the allocated shared memory
 * @param msqid - the id of the shared memory
 */
void init(int& shmid, int& msqid, void*& sharedMemPtr) {
    //keyfile.txt containing 'Hello World' has been manually created

    //generates the key
    key_t key = ftok("keyfile.txt", 'a');

    //IPC_CREAT | 0666 is for a server
    shmid = shmget(key, SHARED_MEMORY_CHUNK_SIZE, ipc);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }
    else{
	(void) fprintf(stderr, "shmget: shmget returned %d\n", shmid);
    }
    //shored memory ID is now stoed in shmid
 
    //attach pointer to shared memory location     
    if ((sharedMemPtr = shmat(shmid, NULL, 0)) == (char*)-1) {
        perror("shmat");
        exit(1);
    }

    //Sets the first 1000 bytes of the block of memory pointed by sharedMemPtr to 0 
    memset(sharedMemPtr, 0, SHARED_MEMORY_CHUNK_SIZE);

    //attach to the message queue
    // Store the IDs and the pointer to the shared memory region in the corresponding parameters
    if ((msqid = msgget(key, 0666)) < 0) {
        perror("msgget");
        exit(1);
    }
}

/**
 * Performs the cleanup functions
 * @param sharedMemPtr - the pointer to the shared memory
 * @param shmid - the id of the shared memory segment
 * @param msqid - the id of the message queue
 */

void cleanUp(const int& shmid, const int& msqid, void* sharedMemPtr) {
    //Detach from shared memory
    printf("Detaching from shared memory\n");
    shmdt(sharedMemPtr);
}

/**
 * The main send function
 * @param fileName - the name of the file
 */
void send(const char* fileName) {
    /* Open the file for reading */
    FILE* fp = fopen(fileName, "r");

    const int size_of_each_message = sizeof(struct message) - sizeof(long);

    /* A buffer to store message we will send to the receiver. */
    struct message sndMsg;
    sndMsg.mtype = SENDER_DATA_TYPE;

    /* A buffer to store message received from the receiver. */
    struct message rcvMsg;
    rcvMsg.mtype = RECV_DONE_TYPE;

    /* Was the file open? */
    if (!fp) 
    {
	perror("fopen");
        exit(-1);
    }

    /* Read the whole file */
    while (!feof(fp)) {
        /* Read at most SHARED_MEMORY_CHUNK_SIZE from the file and store them in shared memory. 
 		 * fread will return how many bytes it has actually read (since the last chunk may be less
 		 * than SHARED_MEMORY_CHUNK_SIZE).
 		 */
        if((sndMsg.size = fread(sharedMemPtr, sizeof(char), SHARED_MEMORY_CHUNK_SIZE, fp)) < 0)
	{
		perror("fread");
		exit(-1);
	}
		
	//Send a message to the receiver telling him that the data is ready
	if (msgsnd(msqid, &sndMsg, size_of_each_message, 0) < 0){ 
            perror("msgsnd");
            exit(1);
        }
	printf("Message sent successfully \n");
        //Wait until the receiver sends us a message of type RECV_DONE_TYPE telling us
        if (msgrcv(msqid, &rcvMsg, size_of_each_message, rcvMsg.mtype, 0) < 0) {
            perror("msgrcv");
            exit(1);
        }
	else{
		printf("Received ack from receiver class   \n");
	}
    }

    //tells reciever that we have nothing more to send
    sndMsg.size = 0;
    msgsnd(msqid, &sndMsg, sizeof(int), 0);

	if( msgsnd(msqid, &sndMsg, sizeof(sndMsg) - sizeof(long), 0) == -1){
            perror("msgsnd");
            exit(1);
        }
        else{
            printf("Notified receiver that we are out of things to send");
        }
    /* Close the file */
    fclose(fp);
}

int main(int argc, char** argv) 
{
	
	/* Check the command line arguments */
	if(argc < 2)
	{
		fprintf(stderr, "USAGE: %s <FILE NAME>\n", argv[0]);
		exit(-1);
	}
		
	/* Connect to shared memory and the message queue */
	init(shmid, msqid, sharedMemPtr);
	
	/* Send the file */
	send(argv[1]);
	
	/* Cleanup */
	cleanUp(shmid, msqid, sharedMemPtr);
		
	return 0;
}

