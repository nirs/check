src = lineio.c main.c check.c

CPPFLAGS = -D_GNU_SOURCE
CFLAGS = -g -Wall -Wextra -Wno-unused-parameter -Werror
LDLIBS = -lev

obj = $(src:.c=.o)

check: $(obj)
	$(LINK.o) $^ $(LDLIBS) -o $@

.PHONY: clean
clean:
	rm -f check *.o
