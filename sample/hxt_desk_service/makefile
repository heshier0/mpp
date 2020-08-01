# Hisilicon Hi35xx sample Makefile

include ../Makefile.param

HI_SUPPORT_MP3DEC := YES

SMP_SRCS := $(wildcard *.c)

SMP_SRCS += $(wildcard $(PWD)/hxt/*.c)
SMP_SRCS += $(wildcard $(PWD)/iflyos/*.c)
SMP_SRCS += $(wildcard $(PWD)/board/*.c)
SMP_SRCS += $(wildcard $(PWD)/utils/*.c)
SMP_SRCS += $(wildcard $(PWD)/media/*.c)
SMP_SRCS += $(wildcard $(PWD)/posture/*.c)

ROOT_DIR = $(shell pwd)
SUBDIRS  = $(shell ls -l | grep ^d | awk '{if($$9 != "output") print $$9}')


# OPENCV_LIBS := -lopencv_core -lopencv_calib3d -lopencv_dnn -lopencv_features2d \
#                -lopencv_flann -lopencv_highgui -lopencv_imgcodecs \
#                -lopencv_imgproc -lopencv_ml -lopencv_objdetect
       # -L/mnt/g/workspace/plugins/opencv4/lib $(OPENCV_LIBS) 




INCS = -I$(ROOT_DIR)/. \
       $(foreach tmp,$(SUBDIRS),-I$(ROOT_DIR)/$(tmp)/.) \
       -I/mnt/g/workspace/plugins/libev/include \
       -I/mnt/g/workspace/plugins/cjson/include    \
       -I/mnt/g/workspace/plugins/libuwsc/include \
       -I/mnt/g/workspace/plugins/libmbedtls/include \
       -I/mnt/g/workspace/plugins/libqrcode/include \
       -I/mnt/g/workspace/plugins/ffmpeg/include \
       -I/mnt/g/workspace/plugins/libgpiod/include \
       -I/mnt/g/workspace/plugins/opencv4/include/opencv4 \
       -I/mnt/g/workspace/plugins/libsitposture/include 
 
LIBS = -L/mnt/g/workspace/plugins/libev/lib -lev \
       -L/mnt/g/workspace/plugins/cjson/lib -lcjson \
       -L/mnt/g/workspace/plugins/libuwsc/lib -luwsc \
       -L/mnt/g/workspace/plugins/libmbedtls/lib -lmbedx509 \
       -L/mnt/g/workspace/plugins/libmbedtls/lib -lmbedtls \
       -L/mnt/g/workspace/plugins/libmbedtls/lib -lmbedcrypto \
       -L/mnt/g/workspace/plugins/libqrcode/lib -lqrcode \
       -L/mnt/g/workspace/plugins/libqrcode/lib -lzbar \
       -L/mnt/g/workspace/plugins/ffmpeg/lib -lavformat -lavcodec -lavutil -lswresample -lswscale \
       -L/mnt/g/workspace/plugins/x264/lib -lx264 \
       -L/mnt/g/workspace/plugins/libgpiod/lib -lgpiod \
       -L/mnt/g/workspace/plugins/libsitposture/lib -lsitposture \
       -L/mnt/g/workspace/plugins/libsitPostrecog/lib -lsitPostrecog 

LIBS +=  -L$(REL_LIB) -lsecurec -lnnie
LIBS += -L/mnt/g/workspace/plugins/opencv4/lib -lopencv_core -lopencv_calib3d -lopencv_dnn -lopencv_features2d -lopencv_flann -lopencv_highgui -lopencv_imgcodecs -lopencv_imgproc -lopencv_ml -lopencv_objdetect


CFLAGS += $(INCS) 


TARGET := HxtDeskService

SMP_SRCS += $(wildcard $(PWD)/../audio/adp/audio_aac_adp.c)
SMP_SRCS += $(wildcard $(PWD)/../audio/adp/audio_dl_adp.c)

# if need mp3dec
ifeq ($(HI_SUPPORT_MP3DEC), YES)
	MP3_DEC_LIB_DIR := $(PWD)/../audio/mp3dec/lib
	CFLAGS += -D__Mp3decSupport__ 
       LIBS += -L$(MP3_DEC_LIB_DIR) -lmp3dec
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
	@$(CC) $(CFLAGS) -DEBUG -o $(TARGET_PATH)/$@ $^ -Wl,--start-group $(MPI_LIBS) $(SENSOR_LIBS) $(AUDIO_LIBA) $(REL_LIB)/libsecurec.a $(REL_LIB)/libsns_imx335.a -Wl,--end-group $(LIBS)

clean:
	@rm -f $(TARGET_PATH)/$(TARGET)
	@rm -f $(OBJS)

