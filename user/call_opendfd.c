#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"


void printb(uint16 v) {
  uint16 mask = (uint16)1 << (sizeof(v) * 8 - 1);
  do printf("%c",mask & v ? '1' : '0');
  while (mask >>= 1);
  printf("\n");
}

int main(void){

    int fds[2];

    printb(opendfd());

    pipe(fds);

    printb(opendfd());
    

    exit(0);

}