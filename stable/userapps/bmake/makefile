# $Id: makefile.in,v 1.1 2012/12/28 21:28:19 sjg Exp $

# a simple makefile for those who don't like anything beyond:
# ./configure; make; make install

prefix= /usr/local
srcdir= /home/dimitris/code/handcrafted-os/nursery/build-attempt3/userapps/bmake

all: build

build clean install test:
	${srcdir}/boot-strap --prefix=${prefix} -o . op=$@

		
