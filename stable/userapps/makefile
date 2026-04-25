include ../config.inc.mk


SUBDIRS := \
	init \
	sash \
	coreutils \
	usertests


# mark directories as phony to always build
.PHONY: $(SUBDIRS)

all: $(SUBDIRS)

$(SUBDIRS):
	make -C $@

.PHONY: install
install:
	for d in $(SUBDIRS); do \
		make -C $$d install; \
	done

clean:
	for d in $(SUBDIRS); do \
		make -C $$d clean; \
	done
