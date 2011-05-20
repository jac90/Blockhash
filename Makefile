rm=/bin/rm -f
MAKE= /usr/bin/make

SUBDIRS-y :=
SUBDIRS-y += lib
SUBDIRS-y += tools

.PHONY: all
all: build

.PHONY: build
build:
	@set -e; for subdir in $(SUBDIRS-y); do \
	$(MAKE) -C $$subdir all;       \
		done

.PHONY: clean
clean:
	$(rm) core *.o *.so *~
	@set -e; for subdir in $(SUBDIRS-y); do \
	$(MAKE) -C $$subdir clean;       \
		done
