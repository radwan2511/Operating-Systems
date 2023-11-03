#include "ustack.h"

void* linked_list_head = 0;
int overall_len_allocated = 0;


struct Header *get_free_block(struct Header **head, uint len)
{
    struct Header *current;
	for(current = linked_list_head; current && !(current->free && current->len >= len); current = current->prev)
	{
		*head = current;
	}
    return current;
}

void* ustack_malloc(uint len)
{
	struct Header *head;
	
	if(len <= 0 || len > MAX_ALLOC_SIZE)
	{
	  return (void*)-1;
	}
	if(linked_list_head)
	{
		struct Header* header = linked_list_head;
		head = get_free_block(&header, len);
		if(!head) 
		{
			// failed to find free block
			head = add_space(header, len);
			if (!head)
			{
				return (void*)-1;
			}
		}
		else 
		{
			// found a free block
			head->free = 0;
		}
	}
	else
	{
	    head = add_space(0, len);
	    if(!head)
	    {
	        return (void*)-1; // failed
	    }
		linked_list_head = head;
	}
	return (void*)(head+1);
}

struct Header *add_space(struct Header* head, uint len)
{
	struct Header* header;
	header = (struct Header *)sbrk(0);
	void* adding = sbrk(len + HEADER_SIZE);
	if (adding == (void*)-1) // sbrk failed
	{
	    return 0; 
	}
	overall_len_allocated += len + HEADER_SIZE;
	if(head)
	{
	    head->prev = header;
	}
	
	header->len = len;
	header->prev = 0;
	header->free = 0;
	
	return header;
}

int ustack_free(void)
{
	struct Header* header = linked_list_head;
	
	if(!linked_list_head)
	{
		return -1;
	}
	
	int len = header->len;
	header->free = 1;
	
	linked_list_head = header->prev;
	if(!linked_list_head)
	{
		//printf("\nhere\n");
	    sbrk(-PGSIZE);
		//sbrk(-overall_len_allocated);
		//overall_len_allocated = 0;
	}
	return len;
}




