# Hisilicon Hi35xx sample Makefile

include ../Makefile.param

HI_SUPPORT_MP3DEC := YES

SMP_SRCS := $(wildcard *.c)

SMP_SRCS += $(wildcard $(PWD)/hxt/*.c)
SMP_SRCS += $(wildcard $(PWD)/iflyos/*.c)
SMP_SRCS += $(wildcard $(PWD)/board/*.c)
SMP_SRCS += $(wildcard $(PWD)/utils/*.c)

ROOT_DIR = $(shell pwd)
SUBDIRS  = $(shell ls -l | grep ^d | awk '{if($$9 != "output") print $$9}')

INCS = -I$(ROOT_DIR)/. \
       $(foreach tmp,$(SUBDIRS),-I$(ROOT_DIR)/$(tmp)/.) \
       -I/mnt/g/workspace/plugins/libev/include \
       -I/mnt/g/workspace/plugins/cjson/include    \
       -I/mnt/g/workspace/plugins/libuwsc/include \
       -I/mnt/g/workspace/plugins/libmbedtls/include \
       -I/mnt/g/workspace/plugins/libqrcode/include 

LIBS = -L/mnt/g/workspace/plugins/libev/lib -lev \
             -L/mnt/g/workspace/plugins/cjson/lib -lcjson \
             -L/mnt/g/workspace/plugins/libuwsc/lib -luwsc \
             -L/mnt/g/workspace/plugins/libmbedtls/lib -lmbedx509 \
             -L/mnt/g/workspace/plugins/libmbedtls/lib -lmbedtls \
             -L/mnt/g/workspace/plugins/libmbedtls/lib -lmbedcrypto \
             -L/mnt/g/workspace/plugins/libqrcode/lib -lqrcode \
             -L/mnt/g/workspace/plugins/libqrcode/lib -lzbar

#TARGET := $(SMP_SRCS:%.c=%)

CFLAGS += $(INCS) 
CFLAGS += -L$(REL_LIB) -lsecurec -Wl,-rpath=/usr/lib


TARGET := HxtDeskService 

SMP_SRCS += $(wildcard $(PWD)/../audio/adp/audio_aac_adp.c)
SMP_SRCS += $(wildcard $(PWD)/../audio/adp/audio_dl_adp.c)

# if need mp3dec
ifeq ($(HI_SUPPORT_MP3DEC), YES)
	MP3_DEC_LIB_DIR := $(PWD)/../audio/mp3dec/lib
	CFLAGS += -D__Mp3decSupport__ -L$(MP3_DEC_LIB_DIR) -lmp3dec
	SMP_SRCS += $(wildcard $(PWD)/../audio/adp/audio_mp3_adp.c)
endif

TARGET_PATH := $(PWD)

# compile linux or HuaweiLite
#include $(PWD)/../$(ARM_ARCH)_$(OSTYPE).mak


# target source
OBJS  := $(SMP_SRCS:%.c=%.o)

CFLAGS += $(COMM_INC)

MPI_LIBS += $(REL_LIB)/libhdmi.a

.PHONY : clean all

all: $(TARGET)

$(TARGET):$(COMM_OBJ) $(OBJS)
	@$(CC) $(CFLAGS) -DEBUG -lpthread -lm -o $(TARGET_PATH)/$@ $^ -Wl,--start-group $(MPI_LIBS) $(SENSOR_LIBS) $(AUDIO_LIBA) $(REL_LIB)/libsecurec.a $(REL_LIB)/libsns_imx335.a -Wl,--end-group $(LIBS)

clean:
	@rm -f $(TARGET_PATH)/$(TARGET)
	@rm -f $(OBJS)


