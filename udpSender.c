#include <stdio.h>      
#include <sys/socket.h> 
#include <arpa/inet.h>  
#include <stdlib.h>     
#include <string.h>     
#include <unistd.h>     
#include <errno.h>      
#include <signal.h>     
#include <fcntl.h>
#define DATALIMIT       511     
#define TIMEOUT_SECS    3       
#define DATAMSG         1       
#define TEARDOWNMSG     4       
#define MAXTRIES        16      
FILE *file;
char* dataBuffer;
unsigned long fileLen;

void ReadFile()
{
    	file = fopen("temp.mp4", "rb");
	//temp=fopen("new.mp4","wb+");
    if (!file)
	{
		fprintf(stderr, "can't open file %s", "1.m4v");
		exit(1);
	}

	fseek(file, 0, SEEK_END);
	fileLen=ftell(file);
	fseek(file, 0, SEEK_SET);

	dataBuffer=(char *)malloc(fileLen+1);

	if (!dataBuffer)
	{
		fprintf(stderr, "Memory error!");
        fclose(file);
		exit(1);
	}
	printf("%ld",fileLen);
	fread(dataBuffer, fileLen, 1, file);
    	//fwrite(buffer,fileLen,1,temp);
	//for(int i=0;i<fileLen;i++)
	//printf ("value is %c \n", buffer[i]);
	//free(buffer);
	fclose(file);
    }

//char* dataBuffer = "temp";

int tries=0;   /* Count of times sent - GLOBAL for signal-handler access */

void DieWithError(char *errorMessage);   
void CatchAlarm(int ignored);            


struct segmentPacket {
    int type;
    int seq_no;
    int length;
    char data[512];
};

struct ACKPacket {
    int type;
    int ack_no;
};


struct segmentPacket createDataPacket (int seq_no, int length, char* data);
struct segmentPacket createTerminalPacket (int seq_no, int length);


int main(int argc, char *argv[])
{
    ReadFile();
    int sock;                               
    struct sockaddr_in recievingServer;     
    struct sockaddr_in fromAddr;            
    unsigned short recievingServerPort;     
    unsigned int fromSize;                  
    struct sigaction myAction;              
    char *servIP;                           
    int respStringLen;                      

    
    int chunkSize;                        
    int windowSize;                       
    int tries =0;                         

    if (argc != 5)   
    {
        fprintf(stderr,"Usage: %s <Server IP> <Server Port> <Chunk Size> <Window Size>\n You gave %d Arguments\n", argv[0], argc);
        exit(1);
    }

   
    servIP = argv[1];                          
    recievingServerPort = atoi(argv[2]);       
    chunkSize = atoi(argv[3]);                 
    windowSize = atoi(argv[4]);                

    
    printf("Attempting to Send to: \n");
    printf("IP:          %s\n", servIP);
    printf("Port:        %d\n", recievingServerPort);

    
    if(chunkSize > DATALIMIT){
        fprintf(stderr, "Error: Chunk Size is too large. Must be < 512 bytes\n");
        exit(1);
    }

    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");


    
    memset(&recievingServer, 0, sizeof(recievingServer));    
    recievingServer.sin_family = AF_INET;
    recievingServer.sin_addr.s_addr = inet_addr(servIP); 
    recievingServer.sin_port = htons(recievingServerPort);       


    
    int dataBufferSize = fileLen;//strlen(dataBuffer);
    int numOfSegments = dataBufferSize / chunkSize;
    printf("DataBufferSize %d",dataBufferSize);
    printf("numOfSegments %d, dataBufferSize %d,chunkSize %d\n",numOfSegments,dataBufferSize,chunkSize);
    /* Might have left overs */
    if (strlen(dataBuffer) % chunkSize > 0){
        numOfSegments++;
    }


    int base = -1;           
    int seqNumber = 0;      
    int dataLength = 0;     

    
    printf("Window Size: %d\n", windowSize);
    printf("Chunk Size:  %d\n", chunkSize);
    printf("Chunks:      %d\n", numOfSegments);

    
    myAction.sa_handler = CatchAlarm;
    if (sigemptyset(&myAction.sa_mask) < 0) 
        DieWithError("sigfillset() failed");
    myAction.sa_flags = 0;

    if (sigaction(SIGALRM, &myAction, 0) < 0)
        DieWithError("sigaction() failed for SIGALRM");

    
    int noTearDownACK = 1;
    while(noTearDownACK){

     
        while(seqNumber <= numOfSegments && (seqNumber - base) <= windowSize){
            struct segmentPacket dataPacket;

            if(seqNumber == numOfSegments)
            {
               
                dataPacket = createTerminalPacket(seqNumber, 0);
                printf("Sending Terminal Packet\n");
            }
            else 
            {
                /* Create Data Packet Struct */
                char seg_data[chunkSize];
                strncpy(seg_data, (dataBuffer + seqNumber*chunkSize), chunkSize);

                dataPacket = createDataPacket(seqNumber, dataLength, seg_data);
                printf("Sending Packet: %d\n", seqNumber);
                //printf("Chunk: %s\n", seg_data);
            }

            
            if (sendto(sock, &dataPacket, sizeof(dataPacket), 0,
                 (struct sockaddr *) &recievingServer, sizeof(recievingServer)) != sizeof(dataPacket))
                DieWithError("sendto() sent a different number of bytes than expected");
            seqNumber++;
        }


        
        alarm(TIMEOUT_SECS);      

        
        printf("Window full: waiting for ACKs\n");

        
        struct ACKPacket ack;
        while ((respStringLen = recvfrom(sock, &ack, sizeof(ack), 0,
               (struct sockaddr *) &fromAddr, &fromSize)) < 0)
        {
            if (errno == EINTR)     
            {
              
                seqNumber = base + 1;

                printf("Timeout: Resending\n");
                if(tries >= 10){
                    printf("Tries exceeded: Closing\n");
                    exit(1);
                }
                else {
                    alarm(0);

                    while(seqNumber <= numOfSegments && (seqNumber - base) <= windowSize){
                        struct segmentPacket dataPacket;

                        if(seqNumber == numOfSegments){
                            
                            dataPacket = createTerminalPacket(seqNumber, 0);
                            printf("Sending Terminal Packet\n");
                        } else {
                            
                            char seg_data[chunkSize];
                            strncpy(seg_data, (dataBuffer + seqNumber*chunkSize), chunkSize);

                            dataPacket = createDataPacket(seqNumber, dataLength, seg_data);
                            printf("Sending Packet: %d\n", seqNumber);
                            
                        }

                        
                        if (sendto(sock, &dataPacket, sizeof(dataPacket), 0,
                             (struct sockaddr *) &recievingServer, sizeof(recievingServer)) != sizeof(dataPacket))
                            DieWithError("sendto() sent a different number of bytes than expected");
                        seqNumber++;
                    }
                    alarm(TIMEOUT_SECS);
                }
                tries++;
            }
            else
            {
                DieWithError("recvfrom() failed");
            }
        }

        
        if(ack.type != 8){
            printf("----------------------- Recieved ACK: %d\n", ack.ack_no);
            if(ack.ack_no > base){
               
                base = ack.ack_no;
            }
        } else {
            printf("Recieved Terminal ACK\n");
            noTearDownACK = 0;
        }


        alarm(0);
        tries = 0;

    }


    close(sock);
    exit(0);
}

void CatchAlarm(int ignored)     /* Handler for SIGALRM */
{
    //printf("In Alarm\n");
}


void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}

struct segmentPacket createDataPacket (int seq_no, int length, char* data){

    struct segmentPacket pkt;

    pkt.type = 1;
    pkt.seq_no = seq_no;
    pkt.length = length;
    memset(pkt.data, 0, sizeof(pkt.data));
    strcpy(pkt.data, data);

    return pkt;
}

struct segmentPacket createTerminalPacket (int seq_no, int length){

    struct segmentPacket pkt;

    pkt.type = 4;
    pkt.seq_no = seq_no;
    pkt.length = 0;
    memset(pkt.data, 0, sizeof(pkt.data));

    return pkt;
}
