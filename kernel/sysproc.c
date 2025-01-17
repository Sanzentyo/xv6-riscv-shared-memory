#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "stddef.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

bool sys_is_top_parent_process(void){
  return is_top_parent_process();
}

void* sys_shmem_access(void){ //.ADD

  //Parameters
  int page_number;
  if (argint(0,&page_number) < 0){
    return NULL;
  }

  //Check parameters
  if (page_number < 0 || page_number > SHMEM - 1){
    return NULL;
  }

  return get_shmem_access(page_number, 0);
}

int sys_shmem_count(void){ //.ADD

  //Prarameters
  int page_number;

  if (argint(0,&page_number) < 0){
    return -1;
  }

  //Check parameters
  if (page_number < 0 || page_number > SHMEM - 1){
    return -1;
  }

  return count_shmem(page_number);
}

// 自分で追加した関数
int sys_shmem_state(void){

  //Parameters
  int page_number;

  if (argint(0,&page_number) < 0){
    return -1;
  }

  //Check parameters
  if (page_number < 0 || page_number > SHMEM - 1){
    return -1;
  }

  return shmem_state(page_number);
}

void sys_shmem_set_writable(void){
  //Parameters
  int page_number;

  if (argint(0,&page_number) < 0){
    return;
  }

  //Check parameters
  if (page_number < 0 || page_number > SHMEM - 1){
    return;
  }

  shmem_set_writable(page_number);
}

void sys_shmem_set_unwritable(void){
  //Parameters
  int page_number;

  if (argint(0,&page_number) < 0){
    return;
  }

  //Check parameters
  if (page_number < 0 || page_number > SHMEM - 1){
    return;
  }

  shmem_set_unwritable(page_number);
}

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

void sys_shmem_write_int(void){
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

  shmem_write_int(page_number, index, value);
}

uint64 sys_shmem_read_int(void){
  //Parameters
  int page_number;
  int index;

  if (argint(0,&page_number) < 0){
    return -1;
  }

  if (argint(1,&index) < 0){
    return -1;
  }

  //Check parameters
  if (page_number < 0 || page_number > SHMEM - 1){
    return -1;
  }

  return shmem_read_int(page_number, index);
}

bool sys_shmem_acquire_access(void){
  //Parameters
  int page_number;

  if (argint(0,&page_number) < 0){
    return false;
  }

  //Check parameters
  if (page_number < 0 || page_number > SHMEM - 1){
    return false;
  }

  return shmem_acquire_access(page_number);
}

void sys_shmem_release_access(void){
  //Parameters
  int page_number;

  if (argint(0,&page_number) < 0){
    return;
  }

  //Check parameters
  if (page_number < 0 || page_number > SHMEM - 1){
    return;
  }

  shmem_release_access(page_number);
}