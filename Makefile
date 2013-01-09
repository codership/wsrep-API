# Copyright (C) 2012 Codership Oy <http://www.codership.com>
#
# A simple static Makefile to compile standalone library and examples.
# In autotools projects use Makefile.am

COMPILE = gcc -Wall -Wextra -Werror -O2 -g

%.o : %.c
	$(COMPILE) -c $<

libwsrep_SRCS = wsrep_loader.c wsrep_dummy.c wsrep_uuid.c wsrep_gtid.c

libwsrep_OBJS = $(patsubst %.c,%.o,$(libwsrep_SRCS))

libwsrep.a: $(libwsrep_OBJS)
	rm -f $@
	ar rcs $@ $^

listener_SRCS = wsrep_listener.c
listener: $(listener_SRCS) libwsrep.a
	$(COMPILE) -I. -o $@ $^ -ldl -lpthread

all: libwsrep.a listener

.PHONY: clean

clean:
	rm -f *.o *.a listener

