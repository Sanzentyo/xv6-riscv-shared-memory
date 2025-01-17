#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define READ  0
#define WRITE 1

int main(int argc, char *argv[]){
    int max_num;
    int fds[2];
    int div_num;

    if(argc != 2){
        printf("arg is nothing or too over\n");
        exit(1);
    }

    max_num = atoi(argv[1]);

    if(max_num <= 1){
        printf("arg is too small\n");
        exit(1);
    }

    if(pipe(fds) < 0){
        printf("pipe() failed in countfree()\n");
        exit(1);
    }

    if(fork() != 0){
        // number generater
        close(fds[READ]);
        for(int i=2;i<=max_num;i++){
            write(fds[WRITE], &i, sizeof(int));
        }
        close(fds[WRITE]);
        
    } else {
        close(fds[WRITE]);  // 親へ書き込むパイプを閉じる
        int recieved_num;
        if(read(fds[READ],&div_num, sizeof(int))==0){
            close(fds[READ]);
            exit(0);
        }
        printf("Process %d Created\n", div_num);
        
        int p_fds[2]; // 割り算などの処理をするプロセス同士のfd
        pipe(p_fds);

        if(fork()==0){
            // 子プロセス
            close(fds[READ]);
            close(p_fds[WRITE]);
            fds[READ] = p_fds[READ];
        } else {
            close(p_fds[READ]);
            while(read(fds[READ],&recieved_num, sizeof(int)) != 0){
                if(recieved_num%div_num != 0){
                    write(p_fds[WRITE], &recieved_num, sizeof(int));
                }
            }
            close(fds[READ]);
            wait(0);
            exit(0);
        }

        while(read(fds[READ], &div_num, sizeof(int)) != 0){
            printf("Process %d Created\n", div_num);

            if(pipe(p_fds) < 0){
                printf("pipe() failed in countfree()\n");
                exit(1);
            }

            if(fork() == 0){
                close(fds[READ]);
                close(p_fds[WRITE]);
                fds[READ] = p_fds[READ];
            } else {
                close(p_fds[READ]);
                while(read(fds[READ], &recieved_num, sizeof(int)) != 0){
                    if(recieved_num % div_num != 0){
                        write(p_fds[WRITE], &recieved_num, sizeof(int));
                    }
                }
                close(fds[READ]);
                close(p_fds[WRITE]);
                wait(0);
                exit(0);
            }
        }

    }


    exit(0);

}