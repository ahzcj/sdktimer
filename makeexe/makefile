CCA = gcc
AR = ar
CFLAGSA =  -D_BUILD_NDK_  -Wall -W -Wstrict-prototypes -fno-common
OBJSA = cccsdk_timer.o mcu_timer.o zcj_pool.o test.o

lib: $(OBJSA)
		$(CCA) -o test $(OBJSA) -lrt -lpthread

$(OBJSA): %.o : %.c
		$(CCA) \
		-c $(CFLAGSA) $< -o $@  

.PHONY : clean
clean :
		rm test $(OBJSA)



		
