#include<stdio.h>
#include<string.h>

void printstuff(char *msg){
  char msg1[strlen(msg)+1];
  /* char msg1[80] = "hello world"; */
  strcpy(msg1, msg);
  const char delim[2] = " ";
  char* token;
  token = strtok(msg1, delim);
  char* ip;
  int port;

  int i = 0;
  while(token != NULL) {
    printf("%s \n", token);
    if (i == 3) {
      ip = token;
    }
    if (i == 4) {
      port = atoi(token);
    }
    i++;
    token = strtok(NULL, delim);
  } 
  printf("%s %d \n", ip, port);
}

int main() {
  printstuff("requestpage 3 read 127.0.0.1 4444");
}
