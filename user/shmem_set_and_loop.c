#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "stddef.h"
#include "kernel/shmem.h"
#include "kernel/param.h"

int main(int argc, char *argv[])
{
    if(argc==2){
        shmem_set_writable(atoi(argv[1]));
        for(;;);
        exit(0);
    }
    else {
        printf("Usage: shmem_acquire [page_number]\n");
        exit(1);
    };
}