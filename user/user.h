#include <stdbool.h>

struct stat;

// system calls
int fork(void);
int exit(int) __attribute__((noreturn));
int wait(int*);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(const char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
int sleep(int);
int uptime(void);
uint64 opendfd(void); //自分で書いたやつ(sys 4の資料の課題)
// https://github.com/Pablo-Izquierdo/RiscV-xv6/blob/main/Projects/P3
// で実装されていた関数
//void *shmem_access(int page_number); // あまり安全じゃないので、使えないようにする
int shmem_count(int page_number);
// 自分で書いた関数
bool is_top_parent_process(void); // 最上位のプロセスかどうかを判定する
int shmem_state(int page_number);
void shmem_set_writable(int page_number);
void shmem_set_unwritable(int page_number);
//void shmem_set_unwritable_and_init_int(int page_number, int size, int* values);
void shmem_set_init_int(int page_number, int index, int value);
void shmem_write_int(int page_number, int index, int value);
int shmem_read_int(int page_number, int index);
bool shmem_acquire_access(int page_number);
void shmem_release_access(int page_number);

// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
void *memmove(void*, const void*, int);
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void fprintf(int, const char*, ...);
void printf(const char*, ...);
char* gets(char*, int max);
uint strlen(const char*);
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
int memcmp(const void *, const void *, uint);
void *memcpy(void *, const void *, uint);
