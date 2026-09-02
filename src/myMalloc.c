#include <errno.h>
#include <pthread.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "myMalloc.h"

#define MALLOC_COLOR "MALLOC_DEBUG_COLOR"
#define ALIGN(size) (((size) + 7) & ~7)

static bool check_env;
static bool use_color;

/*
 * Mutex to ensure thread safety for the freelist
 */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
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
//static inline bool verify_freelist();
static inline header * verify_chunk(header * chunk);
static inline bool verify_tags();

// extra credit functions
void* memalign(size_t alignment, size_t size);
void* valloc(size_t size);
int posix_memalign(void** ptr, size_t alignment, size_t size);
void* pvalloc(size_t size);

static void init();

void print_hello();
static bool isMallocInitialized;

/* extra credit bitmap */
char freelist_bitmap[59];



void print_hello() {
  printf("hello");
}


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
  if (mem == (void *) -1) {
    // sbrk failed – return NULL so caller can handle
    return NULL;
  }
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
  if (free_list_index > N_LISTS - 1) {
      free_list_index = N_LISTS - 1;
  }

  /* iterate through free_lists until reach non empty OR last list*/
  
  //while ((freelistSentinels[free_list_index].next == &freelistSentinels[free_list_index])
  //        && (free_list_index < (N_LISTS - 1))) {
  //  free_list_index++;
  //}
  return free_list_index;
}

/* function to check if free_list_index is going to be empty and set to 0 if it is */

static void check_free_list_index(int free_list_index) {
  if (freelistSentinels[free_list_index].next == &freelistSentinels[free_list_index]) {
    freelist_bitmap[free_list_index >> 3] &= ~(1 << (free_list_index & 7));
    //freelist_bitmap[free_list_index] = 0;
  }
  else {
    freelist_bitmap[free_list_index >> 3] |= (1 << (free_list_index & 7));
    //freelist_bitmap[free_list_index] = 1;
  }
}



/* helper function to remove header from list */

static void remove_header(header * block) {
  if (block -> prev != NULL) {
    block -> prev -> next = block -> next;
  }
  if (block -> next != NULL) {
    block -> next -> prev = block -> prev;
  }
  check_free_list_index(get_free_list_index(get_size(block)));
}

/* helper function to insert header into correct list*/

static void insert_header(header * remaining_block, int new_free_list_index) {
  header *new_sentinel = &freelistSentinels[new_free_list_index];
  new_sentinel -> next -> prev = remaining_block;
  remaining_block->next = new_sentinel->next; 
  remaining_block->prev = new_sentinel; 
  new_sentinel -> next = remaining_block;
  check_free_list_index(new_free_list_index);
}

/* helper function to split */

static header * split_if_necessary(header * current, size_t actual_size, int free_list_index) {

        // Split if necessary. Update size and left size fields of
        //   neighboring block. Update allocation state of allocated
        //   block to ALLOCATED. Return the allocated header.
        size_t orig_size = get_size(current);

        if (orig_size >= actual_size + sizeof(header)) {

          // Keep the left part ('current') free, carve the allocated block
          // out of the right side so the free remainder keeps its address.
          int orig_index = get_free_list_index(orig_size);
          size_t remaining_size = orig_size - actual_size;
          int new_index = get_free_list_index(remaining_size);

          // Resize the free remainder, moving it to another list only if its
          // size class changed. The remove MUST happen before the resize so
          // that remove_header updates the bitmap bit for the ORIGINAL list
          // (it derives the list index from the block's current size).
          if (new_index != orig_index) {
            remove_header(current);
            set_size(current, remaining_size);   // state stays UNALLOCATED
            insert_header(current, new_index);
          } else {
            set_size(current, remaining_size);
          }

          header *allocated_block = get_right_header(current);
          set_size(allocated_block, actual_size);
          set_state(allocated_block, ALLOCATED);
          allocated_block->left_size = remaining_size;

          // Fix the left_size of whatever sits to the right (block or fencepost)
          header *right_block = get_right_header(allocated_block);
          right_block->left_size = actual_size;

          return allocated_block;
        }
        else {

          // Large enough to fulfill the request but not to split: use it whole.
          remove_header(current);
          set_state(current, ALLOCATED);
          return current;
        }
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

  /* bitmap optimization */

  for (int i = free_list_index; i < N_LISTS - 1; i++) {
    if ((freelist_bitmap[i >> 3] >> (i & 7)) & 1) {
      free_list_index = i;
      break;
    }
    free_list_index++;
  }

  /* If free_list_index isn't the last index, set the block to allocated and
     return the block's data pointer. If it is the last index, traverse the last
     linked list */

  if (free_list_index < N_LISTS - 1) {
    header *block = freelistSentinels[free_list_index].next;
    return split_if_necessary(block, actual_size, free_list_index);
  }
  else { 
    header *sentinel = &freelistSentinels[N_LISTS - 1];
    header *current = sentinel->next;
    while (current != sentinel) {
      if (get_size(current) >= actual_size) {
        
        /* Block of sufficient size found */

        return split_if_necessary(current, actual_size, free_list_index);
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
    // The new chunk's right fencepost is the heap's new end in every case.
    header *rightFence = get_right_header(new_chunk);
    header *fenceBeforeChunk = (header *)((char *)new_chunk - (2 * ALLOC_HEADER_SIZE));

    /* check if lastFencePost is next to new chunk and coalesce if necessary */

    if (fenceBeforeChunk == lastFencePost) {

      /* case 1: contiguous with the previous chunk. The two fenceposts
         between them (2 * ALLOC_HEADER_SIZE) can be reclaimed. */

      header * previousBlock = get_left_header(lastFencePost);
      if (get_state(previousBlock) == UNALLOCATED) {

        /* extend the previous free block over the fenceposts and new chunk */
        remove_header(previousBlock);
        set_size(previousBlock,
                 get_size(previousBlock) + (2 * ALLOC_HEADER_SIZE) + get_size(new_chunk));
        rightFence->left_size = get_size(previousBlock);
        insert_header(previousBlock, get_free_list_index(get_size(previousBlock)));
        new_chunk = previousBlock;
      }
      else {

        /* previous block is in use: reuse the old fencepost as the header */
        set_size_and_state(lastFencePost,
                 2 * ALLOC_HEADER_SIZE + get_size(new_chunk), UNALLOCATED);
        rightFence->left_size = get_size(lastFencePost);
        insert_header(lastFencePost, get_free_list_index(get_size(lastFencePost)));
        new_chunk = lastFencePost;
      }
    }
    else {

      /* case 2: not contiguous: register a new OS chunk and insert as-is */

      header *prevFencePost = get_header_from_offset(new_chunk, -ALLOC_HEADER_SIZE);
      insert_os_chunk(prevFencePost);
      insert_header(new_chunk, get_free_list_index(get_size(new_chunk)));
    }

    lastFencePost = rightFence;

    /* check if new size is big enough for the request: actual_size */

    if (get_size(new_chunk) >= actual_size) {
      int new_free_list_index = get_free_list_index(get_size(new_chunk));
      return split_if_necessary(new_chunk, actual_size, new_free_list_index); 
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

  header *current = ptr_to_header(p);

  /* check double free */

  if (get_state(current) == UNALLOCATED) {
    fprintf(stderr, "Double Free Detected\n");
    assert(false); // Terminate the program as specified
  }

  /* Mark this block free immediately. Even when it is coalesced into its left
   * neighbour below (so this header becomes interior), the UNALLOCATED marker
   * left behind lets a later double free of the same pointer be detected. */
  set_state(current, UNALLOCATED);

  header *right = get_right_header(current);
  header *left  = get_left_header(current);
  int right_unallocated = (get_state(right) == UNALLOCATED);
  int left_unallocated  = (get_state(left)  == UNALLOCATED);

  if (!right_unallocated && !left_unallocated) {

      // Case 1: Neither neighbor is free. Just free 'current' in place.

      set_state(current, UNALLOCATED);
      insert_header(current, get_free_list_index(get_size(current)));
  } else if (right_unallocated && !left_unallocated) {

      // Case 2: Only the right block is free. Absorb it into 'current'.

      remove_header(right);
      set_size(current, get_size(current) + get_size(right));
      set_state(current, UNALLOCATED);
      get_right_header(current)->left_size = get_size(current);
      insert_header(current, get_free_list_index(get_size(current)));
  } else if (!right_unallocated && left_unallocated) {

      // Case 3: Only the left block is free. Absorb 'current' into it.

      remove_header(left);
      set_size(left, get_size(left) + get_size(current));
      get_right_header(left)->left_size = get_size(left);
      insert_header(left, get_free_list_index(get_size(left)));
  } else {

     // Case 4: Both neighbors are free. Merge all three into 'left'.

     remove_header(left);
     remove_header(right);
     set_size(left, get_size(left) + get_size(current) + get_size(right));
     get_right_header(left)->left_size = get_size(left);
     insert_header(left, get_free_list_index(get_size(left)));
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
void * malloc(size_t size) {
  pthread_mutex_lock(&mutex);

  if (!isMallocInitialized) {
    isMallocInitialized = 1;
    init();

  }

  header * hdr = allocate_object(size);
  if (hdr == NULL) {
    pthread_mutex_unlock(&mutex);
    return NULL;  // out of memory
  }
  void * data = hdr -> data; 
  pthread_mutex_unlock(&mutex);
    
  return data;
}

void *calloc(size_t nmemb, size_t size) {
  if (nmemb != 0 && size > SIZE_MAX / nmemb) {
    errno = ENOMEM;
    return NULL;
  }
  size_t total = nmemb * size;
  void *p = malloc(total);
  if (!p) return NULL;
  memset(p, 0, total);
  return p;
}

void *realloc(void *ptr, size_t size) {
  if (ptr == NULL) return malloc(size);
  if (size == 0) { free(ptr); return NULL; }

  header *h = ptr_to_header(ptr);
  size_t old_total = get_size(h);
  size_t old_payload = old_total - ALLOC_HEADER_SIZE;

  void *newp = malloc(size);
  if (!newp) return NULL;

  size_t copy = (size < old_payload) ? size : old_payload;
  memcpy(newp, ptr, copy);
  free(ptr);
  return newp;
}

void free(void * p) {
  pthread_mutex_lock(&mutex);
  deallocate_object(p);
  pthread_mutex_unlock(&mutex);
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

