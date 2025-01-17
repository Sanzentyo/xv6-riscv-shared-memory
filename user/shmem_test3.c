#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "stddef.h"
#include "kernel/shmem.h"
#include "kernel/param.h"


// shmemの状態を表示する関数
// page_number: 表示するshmemのページ番号
// 以下の形式で表示
// page_number       shmem[page_number]のstate         shmem[page_number]にアクセスしているプロセスの数
void check_shmem_state(int page_number){
    int state = shmem_state(page_number);
    if (state != NOT_SET) {
        
        if (state == WRITABLE) {
            int cnt = shmem_count(page_number);
            printf("%d\tWRITABLE\t%d\n", page_number, cnt);
        }else if (state == UNWRITABLE){
            int cnt = shmem_count(page_number);
            printf("%d\tUNWRITABLE\t%d\n", page_number, cnt);
        } else {
            int cnt = shmem_count(page_number);
            printf("%d\tINVAILED\t%d\n", page_number, cnt);
        }
    } else if (state == NOT_SET){
        int cnt = shmem_count(page_number);
        printf("%d\tNOT_SET  \t%d\n", page_number, cnt);
    } else {
        int cnt = shmem_count(page_number);
        printf("%d\tINVAILED\t%d\n", page_number, cnt);
    }
}

void check_shmem_all_state(){
    printf("\npage\tstate\n");
    for(int i=0; i<SHMEM; i++){
        check_shmem_state(i);
    }
    printf("\n");
}


int main(int argc, char *argv[])
{
    int page_number = 0;
    check_shmem_all_state();
    shmem_set_init_int(page_number, 0, 42);
    shmem_set_init_int(page_number, 1, 43);
    shmem_set_unwritable(page_number);
    check_shmem_all_state();
    if(fork()==0){
        char *argv1[] = {"shmem_acquire_and_loop", "1", 0};
        int ret = exec("shmem_acquire_and_loop", argv1);
        printf("exec result: %d\n", ret);
    }
    sleep(1);
    char *argv0[] = {"shmem_acquire_and_loop", "0", 0};
    int ret = exec("shmem_acquire_and_loop", argv0);
    printf("exec result: %d\n", ret);
    exit(0);
}