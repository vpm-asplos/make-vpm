#define PERSISTENT_TMALLOC

#include <errno.h>
#include <linux/kernel.h>
#include <sys/syscall.h>
#include <unistd.h>

void *current_pbrk = 0x0;

// syscall of pbrk
// returns the old brk if failed
// returns the new brk if succeeded
void* sys_pbrk(void* paddr) {
	long ret_newpbrk = syscall(333, paddr);
	return (void*)ret_newpbrk;
}

// return 0 if increment succeed
// otherwise return -1 and errno is set to ENOMEM
int pbrk(void* paddr) {
	if(current_pbrk == 0x0){
		current_pbrk = sys_pbrk(0x0);
	}
	void* new_brk = sys_pbrk(paddr);
	if(new_brk == paddr) {
		current_pbrk = new_brk;
		return 0;
	} else {
		current_pbrk = new_brk;
		errno = ENOMEM;
		return -1;
	}
}

// return current pbrk when increment == 0
// otherwise, return the ret value from sys_pbrk
void* psbrk(intptr_t increment) {
	if(current_pbrk == 0x0) {
		current_pbrk = sys_pbrk(0x0);
	}
	if(increment == 0) {
		return current_pbrk;
	} else {
		void* updated_pbrk = (void*)((unsigned long)current_pbrk + increment);
		void* ret_newpbrk = (void*)sys_pbrk(updated_pbrk);
		current_pbrk = ret_newpbrk;
		return ret_newpbrk;
	}
}

