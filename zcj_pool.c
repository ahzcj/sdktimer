/* $Id: pool_dbg.c 3553 2011-05-05 06:14:19Z nanang $ */
/* 
 * Copyright (C) 2008-2011 Teluu Inc. (http://www.teluu.com)
 * Copyright (C) 2003-2008 Benny Prijono <benny@prijono.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
 */

/* Uncomment this to enable TRACE_ */
//#undef TRACE_

#include "zcj_pool.h"

#ifdef WIN32
#include <windows.h>
#define pthread_mutex_t	 HANDLE
#define pthread_mutex_lock(pobject)	 WaitForSingleObject(*pobject,INFINITE)
#define pthread_mutex_unlock(pobject)	 ReleaseMutex(*pobject)
#define pthread_mutex_init(pobject,pattr) (*pobject=CreateMutex(NULL,FALSE,NULL))
#define pthread_mutex_destroy(pobject) CloseHandle(*pobject)

#endif



/* Create pool */
zcj_pool_t* zcj_pool_create_imp( const char *file, int line,
				       const char *name,
				       zcj_size_t initial_size,
				       zcj_size_t increment_size
				         )
{
    zcj_pool_t *pool;
    pthread_mutex_t *pool_lock = NULL;

    ZCJ_UNUSED_ARG(file);
    ZCJ_UNUSED_ARG(line);
    ZCJ_UNUSED_ARG(initial_size);
    ZCJ_UNUSED_ARG(increment_size);

    pool_lock = (pthread_mutex_t *)calloc(sizeof(char),sizeof(pthread_mutex_t));
    if (!pool_lock)
	return NULL;
	
    pool = (zcj_pool_t *)calloc(sizeof(char),sizeof(struct zcj_pool_t));
    if (!pool)
    {
    	free(pool_lock);
    	return NULL;
    }

    if (name) {
	strncpy(pool->obj_name, name, sizeof(pool->obj_name));
	pool->obj_name[sizeof(pool->obj_name)-1] = '\0';
    } else {
	strcpy(pool->obj_name, "altpool");
    }

    pool->first_mem = NULL;
    pool->used_size = 0;
#ifdef WIN32
	pthread_mutex_init(pool_lock, NULL);
	if(*pool_lock == NULL)
	{
		free(pool_lock);
		free(pool);
		return NULL;
	}
#else
	int tmpres =  pthread_mutex_init(pool_lock, NULL);
	if(tmpres != 0)
	{
		free(pool_lock);
		free(pool);
		return NULL;
	}
#endif

    pool->pool_lock = pool_lock;
//	if(g_pool_lock == NULL)
	//	pthread_mutex_init(&g_pool_lock, NULL);
    return pool;
}

/* Reset pool */
void zcj_pool_reset_imp(zcj_pool_t *pool)
{
    if(!pool)
	return;	
    struct zcj_pool_mem *mem,*next;
    pthread_mutex_t *pool_lock=pool->pool_lock;
	//pthread_mutex_lock(&g_pool_lock);
	pthread_mutex_lock(pool_lock);
    mem = pool->first_mem;
    while (mem) {
	next = mem->next;
	pool->used_size -= mem->memsize;
	free(mem);
	mem = next;
    }
	  pool->first_mem = NULL;
	pthread_mutex_unlock(pool_lock);
	//pthread_mutex_unlock(&g_pool_lock);
  
}

/* Release pool */
void zcj_pool_release_imp(zcj_pool_t *pool)
{
    if(!pool)
	return;	
    zcj_pool_reset_imp(pool);
    pthread_mutex_destroy(pool->pool_lock);
    free(pool->pool_lock);
    free(pool);
}

/* Get pool name */
const char* zcj_pool_getobjname_imp(zcj_pool_t *pool)
{
    if(!pool)
	return NULL;	
   // ZCJ_UNUSED_ARG(pool);
    return pool->obj_name;
}



/* Get capacity */
zcj_size_t zcj_pool_get_capacity_imp(zcj_pool_t *pool)
{
    ZCJ_UNUSED_ARG(pool);

    /* Unlimited capacity */
    return 0x7FFFFFFFUL;
}

/* Get total used size */
zcj_size_t zcj_pool_get_used_size_imp(zcj_pool_t *pool)
{
    if(!pool)
	return 0;	
    return pool->used_size;
}

/* Allocate memory from the pool */
void* zcj_pool_alloc_imp( const char *file, int line, 
				 zcj_pool_t *pool, zcj_size_t sz)
{
      if(!pool)
	return NULL;	
    struct zcj_pool_mem *mem;
    int allocsize = sz + sizeof(struct zcj_pool_mem);

     if(allocsize%4 != 0)
    {
    	allocsize += (4-allocsize%4);
    }
    ZCJ_UNUSED_ARG(file);
    ZCJ_UNUSED_ARG(line);
   pthread_mutex_t *pool_lock=pool->pool_lock;

    mem = (struct zcj_pool_mem *)malloc(allocsize);
    if (!mem) {
	return NULL;
    }
	pthread_mutex_lock(pool_lock);
       mem->next = pool->first_mem;
       pool->first_mem = mem;
	mem->memsize = sz;
	pool->used_size += sz;
	pthread_mutex_unlock(pool_lock);
    return ((char*)mem) + sizeof(struct zcj_pool_mem);
}

void zcj_pool_free_imp( const char *file, int line, 
				 zcj_pool_t *pool, void* point)
{

    if(!pool || !point)
	return;	
    struct zcj_pool_mem *mem,*next,*pre;
    pthread_mutex_t *pool_lock=pool->pool_lock;

    int size = sizeof(struct zcj_pool_mem);
    pthread_mutex_lock(pool_lock);
    mem = pool->first_mem;
    pre = pool->first_mem;
	
    while (mem) {
	if(((char*)mem)+size  != point)		
	{
		pre = mem;
		mem = mem->next;
	}
	else
	{
		
		if(mem == pool->first_mem)
		{
			
			pool->first_mem = mem->next;
			
		}
		else
		{
			
			pre->next = mem->next;
			
		}
		//pool->free_size += mem->memsize;
		pool->used_size -= mem->memsize;
		free(mem);
		break;
	}
    }
    
	pthread_mutex_unlock(pool_lock);
}

/* Allocate memory from the pool and zero the memory */
void* zcj_pool_calloc_imp( const char *file, int line, 
				  zcj_pool_t *pool, unsigned cnt, 
				  unsigned elemsz)
{
    if(!pool)
	return NULL;	
    void *mem;

    mem = zcj_pool_alloc_imp(file, line, pool, cnt*elemsz);
    if (!mem)
	return NULL;

    memset(mem,0,cnt*elemsz);
    return mem;
}

/* Allocate memory from the pool and zero the memory */
void* zcj_pool_zalloc_imp( const char *file, int line, 
				  zcj_pool_t *pool, zcj_size_t sz)
{
     if(!pool)
	return NULL;	
    return zcj_pool_calloc_imp(file, line, pool, 1, sz); 
}



