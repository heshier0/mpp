CC              = arm-himix200-linux-gcc
CPP             = arm-himix200-linux-g++
AR              = arm-himix200-linux-ar
STRIP           = arm-himix200-linux-strip

SUBDIRS         = $(shell ls -l | grep ^d | awk '{if($$9 != "output") print $$9}')
ROOT_DIR        = $(shell pwd)
BIN             = AudioProcess
OBJS_DIR        = output/obj
BIN_DIR         = output/bin
CUR_SOURCE      = ${wildcard *.c}
CUR_OBJS        = ${patsubst %.c, %.o, $(CUR_SOURCE)}

INCS            = -I$(ROOT_DIR)/. \
		  $(foreach tmp,$(SUBDIRS),-I$(ROOT_DIR)/$(tmp)/.) \
                  -I/opt/hisi-linux/x86-arm/arm-himix200-linux/target/usr/include \
                  -I/mnt/g/workspace/plugins/libev/include \
                  -I/mnt/g/workspace/plugins/cjson/include    \
                  -I/mnt/g/workspace/plugins/libuwsc/include \
		          -I/mnt/g/workspace/plugins/libmbedtls/include \
                  -I/mnt/g/workspace/plugins/libqrcode/include \
                  -I/mnt/g/workspace/plugins/hisi/include
                  

LIBS            = -L/opt/hisi-linux/x86-arm/arm-himix200-linux/target/usr/lib -lm -ldl -pthread \
                  -L/mnt/g/workspace/plugins/libev/lib -lev \
                  -L/mnt/g/workspace/plugins/cjson/lib -lcjson \
                  -L/mnt/g/workspace/plugins/libuwsc/lib -luwsc \
                  -L/mnt/g/workspace/plugins/libmbedtls/lib -lmbedx509 \
                  -L/mnt/g/workspace/plugins/libmbedtls/lib -lmbedtls \
                  -L/mnt/g/workspace/plugins/libmbedtls/lib -lmbedcrypto \
                  -L/mnt/g/workspace/plugins/libqrcode/lib -lQRCodeDetec \
                  -L/mnt/g/workspace/plugins/libqrcode/lib -lzbar \
                  -L/mnt/g/workspace/plugins/hisi/lib 
		    

CFLAGS          = -DLINUX -DDEBUG -g -Wno-multichar -Wimplicit-function-declaration \
                  -fPIC -O2 -fexpensive-optimizations -frename-registers -fomit-frame-pointer 

CPPFLAGS        = -DLINUX -DEBUG -g -Wno-multichar -Wimplicit-function-declaration \
                  -fPIC -std=c++11 -fexceptions -fpermissive -w -O2 -fexpensive-optimizations -frename-registers -fomit-frame-pointer                              

export CC BIN OBJS_DIR BIN_DIR ROOT_DIR CFLAGS CPPFLAGS INCS LIBS

all:$(SUBDIRS) $(CUR_OBJS) DEBUG

$(SUBDIRS):ECHO
	make -C $@
DEBUG:ECHO
	make -C output
ECHO:
	@echo $(SUBDIRS)
$(CUR_OBJS):%.o:%.c
	$(CC) -c $^ -o $(ROOT_DIR)/$(OBJS_DIR)/$@ $(CFLAGS) $(INCS) 
clean:
	@rm $(OBJS_DIR)/*.o
	@rm -rf $(BIN_DIR)/*
