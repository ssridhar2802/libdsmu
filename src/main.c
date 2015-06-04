#include <stdio.h>
#include <stdlib.h>

#include "libdsmu.h"
#include "mem.h"

int id;

int main(int argc, char *argv[]) {

  if (argc < 3) {
    printf("Usage: main MANAGER_IP MANAGER_PORT\n");
    return 1;
  }

  // Start libdsmu. Shared memory is the 10 pages beginning at 0x12340000.
  char *ip = argv[1];
  int port = atoi(argv[2]); 
  initlibdsmu(ip, port, 0x12340000, 4096 * 10, atoi(argv[3]));
  sleep(2);

  // invalidate test.
/*  while (1) {
    int pgnum;
    printf("> ");
    scanf("%d", &pgnum);

    uintptr_t addr = PGNUM_TO_PGADDR((uintptr_t)pgnum);
    void *p = (void *)addr;
    ((char *)p)[1] = 'a';
    printf("p[1] = %c\n", ((char *)p)[1]);
  }
*/
  
  // Trigger a read fault, then a write fault.
  void *p = (void *)0x12340000;
  printf("Will try to read %p\n", ((int *)p));
  printf("p[0] is %d\n", ((int *)p)[0]);
  sleep(10);

  printf("Setting p[0] to 3\n");
  ((int *)p)[0] = 3;
  printf("p[0] is now %d\n", ((int *)p)[0]);
  sleep(5);
  teardownlibdsmu();
  return 0;
}
