#include "malloc.h"

Malloc_Info mallinfo;
bool init_bit = true;

void _tmalloc_init(){
  if(init_bit == true){
    for(int i=0;i<FASTBIN_SIZE;i++){
      mallinfo.fastbinY[i] = NULL;
    }
    for(int i=0;i<SMALLBIN_SIZE;i++){
      mallinfo.smallbinY[i] = NULL;
    }
    mallinfo.unsortedbin = NULL;
    init_bit = false;
    //make top chunk
    mallinfo.top_address = (MChunk*)sbrk(PAGE_SIZE);
    mallinfo.top_address->size = PAGE_SIZE;
    set_inuse(mallinfo.top_address);
  }else{
    return;
  }
}

void* tmalloc(uint64_t request_size){
  MChunk* maddress = NULL;
  if(request_size > 0x380){
    tmalloc_error("request too many size");
    return NULL;
  }
  uint64_t convert_size = ConvertSize(request_size, 16);
  _tmalloc_init();
  //find fastbin
  if(convert_size <= MAX_FASTBIN_SIZE){
    puts("[DEBUG] call find_fastbin function");
    if((maddress = (MChunk*)find_fastbin(convert_size)) != NULL){
      puts("[DEBUG] finished find_fastbin function");
      return (void*)setting_address(maddress, convert_size);
    }
  }
  else if(convert_size <= 0x380 && convert_size > MAX_FASTBIN_SIZE){
    puts("[DEBUG] call find_unsortedbin function");
    if((maddress = (MChunk*)find_unsortedbin(convert_size)) != NULL){
      return (void*)setting_address(maddress, convert_size);
    }
    puts("[DEBUG] call func_smallbin function");
    if((maddress = (MChunk*)find_smallbin(convert_size)) != NULL){
      return (void*)setting_address(maddress, convert_size);
    }
  }
  puts("[DEBUG] call division_topchunk function");
  if((maddress = (MChunk*)division_topchunk(convert_size)) != NULL){
    return (void*)setting_address(maddress, convert_size);
  }
  tmalloc_error("tmalloc can't allocate memory in heap");
  return NULL;
}

void* setting_address(MChunk* maddress, uint64_t convert_size){
  maddress->prev_size = 0;
  maddress->size = convert_size;
  set_inuse(maddress);
  MChunk* return_chunk = chunk_at_offset(maddress, USERDATA);
  return return_chunk;
}


//fastbin은 항상 마지막을 free된 chunk를 가리키고 있어야 함
void* find_fastbin(uint64_t convert_size){
  uint64_t index = fastbin_index(convert_size);
  if(mallinfo.fastbinY[index] == NULL){
    puts("[DEBUG] find_fastbin return NULL");
    return NULL;
  }
  MChunk* return_chunk = mallinfo.fastbinY[index];
  mallinfo.fastbinY[index] = mallinfo.fastbinY[index]->fd;
  return return_chunk;
}

//새로 만들어진 chunk_chunk_unlink에서 chunk에 대한 chunk_chunk_unlink가 수행됨 + 예외처리도
//base is unsortedbin or smallbin

//base는 이중포인터로 줘야한느거 같은데  로직 한 번 더 체킹 하기
void chunk_unlink(MChunk* P, MChunk** bin_base){
  //단일 청크
  MChunk* FD;
  MChunk* BK;
  if((P->bk == NULL) && (P->fd == NULL)){
    *bin_base = NULL;
  } else if((P->bk == NULL) != (P->fd == NULL)){
    //청크 2개
    if (P->fd == NULL) {
      //맨 뒤
      P->fd = NULL;
    } else {
      //맨 처음
      *bin_base = P->fd;
      P->bk = NULL;
    }
  }
  //그 외
  else{
    FD = P->fd;
    BK = P->bk;
    FD->bk = BK;
    BK->fd = FD;
  }
  return;
}

void* find_unsortedbin(uint64_t convert_size){
  if(mallinfo.unsortedbin == NULL){
    puts("[DEBUG] find_unsortedbin return NULL");
    return NULL;
  } else{
    MChunk* chunk = mallinfo.unsortedbin;
    printf("[DEBUG] request size : %lx\n", convert_size);
    do{
      MChunk* victim = chunk;
      chunk_unlink(victim, &mallinfo.unsortedbin);
      if(chunksize(victim) == convert_size){
        return victim;
      } else{
        put_smallbin(victim);
      }
      chunk = chunk->fd;
    }while(chunk != NULL);
  }
}
//smallbin은 index 관리하기 떄문에 크기가 다 동일하지 않나? unsortedbin 처럼 검사할 필요가 없지
//그냥 가장 처음만 제거하면 되는데
//이 find_smallbin은 malloc 당시에 찾는 청크이다.
//만약 청크 병합으로 smallbin을 찾는다면 다른 chunk_unlink를 통해 제거
void* find_smallbin(uint64_t convert_size){
  uint64_t index = smallbin_index(convert_size);
  if(mallinfo.smallbinY[index] == NULL){
    return NULL;
  } else{
    //smallbin의 가장 첫 번쨰 chunk를 return 한다.
    MChunk* return_chunk = mallinfo.smallbinY[index];
    mallinfo.smallbinY[index] = mallinfo.smallbinY[index]->fd;
    mallinfo.smallbinY[index]->bk = NULL;
    return return_chunk;
  }
}


//만들어진 top chunk에서 top chunk를 가리키는 포인터를 convert_size만큼 옮기자. 그리고 set_inuse도 다시 설정
//만약 탑 청크가 부족하다면 새로 파야함.
void* division_topchunk(uint64_t convert_size){
  puts("[DEBUG] call division_topchunk");
  //if leak topchunk size than request_size
  while(convert_size > chunksize(mallinfo.top_address)){
    grow_topchunk();
  }
  clear_inuse(mallinfo.top_address);
  uint64_t original_topsize = chunksize(mallinfo.top_address);
  MChunk* return_address = mallinfo.top_address;
  MChunk* new_top = chunk_at_offset(mallinfo.top_address, convert_size);
  mallinfo.top_address = new_top;
  mallinfo.top_address->size = original_topsize - convert_size;
  set_inuse(mallinfo.top_address);
  return return_address;
}

//sbrk를 쓰는것이 맞는가 brk가 맞느가
void grow_topchunk(){
  clear_inuse(mallinfo.top_address);
  sbrk(PAGE_SIZE);
  mallinfo.top_address->size += PAGE_SIZE;
  set_inuse(mallinfo.top_address);
}

//chunk consolidate 과정에서 찾아야할 주소
void find_chunk_in_bin(MChunk* target){
  //first find in unsortedbin
  MChunk* chunk = mallinfo.unsortedbin;
  do{
    if(chunk == target){
      chunk_unlink(chunk, &mallinfo.unsortedbin);
      return;
    }
    chunk = chunk->fd;
  }while(chunk != NULL);
  //second find size to index small bin
  uint64_t index = smallbin_index(chunksize(target));
  chunk = mallinfo.smallbinY[index];
  do{
    if(chunk == target){
      chunk_unlink(chunk, &mallinfo.smallbinY[index]);
      return;
    }
    chunk = chunk->fd;
  }while(chunk != NULL);
  printf("[DEBUG] what.... exist bug!\n");
}

void* chunk_consolidate(MChunk* ruler, MChunk* victim){
  ruler->size += chunksize(victim);
  return ruler;
}

void topchunk_consolidate(MChunk* victim){
  uint64_t new_topsize = chunksize(mallinfo.top_address) + chunksize(victim);
  mallinfo.top_address = (MChunk*)chunk_at_offset(mallinfo.top_address, -chunksize(victim));
  set_inuse(mallinfo.top_address);
}

/*-----------------------------------------------------------------*/
//현재 청크가 free 될떄 다음 chunk size를 보고 unsortedbin에 들어갈 chunk의 size이면 그 chunk의 prev_size에 값을 넣어준다.
void tfree(void* maddress){
  if(maddress == NULL){
    tmalloc_error("NULL POINTER FREE?");
    return;
  }
  MChunk* free_chunk = chunk_at_offset(maddress, -USERDATA);
  MChunk* NextChunk = NULL;
  if(chunksize(free_chunk) <= 128){
    put_fastbin(free_chunk);
    return;
  } else{
    //만약 이전 청크가 free된 상타라면
    puts("[DEBUG] free chunk more than fastbin size");
    if(!prev_inuse(free_chunk){
      puts("[DEBUG] previous chunk is freed!");
      find_chunk_in_bin(prev_chunk(free_chunk));
      free_chunk = (MChunk*)chunk_consolidate(prev_chunk(free_chunk), free_chunk);
    }
    //다음 청크가 top chunk라면
    if(next_chunk(free_chunk) == mallinfo.top_address){
      puts("[DEBUG] next chunk is top chunk");
      topchunk_consolidate(free_chunk);
      return;
    }
    //다음 chunk가 free된 chunk라면
    else if(!prev_inuse(next_chunk(next_chunk(free_chunk))){
      puts("[DEBUG] next chunk is freed!");
      NextChunk = (MChunk*)next_chunk(next_chunk(free_chunk));
      find_chunk_in_bin(NextChunk);
      free_chunk = (MChunk*)chunk_consolidate(free_chunk, NextChunk);
    }
    if(next_chunk(free_chunk) != mallinfo.top_address){
      puts("[DEBUG] next chunk isn't top address. So, clear inuse bit");
      NextChunk = (MChunk*)next_chunk(free_chunk);
      clear_inuse(NextChunk);
      NextChunk->prev_size = chunksize(free_chunk);
      //원래는 smallbin이나 어떤 규칙에 다라서 넣어야한느데 예외처리 -> 무조건 unsortedbin으로 넣음
      //put_unsortedbin(free_chunk); //이 위치가 맞는가도 확인하기
    }
    puts("[DEBUG] call put unsortedbin function");
    put_unsortedbin(free_chunk);
    puts("[DEBUG] call debug function");
    debug_linked_list(mallinfo.unsortedbin);
    puts("[DEBUG] finished debug function");
    return;
  }
}

void* return_EndOfList(MChunk* chunk_list){
  MChunk* EndChunk = chunk_list;
  while(EndChunk->fd != NULL){
    EndChunk = EndChunk->fd;
  }
  return EndChunk;
}

void put_smallbin(MChunk* chunk_address){
  printf("[DEBUG] put smallbin %p\n", chunk_address);
  uint64_t index = smallbin_index(chunksize(chunk_address));
  if (mallinfo.smallbinY[index] == NULL) {
    chunk_address->fd = NULL;
    chunk_address->bk = NULL;
    mallinfo.smallbinY[index] = chunk_address;
  } else {
    MChunk* EndChunk = (MChunk*)return_EndOfList(mallinfo.smallbinY[index]);
    chunk_address->fd = NULL;
    chunk_address->bk = EndChunk;
    EndChunk->fd = chunk_address;
  }
}

void debug_linked_list(MChunk* head){
  do{
    printf("[DEBUG] chunk : %p size : %lx\n", head, chunksize(head));
    head = head->fd;
  }while(head != NULL);
}

void put_unsortedbin(MChunk* chunk_address){
  printf("[DEBUG] put unsortedbin %p\n", chunk_address);
  if (mallinfo.unsortedbin == NULL) {
    chunk_address->fd = NULL;
    chunk_address->bk = NULL;
    mallinfo.unsortedbin = chunk_address;
  } else {
    MChunk* EndChunk = (MChunk*)return_EndOfList(mallinfo.unsortedbin);
    chunk_address->fd = NULL;
    chunk_address->bk = EndChunk;
    EndChunk->fd = chunk_address;
  }
}
//fastbin은 마지막에 free된 chunk를 가리켜야함.
//fastbinY[index]랑 fastbinY[index]->fd 중간에 값을 넝어야함.
//중복해서 해지할 때 fastbin top이랑 주소가 같은지 확인
void put_fastbin(MChunk* chunk_address){
  uint64_t index = fastbin_index(chunksize(chunk_address));
  if(mallinfo.fastbinY[index] == NULL){
    chunk_address->fd = NULL;
    mallinfo.fastbinY[index] = chunk_address;
  } else{
    MChunk* top_fastbin = mallinfo.fastbinY[index];
    if(top_fastbin == chunk_address){
      tmalloc_error("fastbin double free bug!"); //#################
    }
    mallinfo.fastbinY[index] = chunk_address;
    mallinfo.fastbinY[index]->fd = top_fastbin->fd;
  }
}

/*===================================================*/

void set_inuse(MChunk* chunk){
  chunk->size += PREV_INUSE;
}

void clear_inuse(MChunk* chunk){
  chunk->size -= PREV_INUSE;
}

void tmalloc_error(const char* err){
  printf("[ERROR] %s\n",err);
}
