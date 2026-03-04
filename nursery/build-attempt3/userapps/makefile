include ../config.mk

ROOTFS := ../build/rootfs
SUBDIRS := edit init shell


# mark directories as phony to always build
.PHONY: $(SUBDIRS)


all: $(SUBDIRS) $(LIBC)
	mkdir -p $(ROOTFS)/bin
	for n in $(SUBDIRS); do \
		cp $$n/$$n $(ROOTFS)/bin; \
	done

$(SUBDIRS):
	make -C $@

clean:
	for d in $(SUBDIRS); do make -C $$d clean; done
	rm -f *.o
