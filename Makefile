.PHONY: all t clean cleanall res thirdlib
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

elog_src=\
	elog/elog.h \
	elog/elog.c \
	elog/elog_internal.h \
	elog/elog_appender.h \
	elog/elog_appender_file.h \
	elog/elog_appender_file.c \
	elog/elog_appender_rollfile.c \
	elog/elog_appender_rollfile.h

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

libshaco_src=\
	libshaco/sc_init.c \
	libshaco/sc_start.c \
	libshaco/sc_sig.c \
	libshaco/sc_check.c \
	libshaco/sc_env.c \
 	libshaco/sc_net.c \
 	libshaco/sc_service.c \
 	libshaco/sc_timer.c \
 	libshaco/sc_log.c \
	libshaco/sc_reload.c \
	libshaco/sc_node.c \
	libshaco/sc_gate.c \
	libshaco/sh_monitor.c \
 	libshaco/dlmodule.c \
	libshaco/sh_util.c
	

cli_src=\
	tool/shaco-cli.c

LDFLAGS=-Wl,-rpath,. \
		shaco.so net.so lur.so base.so -llua -lm -ldl -lrt -rdynamic# -Wl,-E

service_so=\
	service_benchmark.so \
	service_echo.so \
	service_centerc.so \
	service_centers.so \
	service_node.so \
	service_cmds.so \
	service_cmdctl.so \
	service_gate.so \
	service_route.so \
	service_loadbalance.so \
	service_watchdog.so \
	service_uniqueol.so \
	service_match.so \
	service_cmdctlgame.so

worldservice_so=\
	service_world.so \
	service_gamematch.so \
	service_rolelogic.so \
	service_ringlogic.so \
	service_awardlogic.so \
	service_attribute.so

all: \
	shaco.so \
	lur.so \
	net.so \
	base.so \
	redis.so \
	tplt.so \
	elog.so \
	shaco \
	shaco-cli \
	t \
	robot \
	service_log.so \
	$(service_so) \
	service_game.so \
	$(worldservice_so) \
	service_playerdb.so \
	service_rank.so \
	service_benchmarkdb.so \
	service_redisproxy.so \
	service_login.so \
	service_tplthall.so \
	service_tpltroom.so \
	service_cmdctlworld.so \
	service_hall.so

release: CFLAGS += -O2 -fno-strict-aliasing
release: all

$(service_so): %.so: $(service_dir)/%.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $< -Iinclude/libshaco -Inet -Ibase -Imessage

$(worldservice_so): %.so: $(service_dir)/%.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Imessage -Iworld -Itplt -Idatadefine -Wl,-rpath,. tplt.so

service_game.so: $(service_dir)/service_game.c \
	game/fight.c \
	game/fight.h \
	game/genmap.c \
	game/genmap.h
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Imessage -Igame -Itplt -Idatadefine -Wl,-rpath,. tplt.so

service_log.so: $(service_dir)/service_log.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Imessage -Ielog -Wl,-rpath,. elog.so


service_playerdb.so: $(service_dir)/service_playerdb.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Imessage -Iworld -Iredis -Wl,-rpath,. redis.so

service_rank.so: $(service_dir)/service_rank.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Imessage -Iworld -Iredis -Wl,-rpath,. redis.so


service_benchmarkdb.so: $(service_dir)/service_benchmarkdb.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Imessage -Iredis -Wl,-rpath,. redis.so

service_redisproxy.so: $(service_dir)/service_redisproxy.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Imessage -Iredis -Wl,-rpath,. redis.so

service_login.so: $(service_dir)/service_login.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Imessage -Iredis -Wl,-rpath,. redis.so

service_tplthall.so: $(service_dir)/service_tplthall.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Itplt -Idatadefine -Wl,-rpath,. tplt.so

service_tpltroom.so: $(service_dir)/service_tpltroom.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Itplt -Idatadefine -Igame -Wl,-rpath,. tplt.so mapdatamgr.so

service_cmdctlworld.so: $(service_dir)/service_cmdctlworld.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Imessage -Wl,-rpath,. 

service_hall.so: $(service_dir)/service_hall.c \
	hall/player.c \
	hall/player.h \
	hall/playerdb.c \
	hall/playerdb.h \
	hall/rolelogic.c \
	hall/rolelogic.h \
	hall/ringlogic.c \
	hall/ringlogic.h \
	hall/awardlogic.c \
	hall/awardlogic.h \
	hall/attrilogic.c \
	hall/attrilogic.h \
	hall/playlogic.c \
	hall/playlogic.h
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Itplt -Idatadefine -Imessage -Iredis -Ihall -Wl,-rpath,. redis.so

lur.so: $(lur_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -llua

net.so: $(net_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^

redis.so: $(redis_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Ibase

base.so: $(base_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^

tplt.so: $(tplt_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -DUSE_HOSTLOG -Iinclude/libshaco

mapdatamgr.so: game/mapdatamgr.c game/mapdatamgr.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Itplt -Idatadefine -Igame

elog.so: $(elog_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^

shaco.so: $(libshaco_src)
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Ilur -Inet -Ibase

shaco: main/shaco.c
	gcc $(CFLAGS) -o $@ $^ -Iinclude/libshaco -Ilur -Inet -Ibase  $(LDFLAGS)

shaco-cli: $(cli_src)
	gcc $(CFLAGS) -o $@ $^ -lpthread

t: main/test.c net.so lur.so base.so redis.so elog.so
	gcc $(CFLAGS) -o $@ $^ -Iinclude/libshaco -Ilur -Inet -Ibase -Iredis -Ielog $(LDFLAGS) redis.so

robot: main/robot.c cnet/cnet.c cnet/cnet.h net.so
	gcc $(CFLAGS) -o $@ $^ -Ilur -Icnet -Inet -Ibase -Imessage -Wl,-rpath,. net.so

# res
res:
	@rm -rf $(HOME)/.shaco/excel
	@mkdir -pv $(HOME)/.shaco/excel
	@rm -rf ./res/tbl
	@mkdir -pv ./res/tbl
	@rm -rf ./res/tplt
	@mkdir -pv ./res/tplt
	@mkdir -pv ./datadefine
	@rm -rf ./datadefine/tplt_struct.h
	@svn export $(SHACO_SVN_RES)/res/excel $(HOME)/.shaco/excel --force
	@cd tool && \
		python convert_excel.py \
		$(HOME)/.shaco/excel/excelmake_server.xml \
		$(HOME)/.shaco/excel tbl=../res/tbl:c=../res/tplt:map=../res/map && \
		python concat.py ../res/tplt ../datadefine/tplt_struct.h && \
		rm -rf ../res/tplt

# thirdlib
thirdlib:
	@__=`pwd` && cd third && $(MAKE) dist DIST_PATH=$$__/thirdlib

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

testclient: main/testclient.c $(tplt_src) datadefine/tplt_struct.h
	gcc $(CFLAGS) -o $@ $^ -Itplt -Idatadefine

client_dir=D:/wa-client/trunk
install_dir=$(client_dir)/driller/proj.win32/Debug.win32
install_dir_rel=$(client_dir)/driller/proj.win32/Release.win32
source_dir=$(client_dir)/driller/Classes
tool_dir=$(client_dir)/tool
install:
	cp $(client_bin) $(install_dir)
	cp $(client_bin) $(install_dir_rel)
	cp -r net $(source_dir)	
	cp -r cnet $(source_dir)
	cp -r main/robot.c $(source_dir)/cnet
	mkdir .game
	for file in `ls game`; do iconv -f utf-8 -t gbk game/$$file > .game/$$file; done 
	cp -r .game/* $(source_dir)/map
	rm -rf .game
	#cp -r message $(source_dir)
	mkdir .message
	for file in `ls message`; do iconv -f utf-8 -t gbk message/$$file > .message/$$file; done 
	cp -r .message/* $(source_dir)/message
	rm -rf .message
	cp -r tplt $(source_dir)
	cp -r tool/concat.py tool/convert_excel.py tool/excelto $(tool_dir)

# clean
clean:
	rm -f shaco shaco-cli t robot *.so *.dll *.def *.lib *.exp

cleanall: clean
	rm -rf cscope.* tags
