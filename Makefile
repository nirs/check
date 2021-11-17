.PHONY: all go test clean

all: go

go:
	cd $@ && go build

test: all
	py.test

clean:
	rm -f go/check
