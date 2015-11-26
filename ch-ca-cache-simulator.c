/*******************************************************************************
 * Copyright (c) 2013, Sandeep Prakash <123sandy@gmail.com>
 * All rights reserved.
 *
 * \file   cachesim.c
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
#include <unistd.h>
#include <stdbool.h>

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

static bool cachesim_set_lookup_cache(
   CACHE_SET_X *px_cache,
   uint32_t ui_array_idx,
   uint32_t *pui_cache_set);

static CACHESIM_RET_E cachesim_set_mapped_cache_simulate(
   CACHE_SET_X *px_cache,
   bool b_silent);

static CACHESIM_RET_E cachesim_set_mapped_cache_access (
   CACHE_SET_X *px_cache,
   uint32_t ui_index,
   uint32_t ui_total_data_words,
   bool b_silent,
   CACHESIM_SIM_STATS_X *px_stats);

static CACHESIM_RET_E cachesim_set_mapped_cache_simulate_bubble_sort(
   CACHE_SET_X *px_cache,
   bool b_silent,
   uint32_t ui_n);

/****************************** LOCAL FUNCTIONS *******************************/
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

   printf ("Cache Params:\n"
      "\tui_cache_size_words     : %d\n"
      "\tui_associativity        : %d\n"
      "\tui_block_size_words     : %d\n"
      "\tui_word_size_bytes      : %d\n"
      "\tui_no_of_sets           : %d\n"
      "\tui_no_of_blocks_per_set : %d\n", px_cache_params->ui_cache_size_words,
      px_cache_params->ui_associativity, px_cache_params->ui_block_size_words,
      px_cache_params->ui_word_size_bytes, ui_no_of_sets,
      px_cache->ui_no_of_blocks_per_set);

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

   printf ("%4s |", "    ");

   snprintf ((uca_format_string), sizeof(uca_format_string), " %%%dd |",
      ui_width_qualifier);

   for (ui_i = 0; ui_i < px_cache->ui_configured_no_of_sets; ui_i++)
   {
      px_cache_set = &(px_cache->xa_sets[ui_i]);
      printf (uca_format_string, px_cache_set->ui_set_idx);

   }

   printf (" Hit/Miss |\n");

   printf ("%4s-+", "----");

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

   printf ("\n");
}

static CACHESIM_RET_E cachesim_set_mapped_cache_simulate(
   CACHE_SET_X *px_cache,
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

   cachesim_print_log_header (px_cache);

   for (ui_i = 0; ui_i < 6; ui_i++)
   {
      for (ui_j = ui_i; ui_j < 64; ui_j += 6)
      {
         usleep (250);
         cachesim_set_mapped_cache_access (px_cache, ui_j, 64, b_silent, &x_stats);
      }
   }

   printf ("Stats:\n"
      "\t ui_total_accesses         : %d\n"
      "\t ui_hit_count              : %d\n"
      "\t ui_miss_count             : %d\n"
      "\t ui_capacity_miss          : %d\n"
      "\t ui_compulsory_miss        : %d\n"
      "\t ui_conflict_miss          : %d\n", x_stats.ui_total_accesses,
      x_stats.ui_hit_count, x_stats.ui_miss_count, x_stats.ui_capacity_miss,
      x_stats.ui_compulsory_miss, x_stats.ui_conflict_miss);

   printf ("\n");
CLEAN_RETURN:
   return e_ret_val;
}

static CACHESIM_RET_E cachesim_set_mapped_cache_access (
   CACHE_SET_X *px_cache,
   uint32_t ui_index,
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
   CACHE_BLOCK_X *px_cache_block = NULL;
   uint32_t ui_ram_block = CACHESIM_MAX_INDEX_VALUE;
   bool b_compulsory = false;

   if ((NULL == px_cache) || (NULL == px_stats))
   {
      goto CLEAN_RETURN;
   }

   if (false == b_silent)
   {
      printf ("%4d | ", ui_index);
   }

   px_stats->ui_total_accesses++;

   /*
    * Check is the data word is present in the cache.
    */
   b_cache_hit = cachesim_set_lookup_cache (px_cache, ui_index, &ui_cache_set);
   if (true == b_cache_hit)
   {
      px_stats->ui_hit_count++;
   }
   else
   {
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
         b_compulsory = true;
      }
      else
      {
         px_stats->ui_capacity_miss++;
      }

      px_stats->ui_miss_count++;

      /*
       * Word not present in the cache. Fetch from block from RAM.
       */
      e_ret_val = cachesim_set_fetch_data_to_cache (px_cache, ui_index,
         &ui_cache_set);
      if (eCACHESIM_RET_SUCCESS != e_ret_val)
      {
         goto CLEAN_RETURN;
      }
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
         printf ("%8s | %4d |\n", "Hit", ui_cache_set);
      }
      else
      {
         printf ("%8s | %4d |\n",
            (true == b_compulsory) ? "Com Miss" : "Cap Miss", ui_cache_set);
      }
   }
   e_ret_val = eCACHESIM_RET_SUCCESS;
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

   if (false == b_silent)
   cachesim_print_log_header (px_cache);

   for (ui_i = 0; ui_i < ui_n; ui_i++)
   {
      for (ui_j = 0; ui_j < ui_n - ui_i - 1; ui_j++)
      {
         // usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
         cachesim_set_mapped_cache_access (px_cache, ui_j, ui_n, b_silent, &x_stats);
         cachesim_set_mapped_cache_access (px_cache, ui_j + 1, ui_n, b_silent, &x_stats);
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
   printf ("%d \nStats:\n"
      "\t ui_total_accesses         : %d\n"
      "\t ui_hit_count              : %d\n"
      "\t ui_miss_count             : %d\n"
      "\t ui_capacity_miss          : %d\n"
      "\t ui_compulsory_miss        : %d\n"
      "\t ui_conflict_miss          : %d\n", ui_i, x_stats.ui_total_accesses,
      x_stats.ui_hit_count, x_stats.ui_miss_count, x_stats.ui_capacity_miss,
      x_stats.ui_compulsory_miss, x_stats.ui_conflict_miss);
   printf ("\n");
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
            // usleep (CACHESIM_PAUSE_TIME_BW_ACCESSES);
            cachesim_set_mapped_cache_access (px_cache, ((ui_i * ui_n) + ui_j),
               (ui_n * ui_n), b_silent, &x_stats);
            cachesim_set_mapped_cache_access (px_cache, ((ui_i * ui_n) + ui_j),
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

   printf ("%d \nStats:\n"
      "\t ui_total_accesses         : %d\n"
      "\t ui_hit_count              : %d\n"
      "\t ui_miss_count             : %d\n"
      "\t ui_capacity_miss          : %d\n"
      "\t ui_compulsory_miss        : %d\n"
      "\t ui_conflict_miss          : %d\n", ui_i, x_stats.ui_total_accesses,
      x_stats.ui_hit_count, x_stats.ui_miss_count, x_stats.ui_capacity_miss,
      x_stats.ui_compulsory_miss, x_stats.ui_conflict_miss);
   printf ("\n");
CLEAN_RETURN:
   return e_ret_val;
}

int main ()
{
   int i_ret_val = -1;
   CACHESIM_RET_E e_ret_val = eCACHESIM_RET_FAILURE;
   CACHESIM_CACHE_PARAMS_X x_cache_param = {0};
   CACHE_SET_X    *px_set_cache = NULL;

   x_cache_param.ui_associativity = 1;
   x_cache_param.ui_block_size_words = 4;
   x_cache_param.ui_cache_size_words = 32;
   x_cache_param.ui_word_size_bytes = sizeof(uint32_t);
   e_ret_val = cachesim_set_alloc_cache (&px_set_cache, &x_cache_param);
   if (eCACHESIM_RET_SUCCESS != e_ret_val)
   {

   }

   e_ret_val = cachesim_set_mapped_cache_simulate (px_set_cache, false);
   if (eCACHESIM_RET_SUCCESS != e_ret_val)
   {

   }

   e_ret_val = cachesim_set_free_cache (px_set_cache);
   if (eCACHESIM_RET_SUCCESS != e_ret_val)
   {

   }

   x_cache_param.ui_associativity = 2;
   x_cache_param.ui_block_size_words = 4;
   x_cache_param.ui_cache_size_words = 32;
   x_cache_param.ui_word_size_bytes = sizeof(uint32_t);
   e_ret_val = cachesim_set_alloc_cache (&px_set_cache, &x_cache_param);
   if (eCACHESIM_RET_SUCCESS != e_ret_val)
   {

   }

   e_ret_val = cachesim_set_mapped_cache_simulate (px_set_cache, false);
   if (eCACHESIM_RET_SUCCESS != e_ret_val)
   {

   }

   e_ret_val = cachesim_set_free_cache (px_set_cache);
   if (eCACHESIM_RET_SUCCESS != e_ret_val)
   {

   }

   x_cache_param.ui_associativity = 1;
   x_cache_param.ui_block_size_words = 16;
   x_cache_param.ui_cache_size_words = (16 * 64);
   x_cache_param.ui_word_size_bytes = sizeof(uint32_t);
   e_ret_val = cachesim_set_alloc_cache (&px_set_cache, &x_cache_param);
   if (eCACHESIM_RET_SUCCESS != e_ret_val)
   {

   }

   e_ret_val = cachesim_set_mapped_cache_simulate_bubble_sort (px_set_cache,
      true, 4096);
   if (eCACHESIM_RET_SUCCESS != e_ret_val)
   {

   }

   e_ret_val = cachesim_set_free_cache (px_set_cache);
   if (eCACHESIM_RET_SUCCESS != e_ret_val)
   {

   }

   x_cache_param.ui_associativity = 1;
   x_cache_param.ui_block_size_words = 8;
   x_cache_param.ui_cache_size_words = (16 * 8);
   x_cache_param.ui_word_size_bytes = sizeof(uint32_t);
   e_ret_val = cachesim_set_alloc_cache (&px_set_cache, &x_cache_param);
   if (eCACHESIM_RET_SUCCESS != e_ret_val)
   {

   }

   e_ret_val = cachesim_set_mapped_cache_simulate_max_in_matrix (px_set_cache,
      false, 16);
   if (eCACHESIM_RET_SUCCESS != e_ret_val)
   {

   }

   e_ret_val = cachesim_set_free_cache (px_set_cache);
   if (eCACHESIM_RET_SUCCESS != e_ret_val)
   {

   }
   else
   {
      i_ret_val = 0;
   }
   return i_ret_val;
}
