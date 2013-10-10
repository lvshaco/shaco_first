.PHONY: all t clean cleanall
 #-Wpointer-arith -Winline
CFLAGS=-g -Wall -Werror

SHARED=-shared
plat=linux
ifeq ($(plat),linux)
	SHARED += -fPIC
endif
net_src=\
	net.c \
	net.h \
	net_message.h \
	netbuf.c \
	netbuf.h

LDFLAGS=-Wl,-rpath,. net.so

all: net.so test
release: CFLAGS += -O2
release: all

net.so: $(net_src)
	@rm -f $@
ifeq ($(plat),linux)
	gcc $(CFLAGS) $(SHARED) -o $@ $^ #-lws2_32
else
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -lws2_32
endif

test: test.c
	@rm -f $@
ifeq ($(plat),linux)
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $^ -Inet #-lws2_32
else
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $^ -Inet -lws2_32
endif

clean:
	rm -f test net.so 

install:
	mv test.exe e:/net/net/test.exe && mv net.so e:/net/net/net.dll
