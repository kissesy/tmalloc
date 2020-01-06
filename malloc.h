#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#define FASTBIN_SIZE 7
#define SMALLBIN_SIZE 32
#define SMALLBIN 3
#define UNSORTEDBIN 4


#pragma pack(push, 1)
typedef struct _Malloc_Chunk{
	uint64_t prev_size;
	uint64_t size;
	struct _Malloc_Chunk* fd;
	struct _Malloc_Chunk* bk;
}MChunk;
#pragma pack(pop)

typedef struct _Malloc_Info{
	MChunk* fastbinY[FASTBIN_SIZE];
	MChunk* smallbinY[SMALLBIN_SIZE];
	MChunk* unsortedbin;
	MChunk* top_address;
	int a;
}Malloc_Info;

#define PREV_INUSE 0x1
#define SIZE_BITS (PREV_INUSE)

#define PAGE_SIZE 4096
#define MAX_FASTBIN_SIZE 128
#define USERDATA 16

#define chunk_at_offset(p, s)  ((MChunk*) (((char *) (p)) + (s)))
#define prev_inuse(p) ((p)->size & PREV_INUSE))
#define chunksize(p) ((p)->size & ~(SIZE_BITS))
#define next_chunk(p) ((MChunk*)(((char*)(p)) + ((p)->size & ~SIZE_BITS)))
#define prev_chunk(p) ((MChunk*)(((char*)(p)) - ((p)->prev_size)))

//after unlink P's address return
#define smallbin_index(sz) (((uint64_t)(sz) >> 4) - 1)
#define fastbin_index(sz) (((uint64_t)(sz) >> 4) - 1)
#define ConvertSize(request_size, align) ((request_size + (align-1)) & ~(align-1))+16; //일단 16을 더하는 식으로 하자.


/*malloc function*/
void* tmalloc(uint64_t request_size);
//if return is NULL, converte_size's chunk isn't exist in fastbin, is not NULL, return value is chunk's address
void* find_fastbin(uint64_t convert_size);
void* find_unsortedbin(uint64_t convert_size);
void* find_smallbin(uint64_t convert_size);
void _tmalloc_init();
void* division_topchunk(uint64_t convert_size);
void grow_topchunk();
void* setting_address(MChunk* maddress, uint64_t convert_size);


//chunk를 빼내온다.
void chunk_unlink(MChunk* P, MChunk** bin_base);

/*free function*/
void find_chunk_in_bin(MChunk* target);
void* chunk_consolidate(MChunk* ruler, MChunk* victim);
void topchunk_consolidate(MChunk* victim);

void tfree(void* maddress);
void put_smallbin(MChunk* chunk_address);
void put_unsortedbin(MChunk* chunk_address);
void put_fastbin(MChunk* chunk_address);
void* return_EndOfList(MChunk* chunk_list);

/*etc*/
void set_inuse(MChunk* chunk);
void clear_inuse(MChunk* chunk);
void tmalloc_error(const char* err);
void debug_linked_list(MChunk* head);
