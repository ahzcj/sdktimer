include ../config.mk
CCA = $(HISI-PATH)/arm-hisiv100nptl-linux-gcc
AR = $(HISI-PATH)/arm-hisiv100nptl-linux-ar
CFLAGSA =  -Wall -W -Wstrict-prototypes -fno-common
OBJSA = cccsdk_timer.o mcu_timer.o zcj_pool.o test.o

lib: $(OBJSA)
		$(AR) rv libxyzwtimer.a $(OBJSA)

$(OBJSA): %.o : %.c
		$(CCA) \
		-c $(CFLAGSA) $< -o $@  

.PHONY : clean
clean :
		rm libxyzwtimer.a $(OBJSA)
