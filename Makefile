.PHONY: all t clean cleanall

CFLAGS=-g -Wall -Werror
SHARED=-fPIC -shared

service_dir=service
service_src=$(wildcard $(service_dir)/*.c)
service_so=$(patsubst %.c,%.so,$(notdir $(service_src)))

lur_src=\
	lur/lur.c \
	lur/lur.h

net_src=\
	net/net.c \
	net/net.h \
	net/net_event.h \
	net/netbuf.c \
	net/netbuf.h

host_src=\
	host/host_main.c \
 	host/host.c \
 	host/host.h \
 	host/host_net.c \
 	host/host_net.h \
 	host/host_service.c \
 	host/host_service.h \
 	host/host_timer.c \
 	host/host_timer.h \
 	host/host_log.c \
 	host/host_log.h \
 	host/dlmodule.c \
 	host/dlmodule.h
		
#LDFLAGS=-Wl,-rpath,. \
		#-L. net.so lur.so\
	    #-llua -lm -ldl -lrt
LDFLAGS=net.so lur.so -llua -lm -ldl -lrt -rdynamic -Wl,-E

all: lur.so net.so shaco $(service_so)

$(service_so): %.so: $(service_dir)/%.c
	gcc $(CFLAGS) $(SHARED) -o $@ $< -Ihost -Inet -Ibase -Imessage

lur.so: $(lur_src)
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -llua

net.so: $(net_src)
	gcc $(CFLAGS) $(SHARED) -o $@ $^

shaco: $(host_src)
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $^ -Ihost -Ilur -Inet -Ibase 

t: test/test.c net.so lur.so
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $^ -Ihost -Ilur -Inet -Ibase 

clean:
	rm -f shaco t *.so

cleanall: clean
	rm -f cscope.* tags
