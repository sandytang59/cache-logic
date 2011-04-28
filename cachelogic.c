#include "tips.h"

/* The following two functions are defined in util.c */

/* finds the highest 1 bit, and returns its position, else 0 */
unsigned int uint_log2(word w); 

/* return random int from 0..x-1 */
int randomint( int x );

/* The following two global variable is used for LRU. */
cacheBlock ** head;
cacheBlock ** tail;

//if == 0, it is the first time compulsory miss. otherwise, == 1. 
int first_comp_miss_flag = 0;


/*
  This function allows the lru information to be displayed

    assoc_index - the cache unit that contains the block to be modified
    block_index - the index of the block to be modified

  returns a string representation of the lru information
 */
char* lru_to_string(int assoc_index, int block_index)
{
  /* Buffer to print lru information -- increase size as needed. */
  static char buffer[9];
  sprintf(buffer, "%u", cache[assoc_index].block[block_index].lru.value);

  return buffer;
}

/*
  This function initializes the lru information

    assoc_index - the cache unit that contains the block to be modified
    block_number - the index of the block to be modified

*/
void init_lru(int assoc_index, int block_index)
{
  cache[assoc_index].block[block_index].lru.value = 0;
}

/*
  This is the primary function you are filling out,
  You are free to add helper functions if you need them

  @param addr 32-bit byte address
  @param data a pointer to a SINGLE word (32-bits of data)
  @param we   if we == READ, then data used to return
              information back to CPU

              if we == WRITE, then data used to
              update Cache/DRAM
*/
void accessMemory(address addr, word* data, WriteEnable we)
{
  /* Declare variables here */

	//Break the address into tag, index and offset fields.

	unsigned int offset;
	unsigned int tag;
	unsigned int index;

	int offset_bit = uint_log2(block_size);
	int index_bit = uint_log2(set_count);
	int tag_bit = 32 - offset_bit - index_bit;

	//get offset value
	unsigned int mask = (1 << offset_bit) - 1;
	offset = mask & addr;

	//get index value
	mask = (1 << index_bit) - 1;
	index = mask & ( addr >> offset_bit );

	//get tag value
	mask = (1 << tag_bit) - 1;
	tag = mask & ( addr >> (offset_bit + index_bit) );

	//get the number of offsets
	int offset_num = 1 << offset_bit;

	//offset mask, it is used when call accessDRAM.
	unsigned int offset_mask = 0xffffffff << offset_bit;

	//Check instruction is READ or WRITE.
	//if READ
	if(we == READ){
		int i;
		//check every blocks in the index set.
		for(i=0; i < assoc; i++){
			//valid bit is 0.
			if( cache[index].block[i].valid == INVALID){
				break;
			}
			//valid bit is 1.
			else{
				//if it hits.
				if( cache[index].block[i].tag == tag){
					//data point to the first data in the specific block.
					byte* d = &(cache[index].block[i].data[offset]);
					*data = *(word*)d;

					//check policy of replacement.
					//if LRU.
					if(policy == LRU){

						//update LRU list.
						//if only one valid block in the index set.
						if( head[index] == tail[index] && head[index] == &(cache[index].block[i]) ){
							//do nothing
						}
						//if block i is the head block
						else if(head[index] == &(cache[index].block[i])){
							head[index] = cache[index].block[i].lru.data;
							tail[index]->lru.data = &(cache[index].block[i]);
							tail[index] = &(cache[index].block[i]);
							tail[index]->lru.data = NULL;
						}
						//if block i is the tail block
						else if(tail[index] == &(cache[index].block[i])){
							//do nothing..
						}
						//else find the previous block of block i
						else{
							//temporary iterator.
							cacheBlock * iter = head[index];
							//iterator for finding
							while( iter->lru.data != &(cache[index].block[i]) ){
								iter = iter->lru.data;
							}
							//connect previous node to the next node.
							iter->lru.data = cache[index].block[i].lru.data;
							//update the tail block.
							tail[index]->lru.data = &(cache[index].block[i]);
							tail[index] = &(cache[index].block[i]);
							tail[index]->lru.data = NULL;
						}
					}
					//if RANDOM.
					else{
						//do nothing
					}

					highlight_block(index, i);
					highlight_offset(index, i, offset, HIT);

					return;
				}
			}

			//if the last one is already checked, jump out of the loop instead of adding 1 more to i.
			if(i >= (assoc - 1) ){
				break;
			}
		}

		//Now it is a miss

		//Situation 1. Compulsory miss.
		if(cache[index].block[i].valid == INVALID){

			//if it is the first time compulsory miss, malloc spaces for head and tail.
			if(first_comp_miss_flag == 0){
				 head = (cacheBlock**)malloc(set_count * sizeof(cacheBlock*));
				 tail = (cacheBlock**)malloc(set_count * sizeof(cacheBlock*));
				 //set flag to 1.
				 first_comp_miss_flag = 1;
			}


			//check block size in byte
			TransferUnit bl_size = uint_log2(block_size);
			accessDRAM((offset_mask & addr), (byte*)data, bl_size, we);
			//replace the invalid block.
			byte* d_ptr = cache[index].block[i].data;
			byte* p = (byte*)data;

			int t;
			for(t =0; t< offset_num ; t++){
			d_ptr[t] = p[t];
			}

			//Change INVALID to VALID in valid bit.
			cache[index].block[i].valid = VALID;
			//Copy tag into the block.
			cache[index].block[i].tag = tag;

			//if LRU
			if(policy == LRU){

				//if this is the first block of the current set, let head and tail both point to it.
				if(i==0){
					head[index] = &(cache[index].block[i]);
					tail[index] = &(cache[index].block[i]);
				}
				//else connect it to the current tail block, then let it become the new tail block.
				else{
					tail[index]->lru.data = &(cache[index].block[i]);
					tail[index] = &(cache[index].block[i]);
				}

			}
			//return the data
			byte* d = &(cache[index].block[i].data[offset]);
			data = (word*)d;

			highlight_block(index, i);
			highlight_offset(index, i, offset, MISS);

			return;
		}

		//Situation 2. Miss!
		else {
			//if LRU
			if(policy == LRU){
				//check block size in byte
				TransferUnit bl_size = uint_log2(block_size);
				accessDRAM((offset_mask & addr), (byte*)data, bl_size, we);
				//replace by policy LRU.
				byte* d_ptr = head[index]->data;
				byte* p = (byte*)data;

				int t;
				for(t =0; t< offset_num ; t++){
				d_ptr[t] = p[t];
				}

				//high-light
				//use for loop to find the block index i.
				for(i=0; i< assoc; i++){
					if(cache[index].block[i].tag == head[index]->tag){
						break;
					}
				}

				highlight_block(index, i);
				highlight_offset(index, i, offset, MISS);

				//set new tag.
				head[index]->tag = tag;

				//update the lru list
				//if only one valid block in the index set.
				if(head[index] == tail[index]){
					//do nothing
				}
				//otherwise, normal method
				else{
					tail[index]->lru.data = head[index];
					head[index] = head[index]->lru.data;
					tail[index] = tail[index]->lru.data;
					tail[index]->lru.data = NULL;
				}

				//return the data
				byte* d = &(tail[index]->data[offset]);
				data = (word*)d;
			}
			//if RANDOM
			else{
				int rand_index = randomint(assoc);
				//check block size in byte
				TransferUnit bl_size = uint_log2(block_size);
				accessDRAM((offset_mask & addr), (byte*)data, bl_size, we);
				
				byte* d_ptr = cache[index].block[rand_index].data;
				byte* p = (byte*)data;

				int t;
				for(t =0; t< offset_num ; t++){
				d_ptr[t] = p[t];
				}

				//set new tag.
				cache[index].block[rand_index].tag = tag;

				//return the data
				byte* d = &(cache[index].block[rand_index].data[offset]);
				data = (word*)d;

				//highlight
				highlight_block(index, rand_index);
				highlight_offset(index, rand_index, offset, MISS);

			}


			return;
		}

	}

	//if write
	else{
		//if write through
		if(memory_sync_policy == WRITE_THROUGH){
			int j;
			//check every blocks in the index set.
			for(j=0; j < assoc; j++){
				//valid bit is 0.
				if( cache[index].block[j].valid == INVALID){
					break;
				}
			
				else{ 
					//if it is a hit
					if( cache[index].block[j].tag == tag){
					//change data in the memory
					accessDRAM(addr, (byte*)data, WORD_SIZE, we);
					//bring the data back to cache
					TransferUnit bl_size = uint_log2(block_size);
					accessDRAM((offset_mask & addr), (byte*)data, bl_size, READ);
					byte* d_ptr = cache[index].block[j].data;
					byte* p = (byte*)data;

					int t;
					for(t =0; t< offset_num ; t++){
						d_ptr[t] = p[t];
					}

					//if LRU
					if(policy == LRU){
						//update LRU list.

						//if only one valid block in the index set.
						if( head[index] == tail[index] && head[index] == &(cache[index].block[j]) ){
							//do nothing
						}
						//if block j is the head block
						else if(head[index] == &(cache[index].block[j])){
							head[index] = cache[index].block[j].lru.data;
							tail[index]->lru.data = &(cache[index].block[j]);
							tail[index] = &(cache[index].block[j]);
							tail[index]->lru.data = NULL;	
						}
						//if block j is the tail block
						else if(tail[index] == &(cache[index].block[j])){
							//do nothing..
						}
						//else find the previous block of block j
						else{
							//temporary iterator.
							cacheBlock * iter = head[index];
							//iterator for finding
							while(iter->lru.data != &(cache[index].block[j])){
								iter = iter->lru.data;
							}
							//connect previous node to the next node.
							iter->lru.data = cache[index].block[j].lru.data;
							//update the tail block.
							tail[index]->lru.data = &(cache[index].block[j]);
							tail[index] = &(cache[index].block[j]);
							tail[index]->lru.data = NULL;
						}
					}
					//if RANDOM
					else{
						//do nothing.
					}

					//highlight
					highlight_block(index, j);
					highlight_offset(index, j, offset, HIT);

						return;
					}
				}

				//if the last one is already checked, jump out of the loop instead of adding 1 more to i.
				if(j >= (assoc - 1) ){
					break;
				}

			}
			//if it misses
				//Situation 1. Compulsory miss.
			if (cache[index].block[j].valid == INVALID){

					//if it is the first time compulsory miss, malloc spaces for head and tail.
					if( first_comp_miss_flag == 0){
						 head = (cacheBlock**)malloc(set_count * sizeof(cacheBlock*));
						 tail = (cacheBlock**)malloc(set_count * sizeof(cacheBlock*));
						 //set flag to 1.
						 first_comp_miss_flag = 1;
					}

					//change data in the memory
					accessDRAM(addr, (byte*)data, WORD_SIZE, we);
					//bring the data back to cache
					TransferUnit bl_size = uint_log2(block_size);
					accessDRAM((offset_mask & addr), (byte*)data, bl_size, READ);

					byte* d_ptr = cache[index].block[j].data;
					byte* p = (byte*)data;

					int t;
					for(t =0; t< offset_num ; t++){
						d_ptr[t] = p[t];
					}

					//Change INVALID to VALID in valid bit.
					cache[index].block[j].valid = VALID;
					//Copy tag into the block.
					cache[index].block[j].tag = tag;

					//if LRU
					if(policy == LRU){

						//if this is the first block of the current set, let head and tail both point to it.
						if(j == 0){
							head[index] = &(cache[index].block[j]);
							tail[index] = &(cache[index].block[j]);
						}
						//else connect it to the current tail block, then let it become the new tail block.
						else{
							tail[index]->lru.data = &(cache[index].block[j]);
							tail[index] = &(cache[index].block[j]);
						}
					}
					//if RANDOM
					else{
						//do nothing.
					}

					//highlight
					highlight_block(index, j);
					highlight_offset(index, j, offset, MISS);

					return;
				}

				//Situation 2. Miss!
				else {

					//change data in the memory
					accessDRAM(addr, (byte*)data, WORD_SIZE, we);
					//bring the data back to cache
					TransferUnit bl_size = uint_log2(block_size);
					accessDRAM((offset_mask & addr), (byte*)data, bl_size, READ);
						
					//update cache

					//if LRU
					if(policy == LRU){
						//replace by policy LRU.
						byte* d_ptr = head[index]->data;
						byte* p = (byte*)data;

						int t;
						for(t =0; t< offset_num ; t++){
							d_ptr[t] = p[t];
						}

						//high-light
						//use for loop to find the block index i.
						for(j=0; j< assoc; j++){
							if(cache[index].block[j].tag == head[index]->tag){
								break;
							}
						}

						highlight_block(index, j);
						highlight_offset(index, j, offset, MISS);

						//set new tag.
						head[index]->tag = tag;

						//update the lru list
						//if only one valid block in the index set.
						if(head[index] == tail[index]){
							//do nothing
						}
						//otherwise, normal method
						else{
							tail[index]->lru.data = head[index];
							head[index] = head[index]->lru.data;
							tail[index] = tail[index]->lru.data;
							tail[index]->lru.data = NULL;
						}
					}
					else{
						//get random index
						int rand_index = randomint(assoc);

						byte* d_ptr = cache[index].block[rand_index].data;
						byte* p = (byte*)data;

						int t;
						for(t =0; t< offset_num ; t++){
							d_ptr[t] = p[t];
						}

						//set new tag.
						cache[index].block[rand_index].tag = tag;

						highlight_block(index, rand_index);
						highlight_offset(index, rand_index, offset, MISS);

					}
					return;
				}
		}
		//if write back
		else{
			int k;
			//check every blocks in the index set.
			for(k=0; k < assoc; k++){
				//valid bit is 0.
				if( cache[index].block[k].valid == INVALID){
					break;
				}
			
				else{ 
					//if it is a hit
					if( cache[index].block[k].tag == tag){
					//change the data in cache
					//create a temporary unsigned int pointer point to the offset data.
						byte* p_offset1 = &(cache[index].block[k].data[offset]);
						word* p_offset = (word*)p_offset1;
						*p_offset = *data;

					//change the dirty bit into 1
					cache[index].block[k].dirty = DIRTY;

					//if LRU
					if(policy == LRU){
						//update LRU list.
						//if only one valid block in the index set.
						if( head[index] == tail[index] && head[index] == &(cache[index].block[k]) ){
							//do nothing
						}
						//if block k is the head block
						else if(head[index] == &(cache[index].block[k])){
							head[index] = cache[index].block[k].lru.data;
							tail[index]->lru.data = &(cache[index].block[k]);
							tail[index] = &(cache[index].block[k]);
							tail[index]->lru.data = NULL;	
						}
						//if block k is the tail block
						else if(tail[index] == &(cache[index].block[k])){
							//do nothing..
						}
						//else find the previous block of block k
						else{
							//temporary iterator.
							cacheBlock * iter = head[index];
							//iterator for finding
							while(iter->lru.data != &(cache[index].block[k])){
								iter = iter->lru.data;
							}
							//connect previous node to the next node.
							iter->lru.data = cache[index].block[k].lru.data;
							//update the tail block.
							tail[index]->lru.data = &(cache[index].block[k]);
							tail[index] = &(cache[index].block[k]);
							tail[index]->lru.data = NULL;
						}
					}
					else{
						//do nothing
					}

					//highlight
					highlight_block(index, k);
					highlight_offset(index, k, offset, HIT);

						return;
					}
				}

				//if the last one is already checked, jump out of the loop instead of adding 1 more to i.
				if(k >= (assoc - 1) ){
					break;
				}

			}

			//if it misses.
				//Situation 1. Compulsory miss.
			if (cache[index].block[k].valid == INVALID){


					//if it is the first time compulsory miss, malloc spaces for head and tail.
					if(first_comp_miss_flag == 0){
						 head = (cacheBlock**)malloc(set_count * sizeof(cacheBlock*));
						 tail = (cacheBlock**)malloc(set_count * sizeof(cacheBlock*));
						 //set flag to 1.
						 first_comp_miss_flag = 1;
					}

					//bring the data to cache
					//before bring the data into cache, create a container to hold the data from DRAM
					TransferUnit bl_size = uint_log2(block_size);
					byte* container = (byte*)malloc(block_size);
					accessDRAM((offset_mask & addr), container, bl_size, READ);

					byte* d_ptr = cache[index].block[k].data;

					int t;
					for(t =0; t< offset_num ; t++){
						d_ptr[t] = container[t];
					}

					//update the cache with data.
					//create a temporary unsigned int pointer point to the offset data.
						d_ptr = &(cache[index].block[k].data[offset]);
						word* p_offset = (word*)d_ptr;
						*p_offset = *data;
					//change the dirty bit value
					cache[index].block[k].dirty = DIRTY;

					//Change INVALID to VALID in valid bit.
					cache[index].block[k].valid = VALID;
					//Copy tag into the block.
					cache[index].block[k].tag = tag;

					//if LRU
					if(policy == LRU){

						//if this is the first block of the current set, let head and tail both point to it.
						if(k == 0){
							head[index] = &(cache[index].block[k]);
							tail[index] = &(cache[index].block[k]);
						}
						//else connect it to the current tail block, then let it become the new tail block.
						else{
							tail[index]->lru.data = &(cache[index].block[k]);
							tail[index] = &(cache[index].block[k]);
						}
					}
					//if RANDOM
					else{
						//do nothing

					}

					//highlight
					highlight_block(index, k);
					highlight_offset(index, k, offset, MISS);

					return;
				}

				//Situation 2. Miss!
				else {

					int rand_index = randomint(assoc);

					if(policy == RANDOM){
						head[index] = &(cache[index].block[rand_index]);
					}


						//check the dirty bit of the head block of lru list, which is also the replaced block.
						//if it is VIRGIN.
						if(head[index]->dirty == VIRGIN){
							//bring the data back to cache from DRAM
							//before bring the data into cache, create a container to hold the data from DRAM
							
							TransferUnit bl_size = uint_log2(block_size);
							byte* container = (byte*)malloc(block_size);
							accessDRAM((offset_mask & addr), container, bl_size, READ);
						
							//update cache

							//if LRU
							if(policy == LRU){
								//replace by policy LRU.
								byte* d_ptr = head[index]->data;

								int t;
								for(t=0; t< offset_num; t++){
									d_ptr[t] = container[t];
								}

								//store the new data into cache.

								//create a temporary unsigned int pointer point to the offset data.
								d_ptr = &(head[index]->data[offset]);
								word* p_offset = (word*)d_ptr;

								*p_offset = *data;

								//high-light
								//use for loop to find the block index i.
								for(k=0; k< assoc; k++){
									if(cache[index].block[k].tag == head[index]->tag){
										break;
									}
								}

								highlight_block(index, k);
								highlight_offset(index, k, offset, MISS);

								//set dirty bit to 1
								head[index]->dirty = DIRTY;
								//set new tag.
								head[index]->tag = tag;

								//update the lru list
								//if only one valid block in the index set.
								if(head[index] == tail[index]){
									//do nothing
								}
								//otherwise, normal method
								else{
									tail[index]->lru.data = head[index];
									head[index] = head[index]->lru.data;
									tail[index] = tail[index]->lru.data;
									tail[index]->lru.data = NULL;
								}
							}
							//if RANDOM
							else{
								//get random index
								//int rand_index = randomint(assoc);

								byte* d_ptr = cache[index].block[rand_index].data;

								int t;
								for(t=0; t< offset_num; t++){
									d_ptr[t] = container[t];
								}

								//store the new data into cache.

								//create a temporary unsigned int pointer point to the offset data.
								d_ptr = &(cache[index].block[rand_index].data[offset]);
								word* p_offset = (word*)d_ptr;

								*p_offset = *data;

								//set dirty bit to 1
								cache[index].block[rand_index].dirty = DIRTY;
								//set new tag.
								cache[index].block[rand_index].tag = tag;

								//highlight
								highlight_block(index, rand_index);
								highlight_offset(index, rand_index, offset, MISS);
								

							}

							return;
						}
						//if it is DIRTY.
						else{

							//if LRU
							if(policy == LRU){
								//create a temp variable container to hold the data from DRAM
								byte* container = (byte*)malloc(block_size);

								//get the address of the head block, which is going to be replaced.
								address head_addr = 0xffffffff;
								//creating mask
								head_addr = head_addr >> tag_bit;
								//change all tag bits into 0.
								head_addr = head_addr & addr;
								//make a copy of the tag of the head block, and shift it to the original situation.
								unsigned int head_tag = head[index]->tag << (32 - tag_bit);
								//combine to get the final address.
								head_addr = head_addr | head_tag;

								TransferUnit bl_size = uint_log2(block_size);
								//save the head block into DRAM.
								accessDRAM((head_addr & offset_mask), (byte*)head[index]->data, bl_size, WRITE);
								//get the data block from DRAM.
								accessDRAM((offset_mask & addr), container, bl_size, READ);
								//update head block of cache.

								byte* d_ptr = head[index]->data;

								int t;
								for(t=0; t< offset_num; t++){
									d_ptr[t] = container[t];
								}

								//store the new data into the offset of the block.
								//create a temporary unsigned int pointer point to the offset data.
								d_ptr = &(head[index]->data[offset]);
								word* p_offset = (word*)d_ptr;

								*p_offset = *data;

								//high-light
								//use for loop to find the block index i.
								for(k=0; k< assoc; k++){
									if(cache[index].block[k].tag == head[index]->tag){
										break;
									}
								}

								highlight_block(index, k);
								highlight_offset(index, k, offset, MISS);

								//change tag.
								head[index]->tag = tag;
								//change dirty bit.
								head[index]->dirty = DIRTY;

								//update LRU list.
								//if only one valid block in the index set.
								if(head[index] == tail[index]){
									//do nothing
								}
								//otherwise, normal method
								else{
									tail[index]->lru.data = head[index];
									head[index] = head[index]->lru.data;
									tail[index] = tail[index]->lru.data;
									tail[index]->lru.data = NULL;
								}

							}
							//if RANDOM
							else{
								//create a temp variable container to hold the data from DRAM
								byte* container = (byte*)malloc(block_size);

								//int rand_index = randomint(assoc);

								//get the address of the block, which is going to be replaced.
								address replace_addr = 0xffffffff;
								//creating mask
								replace_addr = replace_addr >> tag_bit;
								//change all tag bits into 0.
								replace_addr = replace_addr & addr;
								//make a copy of the tag of the replacing block, and shift it to the original situation.
								unsigned int replace_tag = cache[index].block[rand_index].tag << (32 - tag_bit);
								//combine to get the final address.
								replace_addr = replace_addr | replace_tag;

								TransferUnit bl_size = uint_log2(block_size);
								//save the replacing block into DRAM.
								accessDRAM((replace_addr & offset_mask), (byte*)(cache[index].block[rand_index].data), bl_size, WRITE);
								//get the data block from DRAM.
								accessDRAM((offset_mask & addr), container, bl_size, READ);
								//update replacing block of cache.

								byte* d_ptr = cache[index].block[rand_index].data;

								int t;
								for(t=0; t< offset_num; t++){
									d_ptr[t] = container[t];
								}

								//store the new data into the offset of the block.
								//create a temporary unsigned int pointer point to the offset data.
								d_ptr = &(cache[index].block[rand_index].data[offset]);
								word* p_offset = (word*)d_ptr;

								*p_offset = *data;

								//change tag.
								cache[index].block[rand_index].tag = tag;
								//change dirty bit.
								cache[index].block[rand_index].dirty = DIRTY;

								//highlight
								highlight_block(index, rand_index);
								highlight_offset(index, rand_index, offset, MISS);
							}

							return;
						}
				}
		}
		}

	

  /* handle the case of no cache at all - leave this in */
  if(assoc == 0) {
    accessDRAM(addr, (byte*)data, WORD_SIZE, we);
    return;
  }

  /*
  You need to read/write between memory (via the accessDRAM() function) and
  the cache (via the cache[] global structure defined in tips.h)

  Remember to read tips.h for all the global variables that tell you the
  cache parameters

  The same code should handle random and LRU policies. Test the policy
  variable (see tips.h) to decide which policy to execute. The LRU policy
  should be written such that no two blocks (when their valid bit is VALID)
  will ever be a candidate for replacement.

  Your cache should be able to support write-through mode (any writes to
  the cache get immediately copied to main memory also) and write-back mode
  (and writes to the cache only gets copied to main memory when the block
  is kicked out of the cache.

  Also, cache should do allocate-on-write. This means, a write operation
  will bring in an entire block if the block is not already in the cache.

  To properly work with the GUI, the code needs to tell the GUI code
  when to redraw and when to flash things. Descriptions of the animation
  functions can be found in tips.h
  */

  /* Start adding code here */


  /* This call to accessDRAM occurs when you modify any of the
     cache parameters. It is provided as a stop gap solution.
     At some point, once you have more of your cachelogic in place,
     this line should be removed.
  */
  //accessDRAM(addr, (byte*)data, WORD_SIZE, we);
}