#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>

#include "myMalloc.h"

#define MALLOC_COLOR "MALLOC_DEBUG_COLOR"
#define ALIGN(size) (((size) + 7) & ~7)

static bool check_env;
static bool use_color;

/*
 * Mutex to ensure thread safety for the freelist
 */
static pthread_mutex_t mutex;

/*
 * Array of sentinel nodes for the freelists
 */
header freelistSentinels[N_LISTS];

/*
 * Pointer to the second fencepost in the most recently allocated chunk from
 * the OS. Used for coalescing chunks
 */
header * lastFencePost;

/*
 * Pointer to maintian the base of the heap to allow printing based on the
 * distance from the base of the heap
 */ 
void * base;

/*
 * List of chunks allocated by  the OS for printing boundary tags
 */
header * osChunkList [MAX_OS_CHUNKS];
size_t numOsChunks = 0;

/*
 * direct the compiler to run the init function before running main
 * this allows initialization of required globals
 */
static void init (void) __attribute__ ((constructor));

// Helper functions for manipulating pointers to headers
static inline header * get_header_from_offset(void * ptr, ptrdiff_t off);
static inline header * get_left_header(header * h);
static inline header * ptr_to_header(void * p);

// Helper functions for allocating more memory from the OS
static inline void initialize_fencepost(header * fp, size_t left_size);
static inline void insert_os_chunk(header * hdr);
static inline void insert_fenceposts(void * raw_mem, size_t size);
static header * allocate_chunk(size_t size);

// Helper functions for freeing a block
static inline void deallocate_object(void * p);

// Helper functions for allocating a block
static inline header * allocate_object(size_t raw_size);

// Helper functions for verifying that the data structures are structurally 
// valid
static inline header * detect_cycles();
static inline header * verify_pointers();
static inline bool verify_freelist();
static inline header * verify_chunk(header * chunk);
static inline bool verify_tags();

static void init();

static bool isMallocInitialized;

/**
 * @brief Helper function to retrieve a header pointer from a pointer and an 
 *        offset
 *
 * @param ptr base pointer
 * @param off number of bytes from base pointer where header is located
 *
 * @return a pointer to a header offset bytes from pointer
 */
static inline header * get_header_from_offset(void * ptr, ptrdiff_t off) {
	return (header *)((char *) ptr + off);
}

/**
 * @brief Helper function to get the header to the right of a given header
 *
 * @param h original header
 *
 * @return header to the right of h
 */
header * get_right_header(header * h) {
	return get_header_from_offset(h, get_size(h));
}

/**
 * @brief Helper function to get the header to the left of a given header
 *
 * @param h original header
 *
 * @return header to the right of h
 */
inline static header * get_left_header(header * h) {
  return get_header_from_offset(h, -h->left_size);
}

/**
 * @brief Fenceposts are marked as always allocated and may need to have
 * a left object size to ensure coalescing happens properly
 *
 * @param fp a pointer to the header being used as a fencepost
 * @param left_size the size of the object to the left of the fencepost
 */
inline static void initialize_fencepost(header * fp, size_t left_size) {
	set_state(fp,FENCEPOST);
	set_size(fp, ALLOC_HEADER_SIZE);
	fp->left_size = left_size;
}

/**
 * @brief Helper function to maintain list of chunks from the OS for debugging
 *
 * @param hdr the first fencepost in the chunk allocated by the OS
 */
inline static void insert_os_chunk(header * hdr) {
  if (numOsChunks < MAX_OS_CHUNKS) {
    osChunkList[numOsChunks++] = hdr;
  }
}

/**
 * @brief given a chunk of memory insert fenceposts at the left and 
 * right boundaries of the block to prevent coalescing outside of the
 * block
 *
 * @param raw_mem a void pointer to the memory chunk to initialize
 * @param size the size of the allocated chunk
 */
inline static void insert_fenceposts(void * raw_mem, size_t size) {
  // Convert to char * before performing operations
  char * mem = (char *) raw_mem;

  // Insert a fencepost at the left edge of the block
  header * leftFencePost = (header *) mem;
  initialize_fencepost(leftFencePost, ALLOC_HEADER_SIZE);

  // Insert a fencepost at the right edge of the block
  header * rightFencePost = get_header_from_offset(mem, size - ALLOC_HEADER_SIZE);
  initialize_fencepost(rightFencePost, size - 2 * ALLOC_HEADER_SIZE);
}

/**
 * @brief Allocate another chunk from the OS and prepare to insert it
 * into the free list
 *
 * @param size The size to allocate from the OS
 *
 * @return A pointer to the allocable block in the chunk (just after the 
 * first fencpost)
 */
static header * allocate_chunk(size_t size) {
  void * mem = sbrk(size);
  
  insert_fenceposts(mem, size);
  header * hdr = (header *) ((char *)mem + ALLOC_HEADER_SIZE);
  set_state(hdr, UNALLOCATED);
  set_size(hdr, size - 2 * ALLOC_HEADER_SIZE);
  hdr->left_size = ALLOC_HEADER_SIZE;
  return hdr;
}

/* helper function calculate actual size from raw size*/

static size_t calculate_actual_size(size_t raw_size) {
  size_t aligned_raw_size = ALIGN(raw_size);
  size_t actual_size;
  if (aligned_raw_size <= ALLOC_HEADER_SIZE) {
    actual_size = sizeof(header);
  }
  else {
    actual_size = aligned_raw_size + ALLOC_HEADER_SIZE;
  }
  return actual_size;
}

/* helper function get free_list_index given actual_size */

static int get_free_list_index(size_t actual_size) {
  actual_size -= ALLOC_HEADER_SIZE;
  int free_list_index = (actual_size / 8) - 1;

  /* iterate through free_lists until reach non empty OR last list*/
  
  while ((freelistSentinels[free_list_index].next == &freelistSentinels[free_list_index])
          && (free_list_index < (N_LISTS - 1))) {
    free_list_index++;
  }
  return free_list_index;
}

/* helper function to remove header from list */

static void remove_header(header * block) {
  if (block -> prev != NULL) {
    block -> prev -> next = block -> next;
  }
  if (block -> next != NULL) {
    block -> next -> prev = block -> prev;
  }
}

/* helper function to insert header into correct list*/

static void insert_header(header * remaining_block, int new_free_list_index) {
  header *new_sentinel = &freelistSentinels[new_free_list_index];
  new_sentinel -> next -> prev = remaining_block;
  remaining_block->next = new_sentinel->next; 
  remaining_block->prev = new_sentinel; 
  new_sentinel -> next = remaining_block;
}

/** 
 * @brief Helper allocate an object given a raw request size from the user
 *
 * @param raw_size number of bytes the user needs
 *
 * @return A block satisfying the user's request
 */

static inline header * allocate_object(size_t raw_size) {
  if (raw_size == 0) {
        return NULL;
  }
  size_t actual_size = calculate_actual_size(raw_size);
  int free_list_index = get_free_list_index(actual_size);
 
  /* If free_list_index isn't the last index, set the block to allocated and
     return the block's data pointer. If it is the last index, traverse the last
     linked list */

  if (free_list_index < N_LISTS - 1) {
    header *block = freelistSentinels[free_list_index].next;
    set_state(block, ALLOCATED);
    remove_header(block);
    return block;
  }
  else { 
    header *sentinel = &freelistSentinels[N_LISTS - 1];
    header *current = sentinel->next;
    while (current != sentinel) {
      if (get_size(current) >= actual_size) {
        
        /* Block of sufficient size found */

        /* Split if necessary. Update size and left size fields of 
           neighboring block. Update allocation state of allcoated 
           block to ALLOCATED. Return data pointer */
        
        if (get_size(current) > actual_size + sizeof(header)) {

          /* update remaining, left, and allocated blocks */

          header *remaining_block = current;
          set_size(remaining_block, get_size(current) - actual_size);
          set_state(remaining_block, UNALLOCATED);
          header *allocated_block = (header *) get_right_header(remaining_block);
          set_size(allocated_block, actual_size);
          allocated_block -> left_size = get_size(remaining_block);
          set_state(allocated_block, ALLOCATED);
          header *right_block = get_right_header(allocated_block);
          if (get_size(right_block) != 0) {
            right_block->left_size = get_size(allocated_block);
          }

          /* remaining_block needs to be placed in correct free list */

          int new_free_list_index = (get_size(remaining_block) / 8) - 1;
          if (new_free_list_index > N_LISTS - 1) {
            new_free_list_index = N_LISTS - 1;
          }
          if (new_free_list_index == free_list_index) {
            return allocated_block;
          }
          else {
            remove_header(current);
            insert_header(remaining_block, new_free_list_index);
            return allocated_block;
          }
        }
        else {
              
          /* If block is large enough to fulfill request, but not split */

          set_state(current, ALLOCATED);
          remove_header(current);
          return current;
        }
      }
      current = current->next;
    }
  }

  /* Task 3: Managing Additional Chunks (no available blocks satsify allocation request */
  
  while (true) {
    header * new_chunk = allocate_chunk(ARENA_SIZE);
    if (new_chunk == NULL) {
      return NULL;
    }
    header * fenceBeforeChunk = (new_chunk - (2 * sizeOf(header)));

    /* check if lastFencePost is next to new chunk and coalesce if necessary */

    if (fenceBeforeChunk == lastFencePost) {
      
      /* case 1: neighbors */

      /* Check if previous block is allocated or unallocated */

      header * previousBlock = lastFencePost - (lastFencePost -> left_size);
      if (get_state(previousBlock) == UNALLOCATED) {
        set_size(previousBlock,
                 get_size(previousBlock) + (2 * sizeOf(header)) + get_size(new_chunk));
        lastFencePost = (header *) ((char *) previousBlock + 
                 get_size(previousBlock) + (2 * sizeOf(header)) + get_size(new_chunk));
        new_chunk = previousBlock;
      }
      else {
        set_size(lastFencePost,
                 (2 * sizeOf(header) + get_size(new_chunk)));
        set_state(lastFencePost, UNALLOCATED);
        lastFencePost = (header *) ((char *) lastFencePost + 
                 (2 * sizeOf(header)) + get_size(new_chunk));
        new_chunk = lastFencePost;
      }
    }
    else {

      /* case 2: not neighbors: just insert into free_list */

      insert_header(new_chunk, ); //needs index
    }

    /* check if new size is big enough for the request: actual_size */

    if (new_chunk >= actual_size) {
      return new_chunk;
      break;
    }

  }
}

/**
 * @brief Helper to get the header from a pointer allocated with malloc
 *
 * @param p pointer to the data region of the block
 *
 * @return A pointer to the header of the block
 */
static inline header * ptr_to_header(void * p) {
  return (header *)((char *) p - ALLOC_HEADER_SIZE); //sizeof(header));
}

/* */


/**
 * @brief Helper to manage deallocation of a pointer returned by the user
 *
 * @param p The pointer returned to the user by a call to malloc
 */
static inline void deallocate_object(void * p) {
  if (p == NULL) {
        return;
  }

  /* calculate pointer to header from p */

  header *current = (header *)((char *) p - sizeof(header));
  if (get_state(current) != ALLOCATED) {
    current = (header *)((char *) p - (2 * sizeof(size_t)));
  }

  size_t current_size = get_size(current);
  header *right = get_right_header(current);
  header *left = (header *)((char *)current - current->left_size);
  int right_unallocated = (get_state(right) == UNALLOCATED);
  int left_unallocated = (get_state(left) == UNALLOCATED);

  if (!right_unallocated && !left_unallocated) {

      // Case 1: Neither neighbor is unallocated

      set_state(current, UNALLOCATED);
      
      int free_list_index =  ((get_size(current) - ALLOC_HEADER_SIZE) / 8) - 1;
      insert_header(current, free_list_index);
  } else if (right_unallocated && !left_unallocated) {

      // Case 2: Only the right block is unallocated

      size_t new_size = current_size + get_size(right);
      remove_header(right);
      set_state(current, UNALLOCATED);
      set_size(current, new_size);
      header *new_right = get_right_header(current);
      new_right->left_size = new_size;
      int free_list_index =  ((new_size - ALLOC_HEADER_SIZE) / 8) - 1;
      insert_header(current, free_list_index);
  } else if (!right_unallocated && left_unallocated) {

      // Case 3: Only the left block is unallocated

      size_t new_size = current_size + get_size(left);
      remove_header(left);
      set_state(left, UNALLOCATED);
      set_size(left, new_size);
      header *new_right = get_right_header(left);
      new_right->left_size = new_size;
      int free_list_index =  ((new_size - ALLOC_HEADER_SIZE) / 8) - 1;
      insert_header(left, free_list_index);
  } else {

      // Case 4: Both neighbors are unallocated

      size_t new_size = current_size + get_size(right) + get_size(left);
      remove_header(right);
      remove_header(left);
      set_state(left, UNALLOCATED);
      set_size(left, new_size);
      header *new_right = get_right_header(left);
      new_right->left_size = new_size;
      int free_list_index =  ((new_size - ALLOC_HEADER_SIZE) / 8) - 1;
      insert_header(left, free_list_index);
  }
}



/**
 * @brief Helper to detect cycles in the free list
 * https://en.wikipedia.org/wiki/Cycle_detection#Floyd's_Tortoise_and_Hare
 *
 * @return One of the nodes in the cycle or NULL if no cycle is present
 */
static inline header * detect_cycles() {
  for (int i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    for (header * slow = freelist->next, * fast = freelist->next->next; 
         fast != freelist; 
         slow = slow->next, fast = fast->next->next) {
      if (slow == fast) {
        return slow;
      }
    }
  }
  return NULL;
}

/**
 * @brief Helper to verify that there are no unlinked previous or next pointers
 *        in the free list
 *
 * @return A node whose previous and next pointers are incorrect or NULL if no
 *         such node exists
 */
static inline header * verify_pointers() {
  for (int i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    for (header * cur = freelist->next; cur != freelist; cur = cur->next) {
      if (cur->next->prev != cur || cur->prev->next != cur) {
        return cur;
      }
    }
  }
  return NULL;
}

/**
 * @brief Verify the structure of the free list is correct by checkin for 
 *        cycles and misdirected pointers
 *
 * @return true if the list is valid
 */
static inline bool verify_freelist() {
  header * cycle = detect_cycles();
  if (cycle != NULL) {
    fprintf(stderr, "Cycle Detected\n");
    print_sublist(print_object, cycle->next, cycle);
    return false;
  }

  header * invalid = verify_pointers();
  if (invalid != NULL) {
    fprintf(stderr, "Invalid pointers\n");
    print_object(invalid);
    return false;
  }

  return true;
}

/**
 * @brief Helper to verify that the sizes in a chunk from the OS are correct
 *        and that allocated node's canary values are correct
 *
 * @param chunk AREA_SIZE chunk allocated from the OS
 *
 * @return a pointer to an invalid header or NULL if all header's are valid
 */
static inline header * verify_chunk(header * chunk) {
	if (get_state(chunk) != FENCEPOST) {
		fprintf(stderr, "Invalid fencepost\n");
		print_object(chunk);
		return chunk;
	}
	
	for (; get_state(chunk) != FENCEPOST; chunk = get_right_header(chunk)) {
		if (get_size(chunk)  != get_right_header(chunk)->left_size) {
			fprintf(stderr, "Invalid sizes\n");
			print_object(chunk);
			return chunk;
		}
	}
	
	return NULL;
}

/**
 * @brief For each chunk allocated by the OS verify that the boundary tags
 *        are consistent
 *
 * @return true if the boundary tags are valid
 */
static inline bool verify_tags() {
  for (size_t i = 0; i < numOsChunks; i++) {
    header * invalid = verify_chunk(osChunkList[i]);
    if (invalid != NULL) {
      return invalid;
    }
  }

  return NULL;
}

/**
 * @brief Initialize mutex lock and prepare an initial chunk of memory for allocation
 */
static void init() {
  // Initialize mutex for thread safety
  pthread_mutex_init(&mutex, NULL);

#ifdef DEBUG
  // Manually set printf buffer so it won't call malloc when debugging the allocator
  setvbuf(stdout, NULL, _IONBF, 0);
#endif // DEBUG

  // Allocate the first chunk from the OS
  header * block = allocate_chunk(ARENA_SIZE);

  header * prevFencePost = get_header_from_offset(block, -ALLOC_HEADER_SIZE);
  insert_os_chunk(prevFencePost);

  lastFencePost = get_header_from_offset(block, get_size(block));

  // Set the base pointer to the beginning of the first fencepost in the first
  // chunk from the OS
  base = ((char *) block) - ALLOC_HEADER_SIZE; //sizeof(header);

  // Initialize freelist sentinels
  for (int i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    freelist->next = freelist;
    freelist->prev = freelist;
  }

  // Insert first chunk into the free list
  header * freelist = &freelistSentinels[N_LISTS - 1];
  freelist->next = block;
  freelist->prev = block;
  block->next = freelist;
  block->prev = freelist;
}

/* 
 * External interface
 */
void * my_malloc(size_t size) {
  pthread_mutex_lock(&mutex);
  header * hdr = allocate_object(size);

  /* added line below */

  void * data = hdr -> data; 

  pthread_mutex_unlock(&mutex);
    
  return data;
  /* return hdr; */
}

void * my_calloc(size_t nmemb, size_t size) {
  return memset(my_malloc(size * nmemb), 0, size * nmemb);
}

void * my_realloc(void * ptr, size_t size) {
  void * mem = my_malloc(size);
  memcpy(mem, ptr, size);
  my_free(ptr);
  return mem; 
}

void my_free(void * p) {
  pthread_mutex_lock(&mutex);
  deallocate_object(p);
  pthread_mutex_unlock(&mutex);
}

bool verify() {
  return verify_freelist() && verify_tags();
}

/**
 * @brief Print just the block's size
 *
 * @param block The block to print
 */
void basic_print(header * block) {
	printf("[%zd] -> ", get_size(block));
}

/**
 * @brief Print just the block's size
 *
 * @param block The block to print
 */
void print_list(header * block) {
	printf("[%zd]\n", get_size(block));
}

/**
 * @brief return a string representing the allocation status
 *
 * @param allocated The allocation status field
 *
 * @return A string representing the allocation status
 */
static inline const char * allocated_to_string(char allocated) {
  switch(allocated) {
    case UNALLOCATED: 
      return "false";
    case ALLOCATED:
      return "true";
    case FENCEPOST:
      return "fencepost";
  }
  assert(false);
}

static bool check_color() {
  if (!check_env) {
    // genenv allows accessing environment varibles
    const char * var = getenv(MALLOC_COLOR);
    use_color = var != NULL && !strcmp(var, "1337_CoLoRs");
    check_env = true;
  }
  return use_color;
}

/**
 * @brief Change the tty color based on the block's allocation status
 *
 * @param block The block to print the allocation status of
 */
static void print_color(header * block) {
  if (!check_color()) {
    return;
  }

  switch(get_state(block)) {
    case UNALLOCATED:
      printf("\033[0;32m");
      break;
    case ALLOCATED:
      printf("\033[0;34m");
      break;
    case FENCEPOST:
      printf("\033[0;33m");
      break;
  }
}

static void clear_color() {
  if (check_color()) {
    printf("\033[0;0m");
  }
}

static inline bool is_sentinel(void * p) {
  for (int i = 0; i < N_LISTS; i++) {
    if (&freelistSentinels[i] == p) {
      return true;
    }
  }
  return false;
}

/**
 * @brief Print the free list pointers if RELATIVE_POINTERS is set to true
 * then print the pointers as an offset from the base of the heap. This allows
 * for determinism in testing. 
 * (due to ASLR https://en.wikipedia.org/wiki/Address_space_layout_randomization#Linux)
 *
 * @param p The pointer to print
 */
void print_pointer(void * p) {
  if (is_sentinel(p)) {
    printf("SENTINEL");
  } else {
    if (RELATIVE_POINTERS) {
      printf("%04zd", p - base);
    } else {
      printf("%p", p);
    }
  }
}

/**
 * @brief Verbose printing of all of the metadata fields of each block
 *
 * @param block The block to print
 */
void print_object(header * block) {
  print_color(block);

  printf("[\n");
  printf("\taddr: ");
  print_pointer(block);
  puts("");
  printf("\tsize: %zd\n", get_size(block) );
  printf("\tleft_size: %zd\n", block->left_size);
  printf("\tallocated: %s\n", allocated_to_string(get_state(block)));
  if (!get_state(block)) {
    printf("\tprev: ");
    print_pointer(block->prev);
    puts("");

    printf("\tnext: ");
    print_pointer(block->next);
    puts("");
  }
  printf("]\n");

  clear_color();
}

/**
 * @brief Simple printer that just prints the allocation status of each block
 *
 * @param block The block to print
 */
void print_status(header * block) {
  print_color(block);
  switch(get_state(block)) {
    case UNALLOCATED:
      printf("[U]");
      break;
    case ALLOCATED:
      printf("[A]");
      break;
    case FENCEPOST:
      printf("[F]");
      break;
  }
  clear_color();
}

/*
static void print_bitmap() {
  printf("bitmap: [");
  for(int i = 0; i < N_LISTS; i++) {
    if ((freelist_bitmap[i >> 3] >> (i & 7)) & 1) {
      printf("\033[32m#\033[0m");
    } else {
      printf("\033[34m_\033[0m");
    }
    if (i % 8 == 7) {
      printf(" ");
    }
  }
  puts("]");
}
*/

/**
 * @brief Print a linked list between two nodes using a provided print function
 *
 * @param pf Function to perform the actual printing
 * @param start Node to start printing at
 * @param end Node to stop printing at
 */
void print_sublist(printFormatter pf, header * start, header * end) {  
  for (header * cur = start; cur != end; cur = cur->next) {
    pf(cur); 
  }
}

/**
 * @brief print the full freelist
 *
 * @param pf Function to perform the header printing
 */
void freelist_print(printFormatter pf) {
  if (!pf) {
    return;
  }

  for (size_t i = 0; i < N_LISTS; i++) {
    header * freelist = &freelistSentinels[i];
    if (freelist->next != freelist) {
      printf("L%zu: ", i);
      print_sublist(pf, freelist->next, freelist);
      puts("");
    }
    fflush(stdout);
  }
}

/**
 * @brief print the boundary tags from each chunk from the OS
 *
 * @param pf Function to perform the header printing
 */
void tags_print(printFormatter pf) {
  if (!pf) {
    return;
  }

  for (size_t i = 0; i < numOsChunks; i++) {
    header * chunk = osChunkList[i];
    pf(chunk);
    for (chunk = get_right_header(chunk);
         get_state(chunk) != FENCEPOST; 
         chunk = get_right_header(chunk)) {
        pf(chunk);
    }
    pf(chunk);
    fflush(stdout);
  }
}
