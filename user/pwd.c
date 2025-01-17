#include "kernel/types.h"
#include "kernel/fs.h"
#include "user/user.h"
#include <stdbool.h>

#define MAX_LEN 512

const char current[DIRSIZ] = ".";
const char top[DIRSIZ] = "..";

bool is_name_match(char* s1, const char* s2){
    for(int i=0;i<DIRSIZ;i++){
        if(s1[i]!=s2[i]){
            return false;
        }
    }
    return true;
}

// 自作のstrcat関数
char* strcat_custom(char* destination, const char* source) {
    char* ptr = destination;

    // destinationの末尾まで移動
    while (*ptr != '\0') {
        ptr++;
    }

    // sourceをdestinationの末尾にコピー
    while (*source != '\0') {
        *ptr = *source;
        ptr++;
        source++;
    }

    // 終端文字を追加
    *ptr = '\0';

    return destination;
}

int main(){
    int fd;
    int cc;
    int pre_inum=0;
    struct dirent de;
    char file_path_buffer[64][DIRSIZ];
    char file_path[MAX_LEN] = ".";
    int now=0;
    bool is_root = false;
    //char full_path;
    if ((fd = open(file_path, 0)) < 0) {
        printf("Cannot open: .\n");
        exit(1);
    }
    while ((cc = read(fd, &de, 16)) == 16) {
        if (de.inum == 0) continue;
        // "."と一致するか判定
        if (is_name_match(de.name, current)){
            if(de.inum == 1){
                is_root = true;
                break;
            } else {
                pre_inum = de.inum;
                break;
            }
        }
    }
    close(fd);

    if(!is_root)while(1){
        char file_path[MAX_LEN] = "";
        for(int i=0;i<now+1;i++){
            strcat_custom(file_path, "../");
        }
        strcat_custom(file_path, ".");
        if ((fd = open(file_path, 0)) < 0) {
        printf("Cannot open: .\n");
        exit(1);
        }
        int prepre_inum = pre_inum;
        while ((cc = read(fd, &de, 16)) == 16) {
            if (de.inum == 0) continue;
            // "."と一致するか判定
            if (is_name_match(de.name, current)){
                if(de.inum == 1){
                    is_root = true;
                    continue;
                } else {
                    pre_inum = de.inum;
                    continue;;
                }
            }

            if(de.inum==prepre_inum){
                for(int i=0;i<DIRSIZ;i++)file_path_buffer[now][i] = de.name[i];
            }
        }
        close(fd);
        now++;
        if(is_root)break;

    }

    if(now==0)printf("/\n");
    else {
        for(int i=now-1;i>=0;i--){
            printf("/%s", file_path_buffer[i]);
        }
        printf("\n");
    }

    exit(0);
}
