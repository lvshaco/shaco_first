.PHONY: all t clean cleanall
 #-Wpointer-arith -Winline
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
	net/net_message.h \
	net/netbuf.c \
	net/netbuf.h

base_src=\
	base/array.h \
	base/freeid.h \
	base/hashid.h \
	base/stringsplice.h \
	base/stringtable.h \
	base/util.h \
	base/args.c \
	base/args.h

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
	host/host_dispatcher.c \
	host/host_dispatcher.h \
	host/host_node.c \
	host/host_node.h \
	host/host_reload.c \
	host/host_reload.h \
 	host/dlmodule.c \
 	host/dlmodule.h

cli_src=\
	tool/shaco-cli.c

test_src=\
	test/test.c \
	net.so \
	lur.so \
	base.so

#LDFLAGS=-Wl,-rpath,. \
		#-L. net.so lur.so\
	    #-llua -lm -ldl -lrt
LDFLAGS=-Wl,-rpath,. \
		net.so lur.so base.so -llua -lm -ldl -lrt -rdynamic# -Wl,-E

all: lur.so net.so base.so shaco shaco-cli $(service_so)
release: CFLAGS += -O2
release: all

$(service_so): %.so: $(service_dir)/%.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $< -Ihost -Inet -Ibase -Imessage

lur.so: $(lur_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -llua

net.so: $(net_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^

base.so: $(base_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^

shaco: $(host_src)
	@rm -f $@
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $^ -Ihost -Ilur -Inet -Ibase 

shaco-cli: $(cli_src)
	@rm -f $@
	gcc $(CFLAGS) -o $@ $^ -lpthread

t: $(test_src)
	@rm -f $@
	gcc $(CFLAGS) $(LDFLAGS) -o $@ $^ -Ihost -Ilur -Inet -Ibase 

clean:
	rm -f shaco shaco-cli t *.so

cleanall: clean
	rm -f cscope.* tags
