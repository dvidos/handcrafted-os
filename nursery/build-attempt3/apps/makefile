include ../config.mk

# mark directories as phony to always build
.PHONY: edit init shell


all: edit init shell


edit:
	make -C edit

init:
	make -C init

shell:
	make -C shell

clean:
	make -C edit  clean
	make -C init  clean
	make -C shell clean
	rm -f *.o
