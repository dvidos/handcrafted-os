include ../config.mk
FS := ../build/rootfs
SUBDIRS := edit init shell


# mark directories as phony to always build
.PHONY: $(SUBDIRS)


all: $(SUBDIRS)
	mkdir -p $(FS)/bin
	for d in $(SUBDIRS); do \
		make -C $$d;  \
		cp $$d/$$d $(FS)/bin; \
	done


clean:
	for d in $(SUBDIRS); do make -C $$d clean; done
	rm -f *.o
