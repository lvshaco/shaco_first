SUBDIRS := \
	lualib

.PHONY : all $(SUBDIRS) dist clean

all : $(SUBDIRS)

$(SUBDIRS) :
	$(MAKE) -C $@ || exit 1;

dist :
	@for dir in $(SUBDIRS); do $(MAKE) -C $$dir dist; done

clean :
	@for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean; done
