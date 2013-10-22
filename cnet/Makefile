.PHONY: all t clean cleanall
CFLAGS=-g -Wall -Werror

SHARED=-shared
plat=linux
ifeq ($(plat),linux)
	SHARED += -fPIC
endif

net_dir=../net
net_src=\
	$(net_dir)/net.c \
	$(net_dir)/net.h \
	$(net_dir)/net_message.h \
	$(net_dir)/netbuf.c \
	$(net_dir)/netbuf.h \

cnet_src=\
	cnet.c \
	cnet.h

inc_dir=-I../net \
		-I../message

LDFLAGS=-Wl,-rpath,. cnet.so

all: cnet.so test
release: CFLAGS += -O2
release: all

cnet.so: $(net_src) $(cnet_src)
	@rm -f $@
ifeq ($(plat),linux)
	gcc $(CFLAGS) $(SHARED) -o $@ $^ $(inc_dir) #-lws2_32
else
	gcc $(CFLAGS) $(SHARED) -o $@ $^ $(inc_dir) -lws2_32 -lws2_32 -Wl,--output-def,cnet.def,--out-implib,cnet.lib
	LIB /MACHINE:IX86 /DEF:cnet.def
endif

test: test.c
	@rm -f $@
ifeq ($(plat),linux)
	gcc $(CFLAGS) -o $@ $^ $(inc_dir) $(LDFLAGS)
else
	gcc $(CFLAGS) -o $@ $^ $(inc_dir) $(LDFLAGS)
endif

clean:
	rm -f test *.exe *.so *.a *.def *.lib *.exp

install_dir=D:/wa-client/trunk/cocos2d-x-2.1.4/Debug.win32
source_dir=D:/wa-client/trunk/cocos2d-x-2.1.4/driller/Classes
install:
	cp test.exe $(install_dir)/test.exe
	cp -r ../net $(source_dir)	
	cp -r ../cnet $(source_dir)
	mv $(source_dir)/cnet/cnet.so $(source_dir)/cnet/cnet.dll
	cp -r ../message $(source_dir)
