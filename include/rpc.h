#ifndef _RPC_H_
#define _RPC_H_
 
int sendman(char *str);
 
int initsocks(char *ip, int port);
 
int teardownsocks(void);
 
void *listenman(void *ptr);
 
void confirminvalidate(int pgnum, int id);
 
void confirminvalidate_encoded(int pgnum, char *pgb64, char *ip, int port);
 
int invalidate(char *msg);
 
int dispatch(char *msg);
 
int requestpage(int pgnum, char *type);
 
int handleconfirm(char *msg);

int handlerequestpage(char *msg);

void * initOwnerServer(void *portNoPointer);

int getClientSocketDescription(char *server_addr, int portNo); 
#endif  // _RPC_H_
