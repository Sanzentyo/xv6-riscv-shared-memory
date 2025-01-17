#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "stddef.h"
#include "kernel/shmem.h"

int
main(int argc, char *argv[])
{
    int pid;

    is_top_parent_process();

    if((pid = fork()) == 0){
        for(int i=0; i<6; i++){
            if(fork()!=0){
                break;
            }
            printf("Child process%d\n",i);
            if(is_top_parent_process()){
                printf("this process is top parent process\n");
            } else {
                printf("this process is not top parent process\n");
            }
        }
        exit(0);
    }
    else{
        sleep(10);
        printf("Parent process\n");

        if(is_top_parent_process()){
            printf("this process is top parent process\n");
        } else {
            printf("this process is not top parent process\n");
        }
        exit(0);
        
    }
}