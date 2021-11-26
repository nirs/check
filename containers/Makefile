targets := fedora-35

all: $(targets)

$(targets):
	podman build -t check-$@ -f $@.containerfile .
