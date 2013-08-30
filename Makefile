.PHONY: all clean cleanall

CFLAGS=-g -Werror
SHARED=-fPIC -shared

all: lur.so shaco

LIBS=lur.so -llua -lm -ldl
INC_PATH=-Ilur -Ihost

lur.so: lur/lur.c lur/lur.h
	gcc $(CFLAGS) $(SHARED) -o $@ $^

shaco: host/host_main.c
	gcc $(CFLAGS) -o $@ $^ $(LIBS) $(INC_PATH)

clean:
	rm -f shaco *.so

cleanall: clean
	rm -f cscope.* tags
