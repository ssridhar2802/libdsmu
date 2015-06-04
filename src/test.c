#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/sglib.h"

/* #include "../include/rpc.h" */
/* #include "../include/libdsmu.h" */

struct pagetableentry {
  struct copysetlinkedlist *copyset;//=NULL;
  char *probOwner;
};
  
struct copysetlinkedlist {
  char *ip;
  int port;
  struct copysetlinkedlist * next_ptr;
};

struct pagetableentry pagetable[20];

int addToCopySet(char *ip, int port, int pgnum) {
  struct copysetlinkedlist *entry = malloc(sizeof(struct copysetlinkedlist));
  entry -> ip = ip;
  entry -> port = port;
  SGLIB_LIST_ADD(struct copysetlinkedlist, pagetable[pgnum].copyset, entry, next_ptr);
  /* SGLIB_LIST_MAP_ON_ELEMENTS(struct copysetlinkedlist, pagetable[pgnum].copyset, ll, next_ptr, { */
  /*     printf("%d ", ll->i); */
  /*   }); */
  /* printf("\n"); */
}


int freeList(struct copysetlinkedlist **head){
  struct copysetlinkedlist *curr, *ne;
  curr = *head;
  *head = NULL;
  while(curr != NULL){
    ne = curr->next_ptr;
    /* printf("deleting - %d\n", curr->port); */
    free(curr);
    curr = ne;
  }
}


void deleteEntry(struct copysetlinkedlist **head, char* ip, int port) {
  struct copysetlinkedlist *curr, *ne, *prev;
  curr = *head;
  /* *head = NULL; */
  while(curr != NULL){
    if (!strcmp(curr->ip, ip) && port == curr->port) {
      printf("%d \n", curr->port);
      if (curr->next_ptr != NULL) {
	prev->next_ptr = curr->next_ptr;
	return;
      }
      else if (prev == NULL){
	*head = NULL;
      }
      else {
	prev->next_ptr = NULL;
	return;
      }
    }
    prev = curr;
    curr = curr->next_ptr;
  }
}
  

int printList(struct copysetlinkedlist *head){
  struct copysetlinkedlist *curr, *ne;
  curr = head;
  while(curr != NULL){
    ne = curr->next_ptr;
    printf("%d ", curr->port);
    curr = ne;
  }
}


int main() {
  addToCopySet("127.0.0.1", 4444, 3);
  /* addToCopySet("127.0.0.1", 4446, 3);x */
  struct copysetlinkedlist *ll, *the_list;

  /* printList(pagetable[3].copyset); */

  /* freeList(&pagetable[3].copyset); */
  deleteEntry(&pagetable[3].copyset, "127.0.0.1", 4444);
  printList(pagetable[3].copyset);
  printf("deleted\n %d", pagetable[3].copyset);

  addToCopySet("127.0.0.1", 4445, 3);
  printList(pagetable[3].copyset);

  printf("\n");
}


