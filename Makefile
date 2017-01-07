.PHONY: all c test clean

all: c

c:
	$(MAKE) -C $@

test: c
	py.test

clean:
	$(MAKE) -C c clean
