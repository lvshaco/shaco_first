.PHONY: all clean cleanall

CFLAGS=-g -Werror
SHARED=-fPIC -shared
LDFLAGS=-Wl,-rpath,. \
		-L. \
		-llua -lm -ldl lur.so

service_src=$(wildcard service/*.c)
service_so=$(patsubst %.c,%.so,$(notdir $(service_src)))

all: $(service_so) lur.so shaco

$(service_so): $(service_src)
	gcc $(CFLAGS) $(SHARED) -o $@ $<

lur.so: lur/lur.c lur/lur.h
	gcc $(CFLAGS) $(SHARED) -o $@ $^

INC_PATH=-Ihost -Ilur

shaco: host/host_main.c
	gcc $(CFLAGS) -o $@ $^ $(INC_PATH) $(LDFLAGS)

clean:
	rm -f shaco *.so

cleanall: clean
	rm -f cscope.* tags
