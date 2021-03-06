
# find the OS
uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')

# Compile flags for linux / osx
ifeq ($(uname_S),Linux)
	SHOBJ_CFLAGS ?= -W -Wall -fno-common -g -ggdb -std=c99 -O0 -DMABREDIS_MODULE
	SHOBJ_LDFLAGS ?= -shared
else
	SHOBJ_CFLAGS ?= -W -Wall -dynamic -fno-common -g -ggdb -std=c99 -O0 -DMABREDIS_MODULE 
	SHOBJ_LDFLAGS ?= -bundle -undefined dynamic_lookup
endif

.SUFFIXES: .c .so .o


all: mabredis.so 

vpath %.c beta_fn

.c.o:
	$(CC) -I. $(CFLAGS) $(SHOBJ_CFLAGS) -fPIC -c $< -o $@

mabredis.xo: redismodule.h
multiarm.c: multiarm.h

mabredis.so: mabredis.o multiarm.o pcg.o \
beta_random_variate.o exponential_random_variate.o gamma_random_variate.o uniform_0_1_random_variate.o exponential_variate_inversion.o
	$(LD) -o $@ $^ $(SHOBJ_LDFLAGS) $(LIBS) -lc

clean:
	rm -rf *.o *.so
