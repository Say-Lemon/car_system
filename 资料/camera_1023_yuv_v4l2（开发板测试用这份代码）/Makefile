#定义变量保存gcc,arm-linux-gcc
CC=arm-linux-gcc

T=main

CONFIG=-I./jpeg -L./jpeg -ljpeg -lapi_v4l2_arm1 -lpthread

SRCS=$(wildcard *.c)
OBJS=$(patsubst %.c, %.o, $(SRCS))

$(T):$(OBJS)
	$(CC) -o $@ $^ -lpthread $(CONFIG)

%.o:%.c
	$(CC) -c $< -o $@ $(CONFIG)

clean:
	rm *.o $(TARGET)
