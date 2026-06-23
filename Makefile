#
# Makefile for Smart Car Infotainment System (智能车机项目)
# Based on player_project build system
#

CC = arm-linux-gcc
LVGL_DIR_NAME ?= lvgl
LVGL_DIR ?= ${shell pwd}
CFLAGS ?= -O3 -g0 -I$(LVGL_DIR)/ -I$(LVGL_DIR)/usrCode -I$(LVGL_DIR)/usrCode/cJSON -Wall -std=gnu99
LDFLAGS ?= -lm -lpthread
BIN = car_system

# Collect all .c files under usrCode/
USRCODESRC = ${shell find $(LVGL_DIR)/usrCode -name '*.c'}

# Include LVGL and lv_drivers build rules
include $(LVGL_DIR)/lvgl/lvgl.mk
include $(LVGL_DIR)/lv_drivers/lv_drivers.mk

# Extra source files
CSRCS += $(LVGL_DIR)/mouse_cursor_icon.c

OBJEXT ?= .o

AOBJS = $(ASRCS:.S=$(OBJEXT))
COBJS = $(CSRCS:.c=$(OBJEXT))
USRCODEOBJ = $(USRCODESRC:.c=$(OBJEXT))

all: default

%.o: %.c
	@$(CC) $(CFLAGS) -c $< -o $@
	@echo "CC $<"

default: $(AOBJS) $(COBJS) $(USRCODEOBJ)
	$(CC) -o $(BIN) $(AOBJS) $(COBJS) $(USRCODEOBJ) $(LDFLAGS)

clean:
	rm -f $(BIN) $(AOBJS) $(COBJS) $(USRCODEOBJ)

send:
	scp -O $(BIN) root@192.168.137.226:~/yjr/car_system

# Debug: show collected user source files
show:
	@echo "USRCODESRC = $(USRCODESRC)"
