.PHONY: all c go test clean

all: c go

c:
	$(MAKE) -C $@

go:
	go build -o go/check go/src/check.go

test: all
	py.test

clean:
	$(MAKE) -C c clean
	rm -f go/check
