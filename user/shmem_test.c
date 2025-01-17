#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "stddef.h"
#include "kernel/shmem.h"

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

int
main(int argc, char *argv[])
{
    int page_number = 0; // 0からSHMEM-1の間の値
    check_shmem_state(page_number);

    if(fork() == 0){

        // 子プロセスであることを宣言して、共有メモリの状態を表示
        printf("Child process\n");
        check_shmem_state(page_number);

        // 共有メモリの状態を変更して、その状態を表示
        printf("shmem_set_writable\n");
        shmem_set_writable(page_number);
        check_shmem_state(page_number);

        // 共有メモリに値を書き込む
        shmem_write_int(page_number, 0, 42);
        shmem_write_int(page_number, 1, 43);
    }
    else{
        while(!shmem_acquire_access(page_number));// 子プロセスがshmem[pagenumber]がWRITABLEにsetされるまで待つ
        wait(0); // 子プロセスが終了するまで待つ
        
        // 親プロセスであることを宣言して、共有メモリの状態を表示
        printf("Parent process\n");
        check_shmem_state(page_number);
        
        
        // 共有メモリの値を取得して表示
        printf("Value: %d\n", shmem_read_int(page_number, 0));
        printf("Value: %d\n", shmem_read_int(page_number, 1));
    }
  exit(0);
}