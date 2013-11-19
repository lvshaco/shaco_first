.PHONY : lpeg.so clean

COPT = -O2 -DNDEBUG

LUAINC = ../lua-5.2.1/src

CWARNS = -Wall -Wextra -pedantic \
        -Waggregate-return \
	-Wbad-function-cast \
        -Wcast-align \
        -Wcast-qual \
	-Wdeclaration-after-statement \
	-Wdisabled-optimization \
        -Wmissing-prototypes \
        -Wnested-externs \
        -Wpointer-arith \
        -Wshadow \
	-Wsign-compare \
	-Wstrict-prototypes \
	-Wundef \
        -Wwrite-strings \
	#  -Wunreachable-code \


CFLAGS = $(COPT) -ansi -fPIC -I$(LUAINC)
CC = gcc

# For Linux
#DLLFLAGS = -shared -fpic
DLLFLAGS = -shared -fPIC
ENV = 

# For Mac OS
# ENV = MACOSX_DEPLOYMENT_TARGET=10.4
# DLLFLAGS = -bundle -undefined dynamic_lookup

lpeg.so: lpeg.o
	$(CC) $(DLLFLAGS) lpeg.o -o lpeg.so

lpeg.o: lpeg.c
	$(CC) $(CFLAGS) -c $^

test: test.lua re.lua lpeg.so
	test.lua

clean:
	@rm -rf *.so *.o
