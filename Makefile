.PHONY: all c go test clean

all: c go

c:
	$(MAKE) -C $@

go:
	GOPATH=$(abspath go) go build -o go/check go/src/main.go

test: all
	py.test

clean:
	$(MAKE) -C c clean
	rm -f go/check
