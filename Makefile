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
	tplt/tplt_visitor.c \
	tplt/tplt_visitor.h \
	tplt/tplt_visitor_ops.h \
	tplt/tplt_visitor_ops_implement.c \
	tplt/tplt_visitor_ops_implement.h \
	tplt/tplt.c \
	tplt/tplt.h

base_src=\
	base/mpool.c \
	base/mpool.h \
	base/map.c \
	base/map.h \
	base/hmap.c \
	base/hmap.h \
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
	service_game.so \
	service_load.so

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
	service_playerdb.so \
	service_benchmarkdb.so \
	service_redisproxy.so \
	service_login.so

release: CFLAGS += -O2 -fno-strict-aliasing
release: all t

$(service_so): %.so: $(service_dir)/%.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $< -Ihost -Inet -Ibase -Imessage

$(worldservice_so): %.so: $(service_dir)/%.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Ihost -Inet -Ibase -Imessage -Iworld -Itplt -Idatadefine -Wl,-rpath,. world.so tplt.so

service_playerdb.so: $(service_dir)/service_playerdb.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Ihost -Inet -Ibase -Imessage -Iworld -Iredis -Wl,-rpath,. world.so redis.so

service_benchmarkdb.so: $(service_dir)/service_benchmarkdb.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Ihost -Inet -Ibase -Imessage -Iredis -Wl,-rpath,. redis.so

service_redisproxy.so: $(service_dir)/service_redisproxy.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Ihost -Inet -Ibase -Imessage -Iredis -Wl,-rpath,. redis.so

service_login.so: $(service_dir)/service_login.c
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
	gcc $(CFLAGS) -o $@ $^ -Ihost -Ilur -Inet -Ibase  $(LDFLAGS)

shaco-cli: $(cli_src)
	gcc $(CFLAGS) -o $@ $^ -lpthread

t: test/test.c net.so lur.so base.so redis.so
	gcc $(CFLAGS) -o $@ $^ -Ihost -Ilur -Inet -Ibase -Iredis $(LDFLAGS) redis.so

robot: test/robot.c cnet/cnet.c cnet/cnet.h net.so
	gcc $(CFLAGS) -o $@ $^ -Ilur -Icnet -Inet -Ibase -Imessage -Wl,-rpath,. net.so

# res
res:
	@rm -rf $(HOME)/.shaco/excel
	@mkdir -pv $(HOME)/.shaco/excel
	@rm -rf ./res/tbl
	@mkdir -pv ./res/tbl
	@rm -rf ./res/tplt
	@mkdir -pv ./res/tplt
	@rm -rf ./datadefine/tplt_struct.h
	@svn export $(SHACO_SVN_RES)/res/excel $(HOME)/.shaco/excel --force
	@cd tool && \
		python convert_excel.py \
		$(HOME)/.shaco/excel/excelmake_server.xml \
		$(HOME)/.shaco/excel tbl=../res/tbl:c=../res/tplt && \
		python concat.py ../res/tplt ../datadefine/tplt_struct.h && \
		rm -rf ../res/tplt

# for client
client: cnet.dll tplt.dll

client_bin=\
	cnet.dll \
	cnet.lib \
	tplt.dll \
	tplt.lib \
	tplt.def

cnet_src=\
	cnet/cnet.c \
	cnet/cnet.h

cnet.dll: $(net_src) $(cnet_src)
	gcc $(CFLAGS) -shared -o $@ $^ -Inet -Imessage -lws2_32 -lws2_32 \
		-Wl,--output-def,cnet.def,--out-implib,cnet.lib
	LIB /MACHINE:IX86 /DEF:cnet.def

tplt.dll: $(tplt_src)
	gcc $(CFLAGS) -shared -o $@ $^ -Itplt \
		-Wl,--output-def,tplt.def,--out-implib,tplt.lib
	LIB /MACHINE:IX86 /DEF:tplt.def

client_dir=D:/wa-client/trunk
install_dir=$(client_dir)/driller/proj.win32/Debug.win32
source_dir=$(client_dir)/driller/Classes
tool_dir=$(client_dir)/tool
install:
	cp $(client_bin) $(install_dir)
	cp -r net $(source_dir)	
	cp -r cnet $(source_dir)
	cp -r message $(source_dir)
	cp -r tplt $(source_dir)
	cp -r tool/concat.py tool/convert_excel.py tool/excelto $(tool_dir)

# clean
clean:
	rm -f shaco shaco-cli t *.so *.dll *.def *.lib *.exp

cleanall: clean
	rm -rf cscope.* tags
