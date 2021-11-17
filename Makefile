.PHONY: check test clean

check:
	go build

test: check
	py.test

clean:
	rm -f check
