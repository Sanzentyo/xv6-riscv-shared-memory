#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

char buf[512];

int main(int argc, char *argv[])
{
    int n;

    while((n = read(0, buf, sizeof(buf))) > 0){
        write(1, buf, n);
    }


    exit(0);
}