#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "stddef.h"
#include "kernel/shmem.h"

int main(int argc, char *argv[])
{
    if(argc==2){
        printf("shmem_acquire_access %d\n", atoi(argv[1]));
        if(shmem_acquire_access(atoi(argv[1]))) {
            printf("shmem_acquire_access success\n");
        } else {
            printf("shmem_acquire_access failed\n");
        }
    }
    else {
        printf("Usage: shmem_acquire [page_number]\n");
        exit(1);
    };
    exit(0);
}