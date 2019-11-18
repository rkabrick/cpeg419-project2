/* udp_server.c */
/* Programmed by Adarsh Sethi */
/* November 14, 2019 */

#include <ctype.h>          /* for toupper */
#include <stdio.h>          /* for standard I/O functions */
#include <stdlib.h>         /* for exit */
#include <string.h>         /* for memset */
#include <sys/socket.h>     /* for socket, sendto, and recvfrom */
#include <netinet/in.h>     /* for sockaddr_in */
#include <unistd.h>         /* for close */
#include <errno.h>          // for timeout
#include <time.h>           // for random time seed
#include <math.h>           // for time calculation

#define STRING_SIZE 1024
#define BUFFER_SIZE 80

/* SERV_UDP_PORT is the port number on which the server listens for
   incoming messages from clients. You should change this to a different
   number to prevent conflicts with others in the class. */

#define SERV_UDP_PORT 6680

struct udp_pkt {
  uint16_t len;
  uint16_t seq;
  char data[80];
};

//generate random number to simulate packet loss
int simulateLoss(float packetLoss){
  double randnum = (double)rand() / (double)RAND_MAX;
  if(randnum < packetLoss){
    return 1;
  }
  return 0;
}

int main(int argc, char *argv[]) {

  int input_timeout;
  float input_packetLoss;

  if( argc < 3 ){ //print help text
    printf("Usage: ./udpserver <timeout> <packet loss ratio>\n");
    printf("       Timeout:           An integer 1-10 seconds to determine how long to wait for an ACK\n");
    printf("       Packet Loss Ratio: A real number 0-1 to determine what percent of packets are \"lost\"\n\n");
    exit(1);
  }
  if(sscanf (argv[1], "%i", &input_timeout) != 1) { //parse timeout input
    fprintf(stderr, "Error reading timeout value");
    exit(1);
  }
  if(sscanf (argv[2], "%f", &input_packetLoss)!=1) { //parse loss ratio input
    fprintf(stderr, "Error reading packet loss value");
    exit(1);
  }  

  printf("Starting server with timeout: %d, and packet loss ratio: %0.1f\n\n", input_timeout, input_packetLoss);

  // Use current time as seed for random generator 
  srand(time(0)); 

  int sock_server;  /* Socket on which server listens to clients */

  struct sockaddr_in server_addr;  /* Internet address structure that
                                        stores server address */
  unsigned short server_port;  /* Port number used by server (local port) */

  struct sockaddr_in client_addr;  /* Internet address structure that
                                        stores client address */
  unsigned int client_addr_len;  /* Length of client address structure */

  int bytes_sent, bytes_recd; /* number of bytes sent or received */

  /* open a socket */

  if ((sock_server = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    perror("Server: can't open datagram socket\n");
    exit(1);
  }

  /* initialize server address information */

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = htonl (INADDR_ANY);  /* This allows choice of
                                        any host interface, if more than one
                                        are present */
  server_port = SERV_UDP_PORT; /* Server will listen on this port */
  server_addr.sin_port = htons(server_port);

  /* bind the socket to the local server port */

  if (bind(sock_server, (struct sockaddr *) &server_addr, sizeof (server_addr)) < 0) {
    perror("Server: can't bind to local address\n");
    close(sock_server);
    exit(1);
  }

  /* wait for incoming messages in an indefinite loop */

  printf("I am here to listen ... on port %hu\n\n", server_port);

  client_addr_len = sizeof (client_addr);

  struct udp_pkt recvpkt;
  bytes_recd = recvfrom(sock_server, &recvpkt, sizeof(recvpkt), 0, (struct sockaddr *) &client_addr, &client_addr_len);
  //make sure receive was good
  if(bytes_recd < 0){
    perror("Filename data receive error");
    exit(1);
  }

  //copy filename string out of received packet
  unsigned int filename_len = ntohs(recvpkt.len);
  char filename[filename_len];
  strncpy(filename, recvpkt.data, filename_len); 
  filename[filename_len] = '\0'; //add null terminator to string

  printf("Requested filename: %s\n\n", filename);

  //setup socket timeout
  double micros = pow(10, input_timeout);
  int millis = micros/1000; //make sure to not exceed limits on int value
  struct timeval tv;
  tv.tv_sec = millis/1000;
  tv.tv_usec = millis%1000;
  setsockopt(sock_server, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

  //track statistics
  int numPacketsGenerated = 0;
  int numBytesGenerated = 0;
  int numPacketsTransmitted = 0;
  int numPacketsDropped = 0;
  int numPacketsSuccess = 0;
  int numAcksReceived = 0;
  int numTimeoutExpired = 0;
  
  //connection variables
  unsigned short sequenceNum = 0;
  FILE *fp = NULL;
  fp = fopen(filename, "r");

  if(fp){
    char *buff = calloc(1,BUFFER_SIZE);

    while(fgets(buff,BUFFER_SIZE,fp) != NULL){
      /* prepare the message to send */
      int buffLen = strlen(buff);

      struct udp_pkt newpkt;
      newpkt.len = htons(buffLen);
      newpkt.seq = htons(sequenceNum);
      strncpy(newpkt.data, buff, buffLen);

      printf("Packet %d generated for transmission with %d data bytes\n", sequenceNum, buffLen);
      numPacketsGenerated++;
      numBytesGenerated += buffLen;
      
      //loop transmit until ACK received
      int ack_recvd = 0;

      while( !ack_recvd ){
        numPacketsTransmitted++;
        if(!simulateLoss(input_packetLoss)){
          //send data packet
          bytes_sent = sendto(sock_server, &newpkt, buffLen+4, 0, (struct sockaddr*) &client_addr, client_addr_len);
          if(bytes_sent < 0){
            perror("Data Packet sending error");
            exit(1);
          }

          //print stats
          printf("Packet %d successfully transmitted with %d data bytes\n", sequenceNum, bytes_sent-4);
          numPacketsSuccess++;
        }
        else{
          //print stats
          printf("Packet %d lost\n", sequenceNum);
          numPacketsDropped++;
        }

        //clear buffer
        memset(buff, 0, BUFFER_SIZE);

        //wait for ACK packet
        uint16_t recv_ack;
        bytes_recd = recvfrom(sock_server, &recv_ack, sizeof(recv_ack), 0, (struct sockaddr *) 0, (int *) 0);
        if(bytes_recd < 0){
          if( errno == EAGAIN || errno == EWOULDBLOCK ){
            //print stats
            printf("Timeout expired for packet numbered %d\n", sequenceNum);
            printf("Packet %d generated for re-transmission with %d data bytes\n", sequenceNum, buffLen);
            numTimeoutExpired++;
          }
          else{
            perror("ACK Packet receive error");
            exit(1);
          }
        }
        else{
          //Successfully received ACK
          recv_ack = ntohs(recv_ack);
          if( recv_ack == sequenceNum ){ //verify sequence number
            printf("ACK %d received\n", recv_ack);
            numAcksReceived++;
            sequenceNum = 1 - sequenceNum;
            ack_recvd = 1;
          }  
        } 
      }
    }
    //free memory to cleanup
    free(buff);
    fclose(fp);
  }
  else{
    //Print reason why file couldn't be opened
    perror("Error opening file");

    //send file not found message packet
    char notFoundMsg[16] = "FILE NOT FOUND\n";
    int buffLen = 16;
    
    struct udp_pkt newpkt;
    newpkt.len = htons(buffLen);
    newpkt.seq = htons(sequenceNum);
    strncpy(newpkt.data, notFoundMsg, buffLen);

    bytes_sent = sendto(sock_server, &newpkt, buffLen+4, 0, (struct sockaddr*) &client_addr, client_addr_len);
    if(bytes_sent < 0){
      perror("Not Found Packet sending error");
      exit(1);
    }

    //print stats
    printf("Packet %d transmitted with %d data bytes\n", sequenceNum, bytes_sent-4);
    sequenceNum = 1 - sequenceNum;
  }

  //Send end of transmission packet
  struct udp_pkt eotpkt;
  eotpkt.len = htons(0);
  eotpkt.seq = htons(sequenceNum);

  bytes_sent = sendto(sock_server, &eotpkt, 4, 0, (struct sockaddr*) &client_addr, client_addr_len);
  if(bytes_sent < 0){
    perror("EOT Packet sending error");
    exit(1);
  }

  //print stats
  printf("\nEnd of transmission packet with sequence number %d transmitted\n", sequenceNum);

  //Print total transmission statistics
  printf("\n---Server Statistics---\n");
  printf("Number of data packets generated for transmission: %d\n", numPacketsGenerated);
  printf("Number of data bytes generated for transmission: %d\n", numBytesGenerated);
  printf("Number of packets generated for retransmission: %d\n", numPacketsTransmitted);
  printf("Number of data packets dropped due to loss: %d\n", numPacketsDropped);
  printf("Number of data packets transmitted successfully: %d\n", numPacketsSuccess);
  printf("Number of ACKs received: %d\n", numAcksReceived);
  printf("Number of timeouts expired: %d\n", numTimeoutExpired);
  

  /* close the socket */
  close(sock_server);
  exit(0);
}
