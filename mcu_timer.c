#ifdef WIN32
#include "stdio.h"
#include <winsock2.h>
#include <stdlib.h>
#include <io.h>
#include <time.h>
#include <windows.h>
#include <process.h>
#include <errno.h>
#include <tchar.h>
#include "MyMutex.cpp"
#include <errno.h>
#define	THREAD_FUNCTION	 DWORD WINAPI
#define	THREAD_FUNCTION_RETURN DWORD
#define	THREAD_SPECIFIC_INDEX DWORD
#define	pthread_t	HANDLE
#define	pthread_attr_t DWORD
#define	pthread_create(thhandle,attr,thfunc,tharg) (int)((*thhandle=(HANDLE)_beginthreadex(NULL,0,(THREAD_FUNCTION)thfunc,tharg,0,NULL))==NULL)
#define	pthread_join(thread, result) ((WaitForSingleObject((thread),INFINITE)!=WAIT_OBJECT_0) || !CloseHandle(thread))
#define	pthread_detach(thread) if(thread!=NULL)CloseHandle(thread)
#define	thread_sleep(nms)	Sleep(nms)
#define	pthread_cancel(thread)	TerminateThread(thread,0)
#define	pthread_getspecific(ts_key) TlsGetValue(ts_key)
#define	pthread_setspecific(ts_key, value) TlsSetValue(ts_key, (void *)value)
#define	pthread_self() GetCurrentThreadId()
#define	pthread_exit(p) ExitThread(p)
#define 	pthread_kill(thread, SIGUSR1) TerminateThread(thread,0)
#ifndef pthread_mutex_t
#define pthread_mutex_t	 HANDLE
#endif
#define pthread_cond_t	 my_condition<pthread_mutex_t>
//#define pthread_cond_pt	 my_condition<pthread_mutex_t>*
#define pthread_mutex_lock(pobject)	 WaitForSingleObject(*pobject,INFINITE)
#define pthread_mutex_unlock(pobject)	 ReleaseMutex(*pobject)
#define pthread_mutex_init(pobject,pattr) (*pobject=CreateMutex(NULL,FALSE,NULL))
#define pthread_cond_init(pobject,pattr) (*pobject=CreateEvent(NULL,FALSE,FALSE,NULL))
#define pthread_mutex_destroy(pobject) CloseHandle(*pobject)
#define pthread_cond_destroy(pobject) CloseHandle(*pobject)
#define CV_TIMEOUT	 INFINITE	 /* Tunable value */

#define strcasecmplocal(s1, s2) stricmp(s1, s2)
#define strcmplocal(s1, s2) strcmp(s1, s2)
#define msrp_sleep(t) Sleep(1000*(t))
#define snprintf _snprintf
#define random() rand()
#define srandom(s) srand(s)
#define getpid() GetCurrentThreadId()

#define closelocal(s) do\
{\
	if(closesocketfd_callback)\
	{\
		closesocketfd_callback(s);\
	}\
	else\
	{\
		closesocket(s);\
	}\
}while(0)



#define typeof(t) msrp_##t*

#else

#define typeof(t) msrp_##t*
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h> 
//#include <sys/types.h> 
#include <fcntl.h> 
//#define _int64 size_t
#include <errno.h>
//#include <sys/syscall.h>   
//#define gettid() syscall(__NR_gettid)

#define msrp_sleep(t)  usleep(1000000*(t))
typedef void *HANDLE;
#define MAX_PATH     260
#define DWORD unsigned long
#define LPSOCKADDR struct sockaddr *
#define ULONG unsigned long

#define strcasecmplocal(s1, s2) strcasecmp(s1, s2)
#define strcmplocal(s1, s2) strcmp(s1, s2)

#define closelocal(s) do\
{\
	if(closesocketfd_callback)\
	{\
		closesocketfd_callback(s);\
	}\
	else\
	{\
		close(s);\
	}\
}while(0)

#endif
#include "mcu_timer.h"


static ccc_timer_heap_t	*mcu_timer_heap = NULL;
static  zcj_pool_t		*mcu_timer_pool = NULL;
static int berunning = 0;
//static pthread_cond_t mcu_timer_conn={0};
#ifdef WIN32
static pthread_mutex_t mcu_timer_lock=NULL;
#else
static pthread_mutex_t mcu_timer_lock={0};
#endif
static int  mcu_timerthread_exists = 0;
static int mcu_recv_pair[2]={0};
static fd_set mcu_recv_fds;
static int mcu_recv_fdmax=0;	

#define CCC_LOG_ERROR 1
static void mcu_log(const char *file, int line,int level,char *format, ...)
{	
	
	//printf("",format);
	
}
static int ccc_create_thread(void *function,void *data)
{	
	
	#ifndef WIN32
	pthread_t thread;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	#endif
	#ifdef WIN32
		DWORD threadid;
		if(CreateThread(NULL, 0,  (LPTHREAD_START_ROUTINE)function, data, 0, &threadid) == NULL)
		{
	
			//local_events(MSRP_ERROR, "create ccc_create_thread error");
			return -1;
		}
		
	#else
		if(pthread_create(&(thread), &attr, function, data) != 0)
		{
			//local_events(MSRP_ERROR, "create ccc_create_thread error");
			return -1;
		}
	#endif

	return 0;
}
#ifdef WIN32
static int socketpair(int family, int type, int protocol, int fd[2])
{
	/* This code is originally from Tor. */

	/* This socketpair does not work when localhost is down. So
	 * it's really not the same thing at all. But it's close enough
	 * for now, and really, when localhost is down sometimes, we
	 * have other problems too.
	 */
	int listener = -1;
	int connector = -1;
	int acceptor = -1;
	struct sockaddr_in listen_addr;
	struct sockaddr_in connect_addr;
	int size;
	int saved_errno = -1;
	  WSADATA WsaData;  
	 if (WSAStartup(MAKEWORD(2,2),&WsaData))  
    {  
        printf("The socket failed");  
        return -1;  
    } 
	if (protocol) {
		return -1;
	}
	if (!fd) {
		return -1;
	}

	listener = socket(AF_INET,SOCK_STREAM,0);  
	if (listener < 0)
	{
		printf("%d",errno);
		return -1;
	}
	memset(&listen_addr, 0, sizeof(listen_addr));
	listen_addr.sin_family = AF_INET;
	listen_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	listen_addr.sin_port = 0;	/* kernel chooses port.	 */
	if (bind(listener, (struct sockaddr *) &listen_addr, sizeof (listen_addr))
		== -1)
		goto tidy_up_and_fail;
	if (listen(listener, 1) == -1)
		goto tidy_up_and_fail;

	connector = socket(AF_INET, type, 0);
	if (connector < 0)
		goto tidy_up_and_fail;
	/* We want to find out the port number to connect to.  */
	size = sizeof(connect_addr);
	if (getsockname(listener, (struct sockaddr *) &connect_addr, &size) == -1)
		goto tidy_up_and_fail;
	if (size != sizeof (connect_addr))
		goto abort_tidy_up_and_fail;
	if (connect(connector, (struct sockaddr *) &connect_addr,
				sizeof(connect_addr)) == -1)
		goto tidy_up_and_fail;

	size = sizeof(listen_addr);
	acceptor = accept(listener, (struct sockaddr *) &listen_addr, &size);
	if (acceptor < 0)
		goto tidy_up_and_fail;
	if (size != sizeof(listen_addr))
		goto abort_tidy_up_and_fail;
	closesocket(listener);
	/* Now check we are talking to ourself by matching port and host on the
	   two sockets.	 */
	if (getsockname(connector, (struct sockaddr *) &connect_addr, &size) == -1)
		goto tidy_up_and_fail;
	if (size != sizeof (connect_addr)
		|| listen_addr.sin_family != connect_addr.sin_family
		|| listen_addr.sin_addr.s_addr != connect_addr.sin_addr.s_addr
		|| listen_addr.sin_port != connect_addr.sin_port)
		goto abort_tidy_up_and_fail;
	fd[0] = connector;
	fd[1] = acceptor;
	//localsockfd = acceptor;
	return 0;

 abort_tidy_up_and_fail:
	saved_errno = -1;
 tidy_up_and_fail:
	saved_errno = -2;
	if (listener != -1)
		closesocket(listener);
	if (connector != -1)
		closesocket(connector);
	if (acceptor != -1)
		closesocket(acceptor);

	return saved_errno;
}
#endif
	
static void *mcu_timer_thread(void *data)
{
     ccc_time_val timeout = { 0, 0};
    int c=0,ret = 0;
	char tempbuf[10];
    /* Poll the timer. The timer heap has its own mutex for better 
     * granularity, so we don't need to lock end endpoint. 
     */
    
	//   printf("-------------mcu_timer_thread-------");
     mcu_timerthread_exists  =1;
    while(berunning)
   {
   	    pthread_mutex_lock(&mcu_timer_lock);
	    timeout.sec = timeout.msec = 0;
	    if(!mcu_timer_heap)
	    {
		   pthread_mutex_unlock(&mcu_timer_lock);
		   break;
	    }
	    c = ccc_timer_heap_poll(mcu_timer_heap, &timeout);
	    if ((timeout.sec >0 && timeout.sec < CCC_MAXINT32) 
			|| (timeout.msec >0 && timeout.msec < CCC_MAXINT32))
	    {
	    	///  usleep(1000*(timeout.sec*1000+timeout.msec));
	    	struct timeval val;
		memset(&val,0,sizeof(val));
		val.tv_sec = timeout.sec;
		val.tv_usec = timeout.msec*1000;
		
		FD_SET(mcu_recv_pair[0], &mcu_recv_fds);
		ret = select(mcu_recv_pair[0]+1, &mcu_recv_fds, NULL, NULL, &val);
	    }
	    else if(berunning)
	    {
			//pthread_cond_wait(&mcu_timer_conn, &mcu_timer_lock);
			
			FD_SET(mcu_recv_pair[0], &mcu_recv_fds);
			ret = select(mcu_recv_pair[0]+1, &mcu_recv_fds, NULL, NULL, (struct timeval *)NULL);
			
	    }
	     if(ret > 0)
	     {
			
			recv(mcu_recv_pair[0], (char *)&tempbuf, 10, 0);
			//printf("------------mcu timerthread buf:%s\r\n",tempbuf);
	     }
	    pthread_mutex_unlock(&mcu_timer_lock);
    }
    mcu_timerthread_exists = 0;
    return NULL;

}

int xyzw_timer_init()
{
	if(berunning)
	{
		mcu_log(__FILE__, __LINE__,CCC_LOG_ERROR,"mcu_timer_init timer error,mcu_timer is already running");
		return 0;
	}
	int status;
	int tmpres =1,tmpres2 = 1;
	mcu_timer_pool = zcj_pool_create("mcu_timer", 256, 256);
	/* Create timer heap to manage all timers . */
    status = ccc_timer_heap_create( mcu_timer_pool, MCU_MAX_TIMER_COUNT, 
                                   &mcu_timer_heap);
    if (status != CCC_SUCCESS) {
	goto on_error;
    }
   
    pthread_mutex_t *templock = (pthread_mutex_t *)zcj_pool_calloc(mcu_timer_pool, 1, sizeof(pthread_mutex_t));
#ifdef WIN32
	pthread_mutex_init(templock, NULL);
	if(*templock == NULL)
	{
		mcu_log(__FILE__, __LINE__,CCC_LOG_ERROR,"Error pthread_mutex_init  mcu_timer_init ...\n");
		goto on_error;
	}
#else
	 pthread_mutexattr_t attr;  
	 pthread_mutexattr_init(&attr);  
	 pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);//设置锁的属性为可递归
	tmpres2 = pthread_mutex_init(templock, NULL);
	if(tmpres2 != 0)
	{
		mcu_log(__FILE__, __LINE__,CCC_LOG_ERROR,"Error pthread_mutex_init  mcu_timer_init2...\n");
		pthread_mutexattr_destroy(&attr);  
		goto on_error;
	}
	 pthread_mutexattr_destroy(&attr);  
#endif
   
   
    ccc_timer_heap_set_lock(mcu_timer_heap, templock, CCC_TRUE);

    /* Set maximum timed out entries to process in a single poll. */
    ccc_timer_heap_set_max_timed_out_per_poll(mcu_timer_heap, MCU_MAX_TIMED_OUT_ENTRIES);

  //  pthread_cond_init(&mcu_timer_conn, NULL);   
    
#ifdef WIN32
	pthread_mutex_init(&mcu_timer_lock, NULL);
	if(mcu_timer_lock == NULL)
	{
		mcu_log(__FILE__, __LINE__,CCC_LOG_ERROR,"Error pthread_mutex_init  mcu_timer_lock...\n");
		 goto on_error;
	}
#else
	tmpres = pthread_mutex_init(&mcu_timer_lock, NULL);
	if(tmpres != 0)
	{
		mcu_log(__FILE__, __LINE__,CCC_LOG_ERROR,"Error pthread_mutex_init  mcu_timer_lock...\n");
		 goto on_error;
	}
#endif

    if(socketpair(AF_UNIX, SOCK_STREAM, 0, mcu_recv_pair) < 0)
    {
    	   goto on_error;
    }
    FD_SET(mcu_recv_pair[0], &mcu_recv_fds);
    if(mcu_recv_pair[0] > mcu_recv_fdmax)
	 mcu_recv_fdmax = mcu_recv_pair[0];
    berunning = 1;
	
    if(ccc_create_thread(mcu_timer_thread, NULL)  !=  0)
    {
	  berunning = 0;
	  goto on_error;
    }
    return 0;
	
on_error:
    if (mcu_timer_heap) {
	ccc_timer_heap_destroy(mcu_timer_heap);
	mcu_timer_heap = NULL;
    }
#ifdef WIN32
	pthread_mutex_destroy(&mcu_timer_lock);
       pthread_mutex_destroy(&templock);
#else
     if(tmpres == 0)pthread_mutex_destroy(&mcu_timer_lock);
     if(tmpres2 == 0)pthread_mutex_destroy(&templock);
#endif
     if(mcu_recv_pair[0])close(mcu_recv_pair[0]);
     if(mcu_recv_pair[1])close(mcu_recv_pair[1]);
     //pthread_cond_destroy(&mcu_timer_conn);
     return -1;
}



void xyzw_timer_deinit()
{
	berunning = 0;
	char ch[5] = "test";
	int err = send(mcu_recv_pair[1], ch, 4, 0);
	if(err < 1)
		mcu_log(__FILE__, __LINE__,CCC_LOG_ERROR,"Error recv add unblocking select errno:%d",errno);
	int i=0;
	while(mcu_timerthread_exists && i < 20)
	{
		msrp_sleep(0.1);
		i++;
	}
	pthread_mutex_lock(&mcu_timer_lock);
	if (mcu_timer_heap) 
	{
		ccc_timer_heap_destroy(mcu_timer_heap);
		mcu_timer_heap = NULL;
   	}
	// pthread_cond_signal(&mcu_timer_conn);
	 pthread_mutex_unlock(&mcu_timer_lock);

	
	pthread_mutex_destroy(&mcu_timer_lock);
	FD_ZERO(&mcu_recv_fds);
	//pthread_cond_destroy(&mcu_timer_conn);
	zcj_pool_release(mcu_timer_pool);
	mcu_timer_pool = NULL;
#ifdef WIN32
	if(mcu_recv_pair[0])closesocket(mcu_recv_pair[0]);
	if(mcu_recv_pair[1])closesocket(mcu_recv_pair[1]);
#else
	if(mcu_recv_pair[0])close(mcu_recv_pair[0]);
	if(mcu_recv_pair[1])close(mcu_recv_pair[1]);
#endif
	
}

void xyzw_timer_stop(McuTimerStu *timerstu)
{
	if(!berunning)
	{
		mcu_log(__FILE__, __LINE__,CCC_LOG_ERROR,"mcu_timer_stop timer error,mcu_timer is not running");
		return;
	}
	if(!timerstu)
		return;
	// pthread_mutex_lock(&mcu_timer_lock);
	if (timerstu->timer._timer_id > 0) 
	 {
		ccc_timer_heap_cancel( mcu_timer_heap, &timerstu->timer);
		//timerstu->timer.id = 0;
   	 }
	  //pthread_mutex_unlock(&mcu_timer_lock);
	
	  
}
void xyzw_timer_start(McuTimerStu *timerstu,int delay)
{
	
	if(!berunning)
	{
		mcu_log(__FILE__, __LINE__,CCC_LOG_ERROR,"mcu_timer_start timer error,mcu_timer is not running");
		return;
	}
	if(!timerstu)
		return;
	xyzw_timer_stop(timerstu);
	ccc_timer_entry_init(&timerstu->timer,
			timerstu->timerid,		    /* id */
			timerstu->userdata,		    /* user data */
			timerstu->timer_cb);	    /* callback */

	MCU_TIMER_SCHEDULE(timerstu,delay);
	
}


		
void xyzw_timer_schedule(McuTimerStu *timerstu, int second, const char *src_file,  int src_line)
{
	if(!berunning)
	{
		mcu_log(src_file, src_line,CCC_LOG_ERROR,"mcu_timer_schedule timer error,mcu_timer is not running");
		return;
	}
	ccc_time_val delay;
	memset(&delay,0,sizeof(delay));
	if(second < 1000)
		delay.msec = second;
	else
	{
		delay.sec = second/1000;
		delay.msec = second%1000;
	}
	char ch[5] = "test";
	
	// pthread_mutex_lock(&mcu_timer_lock);
	if(!mcu_timer_heap)
	{
		mcu_log(src_file, src_line,CCC_LOG_ERROR,"mcu_timer_schedule timer error,mcu_timer_heap is null");
		// pthread_mutex_unlock(&mcu_timer_lock);
		return;
	}
	int status = ccc_timer_heap_schedule(mcu_timer_heap,&timerstu->timer,&delay);
	if(status != CCC_SUCCESS)
	{
		mcu_log(src_file, src_line,CCC_LOG_ERROR,"pj_timer_heap_schedule timer error,status:%d",status);
		// pthread_mutex_unlock(&mcu_timer_lock);
		return;
	}
	int err = send(mcu_recv_pair[1], ch, 4, 0);
	//printf("-------------mcu start err:%d\r\n",err);
	if(err < 1)
		mcu_log(__FILE__, __LINE__,CCC_LOG_ERROR,"Error recv add unblocking select errno:%d",errno);
	 // pthread_cond_signal(&mcu_timer_conn);
	// pthread_mutex_unlock(&mcu_timer_lock);
}
