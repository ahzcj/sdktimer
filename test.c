#include "mcu_timer.h"
#include <stdio.h>
#ifdef WIN32
#include <windows.h>
#define ccc_sleep(t) Sleep(t);
#else
#define ccc_sleep(t) usleep(t*1000);
#endif
McuTimerStu timer={0};
McuTimerStu timer2={0};
void ccc_timer_callback(ccc_timer_heap_t *timer_heap, struct ccc_timer_entry *entry)
{
	printf("data :%p",entry->user_data);
	printf("entry :%d\r\n",entry->id);
	if(entry->id == 3)
		MCU_TIMER_SCHEDULE(&timer,2*1000);
	else
		MCU_TIMER_SCHEDULE(&timer2,3*1000);
}
int main()
{
	int res = xyzw_timer_init();
	printf(" mcu_timer_init :%d",res);
	
	timer.timer_cb = ccc_timer_callback;
	timer.timerid = 3;
	timer2.timer_cb = ccc_timer_callback;
	timer2.timerid = 4;
	xyzw_timer_start(&timer,1000);
	ccc_sleep(8*1000);
	xyzw_timer_stop(&timer);
	xyzw_timer_start(&timer2,5000);
	
	//xyzw_timer_start(&timer2,5000);
   //	ccc_sleep(4);
	//mcu_timer_stop(&timer);
	//mcu_timer_stop(&timer2);
	ccc_sleep(100*1000);
	return 0;
}