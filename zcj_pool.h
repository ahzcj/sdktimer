#ifndef _ZCJ_POOL_H_
#define _ZCJ_POOL_H_

#include <stdlib.h>
#include <string.h>
#include "zcj_pooltype.h"

#define zcj_pool_create(nm,init,inc)   \
	zcj_pool_create_imp(__FILE__, __LINE__,nm, init, inc)
#define zcj_pool_free(pool,point)	    \
		zcj_pool_free_imp(__FILE__, __LINE__,pool, point)
		
#define zcj_pool_release(pool)		    zcj_pool_release_imp(pool)
#define zcj_pool_getobjname(pool)	    zcj_pool_getobjname_imp(pool)
#define zcj_pool_reset(pool)		    zcj_pool_reset_imp(pool)
#define zcj_pool_get_capacity(pool)	    zcj_pool_get_capacity_imp(pool)
#define zcj_pool_get_used_size(pool)	    zcj_pool_get_used_size_imp(pool)
#define zcj_pool_alloc(pool,sz)		    \
	zcj_pool_alloc_imp(__FILE__, __LINE__, pool, sz)

#define zcj_pool_calloc(pool,cnt,elem)	    \
	zcj_pool_calloc_imp(__FILE__, __LINE__, pool, cnt, elem)

#define zcj_pool_zalloc(pool,sz)		    \
	zcj_pool_zalloc_imp(__FILE__, __LINE__, pool, sz)

zcj_pool_t* zcj_pool_create_imp( const char *file, int line,
				       const char *name,
				       zcj_size_t initial_size,
				       zcj_size_t increment_size
				         );
void zcj_pool_reset_imp(zcj_pool_t *pool);
void zcj_pool_release_imp(zcj_pool_t *pool);
const char* zcj_pool_getobjname_imp(zcj_pool_t *pool);
zcj_size_t zcj_pool_get_used_size_imp(zcj_pool_t *pool);
void* zcj_pool_alloc_imp( const char *file, int line, 
				 zcj_pool_t *pool, zcj_size_t sz);

void* zcj_pool_zalloc_imp( const char *file, int line, 
				  zcj_pool_t *pool, zcj_size_t sz);
void* zcj_pool_calloc_imp( const char *file, int line, 
				  zcj_pool_t *pool, unsigned cnt, 
				  unsigned elemsz);

void zcj_pool_free_imp( const char *file, int line, 
				 zcj_pool_t *pool, void* point);


#endif
