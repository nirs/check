src = reader.c main.c check.c log.c event.c

CPPFLAGS = -D_GNU_SOURCE
CFLAGS = -g -Wall -Wextra -Wno-unused-parameter -Werror -std=c99
LDLIBS = -lev -laio

obj = $(src:.c=.o)
dep = $(src:.c=.d)

check: $(obj)
	$(LINK.o) $^ $(LDLIBS) -o $@

.PHONY: test
test: check
	py.test

.PHONY: clean
clean:
	rm -f check *.[do]

%.o: %.c %.d
	$(COMPILE.c) $< -MMD -MF $*.d -o $@

%.d: ;

-include $(dep)
