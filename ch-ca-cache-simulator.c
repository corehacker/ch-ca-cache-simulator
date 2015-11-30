/*******************************************************************************
 * Copyright (c) 2013, Sandeep Prakash <123sandy@gmail.com>
 * All rights reserved.
 *
 * \file   ch-ca-cache-simulator.c
 * \author sandeepprakash
 *
 * \date   Feb 4, 2013
 *
 * \brief  A simple cache simulator.
 *
 ******************************************************************************/

/********************************** INCLUDES **********************************/
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#include <stdbool.h>
#include <getopt.h>

/********************************* CONSTANTS **********************************/

/*********************************** MACROS ***********************************/
#define CACHESIM_MAX_NO_OF_BLOCKS_PER_SET                (128)

#define CACHESIM_MAX_NO_OF_SETS                          (128)

#define CACHESIM_PAUSE_TIME_BW_ACCESSES                  (0)

#define CACHESIM_MAX_RAM_BLOCKS                          (1024)

#define CACHESIM_BLOCK_SIZE_BYTES                        \
   (CACHESIM_BLOCK_SIZE_IN_NO_OF_WORDS * CACHESIM_WORD_SIZE_IN_BYTES)

#define CACHESIM_MAX_INDEX_VALUE                         (UINT32_MAX)

#define CACHESIM_DIRECT_MAP_MEMORY_TO_RAM_BLOCK(ui_mem_idx,ui_no_of_words)     \
   (ui_mem_idx / ui_no_of_words)

#define CACHESIM_DIRECT_MAP_RAM_BLOCK_TO_CACHE_BLOCK(ui_ram_block)  \
   (ui_ram_block % CACHESIM_TOTAL_NO_OF_BLOCKS)

#define CACHESIM_SET_MAP_RAM_BLOCK_TO_CACHE_SET(ui_ram_block,ui_no_of_sets)  \
   (ui_ram_block % ui_no_of_sets)

#define CACHESIM_NO_OF_BLOCKS_PER_SET(px_cache_params)                    \
   ((px_cache_params)->ui_associativity)

#define CACHESIM_NO_OF_BLOCKS(px_cache_params)                         \
   ((px_cache_params)->ui_cache_size_words / (px_cache_params)->ui_block_size_words)

#define CACHESIM_NO_OF_SETS(px_cache_params)  \
   (CACHESIM_NO_OF_BLOCKS(px_cache_params) / CACHESIM_NO_OF_BLOCKS_PER_SET(px_cache_params))

#define CACHESIM_BLOCK_SIZE_IN_BYTES(px_cache_params)                        \
   ((px_cache_params)->ui_block_size_words * (px_cache_params)->ui_word_size_bytes)

/******************************** ENUMERATIONS ********************************/
typedef enum _CACHESIM_RET_E
{
   eCACHESIM_RET_SUCCESS         = 0x00000000,

   eCACHESIM_RET_FAILURE,

   eCACHESIM_RET_MAX
} CACHESIM_RET_E;

/************************* STRUCTURE/UNION DATA TYPES *************************/
typedef struct _CACHESIM_CACHE_PARAMS_X
{
   uint32_t ui_cache_size_words;

   uint32_t ui_associativity;

   uint32_t ui_block_size_words;

   uint32_t ui_word_size_bytes;
} CACHESIM_CACHE_PARAMS_X;

typedef enum _CACHESIM_SIMULATION_ALGORITHM_E
{
   eCACHESIM_SIMULATION_ALGORITHM_INVALID,

   eCACHESIM_SIMULATION_ALGORITHM_GENERAL,

   eCACHESIM_SIMULATION_ALGORITHM_BUBBLE_SORT,

   eCACHESIM_SIMULATION_ALGORITHM_MAX_IN_MATRIX,

   eCACHESIM_SIMULATION_ALGORITHM_MAX
} CACHESIM_SIMULATION_ALGORITHM_E;

typedef struct _CACHESIM_CACHE_ARGS_X
{
   uint32_t ui_cache_size_words;

   uint32_t ui_associativity;

   uint32_t ui_block_size_words;

   uint32_t ui_word_size_bytes;

   CACHESIM_SIMULATION_ALGORITHM_E e_algorithm;

   bool b_simulate_pinning;

   bool b_silent;

   uint32_t ui_loop_iterations;
} CACHESIM_CACHE_ARGS_X;

typedef struct _CACHESIM_SIM_STATS_X
{
   uint32_t ui_total_accesses;

   uint32_t ui_hit_count;

   uint32_t ui_miss_count;

   uint32_t ui_capacity_miss;

   uint32_t ui_compulsory_miss;

   uint32_t ui_conflict_miss;

   bool     ba_first_access_to_ram_blk[CACHESIM_MAX_RAM_BLOCKS];
} CACHESIM_SIM_STATS_X;

typedef struct _CACHE_BLOCK_METADATA_X
{
   uint32_t ui_cache_block_idx;

   uint32_t ui_data_start_idx;

   uint32_t ui_data_end_idx;

   bool b_is_pinned;
} CACHE_BLOCK_METADATA_X;

typedef struct _CACHE_BLOCK_DATA_X
{
   uint32_t *pui_data_ptr;

   uint32_t ui_data_size_words;
} CACHE_BLOCK_DATA_X;

typedef struct _CACHE_BLOCK_X
{
   CACHE_BLOCK_METADATA_X  x_metadata;

   CACHE_BLOCK_DATA_X      x_data;
} CACHE_BLOCK_X;

typedef struct _CACHE_SET_DATA_X
{
   uint32_t ui_set_idx;

   uint32_t ui_last_fetched_block;

   CACHE_BLOCK_X     xa_blocks[CACHESIM_MAX_NO_OF_BLOCKS_PER_SET];
} CACHE_SET_DATA_X;

typedef struct _CACHE_SET_X
{
   CACHE_SET_DATA_X        xa_sets[CACHESIM_MAX_NO_OF_SETS];

   uint32_t                ui_configured_no_of_sets;

   uint32_t                ui_no_of_blocks_per_set;

   CACHESIM_CACHE_PARAMS_X x_cache_params;
} CACHE_SET_X;

/************************ STATIC FUNCTION PROTOTYPES **************************/
static CACHESIM_RET_E cachesim_set_alloc_cache(
   CACHE_SET_X **ppx_cache,
   CACHESIM_CACHE_PARAMS_X *px_cache_params);

static CACHESIM_RET_E cachesim_set_free_cache(
   CACHE_SET_X *px_cache);

static CACHESIM_RET_E cachesim_set_fetch_data_to_cache(
   CACHE_SET_X *px_cache,
   uint32_t ui_data_index,
   uint32_t *pui_cache_set);

static CACHESIM_RET_E cachesim_set_fetch_data_to_cache_v2(
   CACHE_SET_X *px_cache,
   uint32_t ui_data_index,
   bool b_pin_block,
   uint32_t *pui_cache_set,
   uint32_t *pui_block_idx);

static bool cachesim_set_lookup_cache(
   CACHE_SET_X *px_cache,
   uint32_t ui_array_idx,
   uint32_t *pui_cache_set);

static bool cachesim_set_lookup_cache_v2(
   CACHE_SET_X *px_cache,
   uint32_t ui_array_idx,
   uint32_t *pui_cache_set,
   uint32_t *pui_block_idx);

static CACHESIM_RET_E cachesim_set_mapped_cache_simulate(
   CACHE_SET_X *px_cache,
   bool b_use_pinning,
   bool b_silent);

static CACHESIM_RET_E cachesim_set_handle_cache_hit (
   CACHE_SET_X *px_cache,
   CACHESIM_SIM_STATS_X *px_stats);

static CACHESIM_RET_E cachesim_set_handle_cache_miss (
	CACHE_SET_X *px_cache,
	uint32_t ui_index,
	bool b_use_pinning,
	bool *pb_compulsory,
    uint32_t *pui_cache_set,
    uint32_t *pui_block_idx,
	CACHESIM_SIM_STATS_X *px_stats);

static void cachesim_set_log_cache_access (
   CACHE_SET_X *px_cache,
   uint32_t ui_cache_set,
   uint32_t ui_block_idx,
   bool b_cache_hit,
   bool b_compulsory,
   bool b_silent);

static CACHESIM_RET_E cachesim_set_mapped_cache_access (
   CACHE_SET_X *px_cache,
   uint32_t ui_index,
   bool b_use_pinning,
   uint32_t ui_total_data_words,
   bool b_silent,
   CACHESIM_SIM_STATS_X *px_stats);

static CACHESIM_RET_E cachesim_set_mapped_cache_simulate_bubble_sort(
   CACHE_SET_X *px_cache,
   bool b_silent,
   uint32_t ui_n);

#ifdef _WIN32
void usleep(unsigned int usec);
#endif

/****************************** LOCAL FUNCTIONS *******************************/
#ifdef _WIN32
void usleep(unsigned int usec)
{
	HANDLE timer;
	LARGE_INTEGER ft;

	ft.QuadPart = -(10 * (__int64)usec);

	timer = CreateWaitableTimer(NULL, TRUE, NULL);
	SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
	WaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);
}
#endif

static void cachesim_print_cache_params (CACHE_SET_X *px_cache)
{
	if (NULL != px_cache)
	{
		CACHESIM_CACHE_PARAMS_X *px_cache_params = &(px_cache->x_cache_params);
		printf ("Cache Params:\n"
			  "\tui_cache_size_words     : %d\n"
			  "\tui_associativity        : %d\n"
			  "\tui_block_size_words     : %d\n"
			  "\tui_word_size_bytes      : %d\n"
			  "\tui_no_of_sets           : %d\n"
			  "\tui_no_of_blocks_per_set : %d\n", px_cache_params->ui_cache_size_words,
			  px_cache_params->ui_associativity, px_cache_params->ui_block_size_words,
			  px_cache_params->ui_word_size_bytes, px_cache->ui_configured_no_of_sets,
			  px_cache->ui_no_of_blocks_per_set);
	}
}

static CACHESIM_RET_E cachesim_set_alloc_cache(
   CACHE_SET_X **ppx_cache,
   CACHESIM_CACHE_PARAMS_X *px_cache_params)
{
   CACHESIM_RET_E e_ret_val = eCACHESIM_RET_FAILURE;
   uint32_t ui_i = 0;
   uint32_t ui_j = 0;
   CACHE_SET_X *px_cache = NULL;
   CACHE_SET_DATA_X *px_set_data = NULL;
   CACHE_BLOCK_X *px_block = NULL;
   CACHE_BLOCK_METADATA_X  *px_metadata = NULL;
   CACHE_BLOCK_DATA_X      *px_data = NULL;
   uint32_t ui_no_of_sets = 0;

   if ((NULL == ppx_cache) || (NULL == px_cache_params))
   {
      goto CLEAN_RETURN;
   }

   /*
    * Calculate the number of sets in the cache. If the associativity is 1
    * then (ui_cache_size_words / ui_block_size_words) will be the number of
    * sets and (ui_cache_size_words / ui_block_size_words) / ui_associativity)
    * otherwise.
    */
   ui_no_of_sets = CACHESIM_NO_OF_SETS(px_cache_params);
   if (ui_no_of_sets > CACHESIM_MAX_NO_OF_SETS)
   {
      goto CLEAN_RETURN;
   }

   px_cache = malloc (sizeof(CACHE_SET_X));
   if (NULL == px_cache)
   {
      goto CLEAN_RETURN;
   }

   (void) memmove (&(px_cache->x_cache_params), px_cache_params,
      sizeof (px_cache->x_cache_params));

   px_cache->ui_configured_no_of_sets = ui_no_of_sets;

   /*
    * Calculate number of blocks per set. This is directly related to
    * associativity.
    */
   px_cache->ui_no_of_blocks_per_set =
      CACHESIM_NO_OF_BLOCKS_PER_SET(&(px_cache->x_cache_params));

   // cachesim_print_cache_params (px_cache);
   /*
    * Initialize each set of the cache.
    */
   for (ui_i = 0; ui_i < px_cache->ui_configured_no_of_sets; ui_i++)
   {
      px_set_data = &(px_cache->xa_sets[ui_i]);
      px_set_data->ui_set_idx = ui_i;
      px_set_data->ui_last_fetched_block = CACHESIM_MAX_INDEX_VALUE;

      /*
       * Initialize each block in the set.
       */
      for (ui_j = 0; ui_j < px_cache->ui_no_of_blocks_per_set; ui_j++)
      {
         px_block = &(px_set_data->xa_blocks[ui_j]);
         px_metadata = &(px_block->x_metadata);
         px_data = &(px_block->x_data);

         px_metadata->ui_cache_block_idx = ui_j;
         px_metadata->ui_data_start_idx = CACHESIM_MAX_INDEX_VALUE;
         px_metadata->ui_data_end_idx = CACHESIM_MAX_INDEX_VALUE;

         px_data->ui_data_size_words = px_cache_params->ui_block_size_words;
         px_data->pui_data_ptr = malloc(CACHESIM_BLOCK_SIZE_IN_BYTES(px_cache_params));
         if (NULL == px_data->pui_data_ptr)
         {
            break;
         }
      }

      if (px_cache->ui_no_of_blocks_per_set != ui_j)
      {
         break;
      }
   }

   if (px_cache->ui_configured_no_of_sets != ui_i)
   {
      e_ret_val = eCACHESIM_RET_FAILURE;
   }
   else
   {
      *ppx_cache = px_cache;
      e_ret_val = eCACHESIM_RET_SUCCESS;
   }

CLEAN_RETURN:
   if (eCACHESIM_RET_SUCCESS != e_ret_val)
   {
      (void) cachesim_set_free_cache (px_cache);
   }
   return e_ret_val;
}

static CACHESIM_RET_E cachesim_set_free_cache(
   CACHE_SET_X *px_cache)
{
   CACHESIM_RET_E e_ret_val = eCACHESIM_RET_FAILURE;
   uint32_t ui_i = 0;
   uint32_t ui_j = 0;
   CACHE_SET_DATA_X *px_set_data = NULL;
   CACHE_BLOCK_X *px_block = NULL;
   CACHE_BLOCK_DATA_X      *px_data = NULL;

   if (NULL == px_cache)
   {
      goto CLEAN_RETURN;
   }

   /*
    * Deinitialize each set of the cache.
    */
   for (ui_i = 0; ui_i < px_cache->ui_configured_no_of_sets; ui_i++)
   {
      px_set_data = &(px_cache->xa_sets[ui_i]);

      /*
       * Deinitialize each block of the set.
       */
      for (ui_j = 0; ui_j < px_cache->ui_no_of_blocks_per_set; ui_j++)
      {
         px_block = &(px_set_data->xa_blocks[ui_j]);
         px_data = &(px_block->x_data);

         if (NULL != px_data->pui_data_ptr)
         {
            free (px_data->pui_data_ptr);
            px_data->pui_data_ptr = NULL;
         }
      }
   }
   free (px_cache);
   px_cache = NULL;
   e_ret_val = eCACHESIM_RET_SUCCESS;
CLEAN_RETURN:
   return e_ret_val;
}

static CACHESIM_RET_E cachesim_set_fetch_data_to_cache(
   CACHE_SET_X *px_cache,
   uint32_t ui_data_index,
   uint32_t *pui_cache_set)
{
   CACHESIM_RET_E e_ret_val = eCACHESIM_RET_FAILURE;
   uint32_t ui_ram_block = 0;
   uint32_t ui_cache_set = 0;
   uint32_t ui_ram_block_start_idx = 0;
   uint32_t ui_ram_block_end_idx = 0;
   CACHE_SET_DATA_X *px_cache_set_data = NULL;
   CACHE_BLOCK_X *px_cache_block = NULL;
   CACHE_BLOCK_METADATA_X  *px_metadata = NULL;
   uint32_t ui_fetch_index = 0;

   if ((NULL == px_cache) || (NULL == pui_cache_set))
   {
      goto CLEAN_RETURN;
   }

   /*
    * Map the memory word given the index to the block in the RAM. The RAM block
    * for this dataset is assumed to start from 0th index.
    */
   ui_ram_block = CACHESIM_DIRECT_MAP_MEMORY_TO_RAM_BLOCK (ui_data_index,
      px_cache->x_cache_params.ui_block_size_words);

   /*
    * Map the RAM block to the corresponding cache set. This will only determine
    * cache set in the cache. The code follows for the FIFO implementation
    * to kick-out the first block which had come into the cache set.
    */
   ui_cache_set = CACHESIM_SET_MAP_RAM_BLOCK_TO_CACHE_SET(ui_ram_block,
      px_cache->ui_configured_no_of_sets);

   px_cache_set_data = &(px_cache->xa_sets[ui_cache_set]);

   /*
    * The last fetched block keeps track of the head of the FIFO queue. Module
    * arithmetic is used simulate FIFO. So if its a 4-way associative then the
    * following is the sequence of blocks in the set that will be replaced:
    * (initial) 0 - 1 - 2 - 3 - 0 - 1 - 2 - 3 - 0 - ...
    */
   if (CACHESIM_MAX_INDEX_VALUE == px_cache_set_data->ui_last_fetched_block)
   {
      px_cache_set_data->ui_last_fetched_block = 0;
   }
   else
   {
      px_cache_set_data->ui_last_fetched_block++;
      px_cache_set_data->ui_last_fetched_block %=
            px_cache->ui_no_of_blocks_per_set;
   }

   /*
    * Temp variable.
    */
   ui_fetch_index = px_cache_set_data->ui_last_fetched_block;

   px_cache_block = &(px_cache_set_data->xa_blocks[ui_fetch_index]);
   px_metadata = &(px_cache_block->x_metadata);

   /*
    * Set the metadata for the current indices of the RAM words.
    */
   ui_ram_block_start_idx = ui_ram_block
      * px_cache->x_cache_params.ui_block_size_words;
   ui_ram_block_end_idx = ui_ram_block_start_idx
      + px_cache->x_cache_params.ui_block_size_words - 1;

   px_metadata->ui_data_start_idx = ui_ram_block_start_idx;
   px_metadata->ui_data_end_idx = ui_ram_block_end_idx;
   *pui_cache_set = ui_cache_set;
   e_ret_val = eCACHESIM_RET_SUCCESS;
CLEAN_RETURN:
   return e_ret_val;
}

static CACHESIM_RET_E cachesim_set_fetch_data_to_cache_v2(
   CACHE_SET_X *px_cache,
   uint32_t ui_data_index,
   bool b_pin_block,
   uint32_t *pui_cache_set,
   uint32_t *pui_block_idx)
{
   CACHESIM_RET_E e_ret_val = eCACHESIM_RET_FAILURE;
   uint32_t ui_ram_block = 0;
   uint32_t ui_cache_set = 0;
   uint32_t ui_ram_block_start_idx = 0;
   uint32_t ui_ram_block_end_idx = 0;
   CACHE_SET_DATA_X *px_cache_set_data = NULL;
   CACHE_BLOCK_X *px_cache_block = NULL;
   CACHE_BLOCK_METADATA_X  *px_metadata = NULL;
   uint32_t ui_fetch_index = 0;

   if ((NULL == px_cache) || (NULL == pui_cache_set))
   {
      goto CLEAN_RETURN;
   }

   /*
    * Map the memory word given the index to the block in the RAM. The RAM block
    * for this dataset is assumed to start from 0th index.
    */
   ui_ram_block = CACHESIM_DIRECT_MAP_MEMORY_TO_RAM_BLOCK (ui_data_index,
      px_cache->x_cache_params.ui_block_size_words);

   /*
    * Map the RAM block to the corresponding cache set. This will only determine
    * cache set in the cache. The code follows for the FIFO implementation
    * to kick-out the first block which had come into the cache set.
    */
   ui_cache_set = CACHESIM_SET_MAP_RAM_BLOCK_TO_CACHE_SET(ui_ram_block,
      px_cache->ui_configured_no_of_sets);

   px_cache_set_data = &(px_cache->xa_sets[ui_cache_set]);

   /*
    * The last fetched block keeps track of the head of the FIFO queue. Module
    * arithmetic is used simulate FIFO. So if its a 4-way associative then the
    * following is the sequence of blocks in the set that will be replaced:
    * (initial) 0 - 1 - 2 - 3 - 0 - 1 - 2 - 3 - 0 - ...
    */
   if (CACHESIM_MAX_INDEX_VALUE == px_cache_set_data->ui_last_fetched_block)
   {
      px_cache_set_data->ui_last_fetched_block = 0;
   }
   else
   {
	   uint32_t ui_try_counter = 0;
	   bool b_cacheable_block_found = false;
	  while (1)
	  {
		  px_cache_set_data->ui_last_fetched_block++;
		  px_cache_set_data->ui_last_fetched_block %=
				  px_cache->ui_no_of_blocks_per_set;
		  ui_fetch_index = px_cache_set_data->ui_last_fetched_block;
		  px_cache_block = &(px_cache_set_data->xa_blocks[ui_fetch_index]);
		  px_metadata = &(px_cache_block->x_metadata);
		  if (false == px_metadata->b_is_pinned)
		  {
			  b_cacheable_block_found = true;
			  break;
		  }
		  // printf ("Skipping pinned block");
		  ui_try_counter++;
		  if (ui_try_counter >= px_cache->ui_no_of_blocks_per_set)
		  {
			  printf ("Tried all blocks in the set. Cannot be cached");
			  b_cacheable_block_found = false;
			  break;
		  }
	  }
	  if (false == b_cacheable_block_found)
	  {
		  e_ret_val = eCACHESIM_RET_FAILURE;
		  goto CLEAN_RETURN;
	  }
   }

   /*
    * Temp variable.
    */
   ui_fetch_index = px_cache_set_data->ui_last_fetched_block;

   px_cache_block = &(px_cache_set_data->xa_blocks[ui_fetch_index]);
   px_metadata = &(px_cache_block->x_metadata);

   /*
    * Set the metadata for the current indices of the RAM words.
    */
   ui_ram_block_start_idx = ui_ram_block
      * px_cache->x_cache_params.ui_block_size_words;
   ui_ram_block_end_idx = ui_ram_block_start_idx
      + px_cache->x_cache_params.ui_block_size_words - 1;

   px_metadata->ui_data_start_idx = ui_ram_block_start_idx;
   px_metadata->ui_data_end_idx = ui_ram_block_end_idx;
   if (px_cache->x_cache_params.ui_associativity >= 2 && 0 == ui_fetch_index && true == b_pin_block)
   {
	   px_metadata->b_is_pinned = true;
   }
   *pui_cache_set = ui_cache_set;
   *pui_block_idx = ui_fetch_index;
   e_ret_val = eCACHESIM_RET_SUCCESS;
CLEAN_RETURN:
   return e_ret_val;
}

static bool cachesim_set_lookup_cache(
   CACHE_SET_X *px_cache,
   uint32_t ui_array_idx,
   uint32_t *pui_cache_set)
{
   bool b_cache_hit = false;
   uint32_t ui_i = 0;
   uint32_t ui_j = 0;
   CACHE_BLOCK_X *px_cache_block = NULL;
   CACHE_BLOCK_METADATA_X  *px_metadata = NULL;
   CACHE_SET_DATA_X *px_set_data = NULL;

   if ((NULL == px_cache) || (NULL == pui_cache_set))
   {
      goto CLEAN_RETURN;
   }

   /*
    * Loop through all the sets to find the data word in the cache.
    */
   for (ui_i = 0; ui_i < px_cache->ui_configured_no_of_sets; ui_i++)
   {
      /*
       * Loop through all the blocks in the set to find the data word in
       * the cache.
       */
      px_set_data = &(px_cache->xa_sets[ui_i]);
      for (ui_j = 0; ui_j < px_cache->ui_no_of_blocks_per_set; ui_j++)
      {
         px_cache_block = &(px_set_data->xa_blocks[ui_j]);
         px_metadata = &(px_cache_block->x_metadata);

         /*
          * If the array index is between the start and end indices in the cache
          * block then it is a cache hit.
          */
         if ((px_metadata->ui_data_start_idx <= ui_array_idx)
            && (px_metadata->ui_data_end_idx >= ui_array_idx))
         {
            *pui_cache_set = ui_i;
            b_cache_hit = true;
            break;
         }
      }
      if (true == b_cache_hit)
      {
         break;
      }
   }
   /*
    * If the loop ends without setting b_cache_hit, then it is a miss.
    */
CLEAN_RETURN:
   return b_cache_hit;
}

static bool cachesim_set_lookup_cache_v2(
   CACHE_SET_X *px_cache,
   uint32_t ui_array_idx,
   uint32_t *pui_cache_set,
   uint32_t *pui_block_idx)
{
   bool b_cache_hit = false;
   uint32_t ui_i = 0;
   uint32_t ui_j = 0;
   CACHE_BLOCK_X *px_cache_block = NULL;
   CACHE_BLOCK_METADATA_X  *px_metadata = NULL;
   CACHE_SET_DATA_X *px_set_data = NULL;

   if ((NULL == px_cache) || (NULL == pui_cache_set))
   {
      goto CLEAN_RETURN;
   }

   /*
    * Loop through all the sets to find the data word in the cache.
    */
   for (ui_i = 0; ui_i < px_cache->ui_configured_no_of_sets; ui_i++)
   {
      /*
       * Loop through all the blocks in the set to find the data word in
       * the cache.
       */
      px_set_data = &(px_cache->xa_sets[ui_i]);
      for (ui_j = 0; ui_j < px_cache->ui_no_of_blocks_per_set; ui_j++)
      {
         px_cache_block = &(px_set_data->xa_blocks[ui_j]);
         px_metadata = &(px_cache_block->x_metadata);

         /*
          * If the array index is between the start and end indices in the cache
          * block then it is a cache hit.
          */
         if ((px_metadata->ui_data_start_idx <= ui_array_idx)
            && (px_metadata->ui_data_end_idx >= ui_array_idx))
         {
            *pui_cache_set = ui_i;
            *pui_block_idx = ui_j;
            b_cache_hit = true;
            break;
         }
      }
      if (true == b_cache_hit)
      {
         break;
      }
   }
   /*
    * If the loop ends without setting b_cache_hit, then it is a miss.
    */
CLEAN_RETURN:
   return b_cache_hit;
}

static void cachesim_print_log_header (
   CACHE_SET_X *px_cache)
{
   uint32_t ui_i = 0;
   uint32_t ui_l = 0;
   CACHE_SET_DATA_X *px_cache_set = NULL;
   char uca_format_string [256] = { '\0' };
   char uca_line_str[256] = { '\0' };
   uint32_t ui_line_str_len = 0;
   uint32_t ui_width_qualifier = 0;

   ui_width_qualifier = px_cache->ui_no_of_blocks_per_set * 9; //strlen ("%4d-%4d");
   ui_width_qualifier += (px_cache->ui_no_of_blocks_per_set - 1);

   printf ("%7s |", " RAM Idx");

   snprintf ((uca_format_string), sizeof(uca_format_string), " %%%dd |",
      ui_width_qualifier);

   for (ui_i = 0; ui_i < px_cache->ui_configured_no_of_sets; ui_i++)
   {
      px_cache_set = &(px_cache->xa_sets[ui_i]);
      printf (uca_format_string, px_cache_set->ui_set_idx);

   }

   printf (" Hit/Miss | Set/Blk Idx |\n");


   printf ("%8s-+", "--------");

   (void) memset (uca_format_string, 0x00, sizeof(uca_format_string));

   snprintf ((uca_format_string), sizeof(uca_format_string), "-%%%ds-",
      ui_width_qualifier);
   for (ui_l = 0; ui_l < ui_width_qualifier; ui_l++)
   {
      ui_line_str_len = strnlen ((uca_line_str), sizeof(uca_line_str));
      snprintf ((uca_line_str + ui_line_str_len), sizeof(uca_line_str), "-");
   }
   for (ui_l = 0; ui_l < px_cache->ui_configured_no_of_sets; ui_l++)
   {
      printf (uca_format_string, uca_line_str);

      if (ui_l < (px_cache->ui_configured_no_of_sets - 1))
      {
         printf ("+");
      }
   }

   printf ("+-%8s-+", "--------");
   printf ("-%11s-+", "-----------");

   printf ("\n");
}

static CACHESIM_RET_E cachesim_set_handle_cache_hit (
	CACHE_SET_X *px_cache,
	CACHESIM_SIM_STATS_X *px_stats)
{
	CACHESIM_RET_E e_ret_val = eCACHESIM_RET_FAILURE;

	if ((NULL == px_cache) || (NULL == px_stats))
    {
	    goto CLEAN_RETURN;
    }
	px_stats->ui_hit_count++;
	e_ret_val = eCACHESIM_RET_SUCCESS;
CLEAN_RETURN:
    return e_ret_val;
}

static CACHESIM_RET_E cachesim_set_handle_cache_miss (
	CACHE_SET_X *px_cache,
	uint32_t ui_index,
	bool b_use_pinning,
	bool *pb_compulsory,
    uint32_t *pui_cache_set,
    uint32_t *pui_block_idx,
	CACHESIM_SIM_STATS_X *px_stats)
{
	CACHESIM_RET_E e_ret_val = eCACHESIM_RET_FAILURE;
    uint32_t ui_ram_block = CACHESIM_MAX_INDEX_VALUE;

	if ((NULL == px_cache) || (NULL == px_stats) || (NULL == pb_compulsory) ||
			(NULL == pui_cache_set) || (NULL == pui_block_idx))
    {
	    goto CLEAN_RETURN;
    }
    /*
     * Get a mapping of the current memory referenced index to the block in
     * the RAM.
     */
    ui_ram_block =
       (ui_index
          / (px_cache->x_cache_params.ui_block_size_words));
    if (ui_ram_block >= CACHESIM_MAX_RAM_BLOCKS)
    {
       printf ("\n\n!!!!!!!!!!!FATAL ERROR!!!!!!!!!!!!\n\n");
       exit (-1);
    }

    /*
     * The ba_first_access_to_ram_blk array holds a flag to tell whether the
     * access to the RAM block was for the first time or not. Therefore for
     * each RAM block check the status, if it false, then it is the first
     * access to that block and a compulsory miss is noted. Otherwise it is a
     * capacity miss. Conflict misses are not being tracked for obvious
     * reasons.
     */
    if (false == px_stats->ba_first_access_to_ram_blk [ui_ram_block])
    {
       px_stats->ba_first_access_to_ram_blk [ui_ram_block] = true;
       px_stats->ui_compulsory_miss++;
       *pb_compulsory = true;
    }
    else
    {
       px_stats->ui_capacity_miss++;
    }

    px_stats->ui_miss_count++;

    /*
     * Word not present in the cache. Fetch from block from RAM.
     */
    e_ret_val = cachesim_set_fetch_data_to_cache_v2 (px_cache, ui_index, b_use_pinning,
       pui_cache_set, pui_block_idx);
CLEAN_RETURN:
    return e_ret_val;
}

static void cachesim_set_log_cache_access (
   CACHE_SET_X *px_cache,
   uint32_t ui_cache_set,
   uint32_t ui_block_idx,
   bool b_cache_hit,
   bool b_compulsory,
   bool b_silent)
{
   uint32_t ui_k = 0;
   uint32_t ui_l = 0;
   CACHE_SET_DATA_X *px_cache_set = NULL;
   CACHE_BLOCK_X *px_cache_block = NULL;
   CACHE_BLOCK_METADATA_X  *px_metadata = NULL;

   if (NULL == px_cache)
   {
	goto CLEAN_RETURN;
   }

   if (false == b_silent)
   {
	  /*
	   * Just for logging purpose.
	   */
	  for (ui_l = 0; ui_l < px_cache->ui_configured_no_of_sets; ui_l++)
	  {
		 px_cache_set = &(px_cache->xa_sets [ui_l]);
		 for (ui_k = 0; ui_k < px_cache->ui_no_of_blocks_per_set; ui_k++)
		 {
			px_cache_block = &(px_cache_set->xa_blocks [ui_k]);
			px_metadata = &(px_cache_block->x_metadata);

			printf ("%4d-%4d",
			   (CACHESIM_MAX_INDEX_VALUE == px_metadata->ui_data_start_idx) ?
					 9999 : px_metadata->ui_data_start_idx,
			   (CACHESIM_MAX_INDEX_VALUE == px_metadata->ui_data_end_idx) ?
					 9999 : px_metadata->ui_data_end_idx);
			if (ui_k < px_cache->ui_no_of_blocks_per_set - 1)
			   printf ("/");
		 }
		 printf (" | ");
	  }

	  if (true == b_cache_hit)
	  {
		 printf ("%8s | %5d/%5d |\n", "Hit", ui_cache_set, ui_block_idx);
	  }
	  else
	  {
		 printf ("%8s | %5d/%5d |\n",
			(true == b_compulsory) ? "Com Miss" : "Cap Miss", ui_cache_set, ui_block_idx);
	  }
   }
CLEAN_RETURN:
   return;
}

static CACHESIM_RET_E cachesim_set_mapped_cache_access (
   CACHE_SET_X *px_cache,
   uint32_t ui_index,
   bool b_use_pinning,
   uint32_t ui_total_data_words,
   bool b_silent,
   CACHESIM_SIM_STATS_X *px_stats)
{
   CACHESIM_RET_E e_ret_val = eCACHESIM_RET_FAILURE;
   uint32_t ui_k = 0;
   uint32_t ui_l = 0;
   CACHE_SET_DATA_X *px_cache_set = NULL;
   CACHE_BLOCK_METADATA_X  *px_metadata = NULL;
   bool b_cache_hit = false;
   uint32_t ui_cache_set = 0;
   uint32_t ui_block_idx = 0;
   CACHE_BLOCK_X *px_cache_block = NULL;
   uint32_t ui_ram_block = CACHESIM_MAX_INDEX_VALUE;
   bool b_compulsory = false;

   if ((NULL == px_cache) || (NULL == px_stats))
   {
      goto CLEAN_RETURN;
   }

   if (false == b_silent)
   {
      printf ("%8d | ", ui_index);
   }

   px_stats->ui_total_accesses++;

   /*
    * Check is the data word is present in the cache.
    */
   b_cache_hit = cachesim_set_lookup_cache_v2 (px_cache, ui_index, &ui_cache_set, &ui_block_idx);
   if (true == b_cache_hit)
   {
      e_ret_val = cachesim_set_handle_cache_hit (px_cache, px_stats);
   }
   else
   {
	   e_ret_val = cachesim_set_handle_cache_miss (px_cache, ui_index, b_use_pinning,&b_compulsory, &ui_cache_set,
	    &ui_block_idx, px_stats);
   }
   cachesim_set_log_cache_access (px_cache, ui_cache_set, ui_block_idx, b_cache_hit, b_compulsory,
      b_silent);
CLEAN_RETURN:
   return e_ret_val;
}

static void cache_sim_log_summary (
	CACHESIM_SIM_STATS_X *px_stats)
{
	   printf ("Stats:\n"
	      "\t ui_total_accesses         : %d\n"
	      "\t ui_hit_count              : %d\n"
	      "\t ui_miss_count             : %d\n"
	      "\t ui_capacity_miss          : %d\n"
	      "\t ui_compulsory_miss        : %d\n"
	      "\t ui_conflict_miss          : %d\n", px_stats->ui_total_accesses,
	      px_stats->ui_hit_count, px_stats->ui_miss_count, px_stats->ui_capacity_miss,
	      px_stats->ui_compulsory_miss, px_stats->ui_conflict_miss);

	   double d_hit_time = 1.0;
	   double d_miss_penalty = 1.5;
	   double d_hit_rate = (double) px_stats->ui_hit_count / (double) px_stats->ui_total_accesses;
	   double d_miss_rate = (double) px_stats->ui_miss_count / (double) px_stats->ui_total_accesses;
	   double d_avg_memory_access_time = d_hit_time + (d_miss_rate * d_miss_penalty);
	   printf ("\t\t d_hit_rate               : %f\n", d_hit_rate);
	   printf ("\t\t d_miss_rate              : %f\n", d_miss_rate);
	   printf ("\t\t d_avg_memory_access_time : %f\n", d_avg_memory_access_time);
	   printf ("\n");
}

static CACHESIM_RET_E cachesim_set_mapped_cache_simulate(
   CACHE_SET_X *px_cache,
   bool b_use_pinning,
   bool b_silent)
{
   CACHESIM_RET_E e_ret_val = eCACHESIM_RET_FAILURE;
   uint32_t ui_i = 0;
   uint32_t ui_j = 0;
   CACHESIM_SIM_STATS_X x_stats = {0};

   if (NULL == px_cache)
   {
      goto CLEAN_RETURN;
   }
   printf ("\n\n++++++++++++++++++++++++++++++++++++++++++++++++\n");
   printf ("+++++++++++++General Simulator++++++++++++++++++\n");
   printf ("++++++++++++++++++++++++++++++++++++++++++++++++\n");
   cachesim_print_cache_params (px_cache);
   if (false == b_silent)
     cachesim_print_log_header (px_cache);
#if 0
   for (ui_i = 0; ui_i < 6; ui_i++)
   {
      for (ui_j = ui_i; ui_j < 64; ui_j += 6)
      {
         usleep (250);
         cachesim_set_mapped_cache_access (px_cache, ui_j, 64, b_silent, &x_stats);
      }
   }
#endif
   for (ui_i = 0; ui_i < 32; ui_i++)
   {
	   usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
	   cachesim_set_mapped_cache_access (px_cache, ui_i, b_use_pinning, 64, b_silent, &x_stats);
   }
   for (ui_i = 33; ui_i < 128; ui_i++)
   {
       usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
       cachesim_set_mapped_cache_access (px_cache, ui_i, b_use_pinning, 64, b_silent, &x_stats);
   }
   for (ui_i = 0; ui_i < 32; ui_i++)
      {
   	   usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
   	   cachesim_set_mapped_cache_access (px_cache, ui_i, b_use_pinning, 64, b_silent, &x_stats);
      }
   for (ui_i = 33; ui_i < 128; ui_i++)
      {
          usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
          cachesim_set_mapped_cache_access (px_cache, ui_i, b_use_pinning, 64, b_silent, &x_stats);
      }
   for (ui_i = 0; ui_i < 32; ui_i++)
      {
   	   usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
   	   cachesim_set_mapped_cache_access (px_cache, ui_i, b_use_pinning, 64, b_silent, &x_stats);
      }
   cache_sim_log_summary (&x_stats);
CLEAN_RETURN:
   return e_ret_val;
}

static CACHESIM_RET_E cachesim_set_mapped_cache_simulate_pinning(
   CACHE_SET_X *px_cache,
   bool b_use_pinning,
   bool b_silent,
   uint32_t ui_loop_iterations)
{
   CACHESIM_RET_E e_ret_val = eCACHESIM_RET_FAILURE;
   uint32_t ui_k = 0;
   uint32_t ui_i = 0;
   uint32_t ui_j = 0;
   CACHESIM_SIM_STATS_X x_stats = {0};

   if (NULL == px_cache)
   {
      goto CLEAN_RETURN;
   }
   printf ("\n++++++++++++++++++++++++++++++++++++++++++++++++\n");
   printf ("+++++++++++++General Simulator++++++++++++++++++\n");
   printf ("++++++++++++++++++++++++++++++++++++++++++++++++\n");
   cachesim_print_cache_params (px_cache);
   if (false == b_silent)
     cachesim_print_log_header (px_cache);
#if 0
   for (ui_i = 0; ui_i < 6; ui_i++)
   {
      for (ui_j = ui_i; ui_j < 64; ui_j += 6)
      {
         usleep (250);
         cachesim_set_mapped_cache_access (px_cache, ui_j, 64, b_silent, &x_stats);
      }
   }
#endif

   for (ui_k = 0; ui_k < ui_loop_iterations; ui_k++)
   {
	   for (ui_i = 0; ui_i < 32; ui_i++)
	      {
	   	   usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
	   	   cachesim_set_mapped_cache_access (px_cache, ui_i, b_use_pinning, 64, b_silent, &x_stats);
	      }
	   for (ui_i = 32 + ui_k; ui_i < (64 + ui_k); ui_i++)
	      {
	          usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
	          cachesim_set_mapped_cache_access (px_cache, ui_i, b_use_pinning, 64, b_silent, &x_stats);
	      }
	   for (ui_i = 0; ui_i < 32; ui_i++)
	      {
	   	   usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
	   	   cachesim_set_mapped_cache_access (px_cache, ui_i, b_use_pinning, 64, b_silent, &x_stats);
	      }
	   for (ui_i = 65 + ui_k; ui_i < (96 + ui_k); ui_i++)
	   	      {
	   	          usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
	   	          cachesim_set_mapped_cache_access (px_cache, ui_i, b_use_pinning, 64, b_silent, &x_stats);
	   	      }
	   for (ui_i = 0; ui_i < 32; ui_i++)
	      {
	   	   usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
	   	   cachesim_set_mapped_cache_access (px_cache, ui_i, b_use_pinning, 64, b_silent, &x_stats);
	      }
	   for (ui_i = 97 + ui_k; ui_i < (128 + ui_k); ui_i++)
	   	   	      {
	   	   	          usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
	   	   	          cachesim_set_mapped_cache_access (px_cache, ui_i, b_use_pinning, 64, b_silent, &x_stats);
	   	   	      }
	   for (ui_i = 0; ui_i < 32; ui_i++)
	      {
	   	   usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
	   	   cachesim_set_mapped_cache_access (px_cache, ui_i, b_use_pinning, 64, b_silent, &x_stats);
	      }
   }


#if 0
   for (ui_i = 0; ui_i < 32; ui_i++)
   {
	   usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
	   cachesim_set_mapped_cache_access (px_cache, ui_i, b_use_pinning, 64, b_silent, &x_stats);
   }
   for (ui_i = 33; ui_i < 128; ui_i++)
   {
       usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
       cachesim_set_mapped_cache_access (px_cache, ui_i, b_use_pinning, 64, b_silent, &x_stats);
   }
   for (ui_i = 0; ui_i < 32; ui_i++)
      {
   	   usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
   	   cachesim_set_mapped_cache_access (px_cache, ui_i, b_use_pinning, 64, b_silent, &x_stats);
      }
   for (ui_i = 33; ui_i < 128; ui_i++)
      {
          usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
          cachesim_set_mapped_cache_access (px_cache, ui_i, b_use_pinning, 64, b_silent, &x_stats);
      }
   for (ui_i = 0; ui_i < 32; ui_i++)
      {
   	   usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
   	   cachesim_set_mapped_cache_access (px_cache, ui_i, b_use_pinning, 64, b_silent, &x_stats);
      }
#endif
   cache_sim_log_summary (&x_stats);
CLEAN_RETURN:
   return e_ret_val;
}

static CACHESIM_RET_E cachesim_set_mapped_cache_simulate_bubble_sort(
   CACHE_SET_X *px_cache,
   bool b_silent,
   uint32_t ui_n)
{
   CACHESIM_RET_E e_ret_val = eCACHESIM_RET_FAILURE;
   uint32_t ui_i = 0;
   uint32_t ui_j = 0;
   // uint32_t ui_n = 4096;
   uint32_t ui_prev_miss_count = 0;
   CACHESIM_SIM_STATS_X x_stats = {0};

   if (NULL == px_cache)
   {
      goto CLEAN_RETURN;
   }

   printf ("\n\n++++++++++++++++++++++++++++++++++++++++++++++++\n");
   printf ("+++++++++++++++Bubble Sort Simulator++++++++++++\n");
   printf ("++++++++++++++++++++++++++++++++++++++++++++++++\n");
   cachesim_print_cache_params (px_cache);

   if (false == b_silent)
     cachesim_print_log_header (px_cache);

   for (ui_i = 0; ui_i < ui_n; ui_i++)
   {
      for (ui_j = 0; ui_j < ui_n - ui_i - 1; ui_j++)
      {
         usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
         cachesim_set_mapped_cache_access (px_cache, ui_j, false, ui_n, b_silent, &x_stats);
         cachesim_set_mapped_cache_access (px_cache, ui_j + 1, false, ui_n, b_silent, &x_stats);
      }
      if (false == b_silent)
      {
         printf ("%d \nStats:\n"
            "\t ui_total_accesses         : %d\n"
            "\t ui_hit_count              : %d\n"
            "\t ui_miss_count             : %d\n"
            "\t ui_capacity_miss          : %d\n"
            "\t ui_compulsory_miss        : %d\n"
            "\t ui_conflict_miss          : %d\n", ui_i,
            x_stats.ui_total_accesses, x_stats.ui_hit_count,
            x_stats.ui_miss_count, x_stats.ui_capacity_miss,
            x_stats.ui_compulsory_miss, x_stats.ui_conflict_miss);

         printf ("Outer Loop: %d, Diff Miss Count: %d\n", ui_i,
            x_stats.ui_miss_count - ui_prev_miss_count);
      }
      ui_prev_miss_count = x_stats.ui_miss_count;
   }
   cache_sim_log_summary (&x_stats);
CLEAN_RETURN:
   return e_ret_val;
}

static CACHESIM_RET_E cachesim_set_mapped_cache_simulate_max_in_matrix (
   CACHE_SET_X *px_cache,
   bool b_silent,
   uint32_t ui_n)
{
   CACHESIM_RET_E e_ret_val = eCACHESIM_RET_FAILURE;
   uint32_t ui_i = 0;
   uint32_t ui_j = 0;
   uint32_t ui_k = 0;
   // uint32_t ui_n = 4096;
   CACHESIM_SIM_STATS_X x_stats = {0};

   if (NULL == px_cache)
   {
      goto CLEAN_RETURN;
   }

   printf ("\n\n++++++++++++++++++++++++++++++++++++++++++++++++\n");
   printf ("+++++++++++++Max In Matrix Simulator++++++++++++\n");
   printf ("++++++++++++++++++++++++++++++++++++++++++++++++\n");
   cachesim_print_cache_params (px_cache);

   if (false == b_silent)
     cachesim_print_log_header (px_cache);

#if 0
   for (ui_i = 0; ui_i < ui_n; ui_i++)
   {
      for (ui_j = 0; ui_j < ui_n; ui_j++)
      {
         // usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
         cachesim_set_mapped_cache_access (px_cache, ((ui_i * ui_n) + ui_j),
            (ui_n * ui_n), b_silent, &x_stats);
         cachesim_set_mapped_cache_access (px_cache, ((ui_i * ui_n) + ui_j),
            (ui_n * ui_n), b_silent, &x_stats);
      }
   }
#endif

   for (ui_k = 0; ui_k < ui_n / 4; ui_k++)
   {
      for (ui_i = ((ui_k / 2) * (ui_n / 2));
            ui_i < ((ui_n / 2) + ((ui_k / 2) * (ui_n / 2))); ui_i++)
      {
         for (ui_j = ((ui_k % 2) * (ui_n / 2));
               ui_j < (ui_n / 2) + ((ui_k % 2) * (ui_n / 2)); ui_j++)
         {
            usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
            cachesim_set_mapped_cache_access (px_cache, ((ui_i * ui_n) + ui_j), false,
               (ui_n * ui_n), b_silent, &x_stats);
            cachesim_set_mapped_cache_access (px_cache, ((ui_i * ui_n) + ui_j), false,
               (ui_n * ui_n), b_silent, &x_stats);
         }
      }
   }

#if 0
   for (ui_i = 0; ui_i < ui_n / 2; ui_i++)
   {
      for (ui_j = 0; ui_j < ui_n / 2; ui_j++)
      {
         // usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
         cachesim_set_mapped_cache_access (px_cache, ((ui_i * ui_n) + ui_j),
            (ui_n * ui_n), b_silent, &x_stats);
         cachesim_set_mapped_cache_access (px_cache, ((ui_i * ui_n) + ui_j),
            (ui_n * ui_n), b_silent, &x_stats);
      }
   }

   for (ui_i = 0; ui_i < ui_n / 2; ui_i++)
   {
      for (ui_j = ui_n / 2; ui_j < ui_n; ui_j++)
      {
         // usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
         cachesim_set_mapped_cache_access (px_cache, ((ui_i * ui_n) + ui_j),
            (ui_n * ui_n), b_silent, &x_stats);
         cachesim_set_mapped_cache_access (px_cache, ((ui_i * ui_n) + ui_j),
            (ui_n * ui_n), b_silent, &x_stats);
      }
   }

   for (ui_i = ui_n / 2; ui_i < ui_n; ui_i++)
   {
      for (ui_j = 0; ui_j < ui_n / 2; ui_j++)
      {
         // usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
         cachesim_set_mapped_cache_access (px_cache, ((ui_i * ui_n) + ui_j),
            (ui_n * ui_n), b_silent, &x_stats);
         cachesim_set_mapped_cache_access (px_cache, ((ui_i * ui_n) + ui_j),
            (ui_n * ui_n), b_silent, &x_stats);
      }
   }

   for (ui_i = ui_n / 2; ui_i < ui_n; ui_i++)
   {
      for (ui_j = ui_n / 2; ui_j < ui_n; ui_j++)
      {
         // usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
         cachesim_set_mapped_cache_access (px_cache, ((ui_i * ui_n) + ui_j),
            (ui_n * ui_n), b_silent, &x_stats);
         cachesim_set_mapped_cache_access (px_cache, ((ui_i * ui_n) + ui_j),
            (ui_n * ui_n), b_silent, &x_stats);
      }
   }
#endif

   cache_sim_log_summary (&x_stats);
CLEAN_RETURN:
   return e_ret_val;
}

static void cachesim_simulate_pinning (CACHESIM_CACHE_ARGS_X *px_cache_args)
{
	CACHESIM_RET_E e_ret_val = eCACHESIM_RET_FAILURE;
	CACHESIM_CACHE_PARAMS_X x_cache_param = {0};
	CACHE_SET_X    *px_set_cache = NULL;
	bool b_silent = px_cache_args->b_silent;

	   x_cache_param.ui_associativity = px_cache_args->ui_associativity;
	   x_cache_param.ui_block_size_words = px_cache_args->ui_block_size_words;
	   x_cache_param.ui_cache_size_words = px_cache_args->ui_cache_size_words;
	   x_cache_param.ui_word_size_bytes = px_cache_args->ui_word_size_bytes;
	   e_ret_val = cachesim_set_alloc_cache (&px_set_cache, &x_cache_param);
	   if (eCACHESIM_RET_SUCCESS != e_ret_val)
	   {

	   }

	   e_ret_val = cachesim_set_mapped_cache_simulate_pinning (px_set_cache, false, b_silent,
			   px_cache_args->ui_loop_iterations);
	   if (eCACHESIM_RET_SUCCESS != e_ret_val)
	   {

	   }

	   e_ret_val = cachesim_set_free_cache (px_set_cache);
	   if (eCACHESIM_RET_SUCCESS != e_ret_val)
	   {

	   }

	   if (true == px_cache_args->b_simulate_pinning)
	   {
		   x_cache_param.ui_associativity = px_cache_args->ui_associativity;
		   x_cache_param.ui_block_size_words = px_cache_args->ui_block_size_words;
		   x_cache_param.ui_cache_size_words = px_cache_args->ui_cache_size_words;
		   x_cache_param.ui_word_size_bytes = px_cache_args->ui_word_size_bytes;
		   e_ret_val = cachesim_set_alloc_cache (&px_set_cache, &x_cache_param);
		   if (eCACHESIM_RET_SUCCESS != e_ret_val)
		   {

		   }

		   e_ret_val = cachesim_set_mapped_cache_simulate_pinning (px_set_cache, true, b_silent,
				   px_cache_args->ui_loop_iterations);
		   if (eCACHESIM_RET_SUCCESS != e_ret_val)
		   {

		   }

		   e_ret_val = cachesim_set_free_cache (px_set_cache);
		   if (eCACHESIM_RET_SUCCESS != e_ret_val)
		   {

		   }
	   }
}

static void cachesim_get_opts_from_args (int argc, char **argv,
		CACHESIM_CACHE_ARGS_X *px_cache_args)
{
	uint32_t ui_opts_idx = 0;
	uint32_t ui_opts_count = 0;
	   int             c;
	   const char    * short_opt = "ha:b:c:w:s:p:l:";
	   struct option   long_opt[] =
	   {
	      {"help",          no_argument,       NULL, 'h'},
	      {"associativity",          required_argument, NULL, 'a'},
		  {"block-size",          required_argument, NULL, 'b'},
		  {"cache-size",          required_argument, NULL, 'c'},
		  {"word-size",          required_argument, NULL, 'w'},
		  {"simulate-algorithm",          required_argument, NULL, 's'},
		  {"simulate-pinning",          required_argument, NULL, 'p'},
		  {"silent",          required_argument, NULL, 'l'},
		  {"loop-iterations",          required_argument, NULL, 'i'},
	      {NULL,            0,                 NULL, 0  }
	   };
	   const char *long_opt_description[] =
	   {
			   "Print this help and exit.",
			   "(default=2) Associativity",
			   "(default=8) Block Size",
			   "(default=64) Cache Size",
			   "(default=4) Word Size",
			   "(default=general)Simulation Algorithm - general|bubble-sort|max-in-matrix",
			   "(default=false) Simulate Pinning (works only with general simulation algorithm",
			   "(defatul=true) Log each memory access and cache details as one line",
			   "(default=1) Number iterations of the general simulation"
	   };
	   ui_opts_count = sizeof(long_opt) / sizeof (struct option);
	while ((c = getopt_long(argc, argv, short_opt, long_opt, NULL)) != -1) {
		switch (c) {
		case -1: /* no more arguments */
		case 0: /* long options toggles */
			break;

		case 'h':
			printf("Usage: %s [OPTIONS]\n", argv[0]);
			for (ui_opts_idx = 0; ui_opts_idx < ui_opts_count; ui_opts_idx++)
			{
				if (NULL == long_opt[ui_opts_idx].name)
				{
					break;
				}
				char temp[20] = {0};
				strcat(temp, "--");
				strcat(temp, long_opt[ui_opts_idx].name);

				printf("  -%c %-20s (OR %22s=%-20s) - %s\n",
						long_opt[ui_opts_idx].val,
						long_opt[ui_opts_idx].name,
						temp,
						long_opt[ui_opts_idx].name,
						long_opt_description[ui_opts_idx]);
			}
			printf("\n");
			exit (0);
			break;
		case 'a':
			printf("you entered \"%s\"\n", optarg);
			px_cache_args->ui_associativity = atoi(optarg);
			break;
		case 'b':
			printf("you entered \"%s\"\n", optarg);
			px_cache_args->ui_block_size_words = atoi(optarg);
			break;
		case 'c':
			printf("you entered \"%s\"\n", optarg);
			px_cache_args->ui_cache_size_words = atoi(optarg);
			break;
		case 'w':
			printf("you entered \"%s\"\n", optarg);
			px_cache_args->ui_word_size_bytes = atoi(optarg);
			break;
		case 's':
			printf("you entered \"%s\"\n", optarg);
			if (0 == strcmp(optarg, "general")) {
				px_cache_args->e_algorithm =
						eCACHESIM_SIMULATION_ALGORITHM_GENERAL;
			} else if (0 == strcmp(optarg, "bubble-sort")) {
				px_cache_args->e_algorithm =
						eCACHESIM_SIMULATION_ALGORITHM_BUBBLE_SORT;
			} else if (0 == strcmp(optarg, "max-in-matrix")) {
				px_cache_args->e_algorithm =
						eCACHESIM_SIMULATION_ALGORITHM_MAX_IN_MATRIX;
			} else {
				px_cache_args->e_algorithm =
						eCACHESIM_SIMULATION_ALGORITHM_GENERAL;
			}
			break;
		case 'p':
			printf("you entered \"%s\"\n", optarg);
			if (0 == strcmp(optarg, "true")) {
							px_cache_args->b_simulate_pinning =
									true;
						} else {
							px_cache_args->b_simulate_pinning =
									false;
						}
			break;
		case 'l':
			if (0 == strcmp(optarg, "false")) {
										px_cache_args->b_silent =
												false;
									} else {
										px_cache_args->b_silent =
												true;
									}
			break;
		case 'i':
			printf("you entered \"%s\"\n", optarg);
						px_cache_args->ui_loop_iterations = atoi(optarg);
						break;
			break;
		default:
			fprintf(stderr, "%s: invalid option -- %c\n", argv[0], c);
			fprintf(stderr, "Try `%s --help' for more information.\n", argv[0]);
		};
	};

	printf("Usage: %s [OPTIONS]\n", argv[0]);
	for (ui_opts_idx = 0; ui_opts_idx < ui_opts_count; ui_opts_idx++)
	{
		if (NULL == long_opt[ui_opts_idx].name)
		{
			break;
		}
		char temp[20] = {0};
		strcat(temp, "--");
		strcat(temp, long_opt[ui_opts_idx].name);

		printf("  -%c %-20s (OR %22s=%-20s) - %s\n",
				long_opt[ui_opts_idx].val,
				long_opt[ui_opts_idx].name,
				temp,
				long_opt[ui_opts_idx].name,
				long_opt_description[ui_opts_idx]);
	}
	printf("\n");


	if (0 == px_cache_args->ui_associativity)
		px_cache_args->ui_associativity = 2;
	if (0 == px_cache_args->ui_block_size_words)
		px_cache_args->ui_block_size_words = 8;
	if (0 == px_cache_args->ui_cache_size_words)
		px_cache_args->ui_cache_size_words = 64;
	if (0 == px_cache_args->ui_word_size_bytes)
		px_cache_args->ui_word_size_bytes = sizeof(uint32_t);
	if (eCACHESIM_SIMULATION_ALGORITHM_INVALID == px_cache_args->e_algorithm)
		px_cache_args->e_algorithm = eCACHESIM_SIMULATION_ALGORITHM_GENERAL;
	if (0 == px_cache_args->ui_loop_iterations)
			px_cache_args->ui_loop_iterations = 1;
}

int main (int argc, char **argv)
{
   int i_ret_val = -1;
   CACHESIM_RET_E e_ret_val = eCACHESIM_RET_FAILURE;
   CACHESIM_CACHE_PARAMS_X x_cache_param = {0};
   CACHESIM_CACHE_ARGS_X x_cache_args = {0};
   CACHE_SET_X    *px_set_cache = NULL;
   bool b_silent = true;

   cachesim_get_opts_from_args (argc, argv, &x_cache_args);

   b_silent = x_cache_args.b_silent;

   switch (x_cache_args.e_algorithm)
   {
   case eCACHESIM_SIMULATION_ALGORITHM_GENERAL:
	   cachesim_simulate_pinning (&x_cache_args);
	   break;
   case eCACHESIM_SIMULATION_ALGORITHM_BUBBLE_SORT:
#if 0
	   x_cache_param.ui_associativity = 1;
	   x_cache_param.ui_block_size_words = 16;
	   x_cache_param.ui_cache_size_words = (16 * 64);
	   x_cache_param.ui_word_size_bytes = sizeof(uint32_t);
#endif
	   x_cache_param.ui_associativity = x_cache_args.ui_associativity;
	   	   x_cache_param.ui_block_size_words = x_cache_args.ui_block_size_words;
	   	   x_cache_param.ui_cache_size_words = x_cache_args.ui_cache_size_words;
	   	   x_cache_param.ui_word_size_bytes = x_cache_args.ui_word_size_bytes;
	   e_ret_val = cachesim_set_alloc_cache (&px_set_cache, &x_cache_param);
	   if (eCACHESIM_RET_SUCCESS != e_ret_val)
	   {

	   }

	   e_ret_val = cachesim_set_mapped_cache_simulate_bubble_sort (px_set_cache,
			   b_silent, 128);
	   if (eCACHESIM_RET_SUCCESS != e_ret_val)
	   {

	   }

	   e_ret_val = cachesim_set_free_cache (px_set_cache);
	   if (eCACHESIM_RET_SUCCESS != e_ret_val)
	   {

	   }
   	   break;
   case eCACHESIM_SIMULATION_ALGORITHM_MAX_IN_MATRIX:
#if 0
	   x_cache_param.ui_associativity = 1;
	   x_cache_param.ui_block_size_words = 8;
	   x_cache_param.ui_cache_size_words = (16 * 8);
	   x_cache_param.ui_word_size_bytes = sizeof(uint32_t);
#endif
	   x_cache_param.ui_associativity = x_cache_args.ui_associativity;
	   	   	   x_cache_param.ui_block_size_words = x_cache_args.ui_block_size_words;
	   	   	   x_cache_param.ui_cache_size_words = x_cache_args.ui_cache_size_words;
	   	   	   x_cache_param.ui_word_size_bytes = x_cache_args.ui_word_size_bytes;
	   e_ret_val = cachesim_set_alloc_cache (&px_set_cache, &x_cache_param);
	   if (eCACHESIM_RET_SUCCESS != e_ret_val)
	   {

	   }

	   e_ret_val = cachesim_set_mapped_cache_simulate_max_in_matrix (px_set_cache,
			   b_silent, 16);
	   if (eCACHESIM_RET_SUCCESS != e_ret_val)
	   {

	   }

	   e_ret_val = cachesim_set_free_cache (px_set_cache);
	   if (eCACHESIM_RET_SUCCESS != e_ret_val)
	   {

	   }
   	   break;

   }
   return i_ret_val;
}
