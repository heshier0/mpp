SUBDIRS=$(shell ls -l | grep ^d | awk '{print $$9}')
CUR_SOURCE=${wildcard *.c}
CUR_OBJS=${patsubst %.c, %.o, $(CUR_SOURCE)}
INCS =  -I../utils/ \
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
             -L/mnt/g/workspace/plugins/libmbedtls/lib -lmbedcrypto
CFLAGS += $(INCS) $(LIBS) 
all:$(SUBDIRS) $(CUR_OBJS)
$(SUBDIRS):ECHO
	make -C $@
$(CUR_OBJS):%.o:%.c
	$(CC) -c $^ -o $@ $(CFLAGS)
ECHO:
	@echo $(SUBDIRS)
