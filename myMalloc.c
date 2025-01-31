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

// extra credit functions
void* memalign(size_t alignment, size_t size);
void* valloc(size_t size);
int posix_memalign(void** ptr, size_t alignment, size_t size);
void* pvalloc(size_t size);

static void init();

static bool isMallocInitialized;

/* extra credit bitmap */
char freelist_bitmap[59];

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
        //   neighboring block. Update allocation state of allcoated 
        //   block to ALLOCATED. Return data pointer 
        int current_index = get_free_list_index(get_size(current));
        
        if (get_size(current) >= actual_size + sizeof(header)) {

          // update remaining, left, and allocated blocks 

          header *remaining_block = current;
          set_size(remaining_block, get_size(current) - actual_size);
          set_state(remaining_block, UNALLOCATED);
          header *allocated_block = (header *) get_right_header(remaining_block);
          set_size(allocated_block, actual_size);
          allocated_block -> left_size = get_size(remaining_block);
          set_state(allocated_block, ALLOCATED);
          
          check_free_list_index(get_free_list_index(get_size(remaining_block)));

          header *right_block = get_right_header(allocated_block);
          if (get_size(right_block) != 0) {
            right_block->left_size = get_size(allocated_block);
          }

          // remaining_block needs to be placed in correct free list 

          int new_free_list_index = get_free_list_index(get_size(remaining_block));
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
              
          // If block is large enough to fulfill request, but not split 

          set_state(current, ALLOCATED);
          remove_header(current);
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

/*
  while ((freelistSentinels[free_list_index].next == &freelistSentinels[free_list_index])
          && (free_list_index < (N_LISTS - 1))) {
    free_list_index++;
  }
 */


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
    check_free_list_index(get_free_list_index(get_size(new_chunk)));
    if (new_chunk == NULL) {
      return NULL;
    }
    header *fenceBeforeChunk = (header *)((char *)new_chunk - (2 * ALLOC_HEADER_SIZE));

    /* check if lastFencePost is next to new chunk and coalesce if necessary */

    if (fenceBeforeChunk == lastFencePost) {
      
      /* case 1: neighbors */

      /* Check if previous block is allocated or unallocated */

      header * previousBlock = (header *)((char *)lastFencePost - lastFencePost->left_size);
      if (get_state(previousBlock) == UNALLOCATED) {
        set_size(previousBlock,
                 get_size(previousBlock) + (2 * ALLOC_HEADER_SIZE) + get_size(new_chunk));
        lastFencePost = (header *) ((char *) previousBlock + 
                 get_size(previousBlock) + (2 * ALLOC_HEADER_SIZE) + get_size(new_chunk));
        new_chunk = previousBlock;
        remove_header(new_chunk);
        int new_free_list_index = get_free_list_index(get_size(new_chunk));
        insert_header(new_chunk, new_free_list_index);
      }
      else {
        set_size(lastFencePost,
                 (2 * ALLOC_HEADER_SIZE + get_size(new_chunk)));
        set_state(lastFencePost, UNALLOCATED);
        new_chunk = lastFencePost;
        lastFencePost = (header *) ((char *) lastFencePost + get_size(new_chunk));
        int new_free_list_index = get_free_list_index(get_size(new_chunk));
        insert_header(new_chunk, new_free_list_index);
      }
    }
    else {

      /* case 2: not neighbors: just insert into free_list.  */
 
      header *prevFencePost = get_header_from_offset(new_chunk, -ALLOC_HEADER_SIZE);
      insert_os_chunk(prevFencePost);
      lastFencePost = (header *) ((char *) new_chunk + get_size(new_chunk));
      int new_free_list_index = get_free_list_index(get_size(new_chunk));
      insert_header(new_chunk, new_free_list_index);
    }

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

  /* check double free */

  if (get_state(current) == UNALLOCATED) {
    fprintf(stderr, "Double Free Detected\n");
    #line 577
    assert(false); // Terminate the program as specified
  }
  size_t current_size = get_size(current);
  header *right = get_right_header(current);
  header *left = (header *)((char *)current - current->left_size);
  int right_unallocated = (get_state(right) == UNALLOCATED);
  int left_unallocated = (get_state(left) == UNALLOCATED);

  if (!right_unallocated && !left_unallocated) {

      // Case 1: Neither neighbor is unallocated

      set_state(current, UNALLOCATED);
      int free_list_index = get_free_list_index(get_size(current));
      insert_header(current, free_list_index);
  } else if (right_unallocated && !left_unallocated) {

      // Case 2: Only the right block is unallocated

      set_state(current, UNALLOCATED);
      size_t new_size = current_size + get_size(right);

      /* stay in same spot of free list if last list*/

      if (get_free_list_index(get_size(right)) == 58) {
        set_size(right, new_size);
        right -> left_size = current -> left_size;
        set_state(right, UNALLOCATED);
        header *new_right = get_right_header(right);
        new_right -> left_size = new_size;
      }
      else {
        remove_header(right);
        set_state(current, UNALLOCATED);
        set_size(current, new_size);
        header *new_right = get_right_header(current);
        new_right->left_size = new_size;
        int free_list_index = get_free_list_index(new_size);
        insert_header(current, free_list_index);
      }
  } else if (!right_unallocated && left_unallocated) {

      // Case 3: Only the left block is unallocated

      set_state(current, UNALLOCATED);
      size_t new_size = current_size + get_size(left);

      /* stay in same spot of free list if last list */

      if (get_free_list_index(get_size(left)) == 58) {
        set_size(left, new_size);
        set_state(left, UNALLOCATED);
        header *new_right = get_right_header(left);
        new_right -> left_size = new_size;
      }
      else {
        remove_header(left);
        set_state(left, UNALLOCATED);
        set_size(left, new_size);
        header *new_right = get_right_header(left);
        new_right->left_size = new_size;
        int free_list_index = get_free_list_index(new_size);
        insert_header(left, free_list_index);
      }
  } else {

     // Case 4: Both neighbors are unallocated

     set_state(current, UNALLOCATED);
     size_t new_size = current_size + get_size(right) + get_size(left);
     remove_header(right);
     remove_header(left);
     set_state(left, UNALLOCATED);
     set_size(left, new_size);
     header *new_right = get_right_header(left);
     new_right->left_size = new_size;
     int free_list_index = get_free_list_index(new_size);
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

/* new */
/*
  if (!isMallocInitialized) {
    isMallocInitialized = 1;
    init();

  }
*/
/* new */

  header * hdr = allocate_object(size);
  void * data = hdr -> data; 
  pthread_mutex_unlock(&mutex);
    
  if (hdr == NULL) {
    return NULL;
  }
  return data;
}

void * my_calloc(size_t nmemb, size_t size) {
  return memset(my_malloc(size * nmemb), 0, size * nmemb);
}

void * my_realloc(void * ptr, size_t size) {

  header * chunk = ptr - (2 * sizeof(size_t));
  size_t old_chunk_size = get_size(chunk);
  header * right = get_right_header(chunk);
  size_t new_size = calculate_actual_size(size);
  
  if ((get_state(right) == UNALLOCATED) && ((get_size(chunk) + get_size(right)) >= new_size)) {
    set_size(chunk, new_size);
    header * new_right = (header *) ((char *) chunk + new_size);
    set_size(new_right, get_size(right) - (new_size - old_chunk_size));
    new_right -> left_size = new_size;
    
    /* set right of right header */

    header * right_of_right = get_right_header(new_right);
    right_of_right -> left_size = get_size(new_right);
       
    /* fix free list */

    remove_header(right);
    insert_header(new_right, get_free_list_index(get_size(new_right)));  
    return ptr;
  }
  else {
    void * mem = my_malloc(size);
    memcpy(mem, ptr, size);
    my_free(ptr);
    return mem;

  }
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

/* extra credit */

// Function 1: memalign()
void* memalign(size_t alignment, size_t size) {
    // Ensure alignment is a power of 2 and greater than or equal to sizeof(void*)
    // Allocate memory with the specified alignment
    // Return pointer to the allocated memory or NULL on failure
}

// Function 2: valloc()
void* valloc(size_t size) {
    // Get the system page size
    // Call memalign() with page size alignment
    // Return pointer to allocated memory or NULL on failure
}

// Function 3: posix_memalign()
int posix_memalign(void** ptr, size_t alignment, size_t size) {
    // Ensure alignment is a power of 2 and a multiple of sizeof(void*)
    // Allocate aligned memory and store the pointer in *ptr
    // Return 0 on success or an error code on failure
}

// Function 4: pvalloc()
void* pvalloc(size_t size) {
    // Get the system page size
    // Round size up to the nearest multiple of page size
    // Call memalign() with page size alignment
    // Return pointer to allocated memory or NULL on failure
}
