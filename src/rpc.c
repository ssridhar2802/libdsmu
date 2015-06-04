#include <netdb.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include<arpa/inet.h>
#include<netinet/in.h>
 
#include "b64.h"
#include "mem.h"
#include "rpc.h"
#include "libdsmu.h"
#include "sglib.h"

#define MAX_COPYSETS 10 
// Socket state.
int serverfd;
struct addrinfo *resolvedAddr;
struct addrinfo hints;

int clientid; 
pthread_mutex_t sockl;
 

struct Endpoint {
	char *ip;
	int port;
};

struct Endpoint Endpoints[MAX_COPYSETS];
extern pthread_condattr_t waitca[MAX_SHARED_PAGES];
extern pthread_cond_t waitc[MAX_SHARED_PAGES];
extern pthread_mutex_t waitm[MAX_SHARED_PAGES];

//LOCKCHANGES BEGIN
extern pthread_condattr_t invalwaitca[MAX_SHARED_PAGES];
extern pthread_cond_t invalwaitc[MAX_SHARED_PAGES];
extern pthread_mutex_t invalwaitm[MAX_SHARED_PAGES];

extern pthread_mutex_t pteLock[MAX_SHARED_PAGES];
//LOCKCHANGES END



struct pagetableentry pagetable[MAX_SHARED_PAGES];


struct Endpoint getEndpoint(int id) {
	return Endpoints[id];
}

int clientListenerFD;

int totalSocketsCreated;

char *get_ip_str(const struct sockaddr *sa, char *s, size_t maxlen)
{
    inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
                    s, maxlen);
    return s;
}
/*
uint16_t get_port_no(const struct sockaddr *sa)
{
    return ntohs(((struct sockaddr_in *)sa)->sin_port);
//    return ((struct sockaddr_in *)sa)->sin_port;
}
*/

void initrpc(){
  int i = 0;
  for (i = 0; i < MAX_COPYSETS; i++) {
    Endpoints[i].ip = "127.0.0.1";
    Endpoints[i].port = 8000+i;
  }
  printf("endpoints initialized\n");
}


in_port_t get_in_port(struct sockaddr *sa)
{
        return (((struct sockaddr_in*)sa)->sin_port);
}
 
// Listen for manager messages and dispatch them.
void *listenman(void *ptr) {
  int ret;
  //printf("Listening...\n");
  while (1) {
    // See how long the payload (actualy message) is by peeking.
    char peekstr[20] = {0};
    ret = recv(serverfd, peekstr, 10, MSG_PEEK | MSG_WAITALL);
    if (ret != 10)
      err(1, "Could not peek into next packet's size");
    int payloadlen = atoi(peekstr);
 
    // Compute size of header (the length string).
    int headerlen;
    char *p = peekstr;
    for (headerlen = 1; *p != ' '; p++)
      headerlen++;
 
    // Read the entire unit (header + payload) from the socket.
    char msg[10000] = {0};
    ret = recv(serverfd, msg, headerlen + payloadlen, MSG_WAITALL);
    if (ret != headerlen + payloadlen)
      err(1, "Could not read entire unit from socket");
 
    const char s[] = " ";
    char *payload = strstr(msg, s) + 1;
    dispatch(payload);
  }
}

 
// Handle newly arrived messages.
// client can receive 
int dispatch(char *msg) {
//#ifdef DEBUG
  printf("< %.40s\n", msg);
//#endif  // DEBUG.
  //printf("dispatch : %.40s %d\n", msg, clientid);
  if (strstr(msg, "INVALIDATE_CONFIRMATION")!=NULL) {
    handleinvalidateconfirm(msg);
  } else if (strstr(msg, "INVALIDATE") != NULL) {
    invalidate(msg);
  } else if (strstr(msg, "RESPONSEPAGE") != NULL) {
    handleconfirm(msg);
  } else if (strstr(msg, "REQUESTPAGE") != NULL) {
    handlerequestpage(msg);
  } else {
    printf("Undefined message.\n");
  }
  return 0;
}
 

int sendClient(char *str, int id) {
  struct Endpoint ep = getEndpoint(id);
  //printf("Sending to IP: %s, Port %d\n", ep.ip, ep.port);
  int clientfd = getClientSocketDescription(ep.ip, ep.port);
  int ret;
 
  #ifdef DEBUG
    printf("> %.40s\n", str);
  #endif  // DEBUG
 
  pthread_mutex_lock(&sockl);
  char msg[10000];
  sprintf(msg, "%zu %s", strlen(str), str);
  /* sprintf(msg, "%s", str); */
  //printf("%d sending %.40s to %d\n", clientid, msg, id);

  ret = send(clientfd, msg, strlen(msg), 0);
  if (ret != strlen(msg))
    err(1, "Could not send the message");
  pthread_mutex_unlock(&sockl);
  close(clientfd);
  return 0;
}
 
// Send a message to the manager.
int sendman(char *str) {
  int ret;
//#ifdef DEBUG
  //printf("> %.40s\n", str);
//#endif  // DEBUG
 
  pthread_mutex_lock(&sockl);
  char msg[10000];
  sprintf(msg, "%zu %s", strlen(str), str);
  ret = send(serverfd, msg, strlen(msg), 0);
  if (ret != strlen(msg))
    err(1, "Could not send the message");
  pthread_mutex_unlock(&sockl);
  return 0;
}
 
 
int getClientSocketDescriptor(char *ip, int port) {
   char sport[5];
  snprintf(sport, 5, "%d", port);
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(ip, sport, &hints, &resolvedAddr) < 0) {
    fprintf(stderr, "Could not resolve the IP address provided.\n");
    return -2;
  }
 
  int clientfd = socket(resolvedAddr->ai_family, resolvedAddr->ai_socktype,
                    resolvedAddr->ai_protocol);
  if (clientfd < 0) {
    fprintf(stderr, "Socket file descriptor was invalid.\n");
    freeaddrinfo(resolvedAddr); // Free the address struct we created on error.
    return -2;
 }
 
  if (connect(clientfd, resolvedAddr->ai_addr,
    resolvedAddr->ai_addrlen) < 0) {
    fprintf(stderr, "Could not connect to the IP address provided.\n");
    freeaddrinfo(resolvedAddr);
    return -2;
  }
 
  freeaddrinfo(resolvedAddr); // Done with the address struct, free it.
 
  if (pthread_mutex_init(&sockl, NULL) != 0) {
    return -3;
  }
 
  return clientfd;
} 
 
// Initialize socket with manager.
// Return 0 on success.
int initsocks(char *ip, int port) {
  char sport[5];
  snprintf(sport, 5, "%d", port);
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  if (getaddrinfo(ip, sport, &hints, &resolvedAddr) < 0) {
    fprintf(stderr, "Could not resolve the IP address provided.\n");
    return -2;
  }
 
  serverfd = socket(resolvedAddr->ai_family, resolvedAddr->ai_socktype,
                    resolvedAddr->ai_protocol);
  if (serverfd < 0) {
    fprintf(stderr, "Socket file descriptor was invalid.\n");
    freeaddrinfo(resolvedAddr); // Free the address struct we created on error.
    return -2;
 }
 
  if (connect(serverfd, resolvedAddr->ai_addr,
    resolvedAddr->ai_addrlen) < 0) {
    fprintf(stderr, "Could not connect to the IP address provided.\n");
    freeaddrinfo(resolvedAddr);
    return -2;
  }
 
  freeaddrinfo(resolvedAddr); // Done with the address struct, free it.
 
  if (pthread_mutex_init(&sockl, NULL) != 0) {
    return -3;
  }
 
  return 0;
}
 
// Cleanup sockets.
int teardownsocks(void) {
  if (pthread_mutex_destroy(&sockl) != 0) {
    return -3;
  }
  close(serverfd);
   if (close(clientListenerFD) < 0){
        perror("Could not close successfully\n");
  }else{
	printf("Socket %d closed succesfully\n", clientListenerFD);
  }
  return 0;
}
 
void confirminvalidate(int pgnum, int id) {
  char msg[100] = {0};
  snprintf(msg, 100, "INVALIDATE_CONFIRMATION %d %d", pgnum, clientid);
  //printf("invalidate confirmation:: %s\n ", msg);
  sendClient(msg, id);
}
 
// Return 0 on success.
int requestpage(int pgnum, char *type) {
  //printf("%d sending request %d\n", clientid, pgnum);
  char msg[100] = {0};
  snprintf(msg, 100, "REQUESTPAGE %d %d %s", pgnum, clientid, type);
  printf(">%s\n", msg);
  return sendman(msg);
}

int requestpageowner(int pgnum, char *type) {
  //printf("REQUESTING PAGE : STEP 2\n");
  char msg[100] = {0};
  snprintf(msg, 100, "REQUESTPAGE %d %d %s", pgnum, clientid, type);
  int id = pagetable[pgnum].probOwner;
  return sendClient(msg, id);
}
 
//owner to requestor
void sendresponse(int pgnum, char *pgb64, char *permission, int id) {
  char msg[10000] = {0};
  snprintf(msg, 100 + strlen(pgb64), "RESPONSEPAGE %d %s %d %s", pgnum, permission, clientid, pgb64);
  //printf("sending response to client %d\n", id);
  sendClient(msg, id);
}

void handleinvalidateconfirm(char *msg) {
  char *spgnum = strstr(msg, "ION ") + 4;
  int pgnum = atoi(spgnum);
  char msg1[strlen(msg)+1];
  strcpy(msg1, msg);
  const char delim[2] = " ";
  char* token;
  token = strtok(msg1, delim);

  int id;

  int i = 0;
  while(token != NULL) {
    if (i == 2) {
      id = atoi(token);
    }
    i += 1;
    token = strtok(NULL, delim);
  } 
  
  pagetable[pgnum].copyset[id]=0;
}
 
int handleconfirm(char *msg) {
   printf("spage number : %s\n", strstr(msg, "RESPONSEPAGE ") + 13); 
  char *spgnum = strstr(msg, "RESPONSEPAGE ") + 13;
  /* printf("spage number : %s\n", spgnum); */
  int pgnum = atoi(spgnum);
  //printf("page number : %d\n", pgnum);
  void *pg = (void *)PGNUM_TO_PGADDR((uintptr_t)pgnum);
  //const char delim[2] = " "; 
  int err;

  //char msg1[30];
  //strcpy(msg1, msg);

  int nspaces;
  //char *token;
  

  //token = strtok(msg1, delim);

  int id=-1;

  /*int i = 0;
  while(token != NULL) {
    if (i == 3) {
      id = atoi(token);
    }
    i += 1;
    token = strtok(NULL, delim);
  }*/

 
  // Acquire mutex for condition variable.
  pthread_mutex_lock(&waitm[pgnum % MAX_SHARED_PAGES]);
 
  // If not using existing page, decode the b64 data into the page.  The b64
  // string begins after the 4th ' ' character in msg.
  if (strstr(msg, "EXISTING") == NULL) {
    char *b64str = msg;
    nspaces = 0;
    while (nspaces < 4) {

      if (b64str[0] == ' ') {
      if(nspaces == 2) {
          id = atoi(b64str);
          printf("------------------------------------------------%d\n",id);
      }        
      nspaces++;
	
      }
      b64str++;
    }
     
    //printf("%d handling responsepage!!!! %.40s \n", clientid, msg);
    /* printf("b64str value %s\n", b64str); */

    char b64data[7000];
    if (b64decode(b64str, b64data) < 0) {
      fprintf(stderr, "Failure decoding b64 string");
      return -1;
    }

    //printf("After decoding", b64data); 
    //fprintf(stderr, "got page %d\n", pgnum);
    // memcpy -- must set to write first to fill in page!
    if ((err = mprotect(pg, 1, (PROT_READ|PROT_WRITE))) != 0) {
      fprintf(stderr, "permission setting of page addr %p failed with error %d\n", pg, err);
      return -1;
    }
    if (memcpy(pg, b64data, PG_SIZE) == NULL) {
      fprintf(stderr, "memcpy failed.\n");
      return -1;
    }
  }
 
  if (strstr(msg, "WRITE") == NULL) {
    if ((err = mprotect(pg, 1, PROT_READ)) != 0) {
      fprintf(stderr, "permission setting of page addr %p failed with error %d\n", pg, err);
      return -1;
    }
  } else {
    if ((err = mprotect(pg, 1, PROT_READ|PROT_WRITE)) != 0) {
      fprintf(stderr, "permission setting of page addr %p failed with error %d\n", pg, err);
      return -1;
    }
  }
 
  // Signal to the page handler that they can now run.
  pthread_cond_signal(&waitc[pgnum % MAX_SHARED_PAGES]);
 
  // Unlock to allow another page fault to be handled.
  pthread_mutex_unlock(&waitm[pgnum % MAX_SHARED_PAGES]);
 
  char reply[20];

  //printf("here..................%d\n", id); 
  sprintf(reply, "CONFIRMATION %d", pgnum);
  if(id==-1)
	sendman(reply);
  else
  	sendClient(reply, id);
 
  return 0;
}

void clearCopySet(int pgNum){
        int i =0;
        for(i = 0; i < MAX_COPYSETS; i++)
                pagetable[pgNum%MAX_SHARED_PAGES].copyset[i] = 0;
}

 
int invalidate_copysets(int pg) {
     int i;
     char msg[25];
     snprintf(msg, 25, "INVALIDATE %d %d", pg, clientid);
     //printf("invalidating copy sets: %s\n", msg);
     for (i=0; i<MAX_COPYSETS; i++) {
       if (pagetable[pg].copyset[i] != 0) {
	  //printf("invalidating copyset %d\n", i);
	  sendClient(msg, i);
       }
     }
     i=0;
     //LOCKCHANGES BEGIN
     pthread_mutex_lock(&invalwaitm[pg % MAX_SHARED_PAGES]);
     while(!isCopySetEmpty(pg))
            pthread_cond_wait(&invalwaitc[pg % MAX_SHARED_PAGES],&invalwaitm[pg % MAX_SHARED_PAGES]);
     clearCopySet(pg);
     pthread_mutex_unlock(&invalwaitm[pg % MAX_SHARED_PAGES]);
     //LOCKCHANGES END

     /*while(!isCopySetEmpty(pg)) {
	sleep(1);
     }*/
     return 0;
}

int isCopySetEmpty(int pgnum) {
	int counter =0,i;
	for(i=0; i<MAX_COPYSETS; i++) 
		if (pagetable[pgnum].copyset[i] !=0) 
			counter++;
	if (counter == 0)
		return 1;
	else 
		return 0;
} 

int addToCopySet(int id, int pgnum) {
  pagetable[pgnum].copyset[id]=1;
}

int handlerequestpage(char *msg) {
  //printf("%d receiving request %s in hrp function\n", clientid, msg);
  char msg1[strlen(msg)+1];
  strcpy(msg1, msg);
  const char delim[2] = " ";
  char* token;
  token = strtok(msg1, delim);

  int id;

  int i = 0;
  while(token != NULL) {
    if (i == 2) {
      id = atoi(token);
    }
    i += 1;
    token = strtok(NULL, delim);
  } 

  /* int err; */
  char *spgnum = strstr(msg, " ") +1;
  int pgnum = atoi(spgnum);
  void *pg = (void *) PGNUM_TO_PGADDR((uintptr_t)pgnum);
  int permission = 0;

  char *perm_str;
  if (strstr(msg, "READ") != NULL) {
    //printf("%d is adding to copysets\n", clientid);
    permission = PROT_READ;
    perm_str = "READ";
    addToCopySet(id, pgnum);
  }
  if (strstr(msg, "WRITE") != NULL) {
    permission = PROT_NONE;
    perm_str="WRITE";
    invalidate_copysets(pgnum);
  }
  if (mprotect(pg, 1, PROT_READ) != 0) {
    fprintf(stderr, "Invalidation of page addr %p failed\n", pg);
    return -1;
  }
  char pgb64[PG_SIZE * 2] = {0};
  b64encode((const char *)pg, PG_SIZE, pgb64);
  if (mprotect(pg, 1, permission) != 0) {
    fprintf(stderr, "Invalidation of page addr %p failed\n", pg);
    return -1;
  }
  //printf("sending response\n");
  sendresponse(pgnum, pgb64, perm_str, id);
  return 0;

}

void ownerLogic(int connfd){
    	char recvBuff[1025];
//	time_t ticks; 
//        ticks = time(NULL);
/*        snprintf(sendBuff, sizeof(sendBuff), "%.24s\r\n", ctime(&ticks));
        int n = write(connfd, sendBuff, strlen(sendBuff)); 
	if(n <0){
		perror("Error writing to socket");
		exit(1);
	}
*/
	int n;
    	while ( (n = read(connfd, recvBuff, sizeof(recvBuff)-1)) > 0)
    	{
        recvBuff[n] = 0;
/*        if(fputs(recvBuff, stdout) == EOF)
        {
            printf("\n Error : Fputs error\n");
        }
*/
	//printf("Recv from client : %s\n",recvBuff);
    } 

    if(n < 0)
    {
        printf("\n Read error \n");
    } 

}

void listenClient(int sockfd) {
    struct Endpoint ep = getEndpoint(clientid);
    //printf("DETAILS--------IP: %s, Port %d, clientid %d\n", ep.ip, ep.port, clientid);
    int ret;
    //printf("Listening Client...A\n");
    // See how long the payload (actualy message) is by peeking.
    char peekstr[20] = {0};
    ret = recv(sockfd, peekstr, 10, MSG_PEEK | MSG_WAITALL);
    if (ret != 10)
      err(1, "Could not peek into next packet's size");
    int payloadlen = atoi(peekstr);
    //printf("Listening...B\n");

    // Compute size of header (the length string).
    int headerlen;
    char *p = peekstr;
    for (headerlen = 1; *p != ' '; p++)
      headerlen++;
    //printf("Listening...C\n");

    // Read the entire unit (header + payload) from the socket.
    char msg[10000] = {0};
    ret = recv(sockfd, msg, headerlen + payloadlen, MSG_WAITALL);
    if (ret != headerlen + payloadlen)
      err(1, "Could not read entire unit from socket");
    //printf("Listening...D\n");

    const char s[] = " ";
    char *payload = strstr(msg, s) + 1;
    dispatch(payload);
}

void * initOwnerServer(void *id){
    clientid = *((int *)id);
    //printf("this is the clientid : %d\n", clientid);
    struct Endpoint endpoint = getEndpoint(clientid);
    int portNo = endpoint.port;
    printf("Starting server at %d\n", portNo);
    int connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr cli_addr; 

    clientListenerFD = socket(AF_INET, SOCK_STREAM, 0);
    if (clientListenerFD < 0){
      perror("ERROR opening socket");
      exit(1);
     }
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET; serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);  serv_addr.sin_port = htons(portNo); 
    int yes = 1;
    if ( setsockopt(clientListenerFD, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1 )
    {
    perror("setsockopt");
    }
    if(bind(clientListenerFD, (struct sockaddr*)&serv_addr, sizeof(serv_addr))){
	perror("Error on binding");
	exit(1);
     }
    printf("listening\n");
    listen(clientListenerFD, 10); 

    
    printf("starting while loop !!!\n");

    while(1)
    {
//	struct sockaddr clientAddress;
	socklen_t addrLen = sizeof(cli_addr);
         connfd = accept(clientListenerFD, &cli_addr, &addrLen);
	if(connfd < 0){
		perror("error on accept");
		exit(1);
	}
 	char *addr = (char *) malloc(3000 *sizeof(char));

	addr = get_ip_str((struct sockaddr *)&cli_addr, addr, 3000 * sizeof(char));

	//printf("port is %d,%d,%u,%u\n",ntohs(get_in_port((struct sockaddr *)&cli_addr)),
	//	get_in_port((struct sockaddr *)&cli_addr), ntohs(get_in_port((struct sockaddr *)&cli_addr)),
	//	get_in_port((struct sockaddr *)&cli_addr));
////
	//uint16_t cPortNo = get_port_no((struct sockaddr *)&cli_addr);
	//printf("The client address is %s\n", addr);
	listenClient(connfd);
	//ownerLogic(connfd);
        close(connfd);
//	close(listenfd);
	//client_sock(addr, 8003);
//	exit(0);
	
     }
}


int getClientSocketDescription(char *server_addr, int portNo){
    int sockfd = 0;// n = 0;
    char recvBuff[1024];
    struct sockaddr_in serv_addr; 

    memset(recvBuff, '0',sizeof(recvBuff));
    totalSocketsCreated++;
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
      printf("\n Error : Could not create socket %d\n", totalSocketsCreated);
      return -1;
    } 

    memset(&serv_addr, '0', sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portNo); 
  
    if(inet_pton(AF_INET, server_addr, &serv_addr.sin_addr)<=0)
    {
        printf("\n inet_pton error occured\n");
        return -1;
    } 

    //printf("Sending from: %d to %s:%d\n",clientid, server_addr, portNo);
    if( connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
       printf("\n Error : Connect Failed \n");
       return -1;
    } 
    return sockfd;

}


// Handle invalidate messages.
int invalidate(char *msg) {
  int err;
  char *spgnum = strstr(msg, " ") + 1;
  int pgnum = atoi(spgnum);
  void *pg = (void *)PGNUM_TO_PGADDR((uintptr_t)pgnum);

  char msg1[strlen(msg)+1];
  strcpy(msg1, msg);
  const char delim[2] = " ";
  char* token;
  token = strtok(msg1, delim);

  int id;

  int i = 0;
  while(token != NULL) {
    if (i == 2) {
      id = atoi(token);
    }
    i += 1;
    token = strtok(NULL, delim);
  } 


 
  // If we don't need to reply with a b64-encoding of the page, just invalidate
  // and reply.
    if ((err = mprotect(pg, 1, PROT_NONE)) != 0) {
      fprintf(stderr, "Invalidation of page addr %p failed with error %d\n", pg, err);
      return -1;
    }
    confirminvalidate(pgnum, id);
    return 0;
 }
