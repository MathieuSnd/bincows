
/**
 * memory modeling:
 * | MR1 | MR2 | //// | MR3 | .... 
 * MR = 64 MB memory range
 * 
 * MR linked list entry: {
 *      MR_base,   // base address of the MR
 *      MR_length, // length, in pages  
 *      MR_next,   // the next MR in the list, or null
 * }
 * 
 * 4 global linked lists:
 *      memory_list[i] = MR linked list entry *
 *  ML0: at least 1 64K contiguous available range (very fast allocation)
 *  ML1: at least 1 32K contiguous available range (fast allocation)
 *  ML1: at least 1 16K contiguous available range (medium allocation)
 *  ML1: at least 1 4K  contiguous available range (slow allocation)
 * 
 *
 *  MR layout;
 *  | ----------------------------- 4K HEADER --------------------------- |      BLOCKS      |
 *  | 1152 header | 128 bitmap3 |  256 bitmap2 | 512 bitmap1 | 2K bitmap0 | 64 MB - 4K pages |
 *  
 *  header: {
 *      size,     // of the range, in pages
 *      rem0,     // number of free       pages (4k)
 *      rem1,     // number of free super pages (16k)
 *      rem2,     // number of free mega  pages (32k) 
 *      rem3,     // number of free ultra pages (64k) 
 *  }
 * 
 * 
*/

/**
 * @brief allocate 
 * 
 * @param size 
 * @return void* 
 */
void* physalloc(size_t size);