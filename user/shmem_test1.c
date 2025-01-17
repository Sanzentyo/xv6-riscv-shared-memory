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

int
main(int argc, char *argv[])
{
    check_shmem_all_state(); // 最初の状態を表示

    // shmemの状態を変更
    shmem_set_unwritable(0);
    shmem_set_writable(1);

    // 値を事前に設定してから、shmemの状態を変更
    int a[] = {0,1,2,3,4};
    for(int i=0;i<5;i++){
        shmem_set_init_int(2, i, a[i]);
    }
    shmem_set_unwritable(2);

    // 変更した後の状態を表示
    check_shmem_all_state();

    // 値を読み込む
    for(int i=0;i<5;i++){
        printf("shmem_read_int(2, %d): %d\n", i, shmem_read_int(2, i));
    }

    if(fork()==0){
        // 子プロセスでshmemの状態を表示し、アクセス数が増えていることを確認
        check_shmem_all_state();
    } else {
        // 子プロセスが終了するまで待機し、その後shmemの状態を表示
        wait(0);
        check_shmem_all_state();

        // shmem[2]へのアクセス権を解放し、その後の状態を表示
        shmem_release_access(2);
        check_shmem_all_state();

        // shmem[2]への状態を変更し、その後の状態と値を表示
        shmem_set_unwritable(2);
        check_shmem_all_state();
        for(int i=0;i<5;i++){
            printf("shmem_read_int(2, %d): %d\n", i, shmem_read_int(2, i));
        }

    }

    exit(0);
}