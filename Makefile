.PHONY: all c go test clean

all: c go

c:
	$(MAKE) -C $@

go:
	cd $@ && go build

test: all
	py.test

clean:
	$(MAKE) -C c clean
	rm -f go/check
