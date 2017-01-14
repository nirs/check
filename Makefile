.PHONY: all c go test clean

all: c go

c:
	$(MAKE) -C $@

go:
	go build -o go/check check/go/src/check

test: all
	py.test

clean:
	$(MAKE) -C c clean
	rm -f go/check
