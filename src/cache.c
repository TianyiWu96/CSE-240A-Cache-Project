//========================================================//
//  cache.c                                               //
//  Source file for the Cache Simulator                   //
//                                                        //
//  Implement the I-cache, D-Cache and L2-cache as        //
//  described in the README                               //
//========================================================//

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "cache.h"

//
// TODO:Student Information
//
const char *studentName = "Fanjin Zeng";
const char *studentID   = "A53238021";
const char *email       = "f1zeng@ucsd.edu";

//------------------------------------//
//        Cache Configuration         //
//------------------------------------//

uint32_t icacheSets;     // Number of sets in the I$
uint32_t icacheAssoc;    // Associativity of the I$
uint32_t icacheHitTime;  // Hit Time of the I$

uint32_t dcacheSets;     // Number of sets in the D$
uint32_t dcacheAssoc;    // Associativity of the D$
uint32_t dcacheHitTime;  // Hit Time of the D$

uint32_t l2cacheSets;    // Number of sets in the L2$
uint32_t l2cacheAssoc;   // Associativity of the L2$
uint32_t l2cacheHitTime; // Hit Time of the L2$
uint32_t inclusive;      // Indicates if the L2 is inclusive

uint32_t blocksize;      // Block/Line size
uint32_t memspeed;       // Latency of Main Memory

//------------------------------------//
//          Cache Statistics          //
//------------------------------------//

uint64_t icacheRefs;       // I$ references
uint64_t icacheMisses;     // I$ misses
uint64_t icachePenalties;  // I$ penalties

uint64_t dcacheRefs;       // D$ references
uint64_t dcacheMisses;     // D$ misses
uint64_t dcachePenalties;  // D$ penalties

uint64_t l2cacheRefs;      // L2$ references
uint64_t l2cacheMisses;    // L2$ misses
uint64_t l2cachePenalties; // L2$ penalties

//------------------------------------//
//        Cache Data Structures       //
//------------------------------------//

//
//TODO: Add your Cache data structures here
//

typedef struct Block
{
  struct Block *prev, *next;
  uint32_t val;
}Block;

typedef struct Set
{
  uint32_t size;
  Block *front, *back;
}Set;

Block* createBlock(uint32_t val)
{
  Block *b = (Block*)malloc(sizeof(Block));
  b->val = val;
  b->prev = NULL;
  b->next = NULL;

  return b;
}

void setPush(Set* s,  Block *b)
{
  if(s->size)
  {
    b->prev = s->back;
    s->back = b;
  }
  else
  {
    s->front = b;
    s->back = b;
  }
  (s->size)++;
}

void setPop(Set* s){
  if(!s->size)
    return;

  Block *p = s->front;
  s->front = p->next;

  if(s->front)
    s->front->prev = NULL;

  (s->size)--;
  free(p);
}

Block* setPopIndex(Set* s, int index){
  if(index > s->size)
    return NULL;

  Block *p = s->front;

  if(s->size == 1){
    s->front = NULL;
    s->back = NULL;
  }
  else if (index == 0)
  {
    s->front = p->next;
    s->front->prev = NULL;
  }
  else if (index == s->size - 1)
  {
    p = s->back;
    s->back = s->back->prev;
    s->back->next = NULL;
  }
  else{
    for(int i=0; i<index; i++)
      p = p->next;
    p->prev->next = p->next;
    p->next->prev = p->prev;
  }

  p->next = NULL;
  p->prev = NULL;

  (s->size)--;
  return p;
}

Set *icache;
Set *dcache;
Set *l2cache;

uint32_t offset_size;
uint32_t offset_mask;

uint32_t icache_index_mask;
uint32_t dcache_index_mask;
uint32_t l2cache_index_mask;

uint32_t icache_index_size;
uint32_t dcache_index_size;
uint32_t l2cache_index_size;

uint32_t icache_tag_size;
uint32_t dcache_tag_size;
uint32_t l2cache_tag_size;

uint32_t icache_tag_mask;
uint32_t dcache_tag_mask;
uint32_t l2cache_tag_mask;


//------------------------------------//
//          Cache Functions           //
//------------------------------------//


// Initialize the Cache Hierarchy
//
void
init_cache()
{
  // Initialize cache stats
  icacheRefs        = 0;
  icacheMisses      = 0;
  icachePenalties   = 0;
  dcacheRefs        = 0;
  dcacheMisses      = 0;
  dcachePenalties   = 0;
  l2cacheRefs       = 0;
  l2cacheMisses     = 0;
  l2cachePenalties  = 0;

  icache = (Set*)malloc(sizeof(Set) * icacheSets);
  dcache = (Set*)malloc(sizeof(Set) * dcacheSets);
  l2cache = (Set*)malloc(sizeof(Set) * l2cacheSets);

  for(int i=0; i<icacheSets; i++)
  {
    icache[i].size = 0;
    icache[i].front = NULL;
    icache[i].back = NULL;
  }

  for(int i=0; i<dcacheSets; i++)
  {
    dcache[i].size = 0;
    dcache[i].front = NULL;
    dcache[i].back = NULL;
  }

  for(int i=0; i<l2cacheSets; i++)
  {
    l2cache[i].size = 0;
    l2cache[i].front = NULL;
    l2cache[i].back = NULL;
  }

    offset_size = (uint32_t)log2(blocksize);
    offset_mask = (1 << offset_size) - 1;


    icache_index_size = (uint32_t)log2(icacheSets);
    dcache_index_size = (uint32_t)log2(dcacheSets);
    l2cache_index_size = (uint32_t)log2(l2cacheSets);

    icache_index_mask = ((1 << icache_index_size) - 1) << offset_size;
    dcache_index_mask = ((1 << dcache_index_size) - 1) << offset_size;
    l2cache_index_mask = ((1 << l2cache_index_size) - 1) << offset_size;

    icache_tag_size = 32 - icache_index_size - offset_size;
    dcache_tag_size = 32 - dcache_index_size - offset_size;
    l2cache_tag_size = 32 - l2cache_index_size - offset_size;

    icache_tag_mask = (1 << icache_tag_size) - 1;
    dcache_tag_mask = (1 << dcache_tag_size) - 1;
    l2cache_tag_mask = (1 << l2cache_tag_size) - 1;
}

// Perform a memory access through the icache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t icache_access(uint32_t addr)
{
  icacheRefs += 1;

  uint32_t offset = addr & offset_mask;
  uint32_t index = (addr & icache_index_mask) >> offset_size;
  uint32_t tag = addr >> (icacheSets + offset_size);

  Block *p = icache[index].front;

  for(int i=0; i<icache[index].size; i++){
    if((p->val & icache_tag_mask) == tag){ // Hit
      Block *b = setPopIndex(&icache[index], i); // Get the hit block
      setPush(&icache[index],  b); // move to end of set queue
      return icacheHitTime;
    }
    p = p->next;
  }

  icacheMisses += 1;

  // icache[index][rand()%icacheAssoc] = tag; // random replacement
  // Miss replacement - LRU
  Block *b = createBlock(tag);

  if(icache[index].size == icacheAssoc) // set filled, replace LRU (front of set queue)
    setPop(&icache[index]);
  setPush(&icache[index],  b);

  uint32_t penalty = l2cache_access(addr);
  icachePenalties += penalty;

  return penalty + icacheHitTime;
}

// Perform a memory access through the dcache interface for the address 'addr'
// Return the access time for the memory operation
//
uint32_t dcache_access(uint32_t addr)
{
  dcacheRefs += 1;

  uint32_t offset = addr & offset_mask;
  uint32_t index = (addr & dcache_index_mask) >> offset_size;
  uint32_t tag = addr >> (dcacheSets + offset_size);

  Block *p = dcache[index].front;

  for(int i=0; i<dcache[index].size; i++){
    if((p->val & dcache_tag_mask) == tag){ // Hit
      Block *b = setPopIndex(&dcache[index], i); // Get the hit block
      setPush(&dcache[index],  b); // move to end of set queue
      return dcacheHitTime;
    }
    p = p->next;
  }

  dcacheMisses += 1;

  // dcache[index][rand()%icacheAssoc] = tag; // random replacement
  // Miss replacement - LRU
  Block *b = createBlock(tag);

  if(dcache[index].size == dcacheAssoc) // set filled, replace LRU (front of set queue)
    setPop(&dcache[index]);
  setPush(&dcache[index],  b);

  uint32_t penalty = l2cache_access(addr);
  dcachePenalties += penalty;

  return penalty + dcacheHitTime;
}

// Perform a memory access to the l2cache for the address 'addr'
// Return the access time for the memory operation
//
uint32_t
l2cache_access(uint32_t addr)
{
  l2cacheRefs += 1;

  uint32_t offset = addr & offset_mask;
  uint32_t index = (addr & l2cache_index_mask) >> offset_size;
  uint32_t tag = addr >> (l2cacheSets + offset_size);

  Block *p = l2cache[index].front;

  for(int i=0; i<l2cache[index].size; i++){
    if((p->val & l2cache_tag_mask) == tag){ // Hit
      Block *b = setPopIndex(&l2cache[index], i); // Get the hit block
      setPush(&l2cache[index],  b); // move to end of set queue
      return l2cacheHitTime;
    }
    p = p->next;
  }

  l2cacheMisses += 1;

  // l2cache[index][rand()%icacheAssoc] = tag; // random replacement
  // Miss replacement - LRU
  Block *b = createBlock(tag);

  printf("%d", l2cache[index].size);
  if(l2cache[index].size == l2cacheAssoc) // set filled, replace LRU (front of set queue)
    setPop(&l2cache[index]);
  setPush(&l2cache[index],  b);

  l2cachePenalties += memspeed;
  return memspeed + l2cacheHitTime;
}
