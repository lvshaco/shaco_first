.PHONY: all t clean cleanall res
 #-Wpointer-arith -Winline
CFLAGS=-g -Wall -Werror 
SHARED=-fPIC -shared

service_dir=service
#service_src=$(wildcard $(service_dir)/*.c)
#service_so=$(patsubst %.c,%.so,$(notdir $(service_src)))

lur_src=\
	lur/lur.c \
	lur/lur.h

net_src=\
	net/net.c \
	net/net.h \
	net/net_message.h \
	net/netbuf.c \
	net/netbuf.h

redis_src=\
	redis/redis.c \
	redis/redis.h

tplt_src=\
	tplt/tplt_internal.h \
	tplt/tplt_holder.c \
	tplt/tplt_holder.h \
	tplt/tplt.c \
	tplt/tplt.h \
	tplt/tplt_define.h

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
	host/host_reload.c \
	host/host_reload.h \
	host/host_dispatcher.c \
	host/host_dispatcher.h \
	host/host_node.c \
	host/host_node.h \
	host/host_gate.c \
	host/host_gate.h \
	host/host_assert.h \
 	host/dlmodule.c \
 	host/dlmodule.h

cli_src=\
	tool/shaco-cli.c

world_src=\
	world/player.c \
	world/player.h

test_src=\
	test/test.c \
	net.so \
	lur.so \
	base.so \
	redis.so

LDFLAGS=-Wl,-rpath,. \
		net.so lur.so base.so -llua -lm -ldl -lrt -rdynamic# -Wl,-E

service_so=\
	service_benchmark.so \
	service_echo.so \
	service_log.so \
	service_dispatcher.so \
	service_centerc.so \
	service_centers.so \
	service_node.so \
	service_cmds.so \
	service_cmdctl.so \
	service_gate.so \
	service_forward.so \
	service_game.so

worldservice_so=\
	service_world.so \
	service_gamematch.so

all: \
	lur.so \
	net.so \
	base.so \
	redis.so \
	tplt.so \
	shaco \
	shaco-cli \
	$(service_so) \
	world.so \
	$(worldservice_so) \
	service_worlddb.so \
	service_benchmarkdb.so \
	service_redisproxy.so

release: CFLAGS += -O2 -fno-strict-aliasing
release: all

$(service_so): %.so: $(service_dir)/%.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $< -Ihost -Inet -Ibase -Imessage

$(worldservice_so): %.so: $(service_dir)/%.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Ihost -Inet -Ibase -Imessage -Iworld -Itplt -Wl,-rpath,. world.so tplt.so

service_worlddb.so: $(service_dir)/service_worlddb.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Ihost -Inet -Ibase -Imessage -Iworld -Iredis -Wl,-rpath,. world.so redis.so

service_benchmarkdb.so: $(service_dir)/service_benchmarkdb.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Ihost -Inet -Ibase -Imessage -Iredis -Wl,-rpath,. redis.so

service_redisproxy.so: $(service_dir)/service_redisproxy.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Ihost -Inet -Ibase -Imessage -Iredis -Wl,-rpath,. redis.so

world.so: $(world_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iworld -Ibase -Imessage

lur.so: $(lur_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -llua

net.so: $(net_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^

redis.so: $(redis_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^

base.so: $(base_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^

tplt.so: $(tplt_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -DUSE_HOSTLOG -Ihost

shaco: $(host_src)
	@rm -f $@
	gcc $(CFLAGS) -o $@ $^ -Ihost -Ilur -Inet -Ibase  $(LDFLAGS)

shaco-cli: $(cli_src)
	@rm -f $@
	gcc $(CFLAGS) -o $@ $^ -lpthread

t: $(test_src)
	@rm -f $@
	gcc $(CFLAGS) -o $@ $^ -Ihost -Ilur -Inet -Ibase -Iredis $(LDFLAGS) redis.so

res:
	@rm -rf $(HOME)/.shaco/excel
	@mkdir -pv $(HOME)/.shaco/excel
	@rm -rf ./res/tbl
	@mkdir -pv ./res/tbl
	@rm -rf ./res/tplt
	@mkdir -pv ./res/tplt
	@rm -rf ./tplt/tplt_define.h
	@svn export $(SHACO_SVN_RES)/res/excel $(HOME)/.shaco/excel --force
	@cd tool && \
		python convert_excel.py \
		$(HOME)/.shaco/excel/excelmake_server.xml \
		$(HOME)/.shaco/excel tbl=../res/tbl:c=../res/tplt && \
		python concat.py ../res/tplt ../tplt/tplt_define.h && \
		rm -rf ../res/tplt

clean:
	rm -f shaco shaco-cli t *.so

cleanall: clean
	rm -rf cscope.* tags res
