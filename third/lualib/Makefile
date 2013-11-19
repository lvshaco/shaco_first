LUADIR := lua-5.2.1
LUASDIR := luaslib
LPEGDIR := lpeg-0.10.2
FSDIR := luafilesystem-1.4.2
SIGDIR := LuaSignal-0.1
SOCKDIR := luasocket-2.0.2

SUBDIRS := \
	$(LUADIR) \
	$(LUASDIR) \
	$(LPEGDIR) \
	$(FSDIR) \
	$(SIGDIR) \
	$(SOCKDIR)
SODIRS := \
	$(LUADIR)/src \
	$(LUASDIR) \
	$(LPEGDIR) \
	$(FSDIR)/src \
	$(SIGDIR)/src \
	$(SOCKDIR)/src

.PHONY : all $(SUBDIRS) dist clean

all : $(SUBDIRS)

$(SUBDIRS) :
	$(MAKE) -C $@ || exit 1;

dist : 
	@for dir in $(SODIRS); do cp $$dir/*.so $(DIST_PATH)/lua; done
	@cp $(SOCKDIR)/src/*.lua $(DIST_PATH)/lua

clean :
	@for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean; done

