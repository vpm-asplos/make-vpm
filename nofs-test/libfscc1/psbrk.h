#define PERSISTENT_TMALLOC

int pbrk(void* paddr);
void* psbrk(intptr_t increment);
