#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/riscv.h"
#include "user/user.h"
#include "kernel/param.h"


#define MAX_ALLOC_SIZE 512

struct Header {
	uint len;
	uint free;
	struct Header *prev;
};

#define HEADER_SIZE sizeof(struct Header)


void* ustack_malloc(uint len);

struct Header *get_free_block(struct Header **first, uint len);

struct Header *add_space(struct Header* head, uint len);

int ustack_free(void);



