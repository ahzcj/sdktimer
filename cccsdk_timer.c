#include "cccsdk_timer.h"
#define THIS_FILE	"cccsdk_timer.c"

#define HEAP_PARENT(X)	(X == 0 ? 0 : (((X) - 1) / 2))
#define HEAP_LEFT(X)	(((X)+(X))+1)


#define DEFAULT_MAX_TIMED_OUT_PER_POLL  (64)

#ifdef WIN32
#include <windows.h>
#define pthread_mutex_t	 HANDLE
#define pthread_mutex_lock(pobject)	 WaitForSingleObject(*pobject,INFINITE)
#define pthread_mutex_unlock(pobject)	 ReleaseMutex(*pobject)
#define pthread_mutex_init(pobject,pattr) (*pobject=CreateMutex(NULL,FALSE,NULL))
#define pthread_mutex_destroy(pobject) CloseHandle(*pobject)

#endif

/**
 * The implementation of timer heap.
 */
;

void ccc_time_val_normalize(ccc_time_val *t)
{
    if (t->msec >= 1000) {
	t->sec += (t->msec / 1000);
	t->msec = (t->msec % 1000);
    }
    else if (t->msec <= -1000) {
	do {
	    t->sec--;
	    t->msec += 1000;
        } while (t->msec <= -1000);
    }

    if (t->sec >= 1 && t->msec < 0) {
	t->sec--;
	t->msec += 1000;

    } else if (t->sec < 0 && t->msec > 0) {
	t->sec++;
	t->msec -= 1000;
    }
}


static inline void lock_timer_heap( ccc_timer_heap_t *ht )
{
    if (ht->lock) {
	pthread_mutex_lock(ht->lock);
    }
}

static inline void unlock_timer_heap( ccc_timer_heap_t *ht )
{
    if (ht->lock) {
	pthread_mutex_unlock(ht->lock);
    }
}


static void copy_node( ccc_timer_heap_t *ht, int slot, ccc_timer_entry *moved_node )
{

    // Insert <moved_node> into its new location in the heap.
    ht->heap[slot] = moved_node;
    
    // Update the corresponding slot in the parallel <timer_ids_> array.
    ht->timer_ids[moved_node->_timer_id] = slot;
}

static int pop_freelist( ccc_timer_heap_t *ht )
{
    // We need to truncate this to <int> for backwards compatibility.
    int new_id = ht->timer_ids_freelist;
    
    // The freelist values in the <timer_ids_> are negative, so we need
    // to negate them to get the next freelist "pointer."
    ht->timer_ids_freelist =
	-ht->timer_ids[ht->timer_ids_freelist];
    
    return new_id;
    
}

static void push_freelist (ccc_timer_heap_t *ht, int old_id)
{

    // The freelist values in the <timer_ids_> are negative, so we need
    // to negate them to get the next freelist "pointer."
    ht->timer_ids[old_id] = -ht->timer_ids_freelist;
    ht->timer_ids_freelist = old_id;
}


static void reheap_down(ccc_timer_heap_t *ht, ccc_timer_entry *moved_node,
                        size_t slot, size_t child)
{

    while (child < ht->cur_size)
    {
	// Choose the smaller of the two children.
	if (child + 1 < ht->cur_size
	    && CCC_TIME_VAL_LT(ht->heap[child + 1]->_timer_value, ht->heap[child]->_timer_value))
	    child++;
	
	// Perform a <copy> if the child has a larger timeout value than
	// the <moved_node>.
	if (CCC_TIME_VAL_LT(ht->heap[child]->_timer_value, moved_node->_timer_value))
        {
	    copy_node( ht, slot, ht->heap[child]);
	    slot = child;
	    child = HEAP_LEFT(child);
        }
	else
	    // We've found our location in the heap.
	    break;
    }
    
    copy_node( ht, slot, moved_node);
}

static void reheap_up( ccc_timer_heap_t *ht, ccc_timer_entry *moved_node,
		       size_t slot, size_t parent)
{
    // Restore the heap property after an insertion.
    
    while (slot > 0)
    {
	// If the parent node is greater than the <moved_node> we need
	// to copy it down.
	if (CCC_TIME_VAL_LT(moved_node->_timer_value, ht->heap[parent]->_timer_value))
        {
	    copy_node(ht, slot, ht->heap[parent]);
	    slot = parent;
	    parent = HEAP_PARENT(slot);
        }
	else
	    break;
    }
    
    // Insert the new node into its proper resting place in the heap and
    // update the corresponding slot in the parallel <timer_ids> array.
    copy_node(ht, slot, moved_node);
}


static ccc_timer_entry * remove_node( ccc_timer_heap_t *ht, size_t slot)
{
    ccc_timer_entry *removed_node = ht->heap[slot];
    
    // Return this timer id to the freelist.
    push_freelist( ht, removed_node->_timer_id );
    
    // Decrement the size of the heap by one since we're removing the
    // "slot"th node.
    ht->cur_size--;
    
    // Set the ID
    removed_node->_timer_id = -1;

    // Only try to reheapify if we're not deleting the last entry.
    
    if (slot < ht->cur_size)
    {
	int parent;
	ccc_timer_entry *moved_node = ht->heap[ht->cur_size];
	
	// Move the end node to the location being removed and update
	// the corresponding slot in the parallel <timer_ids> array.
	copy_node( ht, slot, moved_node);
	
	// If the <moved_node->time_value_> is great than or equal its
	// parent it needs be moved down the heap.
	parent = HEAP_PARENT (slot);
	
	if (CCC_TIME_VAL_GTE(moved_node->_timer_value, ht->heap[parent]->_timer_value))
	    reheap_down( ht, moved_node, slot, HEAP_LEFT(slot));
	else
	    reheap_up( ht, moved_node, slot, parent);
    }
    
    return removed_node;
}

static void grow_heap(ccc_timer_heap_t *ht)
{
    // All the containers will double in size from max_size_
    size_t new_size = ht->max_size * 2;
    int *new_timer_ids;
    ccc_size_t i;
    
    // First grow the heap itself.
    
    ccc_timer_entry **new_heap = 0;
    
    new_heap = (ccc_timer_entry**) 
    	       zcj_pool_alloc(ht->pool, sizeof(ccc_timer_entry*) * new_size);
    memcpy(new_heap, ht->heap, ht->max_size * sizeof(ccc_timer_entry*));
    //delete [] this->heap_;
    ht->heap = new_heap;
    
    // Grow the array of timer ids.
    
    new_timer_ids = 0;
    new_timer_ids = (int*)
    		    zcj_pool_alloc(ht->pool, new_size * sizeof(int));
    
    memcpy( new_timer_ids, ht->timer_ids, ht->max_size * sizeof(int));
    
    //delete [] timer_ids_;
    ht->timer_ids = new_timer_ids;
    
    // And add the new elements to the end of the "freelist".
    for (i = ht->max_size; i < new_size; i++)
	ht->timer_ids[i] = -((int) (i + 1));
    
    ht->max_size = new_size;
}

static void insert_node(ccc_timer_heap_t *ht, ccc_timer_entry *new_node)
{
    if (ht->cur_size + 2 >= ht->max_size)
	grow_heap(ht);
    
    reheap_up( ht, new_node, ht->cur_size, HEAP_PARENT(ht->cur_size));
    ht->cur_size++;
}


static int schedule_entry( ccc_timer_heap_t *ht,
				  ccc_timer_entry *entry, 
				   const ccc_time_val *future_time )
{
    if (ht->cur_size < ht->max_size)
    {
	// Obtain the next unique sequence number.
	// Set the entry
	entry->_timer_id = pop_freelist(ht);
	entry->_timer_value = *future_time;
	insert_node( ht, entry);
	return 0;
    }
    else
	return -1;
}


static int cancel(ccc_timer_heap_t *ht, 
		   ccc_timer_entry *entry, 
		   int dont_call)
{
  long timer_node_slot;

  // Check to see if the timer_id is out of range
  if (entry->_timer_id < 0 || (ccc_size_t)entry->_timer_id > ht->max_size)
    return 0;

  timer_node_slot = ht->timer_ids[entry->_timer_id];

  if (timer_node_slot < 0) // Check to see if timer_id is still valid.
    return 0;

  if (entry != ht->heap[timer_node_slot])
    {
      assert(entry == ht->heap[timer_node_slot]);
      return 0;
    }
  else
    {
      remove_node( ht, timer_node_slot);

      if (dont_call == 0)
        // Call the close hook.
	(*ht->callback)(ht, entry);
      return 1;
    }
}


/*
 * Calculate memory size required to create a timer heap.
 */
ccc_size_t ccc_timer_heap_mem_size(ccc_size_t count)
{
    return /* size of the timer heap itself: */
           sizeof(ccc_timer_heap_t) + 
           /* size of each entry: */
           (count+2) * (sizeof(ccc_timer_entry*)+sizeof(int)) +
           /* lock, pool etc: */
           132;
}

/*
 * Create a new timer heap.
 */
int ccc_timer_heap_create( zcj_pool_t *pool,
					  ccc_size_t size,
                                          ccc_timer_heap_t **p_heap)
{
    ccc_timer_heap_t *ht;
    ccc_size_t i;

    if(!pool || !p_heap)
	return -1;

    *p_heap = NULL;

    /* Magic? */
    size += 2;

    /* Allocate timer heap data structure from the pool */
    ht = (ccc_timer_heap_t*)zcj_pool_alloc(pool, sizeof(ccc_timer_heap_t));
    if (!ht)
        return -1;

    /* Initialize timer heap sizes */
    ht->max_size = size;
    ht->cur_size = 0;
    ht->max_entries_per_poll = DEFAULT_MAX_TIMED_OUT_PER_POLL;
    ht->timer_ids_freelist = 1;
    ht->pool = pool;

    /* Lock. */
    ht->lock = NULL;
    ht->auto_delete_lock = 0;

    // Create the heap array.
    ht->heap = (ccc_timer_entry**)
    	       zcj_pool_alloc(pool, sizeof(ccc_timer_entry*) * size);
    if (!ht->heap)
        return -1;

    // Create the parallel
    ht->timer_ids = (int *)
    		    zcj_pool_alloc( pool, sizeof(int) * size);
    if (!ht->timer_ids)
        return -1;

    // Initialize the "freelist," which uses negative values to
    // distinguish freelist elements from "pointers" into the <heap_>
    // array.
    for (i=0; i<size; ++i)
	ht->timer_ids[i] = -((int) (i + 1));

    *p_heap = ht;
    return 0;
}

void  ccc_timer_heap_destroy( ccc_timer_heap_t *ht )
{
    if (ht->lock && ht->auto_delete_lock) {
        pthread_mutex_destroy(ht->lock);
        ht->lock = NULL;
    }
}

void ccc_timer_heap_set_lock(ccc_timer_heap_t *ht,
                                      pthread_mutex_t *lock,
                                      int auto_del )
{
    if (ht->lock && ht->auto_delete_lock)
        pthread_mutex_destroy(ht->lock);

    ht->lock = lock;
    ht->auto_delete_lock = auto_del;
}


unsigned ccc_timer_heap_set_max_timed_out_per_poll(ccc_timer_heap_t *ht,
                                                          unsigned count )
{
    unsigned old_count = ht->max_entries_per_poll;
    ht->max_entries_per_poll = count;
    return old_count;
}

ccc_timer_entry* ccc_timer_entry_init( ccc_timer_entry *entry,
                                             int id,
                                             void *user_data,
                                             ccc_timer_heap_callback *cb )
{
    if(!entry ||! cb)
	return NULL;

    entry->_timer_id = -1;
    entry->id = id;
    entry->user_data = user_data;
    entry->cb = cb;

    return entry;
}

static int schedule_w_grp_lock(ccc_timer_heap_t *ht,
                                       ccc_timer_entry *entry,
                                       const ccc_time_val *delay,
                                       ccc_bool_t set_id,
                                       int id_val )
{
    int status;
    ccc_time_val expires;

    CCC_ASSERT_RETURN(ht && entry && delay, -1);
    CCC_ASSERT_RETURN(entry->cb != NULL, -1);

    /* Prevent same entry from being scheduled more than once */
    CCC_ASSERT_RETURN(entry->_timer_id < 1, -1);

    ccc_gettickcount(&expires);
    CCC_TIME_VAL_ADD(expires, *delay);
    
    lock_timer_heap(ht);
    status = schedule_entry(ht, entry, &expires);
    if (status == 0) {
	if (set_id)
	    entry->id = id_val;
    }
    unlock_timer_heap(ht);

    return status;
}


int ccc_timer_heap_schedule( ccc_timer_heap_t *ht,
                                            ccc_timer_entry *entry,
                                            const ccc_time_val *delay)
{
    return schedule_w_grp_lock(ht, entry, delay, CCC_FALSE, 1);
}



static int cancel_timer(ccc_timer_heap_t *ht,
			ccc_timer_entry *entry,
			ccc_bool_t set_id,
			int id_val)
{
    int count;

    CCC_ASSERT_RETURN(ht && entry, -1);

    lock_timer_heap(ht);
    count = cancel(ht, entry, 1);
    if (set_id) {
	entry->id = id_val;
    }
   
    unlock_timer_heap(ht);

    return count;
}

int ccc_timer_heap_cancel( ccc_timer_heap_t *ht,
				  ccc_timer_entry *entry)
{
    return cancel_timer(ht, entry, CCC_FALSE, 0);
}

int ccc_timer_heap_cancel_if_active(ccc_timer_heap_t *ht,
                                           ccc_timer_entry *entry,
                                           int id_val)
{
    return cancel_timer(ht, entry, CCC_TRUE, id_val);
}

/**
 * This structure represents high resolution (64bit) time value. The time
 * values represent time in cycles, which is retrieved by calling
 * #pj_get_timestamp().
 */
typedef union ccc_timestamp
{
    unsigned long long u64;        /**< The whole 64-bit value, where available. */
} ccc_timestamp;

#ifdef WIN32
/*
 * Use QueryPerformanceCounter and QueryPerformanceFrequency.
 * This should be the default implementation to be used on Windows.
 */
int ccc_get_timestamp(ccc_timestamp *ts)
{
    LARGE_INTEGER val;

    if (!QueryPerformanceCounter(&val))
	return -1;

    ts->u64 = val.QuadPart;
    return CCC_SUCCESS;
}

int  ccc_get_timestamp_freq(ccc_timestamp *freq)
{
    LARGE_INTEGER val;

    if (!QueryPerformanceFrequency(&val))
	return -1;

    freq->u64 = val.QuadPart;
    return CCC_SUCCESS;
}

#elif _LINUX
#include <sys/time.h>
#include <time.h>
#include <errno.h>

#define NSEC_PER_SEC	1000000000

int ccc_get_timestamp(ccc_timestamp *ts)
{
    struct timespec tp;

    if (clock_gettime(CLOCK_MONOTONIC, &tp) != 0) {
	return -1;
    }

    ts->u64 = tp.tv_sec;
    ts->u64 *= NSEC_PER_SEC;
    ts->u64 += tp.tv_nsec;

    return CCC_SUCCESS;
}

int ccc_get_timestamp_freq(ccc_timestamp *freq)
{
    freq->u64 = NSEC_PER_SEC;

    return CCC_SUCCESS;
}

#elif _IOS
#include <mach/mach.h>
#include <mach/clock.h>
#include <errno.h>

#define NSEC_PER_SEC	1000000000

int ccc_get_timestamp(ccc_timestamp *ts)
{
    mach_timespec_t tp;
    int ret;
    clock_serv_t serv;

    ret = host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &serv);
    if (ret != KERN_SUCCESS) {
	return -1;
    }

    ret = clock_get_time(serv, &tp);
    if (ret != KERN_SUCCESS) {
	return -1;
    }

    ts->u64 = tp.tv_sec;
    ts->u64 *= NSEC_PER_SEC;
    ts->u64 += tp.tv_nsec;

    return CCC_SUCCESS;
}

int ccc_get_timestamp_freq(ccc_timestamp *freq)
{
    freq->u64 = NSEC_PER_SEC;

    return CCC_SUCCESS;
}

#else
#include <sys/time.h>
#include <errno.h>

#define USEC_PER_SEC	1000000

int ccc_get_timestamp(ccc_timestamp *ts)
{
    struct timeval tv;

    if (gettimeofday(&tv, NULL) != 0) {
	return -1;
    }

    ts->u64 = tv.tv_sec;
    ts->u64 *= USEC_PER_SEC;
    ts->u64 += tv.tv_usec;

    return CCC_SUCCESS;
}
int ccc_get_timestamp_freq(ccc_timestamp *freq)
{
    freq->u64 = USEC_PER_SEC;

    return CCC_SUCCESS;
}

#endif

static ccc_highprec_t elapsed_msec( const ccc_timestamp *start,
                                   const ccc_timestamp *stop )
{
    ccc_timestamp ts_freq;
    ccc_highprec_t freq, elapsed;

    if (ccc_get_timestamp_freq(&ts_freq) != CCC_SUCCESS)
        return 0;

    freq = ts_freq.u64;

    /* Avoid division by zero. */
    if (freq == 0) freq = 1;

    /* Get elapsed time in cycles. */
    elapsed = stop->u64 - start->u64;

    /* usec = elapsed * MSEC / freq */
    ccc_highprec_mul(elapsed, CCC_MSEC);
    ccc_highprec_div(elapsed, freq);

    return elapsed;
}

ccc_time_val ccc_elapsed_time( const ccc_timestamp *start,
                                     const ccc_timestamp *stop )
{
    ccc_highprec_t elapsed = elapsed_msec(start, stop);
    ccc_time_val tv_elapsed;

    if (elapsed == 0) {
        tv_elapsed.sec = tv_elapsed.msec = 0;
        return tv_elapsed;
    } else {
        ccc_highprec_t sec, msec;

        sec = elapsed;
        ccc_highprec_div(sec, CCC_MSEC);
        tv_elapsed.sec = (long)sec;

        msec = elapsed;
        ccc_highprec_mod(msec, CCC_MSEC);
        tv_elapsed.msec = (long)msec;

        return tv_elapsed;
    }
}

int ccc_gettickcount(ccc_time_val *tv)
{
    ccc_timestamp ts, start;
    int status;

    if ((status = ccc_get_timestamp(&ts)) != CCC_SUCCESS)
        return status;

   // pj_set_timestamp32(&start, 0, 0);
   start.u64 = 0;
    *tv = ccc_elapsed_time(&start, &ts);

    return CCC_SUCCESS;
}

unsigned ccc_timer_heap_poll( ccc_timer_heap_t *ht, 
                                     ccc_time_val *next_delay )
{
    ccc_time_val now;
    unsigned count;

    CCC_ASSERT_RETURN(ht, 0);

    lock_timer_heap(ht);
    if (!ht->cur_size && next_delay) {
	next_delay->sec = next_delay->msec = CCC_MAXINT32;
        unlock_timer_heap(ht);
	return 0;
    }

    count = 0;
    ccc_gettickcount(&now);

    while ( ht->cur_size && 
	    CCC_TIME_VAL_LTE(ht->heap[0]->_timer_value, now) &&
            count < ht->max_entries_per_poll ) 
    {
	ccc_timer_entry *node = remove_node(ht, 0);

	++count;

	

	unlock_timer_heap(ht);

	if (node->cb)
	    (*node->cb)(ht, node);

	lock_timer_heap(ht);
    }
    if (ht->cur_size && next_delay) {
	*next_delay = ht->heap[0]->_timer_value;
	CCC_TIME_VAL_SUB(*next_delay, now);
	if (next_delay->sec < 0 || next_delay->msec < 0)
	    next_delay->sec = next_delay->msec = 0;
    } else if (next_delay) {
	next_delay->sec = next_delay->msec = CCC_MAXINT32;
    }
    unlock_timer_heap(ht);

    return count;
}

ccc_size_t ccc_timer_heap_count( ccc_timer_heap_t *ht )
{
    CCC_ASSERT_RETURN(ht, 0);

    return ht->cur_size;
}

int ccc_timer_heap_earliest_time(ccc_timer_heap_t * ht,
					         ccc_time_val *timeval)
{
    if (ht->cur_size == 0)
        return -1;

    lock_timer_heap(ht);
    *timeval = ht->heap[0]->_timer_value;
    unlock_timer_heap(ht);

    return CCC_SUCCESS;
}


