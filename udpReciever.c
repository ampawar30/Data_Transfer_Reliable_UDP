#include <stdio.h>      
#include <sys/socket.h> 
#include <arpa/inet.h>  
#include <stdlib.h>     
#include <string.h>     
#include <unistd.h>     
#include<fcntl.h>
#define ECHOMAX 50000     

void DieWithError(char *errorMessage);                      
struct ACKPacket createACKPacket (int ack_type, int base);  
int is_lost(float loss_rate);                               



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

int main(int argc, char *argv[])
{
    int sock;                        
    struct sockaddr_in echoServAddr; 
    struct sockaddr_in echoClntAddr; 
    unsigned int cliAddrLen;         
    char echoBuffer[ECHOMAX];        
    unsigned short echoServPort;     
    int recvMsgSize;                 
    int chunkSize;                   
    float loss_rate = 0;             


    srand48(2345);

    if (argc < 3 || argc > 4)         
    {
        fprintf(stderr,"Usage:  %s <UDP SERVER PORT> <CHUNK SIZE> [<LOSS RATE>]\n", argv[0]);
        exit(1);
    }

    lues */
    echoServPort = atoi(argv[1]);  /* First arg:  local port */
    chunkSize = atoi(argv[2]);  /* Second arg:  size of chunks */


    if(argc == 4){
        loss_rate = atof(argv[3]);

    }

    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");

    memset(&echoServAddr, 0, sizeof(echoServAddr));   
    echoServAddr.sin_family = AF_INET;               
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    echoServAddr.sin_port = htons(echoServPort);     

    
    if (bind(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
        DieWithError("bind() failed");

    /* Initialize variables to their needed start values */
   // char dataBuffer[1000000];3893212
   char *dataBuffer;
   dataBuffer=(char *)malloc(3893212+1);
   if (!dataBuffer)
        {
            fprintf(stderr, "Memory error!");
            exit(1);
        }
    int base = -2;
    int seqNumber = 0;

    for (;;) 
    {
        
        cliAddrLen = sizeof(echoClntAddr);

        
        struct segmentPacket dataPacket;

        
        struct ACKPacket ack;

        
        if ((recvMsgSize = recvfrom(sock, &dataPacket, sizeof(dataPacket), 0,
            (struct sockaddr *) &echoClntAddr, &cliAddrLen)) < 0)
            DieWithError("recvfrom() failed");

            seqNumber = dataPacket.seq_no;

        if(!is_lost(loss_rate)){
        
            if(dataPacket.seq_no == 0 && dataPacket.type == 1)
            {
                printf("Recieved Initial Packet from %s\n", inet_ntoa(echoClntAddr.sin_addr));
                memset(dataBuffer, 0, sizeof(dataBuffer));
                strcpy(dataBuffer, dataPacket.data);
                base = 0;
                ack = createACKPacket(2, base);
            } else if (dataPacket.seq_no == base + 1)
            {
                
                printf("Recieved  Subseqent Packet #%d\n", dataPacket.seq_no);
                strcat(dataBuffer, dataPacket.data);
                base = dataPacket.seq_no;
                ack = createACKPacket(2, base);
            } else if (dataPacket.type == 1 && dataPacket.seq_no != base + 1)
            {
                
                printf("Recieved Out of Sync Packet #%d\n", dataPacket.seq_no);
                
                ack = createACKPacket(2, base);
            }

            
            if(dataPacket.type == 4 && seqNumber == base ){
                base = -1;
                
                ack = createACKPacket(8, base);
            }

            
            if(base >= 0){
                printf("------------------------------------  Sending ACK #%d\n", base);
                if (sendto(sock, &ack, sizeof(ack), 0,
                     (struct sockaddr *) &echoClntAddr, sizeof(echoClntAddr)) != sizeof(ack))
                    DieWithError("sendto() sent a different number of bytes than expected");
            } else if (base == -1) {
                printf("Recieved Teardown Packet\n");

                if (sendto(sock, &ack, sizeof(ack), 0,
                     (struct sockaddr *) &echoClntAddr, sizeof(echoClntAddr)) != sizeof(ack))
                    DieWithError("sendto() sent a different number of bytes than expected");
            }


            if(dataPacket.type == 4 && base == -1){
                printf("\n MESSAGE RECIEVED\n %s\n\n", dataBuffer);
               int fd;
               if((fd=open("output.mp4",O_CREAT|O_WRONLY|O_APPEND,0644))==-1)
                       {
                    fprintf(stdout,"open fail");
                        return EXIT_FAILURE;
                        }
                printf("%s",dataBuffer);
                write(fd,dataBuffer,3893212);
               //3331276
               //temp=fopen("output.txt","wb+");
               //fwrite(dataBuffer,3893212,1,temp);
               //printf("bats\n\n\n\n\n\n\n");
               //fseek(temp,sizeof(dataBuffer),SEEK_SET);
               close(fd);
               // memset(dataBuffer, 0, sizeof(dataBuffer));
               free(dataBuffer);
               break;
            }
            
        } else {
            printf("SIMULATED LOSE\n");
        }

    }

     /*FILE *temp;
     temp=fopen("output.mp4","wb+");
     fwrite(dataBuffer,3331276+1,1,temp);
     fclose(temp);
      NOT REACHED */
}


void DieWithError(char *errorMessage)
{
    perror(errorMessage);
    exit(1);
}


struct ACKPacket createACKPacket (int ack_type, int base){
        struct ACKPacket ack;
        ack.type = ack_type;
        ack.ack_no = base;
        return ack;
}

/* The given lost rate function */
int is_lost(float loss_rate) {
    double rv;
    rv = drand48();
    if (rv < loss_rate)
    {
        return(1);
    } else {
        return(0);
    }
}
