
# システムプログラミング最終課題

# 外部説明書
## 目的
forkしたユーザープロセス間でメモリを共有できるようにxv6カーネルを拡張する

- 共有メモリ用のテーブルをカーネルに追加
- プロセスごとに共有メモリにアクセスするための関数を追加
- forkするときに、親プロセスがアクセスしている共有メモリの情報を引き継ぐ
- 同時書き込みができないようにセマフォのような機構を実装
- Read Onlyの共有メモリとRead/Writeができる共有メモリを別で作る

また、現在のプロセスがexec()で実行されたプロセスである場合はtrueを、`fork()`で実行された子プロセスである場合はfalseを返す関数`is_top_parent_process()`を定義した

## 全体の説明
proc.cで定義された`struct shmem shmem[SHMEM]`という共有メモリの物理アドレスとそのメモリにアクセスしているプロセス数(ユーザー数)、状態、アクセスしているプロセスがどのpidのプロセスからforkされたまたはそのプロセスであるかどうかを示す値を保持している構造体がある。  
ユーザーは`SHMEM`(`kernel/param.h`で定義)個の共有メモリにそれぞれ専用の関数を用いてアクセスすることができる。共有メモリのサイズはそれぞれ`kalloc()`で確保される4096バイトである。  
``` c
//Shared memory struct
struct shmem { //.ADD
  struct spinlock state_lock;     // lock for the state of the shared memory 自分で追加
  struct spinlock lock;        // lock for the shared memory 自分で追加
  void* PHADDR;                   // physical address of physical page share memory
  int numberofusers;                 // number of users of that physical page
  int state;                  // is the page writable or unwritable or not set 自分で追加
  int using_top_pid;          // top_pid of the process that is using this page 自分で追加
};
```
共有メモリはそれぞれ`state`(`NOT_SET`, `WRITABLE`, `UNWRITABLE`のいずれか)を持っており、使用時は、まず`NOT_SET`の共有メモリの状態を`WRITABLE`か`UNWRITABLE`に設定してから使用する。このとき、共有メモリを設定したプロセスにはアクセス権限が与えられる。`NOT_SET`の共有メモリには、最初にexec()で実行されたプロセスのみが事前に値を設定でき、読み取りはできない。`WRITABLE`のメモリには、アクセス権を持つプロセスから値の書き込みと読み取りができる。`UNWRITABLE`のメモリには、アクセス権を持つプロセスから値の読み取りができる。また、forkしてできた子プロセスは親プロセスのアクセス権限を引き継ぎ、対応する`shmem`へのアクセスが可能となる。  
アクセスが減るタイミングでアクセスしているプロセスの数が0になると、値がすべて0に設定され`state`も`NOT_SET`に設定される。  
`state`が変更されるのは、

- `NOT_SET`から`WRITABLE`か`UNWRITABLE`に設定される場合
- `shmem_release_access(int page_number)`を使用してアクセス権を解放するか、`exit()`が実行され`page_number`のアクセス権を持っていいたプロセスが終了するかのいずれかで`shmem[page_number]`の`numberofusers`が減少し、ちょうど値が0になり、`WRITABLE`か`UNWRITABLE`から`NOT_SET`に設定される場合

のみである。また、

|state|read|write|set|
|---|---|---|---|
|`NOT_SET`|×|×|○|
|`WRITABLE`|○|○|×|
|`UNWRITABLE`|○|×|×|

が`state`の`shmem`に対して可能な操作である。

### 書き込み可能な共有メモリを使用する(WRITABLE)
書き込み可能な共有メモリを使用するには、まず`shmem_set_writable(int page_number)`を使用して、使いたい`shmem`の`state`を`page_number`で指定して`WRITABLE`に設定をする。`state`が`WRITABLE`の時のみ、`shmem_write_int(int page_number, int index, int value)`を使用することができ、指定した`index`の物理アドレスに`value`の値が設定される。また、値を取得する時は、`shmem_read_int(int page_number, int index)`を使用する。これは、`state`が`WRITABLE`か`UNWRITABLE`の時に使用でき、指定した`index`の物理アドレスに保存された`value`の値が取得できる。また、同じ`shmem`に対する書き込みと読み込みのいずれの操作も同時に実行されることはない。  
また、`fork()`した後に別プロセスで`state`が設定された場合、そのプロセスはアクセス権を持たないため、`bool shmem_acquire_access(int page_number)`でアクセス権を要求する必要がある。この関数はアクセスの獲得が成功したかどうかを`bool`で返し、既にアクセスを持っているか、完全に別のプロセスで使用されている`shmem`へのアクスを要求している場合はpanicを起こす。

以下は、これらの関数を使用して、親プロセスで書き込まれた値を子プロセスから読み込み、ターミナルに出力するサンプルコードである。

``` c
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
```
このコードの実行結果は以下の通りである。
``` shell
$ shmem_test
0       NOT_SET         0
Child process
0       NOT_SET         0
shmem_set_writable
0       WRITABLE        2
Parent process
0       WRITABLE        1
Value: 42
Value: 43
```

### 書き込み不可能な共有メモリを使用する(UNWRITABLE)
書き込み不可能な共有メモリを使用するには、`shmem_set_unwritable(int page_number)`を使用して、使いたい`shmem`の`state`を`page_number`で指定して`UNWRITABLE`に設定する。設定された値は`shmem_read_int(int page_number, int index)`を使用して取得することができる。このとき、`UNWRITABLE`に設定された共有メモリの値は変更されることがないので、同時に`shmem_read_int(int page_number, int index)`が同じ`shmem`に対して実行され、読み込まれることが可能である。  
`state`を設定する前(`state`が`NOT_SET`の時)に、`shmem_set_init_int(int page_number, int index, int value)`を使用して値を設定することができる。`UNWRITABLE`に設定された場合に、共有メモリの値を変更することはできないので、事前に必要な値をこの関数を使用して設定しておく必要がある。また、`shmem_set_init_int()`が実行された時点で、対象の`shmem`には`top_pid`が一致するプロセスしかアクセスできなくなる。この`shmem`の`state`が`NOT_SET`の状態で、`top_pid`が一致するプロセスで`exit()`が実行されると、全ての値が0に上書きされる。
また、`exit()`以外にも、`void shmem_release_access(int page_number)`で`shmem`へのアクセスを解放することができる。解放する`shmem`へのアクセスを持っていない場合はpanicを起こす。

以下はこれらの関数を使用したサンプルコードである。
``` c
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
```
このコードの実行結果は以下の通りである。
``` c
$ shmem_test1

page    state
0       NOT_SET         0
1       NOT_SET         0
2       NOT_SET         0
3       NOT_SET         0


page    state
0       UNWRITABLE      1
1       WRITABLE        1
2       UNWRITABLE      1
3       NOT_SET         0

shmem_read_int(2, 0): 0
shmem_read_int(2, 1): 1
shmem_read_int(2, 2): 2
shmem_read_int(2, 3): 3
shmem_read_int(2, 4): 4

page    state
0       UNWRITABLE      2
1       WRITABLE        2
2       UNWRITABLE      2
3       NOT_SET         0


page    state
0       UNWRITABLE      1
1       WRITABLE        1
2       UNWRITABLE      1
3       NOT_SET         0


page    state
0       UNWRITABLE      1
1       WRITABLE        1
2       NOT_SET         0
3       NOT_SET         0


page    state
0       UNWRITABLE      1
1       WRITABLE        1
2       UNWRITABLE      1
3       NOT_SET         0

shmem_read_int(2, 0): 0
shmem_read_int(2, 1): 0
shmem_read_int(2, 2): 0
shmem_read_int(2, 3): 0
shmem_read_int(2, 4): 0
```

### 別プロセスの排除
別プロセス(`top_pid`が違うプロセス)からは、`shmem`のアクセス数と状態のみが取得可能であり、それ以外の操作をしようとした場合はpanicを起こすようになっている。アクセス権を持たないプロセスからの`shmem`への値の書き込み/読み込みを行うには、アクセス権を要求する必要があるので、`shmem_access_acquire()`で別プロセスからアクセス権を取得することができないのであれば、別プロセスからアクセスを完全に排除できていることになる。

`shmem_access_acquire()`が要因のpanicを起こし、別プロセスからの`shmem`へのアクセスを排除している結果を出力するコードを2例示す。まず１例目は以下の通りである。
``` c
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
    check_shmem_all_state();

    if(fork()==0){
        char *argv1[] = {"shmem_set_and_loop", "0", 0};
        int ret = exec("shmem_set_and_loop", argv1);
        printf("exec result: %d\n", ret);
    }
    while(shmem_count(0) == 0);
    check_shmem_all_state();
    shmem_acquire_access(0);
    check_shmem_all_state();
    exit(0);
}
```
これは、`exec()`で`shmem_set_and_loop`である、
``` c
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
```
を実行し、`shmem`の`state`を`WRITABLE`にした状態で待機させてから、同じ`shmem`へのアクセスを要求するプログラムである。これを実行すると以下のような結果となり、別プロセスからのアクセスを排除できていることがわかる。
```shell
$ shmem_test2

page    state
0       NOT_SET         0
1       NOT_SET         0
2       NOT_SET         0
3       NOT_SET         0


page    state
0       WRITABLE        1
1       NOT_SET         0
2       NOT_SET         0
3       NOT_SET         0

panic: This shmem page already have access to other process
```
次に２例目は以下の通りである。
``` c
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
```
これは、0の`shmem`に事前に値を設定してから、`UNWRITABLE`に設定し、`exec()`で`shmem_acquire_and_loop`である、
``` c
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
```
を実行して、`NOT_SET`である1のアクセス権を取得しようとして失敗し、別プロセスが使用している0の`shmem`のアクセス権を取得しようとしてpanicを起こすコードである。実行結果は以下の通りになる。
``` shell
$ shmem_test3

page    state
0       NOT_SET         0
1       NOT_SET         0
2       NOT_SET         0
3       NOT_SET         0


page    state
0       UNWRITABLE      1
1       NOT_SET         0
2       NOT_SET         0
3       NOT_SET         0

shmem_acquire_access 1
shmem_acquire_access failed
shmem_acquire_access 0
panic: The process already have access to this shmem page
```

このように、別プロセスから`shmem`へのアクセスは排除できていることがわかる。


### `bool is_top_parent_process()`の説明
`exec()`によって実行されたプロセスで実行すると`true`を返し、`fork()`によって複製された子プロセスで実行すると`false`を返す。以下は使用例である。
``` c
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
```
これを実行すると以下のような出力になり、`exec()`で実行されたプロセスのみに`true`を返していることが確認できる。
``` shell
$ is_top
Child process0
this process is not top parent process
Child process1
this process is not top parent process
Child process2
this process is not top parent process
Child process3
this process is not top parent process
Child process4
this process is not top parent process
Child process5
this process is not top parent process
Parent process
this process is top parent process
```



# 内部説明書
簡素な共有メモリの機能をxv6-riscvで実装していた https://github.com/Pablo-Izquierdo/RiscV-xv6/blob/main/Projects/P3 を元に機能追加を行った。まず、最初から実装されていた主要な構造体と関数を説明する。

## P3にて実装されていた主要な構造体
``` c
//Shared memory struct
struct shmem { //.ADD
  void* PHADDR;                   // physical address of physical page share memory
  int numberofusers;                 // number of users of that physical page
};
```
これは、`kernel/proc.h`で定義されており、`kernel/proc.c`で
``` c
struct shmem shmem[SHMEM];
```
と定義されていた。`SHMEM`は`kernel/param.h`で
``` c
#define SHMEM 4
```
と定義されており、これが使用できる`shmem`の上限数を定義している。`kernel/proc.c`で定義される
``` c
void safe_physical_address (void * phaddr, int i){
  shmem[i].PHADDR=phaddr;
  shmem[i].numberofusers=0;
}
```
を使用して、`kernel/main.c`で、
``` c
for(int i=0; i<SHMEM; i++){ // generate shmem pages at the begining
      safe_physical_address (kalloc(), i);
}
```
のように、`kalloc()`で確保したページテーブルの物理アドレスを格納する。

また、`struct proc`について、
```c
  struct procshmem shmem[SHMEM];
  int shmemaccess;             //number of shmem pages that the process has access
```
が追加されているが、procshmemは
``` c
//Shared memory struct for process
struct procshmem { //.ADD
  void* VADDR;                   // virtual address of physical page share memory
};
```
と定義されている。

## P3にて実装されていた主要な関数
`shmem[SHMEM]`に格納された物理アドレスにアクセスするには、`proc.c`で定義される`void* get_shmem_access (int page_number, int pid)`を使用する。これは、`struct shmem[page_number]`に格納されている物理アドレスを使用したプロセスの仮想アドレスにマッピングし、そのマッピングした仮想アドレスを返す。その前の時点で同じ`shmem`に対して、`get_shmem_access`が使用されていた場合は、最初に実行した時点で、`proc.shmem[SHMEM]`に格納された仮想アドレスを返す。実際にユーザープロセスで使用される関数は`user.h`でプロトタイプ宣言をされる`void *shmem_access(int page_number)`である。
``` c
/*
* Give access shme to the caller process.
* if pid!=0 give the access to the process with that pid
* return the Virtual address where the shmem page has been mapped
*/
void* get_shmem_access (int page_number, int pid){ //.ADD

  struct proc *p = myproc();
  long int VA = (long int)NULL;

  if (pid != 0){
    for(struct proc *j = proc; j < &proc[NPROC]; j++){
      if(pid == j->pid){
         p = j;
         break;
      }
    }
  }

  acquire(&p->lock);
  if(p->shmem[page_number].VADDR != NULL){ //if the process already have access, return its VA
    release(&p->lock);
    return p->shmem[page_number].VADDR;
  }
  
  //add this physical page to process pagetable with W/R permisions (HEAP HAS GBYTES OF SPACES, SO I PUT SHMEM ON TOP HEAP)
  VA=VASHMEM(p->shmemaccess);
  if(mappages(p->pagetable, VA, PGSIZE,
            (uint64)shmem[page_number].PHADDR, PTE_R|PTE_W|PTE_U) < 0){
    release(&p->lock);
    return NULL;
  }
  
  shmem[page_number].numberofusers++;
  p->shmem[page_number].VADDR=(void*)VA;
  p->shmemaccess++;
  release(&p->lock);
  return (void*)VA;

}
```
syscallで直接実行される`sysproc.c`に記述される関数は、
``` c
void* sys_shmem_access(void){ //.ADD

  //Parameters
  int page_number;
  if (argint(0,&page_number) < 0){
    return NULL;
  }

  //Check parameters
  if (page_number < 0 || page_number > 3){
    return NULL;
  }

  return get_shmem_access(page_number, 0);
}
```
のように記述されている。

また、対応する`shmem`にアクセスしているプロセス数を取得する`int count_shmem (int page_number)`という関数も`proc.c`で定義され、同様にユーザープロセスで使用される関数は`user.h`でプロトタイプ宣言されている`int shmem_count(int page_number)`である。
``` c
/*
* Return the number of process with access to a expecify shmem page
*/
int count_shmem (int page_number){//.ADD

  return shmem[page_number].numberofusers;

}
```

## 新しく自分で変更した実装の概要
P3は、確保したメモリの物理アドレスをアクセスしようとするユーザープロセスの仮想アドレスにマッピングしている実装のみがされている。`shmem`にアクセスしているプロセス数は取得できるが、そのプロセス数が0になった場合に、共有メモリの内容を初期化するといった処理が存在しない。そのため、プログラムを終了した後に、別のプログラムから同じ`page_number`の`shmem`にアクセスすると、前のプログラムで書き込まれた値がそのまま取得できてしまう。また、fork()してできたわけではない別のプロセスからもアクセスでき、同時に書き込まれることも起こり得る。

以上のことを防ぐため、まず、物理アドレスを仮想アドレスにマッピングする`void *shmem_access(int page_number)`を削除し、直接ポインタに対して操作する手段をなくした。次に、`shmem`に`state`(`NOT_SET`, `WRITABLE`, `UNWRITABLE`のいずれか)を`shmem`へのアクセス権を取得する関数と解放する関数と状態を取得する関数、値を読み取る関数と書き込む関数、値を事前に設定する関数をそれぞれ定義し、これらの関数のみを用いることで`shmem`にアクセスできるようにした。`proc`と`shmem`にはその実装に際して必要になったパラメータを適宜追加し、`exit()`と`fork()`についても必要な変更を行った。`shmem`のアクセス数が減少する可能性のある`exit()`と`shmem_access_release()`に、アクセス数が0になった場合は値をすべて0に初期化する処理を追加した。`proc`に`int top_pid`を、`shmem`に`int using_top_pid`を追加し、同じ`exec()`で実行されたプロセス自体か`fork()`で実行された親子関係のつながりがある関数かを判別できるようにした。また、この値を使用して、`exec()`で実行される関数かを判別する`bool is_top_parent_process(void)`を追加した。

追加した関数はP3の実装を参考に必要な変更(`syscall.c`や`syscall.h`、`usys.pl`や`defs.h`、`Makefile`や`sysproc.c`、`user.h`などへの追記)を行なっている。特筆すべき変更点としては、`sysproc.c`で行われていたページのチェックを、
``` c
void sys_shmem_set_init_int(void){
  //Parameters
  int page_number;
  int index;
  int value;

  if (argint(0,&page_number) < 0){
    return;
  }

  if (argint(1,&index) < 0){
    return;
  }

  if (argint(2,&value) < 0){
    return;
  }

  //Check parameters
  if (page_number < 0 || page_number > SHMEM - 1){
    return;
  }

  shmem_set_init_int(page_number, index, value);

}
```
のように`3`ではなく、`SHMEM - 1`と記述したところである。そのため、以下の実装した関数の説明については、`proc.c`の関数のみを対象に説明を行う。

基本的にアクセス権限がない`shmem`へのアクセスや`state`の不正な変更が起こった場合は関数を使用しているコード側に問題があるので、`shmem_access_acquire()`以外は`bool`などで結果を返すのではなく、panicを起こしてカーネル自体を停止させるようにしている。

### `struct proc`の変更
P3の`proc.h`で定義されている`struct proc`を元に以下の変更を行なった。

- `shmem`へのアクセス数をカウントする`int shmemaccess`の名前を`int shmem_access_count`に変更
- そのプロセスが対象の`shmem`を持っているかを表す`bool`の配列`bool shmem_access[SHMEM]`を追加
- `exec()`で実行された一番上に位置する親のプロセスの`pid`を記録する`int top_pid`を追加
- `shmem`の物理アドレスをマッピングした仮想アドレスを保持する`struct procshmem shmem[SHMEM]`を削除

また、これに伴って、`struct procshmem`の定義も削除している。

``` c
// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID
  int shmem_access_count;              //number of shmem pages that the process has access 名前だけ変更(shmemaccessから)
  bool shmem_access[SHMEM];    //if the process has access to the shmem page 自分で追加

  // wait_lock must be held when using this:
  struct proc *parent;         // Parent process
  
  int top_pid;                 // topのpidを示す 自分で追加

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
};
```

### `struct shmem`の変更
P3の`proc.h`で定義されている`struct proc`を元に以下の変更を行なった。

- `state`の同時変更を防ぐためのspinlock`struct spinlock state_lock`を追加
- `WRITABLE`の時に値への同時アクセスを防ぐためのspinlock`struct spinlock lock`を追加
- `shmem`への可能な操作を示す状態を表す`int state`を追加
- `shmem`へ別のプロセスがアクセスすることを防ぐために、最初にアクセスしたプロセスの`top_pid`を記録する`int using_top_pid`を追加

```c
//Shared memory struct
struct shmem { //.ADD
  struct spinlock state_lock;     // lock for the state of the shared memory 自分で追加
  struct spinlock lock;        // lock for the shared memory 自分で追加
  void* PHADDR;                   // physical address of physical page share memory
  int numberofusers;                 // number of users of that physical page
  int state;                  // is the page writable or unwritable or not set 自分で追加
  int using_top_pid;          // top_pid of the process that is using this page 自分で追加
};
```

### `state`の定義
`state`については`shmem.h`に以下のようにマクロで定義されている。
``` c
// 自分で追加
#define NOT_SET -1
#define UNWRITABLE 0
#define WRITABLE 1
```
また、`state`ごとに可能な操作については以下の表の通りである。
|state|read|write|set|
|---|---|---|---|
|`NOT_SET`|×|×|○|
|`WRITABLE`|○|○|×|
|`UNWRITABLE`|○|×|×|

### `int shmem_state(int page_number)`の実装
該当する`shmem`の`state`を返り値として返す。主にデバッグ用に定義した関数であり、意味がないのでspinlockをかける処理はしていない。ただし、`state`が更新され得る場面ではspinlockがかけられるため、動的に`shmem`を確保する処理を書く際にこの関数を用いても問題は起こらないと考える。
``` c
// shmemのstateを取得
int shmem_state(int page_number){
  return shmem[page_number].state;
}
```

### `void shmem_set_writable(int page_number)`の実装
`state`が変更される関数なので、`shmem[page_number].state_lock`でspinlockをかけ、同時に変更されることがないようにする。`NOT_SET`以外の状態の`shmem`を変更しようとした場合はpanicを起こさせる。次に、実行している`proc`のアドレスを取得し、spinlockをかける。`top_pid`が記録されていない場合は実行している`proc`の`top_pid`を`shmem`に記録させ、記録されている場合は`top_pid`が一致するかを調べることで、`top_pid`が一致しない別のプロセスからアクセスを弾くことができるようにする。`proc`に`state`を変更する`shmem`への所有権を与える。このとき、`state`が`NOT_SET`であることから`proc`への所有権を持っていないことが保証されるので、そのことに関するチェックは行わない。`proc`への変更が終わったので、spinlockを解除する。書き込みが行われ得るので、`shmem.lock`を初期化する。全ての処理が終わってから、`state`を`WRITABLE`に更新し、spinlockも解除する。
``` c
// shmemに書き込みを可能にし、spinlockをセット
void shmem_set_writable(int page_number){
  acquire(&shmem[page_number].state_lock);
  if (shmem[page_number].state != NOT_SET){
    panic("shmem state is already set");
  }
  struct proc *p = myproc();
  acquire(&p->lock);
  if(shmem[page_number].using_top_pid == -1) shmem[page_number].using_top_pid = p->top_pid; // トッププロセスのpidを記録
  else if (shmem[page_number].using_top_pid != p->top_pid) panic("shmem is already used by other process");
  p->shmem_access[page_number] = true; // 所有権を持っていることを示す
  p->shmem_access_count++; // 所有権を持っているshmemの数を増やす
  release(&p->lock);
  shmem[page_number].numberofusers++; // カウンタを増やす
  char name[7] = "shmem_";
  name[6] = (char)(page_number + '0');
  initlock(&shmem[page_number].lock, name);
  shmem[page_number].state = WRITABLE; // 処理が終わってからstateを更新
  release(&shmem[page_number].state_lock);
}
```

### `void shmem_set_unwritable(int page_number)`の実装
`state`が変更される関数なので、`shmem[page_number].state_lock`でspinlockをかけ、同時に変更されることがないようにする。`NOT_SET`以外の状態の`shmem`を変更しようとした場合はpanicを起こさせる。次に、実行している`proc`のアドレスを取得し、spinlockをかける。`top_pid`が記録されていない場合は実行している`proc`の`top_pid`を`shmem`に記録させ、記録されている場合は`top_pid`が一致するかを調べることで、`top_pid`が一致しない別のプロセスからアクセスを弾くことができるようにする。`proc`に`state`を変更する`shmem`への所有権を与える。このとき、`state`が`NOT_SET`であることから`proc`への所有権を持っていないことが保証されるので、そのことに関するチェックは行わない。`proc`への変更が終わったので、spinlockを解除する。書き込みが行われ得ないので、`shmem.lock`を初期化しない。全ての処理が終わってから、`state`を`UNWRITABLE`に更新し、spinlockも解除する。
``` c
// shmemの書き込みを禁止し、spinlockはセットしない
void shmem_set_unwritable(int page_number){
  acquire(&shmem[page_number].state_lock);
  if (shmem[page_number].state != NOT_SET){
    panic("shmem state is already set");
  }
  struct proc *p = myproc();
  acquire(&p->lock);
  if(shmem[page_number].using_top_pid == -1) shmem[page_number].using_top_pid = p->top_pid; // トッププロセスのpidを記録
  else if (shmem[page_number].using_top_pid != p->top_pid) panic("shmem is already used by other process");
  p->shmem_access[page_number] = true; // 所有権を持っていることを示す
  p->shmem_access_count++; // 所有権を持っているshmemの数を増やす
  release(&p->lock);
  shmem[page_number].numberofusers++; // カウンタを増やす
  shmem[page_number].state = UNWRITABLE; // 処理が終わってからstateを更新
  release(&shmem[page_number].state_lock);
  
}
```

### `void shmem_set_init_int(int page_number, int index, int value)`の実装
変更中に`state`と`using_top_pid`が変更されると問題があるので、spinlockをかける。`NOT_SET`以外の状態の`shmem`を変更しようとした場合はpanicを起こさせる。次に、実行している`proc`のアドレスを取得し、spinlockをかける。`top_pid`が記録されていない場合は実行している`proc`の`top_pid`を`shmem`に記録させ、記録されている場合は`top_pid`が一致するかを調べることで、`top_pid`が一致しない別のプロセスからアクセスを弾くことができるようにする。`proc`を参照する処理が終わったので、spinlockを解除する。`shmem[page_number].PHADDR`を`void *`から`int *`にキャストして、該当する`index`の場所に値を書き込む。処理が完了したのでspinlockを解除する。
``` c
// NOT_SETの状態でshmemにint型の値を書き込む
void shmem_set_init_int(int page_number, int index, int value){
  acquire(&shmem[page_number].state_lock);
  if(shmem[page_number].state != NOT_SET){
    panic("shmem state is already set");
  }

  struct proc *p = myproc();
  acquire(&p->lock);
  if(shmem[page_number].using_top_pid == -1) shmem[page_number].using_top_pid = p->top_pid;
  else if(shmem[page_number].using_top_pid != p->top_pid) panic("shmem is already used by other process");
  release(&p->lock);
  int *shared_memory = (int *)shmem[page_number].PHADDR;
  shared_memory[index] = value;
  release(&shmem[page_number].state_lock);
}
```

### `void shmem_write_int(int page_number, int index ,int value)`の実装
`state`が`WRITABLE`でない場合は、デバック用にどの`shmem`に対するエラーかを明示してpanicを起こす。また、`WRITABLE`から別の`state`になるのは、`exit()`か`shmem_access_release()`が実行されて`shmem`のアクセス数が0になった場合であり、書き込む`shmem`へのアクセス権をこの関数が実行される`proc`が保持している時点でアクセス数は0にならず、`state`が変更されないことが保証されるので、`state`のためのspinlockをかける必要はない。また、`proc`がアクセス権限を持っていない場合は次の処理でチェックを行なってpanicを起こさせている。この時も、`proc.shmem_access`への変更が行われないことは保証されているので、spinlockをかけない。所有権を与える最後に、値を変更するのでspinlockをかけてから、`shmem[page_number].PHADDR`を`void *`から`int *`にキャストして、該当する`index`の場所に値を書き込み、spinlockを解除する。
``` c
// shmemにint型の値を書き込む
void shmem_write_int(int page_number, int index ,int value){
  // Check if the page is writable
  if (shmem[page_number].state != WRITABLE){
    char name[12] = "shmem_";
    name[6] = (char)(page_number + '0');
    char tmp[] = "_lock";
    for(int i=0;i<5;i++){
      name[7+i] = tmp[i];
    }
    panic(name);
  }

  // 所有権を持っていなかったら書き込めない
  struct proc *p = myproc();
  if (!p->shmem_access[page_number]){
    panic("The process don't have access to this shmem page");
  }

  acquire(&shmem[page_number].lock);
  int *shared_memory = (int *)shmem[page_number].PHADDR;
  shared_memory[index] = value;
  release(&shmem[page_number].lock);
}
```

### `int shmem_read_int(int page_number, int index)`の実装
`state`が`NOT_SET`である場合は、デバック用にどの`shmem`に対するエラーかを明示してpanicを起こす。また、`WRITABLE`と`UNWRITABLE`から`NOT_SET`に`state`がなるのは、`exit()`か`shmem_access_release()`が実行されて`shmem`のアクセス数が0になった場合であり、書き込む`shmem`へのアクセス権をこの関数が実行される`proc`が保持している時点でアクセス数は0にならず、`state`が変更されないことが保証されるので、`state`のためのspinlockをかける必要はない。また、`proc`がアクセス権限を持っていない場合は次の処理でチェックを行なってpanicを起こさせている。この時も、`proc.shmem_access`への変更が行われないことは保証されているので、spinlockをかけない。最後に、`WRITABLE`のときは値が変更され得るのでspinlockをかけてから、`shmem[page_number].PHADDR`を`void *`から`int *`にキャストして、該当する`index`の場所に値を読み込んで、spinlockを解除してから、読み込んだ値を返す。`UNWRITABLE`の時は、値が変更されないのでspinlockをかけずに同様の処理を行う。そのような処理は含まれないのでありえないが、デバッグ用に`state`が`NOT_SET`と`WRITABLE`と`UNWRITABLE`以外の値が入っていた場合はpanicを起こさせる。

``` c
// shmemからint型の値を読み込む
int shmem_read_int(int page_number, int index){
  // Check if the page is writable
  if (shmem[page_number].state == NOT_SET){
    char name[12] = "shmem_";
    name[6] = (char)(page_number + '0');
    char tmp[] = "_lock";
    for(int i=0;i<5;i++){
      name[7+i] = tmp[i];
    }
    panic(name);
  }

  // 所有権を持っていなかったら読み込めない
  struct proc *p = myproc();
  if (!p->shmem_access[page_number]){
    panic("The process don't have access to this shmem page");
  }

  // UNWRITABLEの場合はlockを取らずに読み込む
  if (shmem[page_number].state == UNWRITABLE){
    int *shared_memory = (int *)shmem[page_number].PHADDR;
    int value = shared_memory[index];
    return value;
  } else if (shmem[page_number].state == WRITABLE){ // WRITABLEの場合はlockを取って読み込む
    acquire(&shmem[page_number].lock);
    int *shared_memory = (int *)shmem[page_number].PHADDR;
    int value = shared_memory[index];
    release(&shmem[page_number].lock);
    return value;
  } else {
    panic("shmem state is invalid");
  }
  
}
```

### 'bool shmem_acquire_access(int page_number)'の実装
別プロセスで初期化が終わってからアクセス権を取得して、処理を開始するコードを書くことを想定しているので、`bool`を返り値としている。まず、`state`のためのspinlockをかける。`state`が`NOT_SET`の場合は、spinlockを解除してから`false`を返す。この関数を実行している`proc`へのアドレスを取得し、spinlockをかける。所有権を持っていた場合はこれの状態を起こすコードに問題があるのでpanicを起こす。持っておらず、`top_pid`が一致しない場合は、別プロセスからのアクセスを排除するため、panicを起こす。持っておらず、`top_pid`が一致する場合は、アクセス権を与え、spinlockを全て解除し、`true`を返す。また、このとき`state`が`NOT_SET`ではないので、`top_pid`が-1でないことが保証されている。
``` c
// procにshmemのアクセス権を設定
bool shmem_acquire_access(int page_number){
  acquire(&shmem[page_number].state_lock);
  if (shmem[page_number].state == NOT_SET){
    release(&shmem[page_number].state_lock);
    return false;
  }


  struct proc *p = myproc();
  acquire(&p->lock);
  if(!p->shmem_access[page_number]){
    if ((p->top_pid != shmem[page_number].using_top_pid)){
      panic("This shmem page already have access to other process");
    }
    p->shmem_access[page_number] = true;
    shmem[page_number].numberofusers++;
    release(&p->lock);
    release(&shmem[page_number].state_lock);
    return true;
  }else{
    panic("The process already have access to this shmem page");
  }
}
```

### `void shmem_release_access(int page_number)`の実装
まず、`state`のためのspinlockをかける。`state`が`NOT_SET`の場合はpanicを起こし、`shmem`のアクセス数が0以下の場合はpanicを起こす。次に、この関数を実行している`proc`へのアドレスを取得し、spinlockをかける。所有権を持っていた場合は所有権を解放し、`shmem`のアクセス数が0になっていた場合は、`state`を`NOT_SET`にして、物理アドレス先の値とspinlockの`name`と`top_pid`を初期化し、spinlockを全て解除する。持っていない場合は、この関数を使用するコード側に問題があるのでpanicを起こす。
``` c
// procからshmemのアクセス権を解放
void shmem_release_access(int page_number){
  acquire(&shmem[page_number].state_lock);
  if (shmem[page_number].state == NOT_SET){
    panic("shmem state is not set");
  }

  if (shmem[page_number].numberofusers == 0){
    panic("shmem state is invalid");
  } else if (shmem[page_number].numberofusers < 0){
    panic("shmem numberofusers is invalid");
  }

  struct proc *p = myproc();
  acquire(&p->lock);
  if(p->shmem_access[page_number]){
    
    p->shmem_access[page_number] = false;
    shmem[page_number].numberofusers--;
    if(shmem[page_number].numberofusers == 0){ // numberofusersが減少し得る処理なので0になっているか確認し、0なら初期化
      shmem[page_number].state = NOT_SET; // NOT_SETの状態に戻す
      memset((void*)shmem[page_number].PHADDR, 0, PGSIZE); // 全部0で初期化
      shmem[page_number].lock.name = NULL; // spinlockのnameを一応NULLにしておく
      shmem[page_number].using_top_pid = -1; // プロセスがアクセスしているかどうかの情報をリセット
    }
    release(&p->lock);
    release(&shmem[page_number].state_lock);
  }else {
    panic("The process don't have access to this shmem page");
  }
}
```

### `exit()`の変更
`proc.c`に定義される`void exit(int status)`の`wakeup(p->parent);`の後に
``` c
acquire(&p->lock);

  for(int i=0; i<SHMEM; i++){ //.ADD
    acquire(&shmem[i].state_lock);
    // アクセスを持っていたのならカウンタを減らす
    if(p->shmem_access[i]){
      shmem[i].numberofusers--;
    }

    // numberofusersが0かつtop_pidが一致すれば、そのページの中身を適当なダミー(0)で初期化し、NOT_SETにする
    if(shmem[i].numberofusers == 0 && (shmem[i].using_top_pid == p->top_pid)){
      shmem[i].state = NOT_SET;
      shmem[i].using_top_pid = -1;
      memset((void*)shmem[i].PHADDR, 0, PGSIZE);
    }
    release(&shmem[i].state_lock);
  }
```
を追加した。全ての`shmem`に対し、`state`のためのspinlockをかけてから`exit()`を実行したプロセスがアクセス権をもっているかがチェックされ、アクセス権を持っていた場合は、該当する`shmem`のアクセス数を減らす。その後、アクセス数が0かつ`top_pid`が一致した場合は、`state`を`NOT_SET`に変更してから、`using_top_pid`と物理アドレス先の値を初期化する。その後、spinlockを解除する。そのため、`shmem_set_init_int()`で値が設定され、`NOT_SET`の時に、`top_pid`が一致するプロセスで`exit()`が実行されると設定した値は初期化される。これにより、別のプロセスが`shmem_set_init_int()`で設定した値を読み取ることができないようになっている。

### `fork()`の変更
`proc.c`に定義される`int fork(void)`の`release(&np->lock);`の後に、
``` c
acquire(&wait_lock);
  np->parent = p;
  np->top_pid = p->top_pid; // トッププロセスのpidを引き継ぐ
  release(&wait_lock);

  acquire(&np->lock);
  np->state = RUNNABLE;
  for(int i=0; i<SHMEM; i++){ //.ADD
    if((np->shmem_access[i] = p->shmem_access[i])){
        // 親がアクセスしているshmemページを子にもアクセスさせる
        // その分のカウンタを増やす
        shmem[i].numberofusers++;
    }
  }
  release(&np->lock);
```
を追加した。親プロセスの`top_pid`と`shmem`のアクセスを引き継がせている。`shmem`のアクセス権を持っていた場合は、引き継ぐ分、`shmem`のアクセス数を増やしている。

### `bool is_top_parent_process(void)`の実装
この関数が`exec()`で実行されているならば`true`を、`fork()`で実行されているならば`false`を返す。判定は、`pid`が`top_pid`と一致しているかで行なっている。`proc`が取得できなかった場合はpanicを起こす。
``` c
// 今のプロセスが最上位のプロセスかを判定
bool is_top_parent_process(void) {
  struct proc *p = myproc();
  if (p == NULL) {
      panic("is_top_parent_process: myproc() returned NULL");
  }
  
  return (p->top_pid == p->pid); // トッププロセスかどうかを返す
}
```

### 目的以外の実装の変更
ユーザー関数が一定以上の数になると、ビルド中に、
``` shell
shmem_test user/_shmem_test1 user/_shmem_test2 user/_shmem_acquire user/_is_top user/_prim user/_call_opendfd 
nmeta 46 (boot, super, log blocks 30 inode blocks 13, bitmap blocks 1) blocks 954 total 1000
read: Undefined error: 0
make: *** [fs.img] Error 1
```
というエラーがでたため、`kernel/param.h`を
``` c
#define FSSIZE       1000  // size of file system in blocks
```
から
``` c
#define FSSIZE       2000  // size of file system in blocks
```
に変更をした。これは、ファイルシステムイメージのサイズの上限である1000ブロックを超過していたために出ていたエラーであり、その上限を増やすことで解決をした。