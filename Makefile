.PHONY: all t robot clean cleanall res thirdlib
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
	base/array.h \
	base/freeid.h \
	base/hashid.h \
	base/stringsplice.h \
	base/stringtable.h \
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
	libshaco/sh_monitor.c \
 	libshaco/dlmodule.c \
	libshaco/sh_util.c \
	libshaco/sh_hash.c
	

cli_src=\
	tool/shaco-cli.c

LDFLAGS=-Wl,-rpath,. \
		shaco.so net.so lur.so base.so -llua -lm -ldl -lrt -rdynamic# -Wl,-E


#service_benchmarkdb.so

service_so=\
	service_echo.so \
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
	service_benchmarklog.so \
	service_benchmark.so

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
	service_gamelog.so \
	$(service_so) \
	service_room.so \
	service_rank.so \
	service_redisproxy.so \
	service_hall.so \
	service_auth.so \
	service_robot.so

release: CFLAGS += -O2 -fno-strict-aliasing
release: all

$(service_so): %.so: $(service_dir)/%.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $< -Iinclude/libshaco -Inet -Ibase -Imsg

service_room.so: $(service_dir)/service_room.c \
	mod_room/room_tplt.c \
	mod_room/room_tplt.h \
	mod_room/fight.c \
	mod_room/fight.h \
	mod_room/genmap.c \
	mod_room/genmap.h \
	mod_room/mapdatamgr.c \
	mod_room/mapdatamgr.h \
	mod_room/roommap.c \
	mod_room/roommap.h
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Imsg -Imod_room -Itplt -Idatadefine -Wl,-rpath,. tplt.so 

service_log.so: $(service_dir)/service_log.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Imsg -Ielog -Wl,-rpath,. elog.so

service_gamelog.so: $(service_dir)/service_gamelog.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Imsg -Ielog -Wl,-rpath,. elog.so


service_rank.so: $(service_dir)/service_rank.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Imsg -Iworld -Iredis -Wl,-rpath,. redis.so


service_benchmarkdb.so: $(service_dir)/service_benchmarkdb.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Imsg -Iredis -Wl,-rpath,. redis.so

service_redisproxy.so: $(service_dir)/service_redisproxy.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Imsg -Iredis -Wl,-rpath,. redis.so

service_auth.so: $(service_dir)/service_auth.c
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Imsg -Iredis -Wl,-rpath,. redis.so

service_hall.so: $(service_dir)/service_hall.c \
	mod_hall/hall_tplt.c \
	mod_hall/hall_tplt.h \
	mod_hall/hall_player.c \
	mod_hall/hall_player.h \
	mod_hall/hall_playerdb.c \
	mod_hall/hall_playerdb.h \
	mod_hall/hall_role.c \
	mod_hall/hall_role.h \
	mod_hall/hall_ring.c \
	mod_hall/hall_ring.h \
	mod_hall/hall_award.c \
	mod_hall/hall_award.h \
	mod_hall/hall_attribute.c \
	mod_hall/hall_attribute.h \
	mod_hall/hall_play.c \
	mod_hall/hall_play.h
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Itplt -Idatadefine -Imsg -Iredis -Imod_hall -Wl,-rpath,. redis.so tplt.so

service_robot.so: $(service_dir)/service_robot.c \
	mod_hall/hall_attribute.c \
	mod_hall/hall_attribute.h \
	mod_robot/robot.h \
	mod_robot/robot_tplt.c \
	mod_robot/robot_tplt.h
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Inet -Ibase -Imsg -Iredis -Itplt -Idatadefine -Imod_hall -Imod_robot -Wl,-rpath,. redis.so tplt.so

lur.so: $(lur_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -llua

net.so: $(net_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^

redis.so: $(redis_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Ibase -Iinclude/libshaco

base.so: $(base_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^

tplt.so: $(tplt_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -DUSE_HOSTLOG -Iinclude/libshaco

elog.so: $(elog_src)
	@rm -f $@
	gcc $(CFLAGS) $(SHARED) -o $@ $^

shaco.so: $(libshaco_src)
	gcc $(CFLAGS) $(SHARED) -o $@ $^ -Iinclude/libshaco -Ilur -Inet -Ibase

shaco: main/shaco.c
	gcc $(CFLAGS) -o $@ $^ -Iinclude/libshaco -Ilur -Inet -Ibase  $(LDFLAGS)

shaco-cli: $(cli_src)
	gcc $(CFLAGS) -o $@ $^ -lpthread

t: main/test.c shaco.so net.so lur.so base.so redis.so elog.so
	gcc $(CFLAGS) -o $@ $^ -Iinclude/libshaco -Ilur -Inet -Ibase -Iredis -Ielog $(LDFLAGS) redis.so 

robot: main/robot.c cnet/cnet.c cnet/cnet.h net.so
	gcc $(CFLAGS) -o $@ $^ -Ilur -Icnet -Inet -Ibase -Imsg -Wl,-rpath,. net.so

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
	gcc $(CFLAGS) -shared -o $@ $^ -Inet -Imsg -lws2_32 \
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
	for file in `ls game`; do iconv -f utf-8 -t gbk mod_room/$$file > .mod_room/$$file; done 
	cp -r .mod_room/* $(source_dir)/map
	rm -rf .game
	#cp -r msg $(source_dir)
	mkdir .msg
	for file in `ls msg`; do iconv -f utf-8 -t gbk msg/$$file > .msg/$$file; done 
	cp -r .msg/* $(source_dir)/msg
	rm -rf .msg
	cp -r tplt $(source_dir)
	cp -r tool/concat.py tool/convert_excel.py tool/excelto $(tool_dir)

# clean
clean:
	rm -f shaco shaco-cli t robot *.so *.dll *.def *.lib *.exp

cleanall: clean
	rm -rf cscope.* tags
