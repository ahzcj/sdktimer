

#ifndef __CCC_TIMER_H__
#define __CCC_TIMER_H__


#ifdef WIN32
#ifndef pthread_mutex_t
#define pthread_mutex_t void*
#endif
#else
#include <pthread.h>
#endif

#include<assert.h>
#include "zcj_pool.h"

#define ccc_size_t unsigned int

typedef struct ccc_time_val
{
    /** The seconds part of the time. */
    long    sec;

    /** The miliseconds fraction of the time. */
    long    msec;

} ccc_time_val;
/**
 * Normalize the value in time value.
 * @param t     Time value to be normalized.
 */
void ccc_time_val_normalize(ccc_time_val *t);
/**
 * Get the total time value in miliseconds. This is the same as
 * multiplying the second part with 1000 and then add the miliseconds
 * part to the result.
 *
 * @param t     The time value.
 * @return      Total time in miliseconds.
 * @hideinitializer
 */
#define CCC_TIME_VAL_MSEC(t)	((t).sec * 1000 + (t).msec)

/**
 * This macro will check if \a t1 is equal to \a t2.
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if both time values are equal.
 * @hideinitializer
 */
#define CCC_TIME_VAL_EQ(t1, t2)	((t1).sec==(t2).sec && (t1).msec==(t2).msec)

/**
 * This macro will check if \a t1 is greater than \a t2
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if t1 is greater than t2.
 * @hideinitializer
 */
#define CCC_TIME_VAL_GT(t1, t2)	((t1).sec>(t2).sec || \
                                ((t1).sec==(t2).sec && (t1).msec>(t2).msec))

/**
 * This macro will check if \a t1 is greater than or equal to \a t2
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if t1 is greater than or equal to t2.
 * @hideinitializer
 */
#define CCC_TIME_VAL_GTE(t1, t2)	(CCC_TIME_VAL_GT(t1,t2) || \
                                 CCC_TIME_VAL_EQ(t1,t2))

/**
 * This macro will check if \a t1 is less than \a t2
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if t1 is less than t2.
 * @hideinitializer
 */
#define CCC_TIME_VAL_LT(t1, t2)	(!(CCC_TIME_VAL_GTE(t1,t2)))

/**
 * This macro will check if \a t1 is less than or equal to \a t2.
 *
 * @param t1    The first time value to compare.
 * @param t2    The second time value to compare.
 * @return      Non-zero if t1 is less than or equal to t2.
 * @hideinitializer
 */
#define CCC_TIME_VAL_LTE(t1, t2)	(!CCC_TIME_VAL_GT(t1, t2))

/**
 * Add \a t2 to \a t1 and store the result in \a t1. Effectively
 *
 * this macro will expand as: (\a t1 += \a t2).
 * @param t1    The time value to add.
 * @param t2    The time value to be added to \a t1.
 * @hideinitializer
 */
#define CCC_TIME_VAL_ADD(t1, t2)	    do {			    \
					(t1).sec += (t2).sec;	    \
					(t1).msec += (t2).msec;	    \
					ccc_time_val_normalize(&(t1)); \
				    } while (0)


/**
 * Substract \a t2 from \a t1 and store the result in \a t1. Effectively
 * this macro will expand as (\a t1 -= \a t2).
 *
 * @param t1    The time value to subsctract.
 * @param t2    The time value to be substracted from \a t1.
 * @hideinitializer
 */
#define CCC_TIME_VAL_SUB(t1, t2)	    do {			    \
					(t1).sec -= (t2).sec;	    \
					(t1).msec -= (t2).msec;	    \
					ccc_time_val_normalize(&(t1)); \
				    } while (0)

#define CCC_ASSERT_RETURN(expr,retval)    \
	    do { \
		if (!(expr)) { return retval; } \
	    } while (0)

#define CCC_MAXINT32  0x7FFFFFFFL

typedef int  ccc_bool_t;


#define CCC_SUCCESS  0

#define CCC_TRUE  1
#define CCC_FALSE 0

#define ccc_highprec_t long long

#define ccc_highprec_mul(a1,a2)   (a1 = a1 * a2)
#define ccc_highprec_div(a1,a2)   (a1 = a1 / a2)
#define CCC_MSEC    (1000)
#define ccc_highprec_mod(a1,a2)   (a1 = a1 % a2)
/** 
 * Forward declaration for pj_timer_entry. 
 */
struct ccc_timer_entry;
struct ccc_timer_heap_t;

/**
 * The type of callback function to be called by timer scheduler when a timer
 * has expired.
 *
 * @param timer_heap    The timer heap.
 * @param entry         Timer entry which timer's has expired.
 */
typedef void ccc_timer_heap_callback(struct ccc_timer_heap_t *timer_heap,
				    struct ccc_timer_entry *entry);



typedef struct ccc_timer_heap_t
{
    /** Pool from which the timer heap resize will get the storage from */
    zcj_pool_t *pool;

    /** Maximum size of the heap. */
    ccc_size_t max_size;

    /** Current size of the heap. */
    ccc_size_t cur_size;

    /** Max timed out entries to process per poll. */
    unsigned max_entries_per_poll;

    /** Lock object. */
    pthread_mutex_t *lock;

    /** Autodelete lock. */
    int auto_delete_lock;

    /**
     * Current contents of the Heap, which is organized as a "heap" of
     * pj_timer_entry *'s.  In this context, a heap is a "partially
     * ordered, almost complete" binary tree, which is stored in an
     * array.
     */
    struct ccc_timer_entry **heap;

    /**
     * An array of "pointers" that allows each pj_timer_entry in the
     * <heap_> to be located in O(1) time.  Basically, <timer_id_[i]>
     * contains the slot in the <heap_> array where an pj_timer_entry
     * with timer id <i> resides.  Thus, the timer id passed back from
     * <schedule_entry> is really an slot into the <timer_ids> array.  The
     * <timer_ids_> array serves two purposes: negative values are
     * treated as "pointers" for the <freelist_>, whereas positive
     * values are treated as "pointers" into the <heap_> array.
     */
    int *timer_ids;

    /**
     * "Pointer" to the first element in the freelist contained within
     * the <timer_ids_> array, which is organized as a stack.
     */
    int timer_ids_freelist;

    /** Callback to be called when a timer expires. */
    ccc_timer_heap_callback *callback;

}ccc_timer_heap_t;

/**
 * This structure represents an entry to the timer.
 */
typedef struct ccc_timer_entry
{
    /** 
     * User data to be associated with this entry. 
     * Applications normally will put the instance of object that
     * owns the timer entry in this field.
     */
    void *user_data;

    /** 
     * Arbitrary ID assigned by the user/owner of this entry. 
     * Applications can use this ID to distinguish multiple
     * timer entries that share the same callback and user_data.
     */
    int id;

    /** 
     * Callback to be called when the timer expires. 
     */
    ccc_timer_heap_callback *cb;

    /** 
     * Internal unique timer ID, which is assigned by the timer heap. 
     * Application should not touch this ID.
     */
    int _timer_id;

    /** 
     * The future time when the timer expires, which the value is updated
     * by timer heap when the timer is scheduled.
     */
    ccc_time_val _timer_value;
} ccc_timer_entry;


/**
 * Calculate memory size required to create a timer heap.
 *
 * @param count     Number of timer entries to be supported.
 * @return          Memory size requirement in bytes.
 */
ccc_size_t ccc_timer_heap_mem_size(ccc_size_t count);

/**
 * Create a timer heap.
 *
 * @param pool      The pool where allocations in the timer heap will be 
 *                  allocated. The timer heap will dynamicly allocate 
 *                  more storate from the pool if the number of timer 
 *                  entries registered is more than the size originally 
 *                  requested when calling this function.
 * @param count     The maximum number of timer entries to be supported 
 *                  initially. If the application registers more entries 
 *                  during runtime, then the timer heap will resize.
 * @param ht        Pointer to receive the created timer heap.
 *
 * @return          PJ_SUCCESS, or the appropriate error code.
 */
int ccc_timer_heap_create( zcj_pool_t *pool,
					   ccc_size_t count,
                                           ccc_timer_heap_t **ht);

/**
 * Destroy the timer heap.
 *
 * @param ht        The timer heap.
 */
void ccc_timer_heap_destroy( ccc_timer_heap_t *ht );


/**
 * Set lock object to be used by the timer heap. By default, the timer heap
 * uses dummy synchronization.
 *
 * @param ht        The timer heap.
 * @param lock      The lock object to be used for synchronization.
 * @param auto_del  If nonzero, the lock object will be destroyed when
 *                  the timer heap is destroyed.
 */
void ccc_timer_heap_set_lock( ccc_timer_heap_t *ht,
                                      pthread_mutex_t *lock,
                                     int  auto_del);

/**
 * Set maximum number of timed out entries to process in a single poll.
 *
 * @param ht        The timer heap.
 * @param count     Number of entries.
 *
 * @return          The old number.
 */
unsigned ccc_timer_heap_set_max_timed_out_per_poll(ccc_timer_heap_t *ht,
                                                           unsigned count );

/**
 * Initialize a timer entry. Application should call this function at least
 * once before scheduling the entry to the timer heap, to properly initialize
 * the timer entry.
 *
 * @param entry     The timer entry to be initialized.
 * @param id        Arbitrary ID assigned by the user/owner of this entry.
 *                  Applications can use this ID to distinguish multiple
 *                  timer entries that share the same callback and user_data.
 * @param user_data User data to be associated with this entry. 
 *                  Applications normally will put the instance of object that
 *                  owns the timer entry in this field.
 * @param cb        Callback function to be called when the timer elapses.
 *
 * @return          The timer entry itself.
 */
ccc_timer_entry* ccc_timer_entry_init( ccc_timer_entry *entry,
                                              int id,
                                              void *user_data,
                                              ccc_timer_heap_callback *cb );

/**
 * Schedule a timer entry which will expire AFTER the specified delay.
 *
 * @param ht        The timer heap.
 * @param entry     The entry to be registered. 
 * @param delay     The interval to expire.
 * @return          PJ_SUCCESS, or the appropriate error code.
 */

int ccc_timer_heap_schedule( ccc_timer_heap_t *ht,
					    ccc_timer_entry *entry, 
					     const ccc_time_val *delay);

/**
 * Cancel a previously registered timer. This will also decrement the
 * reference counter of the group lock associated with the timer entry,
 * if the entry was scheduled with one.
 *
 * @param ht        The timer heap.
 * @param entry     The entry to be cancelled.
 * @return          The number of timer cancelled, which should be one if the
 *                  entry has really been registered, or zero if no timer was
 *                  cancelled.
 */
int ccc_timer_heap_cancel( ccc_timer_heap_t *ht,
				   ccc_timer_entry *entry);

/**
 * Cancel only if the previously registered timer is active. This will
 * also decrement the reference counter of the group lock associated
 * with the timer entry, if the entry was scheduled with one. In any
 * case, set the "id" to the specified value.
 *
 * @param ht        The timer heap.
 * @param entry     The entry to be cancelled.
 * @param id_val    Value to be set to "id"
 *
 * @return          The number of timer cancelled, which should be one if the
 *                  entry has really been registered, or zero if no timer was
 *                  cancelled.
 */
int ccc_timer_heap_cancel_if_active(ccc_timer_heap_t *ht,
                                            ccc_timer_entry *entry,
                                            int id_val);

/**
 * Get the number of timer entries.
 *
 * @param ht        The timer heap.
 * @return          The number of timer entries.
 */
ccc_size_t ccc_timer_heap_count( ccc_timer_heap_t *ht );

/**
 * Get the earliest time registered in the timer heap. The timer heap
 * MUST have at least one timer being scheduled (application should use
 * #pj_timer_heap_count() before calling this function).
 *
 * @param ht        The timer heap.
 * @param timeval   The time deadline of the earliest timer entry.
 *
 * @return          PJ_SUCCESS, or PJ_ENOTFOUND if no entry is scheduled.
 */
int ccc_timer_heap_earliest_time( ccc_timer_heap_t *ht, 
					          ccc_time_val *timeval);

/**
 * Poll the timer heap, check for expired timers and call the callback for
 * each of the expired timers.
 *
 * Note: polling the timer heap is not necessary in Symbian. Please see
 * @ref PJ_SYMBIAN_OS for more info.
 *
 * @param ht         The timer heap.
 * @param next_delay If this parameter is not NULL, it will be filled up with
 *		     the time delay until the next timer elapsed, or 
 *		     PJ_MAXINT32 in the sec part if no entry exist.
 *
 * @return           The number of timers expired.
 */
unsigned ccc_timer_heap_poll( ccc_timer_heap_t *ht, 
                                      ccc_time_val *next_delay);


int ccc_gettickcount(ccc_time_val *tv);


#endif

