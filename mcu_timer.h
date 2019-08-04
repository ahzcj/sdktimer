#ifndef _MCU_TIMER_H_
#define _MCU_TIMER_H_

#include "cccsdk_timer.h"
#define MCU_MAX_TIMER_COUNT  1000
#define MCU_MAX_TIMED_OUT_ENTRIES 20

#define MCU_TIMER_SCHEDULE(timerstu,delay) \
		xyzw_timer_schedule(timerstu,delay,__FILE__, __LINE__)

typedef struct McuTimerStu
{
	ccc_timer_entry timer;
	int timerid;
    	void *userdata;
	ccc_timer_heap_callback *timer_cb;
}McuTimerStu;

int xyzw_timer_init();
void xyzw_timer_deinit();
void xyzw_timer_start(McuTimerStu *timerstu,int delay);
void xyzw_timer_stop(McuTimerStu *timerstu);
void xyzw_timer_schedule(McuTimerStu *timerstu, int second, const char *src_file,  int src_line);

#endif

