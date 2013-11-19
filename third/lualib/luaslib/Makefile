
CFLAGS := -g -Wall
SHARED := -shared -fPIC

INC_PATH := \
	-I. \
	-I../lua-5.2.1/src

ALLT := \
	liblbase.so \
	liblxdr.so \
	liblsys.so

.PHONY : all $(ALLT) clean

all : $(ALLT)

liblbase.so : baselib.c
	gcc $(CFLAGS) $(SHARED) -o $@ $^ $(INC_PATH)

liblxdr.so : xdrlib.c xdr.c xdr.h
	gcc $(CFLAGS) $(SHARED) -o $@ $^ $(INC_PATH)

liblsys.so : syslib.c
	gcc $(CFLAGS) $(SHARED) -o $@ $^ $(INC_PATH)

clean :
	@rm -rf *.so *.o
