/* udp_client.c */ 
/* Programmed by Theo Fleck */
/* November 14, 2019 */

#include <stdio.h>          /* for standard I/O functions */
#include <stdlib.h>         /* for exit */
#include <string.h>         /* for memset, memcpy, and strlen */
#include <netdb.h>          /* for struct hostent and gethostbyname */
#include <sys/socket.h>     /* for socket, sendto, and recvfrom */
#include <netinet/in.h>     /* for sockaddr_in */
#include <unistd.h>         /* for close */
#include <errno.h>          // for timeout
#include <time.h>           // for random time seed

#define STRING_SIZE 1024

#define SERVER_HOSTNAME "127.0.0.1"
#define SERVER_PORT 6680

struct udp_pkt {
  uint16_t len;
  uint16_t seq;
  char data[80];
};

//generate random number to simulate ACK loss
int simulateACKLoss(float ackLoss){
  double randnum = (double)rand() / (double)RAND_MAX;
  if(randnum < ackLoss){
    return 1;
  }
  return 0;
}

int main(int argc, char *argv[]) {

  char filename[STRING_SIZE];
  float input_ackLoss;
  
  if( argc < 3 ){ //print help text
    printf("Usage: ./udpclient <file name> <ack loss ratio>\n");
    printf("       File Name:      Name of file to retrieve from the server, with extension\n");
    printf("       ACK Loss Ratio: A real number 0-1 to determine what percent of ACks are \"lost\"\n\n");
    exit(1);
  }
  if(strlen(argv[1]) >= STRING_SIZE) { //parse filename input
    fprintf(stderr, "Filename is too long");
    exit(1);
  }
  strcpy(filename, argv[1]);

  if(sscanf (argv[2], "%f", &input_ackLoss)!=1) { //parse loss ratio input
    fprintf(stderr, "Error reading ACK loss value");
    exit(1);
  }

  printf("Connecting to server to retrieve filename: %s, with ACK loss ratio: %0.1f\n\n", filename, input_ackLoss);

  // Use current time as seed for random number generator 
  srand(time(0)); 

  int sock_client;  /* Socket used by client */ 

  struct sockaddr_in client_addr;  /* Internet address structure that
                                        stores client address */
  unsigned short client_port;  /* Port number used by client (local port) */

  struct sockaddr_in server_addr;  /* Internet address structure that
                                        stores server address */
  struct hostent * server_hp;      /* Structure to store server's IP
                                        address */

  int bytes_sent, bytes_recd; /* number of bytes sent or received */

  /* open a socket */

  if ((sock_client = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
    perror("Client: can't open datagram socket\n");
    exit(1);
  }

  /* Note: there is no need to initialize local client address information
            unless you want to specify a specific local port.
            The local address initialization and binding is done automatically
            when the sendto function is called later, if the socket has not
            already been bound. 
            The code below illustrates how to initialize and bind to a
            specific local port, if that is desired. */

  /* initialize client address information */

  client_port = 0;   /* This allows choice of any available local port */

  /* Uncomment the lines below if you want to specify a particular 
             local port: */
  /*
   printf("Enter port number for client: ");
   scanf("%hu", &client_port);
   */

  /* clear client address structure and initialize with client address */
  memset(&client_addr, 0, sizeof(client_addr));
  client_addr.sin_family = AF_INET;
  client_addr.sin_addr.s_addr = htonl(INADDR_ANY); /* This allows choice of
                                        any host interface, if more than one 
                                        are present */
  client_addr.sin_port = htons(client_port);

  /* bind the socket to the local client port */

  if (bind(sock_client, (struct sockaddr *) &client_addr,
           sizeof (client_addr)) < 0) {
    perror("Client: can't bind to local address\n");
    close(sock_client);
    exit(1);
  }

  /* end of local address initialization and binding */

  /* initialize server address information */

  if ((server_hp = gethostbyname(SERVER_HOSTNAME)) == NULL) {
    perror("Client: invalid server hostname\n");
    close(sock_client);
    exit(1);
  }

  /* Clear server address structure and initialize with server address */
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  memcpy((char *)&server_addr.sin_addr, server_hp->h_addr, server_hp->h_length);
  unsigned short server_port = SERVER_PORT;
  server_addr.sin_port = htons(server_port);

  /* send filename */
  unsigned int sequenceNum = 0;
  unsigned int buffLen = strlen(filename);

  struct udp_pkt newpkt;
  newpkt.len = htons(buffLen);
  newpkt.seq = htons(sequenceNum);
  strncpy(newpkt.data, filename, buffLen);

  bytes_sent = sendto(sock_client, &newpkt, buffLen+4, 0, (struct sockaddr *) &server_addr, sizeof (server_addr));
  if(bytes_sent < 0){
    perror("Filename Packet sending error");
    exit(1);
  }
  
  //track statistics
  int numPacketsReceived = 0;
  int numDuplicatePackets = 0;
  int numPacketsNoDups = 0;
  int numBytesDelivered = 0;
  int numAcksTransmitted = 0;
  int numAcksDropped = 0;
  int numAcksGenerated = 0;

  //connection variables
  int notEot = 1;
  FILE *fp = fopen("out.txt", "wb");

  //Receive in a loop until end of transmission received
  while(notEot){
    /* get response from server */

    struct udp_pkt recvpkt;
    bytes_recd = recvfrom(sock_client, &recvpkt, sizeof(recvpkt), 0, (struct sockaddr *) 0, (int *) 0);
    if(bytes_recd < 0){
      perror("Data Packet receive error");
      exit(1);
    }
    unsigned int len = ntohs(recvpkt.len);
    unsigned int seq = ntohs(recvpkt.seq);

    if(len == 0){ //check for EOT packet
      printf("\nEnd of Transmission Packet with sequence number %d received\n", seq);
      notEot = 0;
    }
    else {
      numPacketsReceived++;
      if(sequenceNum == seq){
        //print stats
        printf("Packet %d received with %d data bytes\n", seq, bytes_recd-4);
        numPacketsNoDups++;
        numBytesDelivered += len;
        //write received data to file
        if(fp){
          fwrite(recvpkt.data,1,len, fp);
          printf("Packet %d delivered to user\n", seq);
        }
        else{
          perror("Error writing to file");
        }
        sequenceNum = 1 - sequenceNum;
      }
      else{
        printf("Duplicate packet %d received with %d data bytes\n", seq, bytes_recd-4);
        numDuplicatePackets++;
      }
      
      //send ACK
      printf("ACK %d generated for transmission\n", seq);
      numAcksGenerated++;
      if(!simulateACKLoss(input_ackLoss)){
        uint16_t ack_seq = htons(seq);        
        bytes_sent = sendto(sock_client, &ack_seq, sizeof(ack_seq), 0, (struct sockaddr *) &server_addr, sizeof (server_addr));
        if(bytes_sent < 0){
          perror("ACK Packet sending error");
          exit(1);
        }
        printf("ACK %d successfully transmitted\n", seq);
        numAcksTransmitted++;
      }
      else{
        printf("ACK %d lost\n", seq);
        numAcksDropped++;
      }

    }
  }
  if(fp){
    fclose(fp); //close output file
  }

  //print total transmission statistics
  printf("\n---Client Statistics---\n");
  printf("Number of data packets received successfully: %d\n", numPacketsReceived);
  printf("Number of duplicate data packets received: %d\n", numDuplicatePackets);
  printf("Number of data packets received successfully (w/o duplicates): %d\n", numPacketsNoDups);
  printf("Number of data bytes delivered to user: %d\n", numBytesDelivered);
  printf("Number of ACKs transmitted successfully: %d\n", numAcksTransmitted);
  printf("Number of ACKs generated, but dropped due to loss: %d\n", numAcksDropped);
  printf("Number of ACKs generated: %d\n", numAcksGenerated);

  /* close the socket */
  close(sock_client);
}
