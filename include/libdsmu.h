#ifndef _LIBDSMU_H_
#define _LIBDSMU_H_
 
#include <signal.h>
#include <stdint.h>
#include <ucontext.h>
 
//
// Initialize distributed shared memory.
// The manager is listening on port.
// Shared memory will begin at starta and will include all pages that include
// addresses in the range [starta, starta + len).
//
int initlibdsmu(char *ip, int port, uintptr_t starta, size_t len, int myID);
 
int teardownlibdsmu(void);
 
#define SHRPOL_NONE (0)
#define SHRPOL_INIT_ZERO (1 << 0)
 
int addsharedregion(uintptr_t starta, size_t len, int policy);
 
struct sharedregion {
  uintptr_t start;
  size_t len;
  uint16_t policy;
};
 
//struct pagetable {
//  struct pagetableentry entries[MAX_SHARED_PAGES];
//};

struct pagetableentry {
  int copyset[10];
  char *probOwner;
};
  
struct copysetlinkedlist {
  char *ip;
  int port;
  struct copysetlinkedlist * next_ptr;
};
 
#endif  // _LIBDSMU_H_
