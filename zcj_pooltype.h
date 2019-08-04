
#define zcj_size_t   unsigned int
#define ZCJ_UNUSED_ARG(arg)  (void)arg
#ifdef WIN32
#ifndef pthread_mutex_t
#define pthread_mutex_t void*
#endif
#else
#include <pthread.h>
#endif


struct zcj_pool_mem
{
    zcj_size_t		memsize;
    struct zcj_pool_mem *next;
};

 
typedef struct zcj_pool_t
{
    struct zcj_pool_mem *first_mem;
    char	        obj_name[32];
    zcj_size_t		used_size;
  //  long long		free_size;
    pthread_mutex_t *pool_lock;
}zcj_pool_t;

