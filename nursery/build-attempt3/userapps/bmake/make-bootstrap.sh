#!/bin/sh

set -e

prefix=/usr/local
srcdir=/home/dimitris/code/handcrafted-os/nursery/build-attempt3/userapps/bmake

DEFAULT_SYS_PATH="${prefix}/share/mk"

case "yes" in
yes) XDEFS="-DUSE_META ${XDEFS}";;
esac

CC="gcc"
CFLAGS="-g -O2 -I. -I${srcdir} -DHAVE_CONFIG_H  -DMAKE_NATIVE ${XDEFS} -DBMAKE_PATH_MAX=1024"

MAKE_VERSION=20260313

MDEFS="-DMAKE_VERSION=\"$MAKE_VERSION\" \
-DMACHINE=\"x86_64\" \
-DMACHINE_ARCH=\"x86_64\" \
-DMAKE_OS=\"\" \
-D_PATH_DEFSYSPATH=\"${DEFAULT_SYS_PATH}\""


LDFLAGS=""
LIBS=""

toUpper() {
    ${TR:-tr} abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ
}

do_compile2() {
	obj="$1"; shift
	src="$1"; shift
	echo ${CC} -c ${CFLAGS} "$@" -o "$obj" "$src"
	${CC} -c ${CFLAGS} "$@" -o "$obj" "$src"
}

do_compile() {
	obj="$1"; shift
	case "$1" in
	*.c) src=$1; shift;;
	*) src=`basename "$obj" .o`.c;;
	esac

	for d in "$srcdir" "$srcdir/lst.lib"
	do
		test -s "$d/$src" || continue

		do_compile2 "$obj" "$d/$src" "$@" || exit 1
		return
	done
	echo "Unknown object file '$obj'" >&2
	exit 1
}

do_link() {
	output="$1"; shift
	echo ${CC} ${LDSTATIC} ${LDFLAGS} -o "$output" "$@" ${LIBS}
	${CC} ${LDSTATIC} ${LDFLAGS} -o "$output" "$@" ${LIBS}
}

BASE_OBJECTS="arch.o buf.o compat.o cond.o dir.o for.o hash.o \
lst.o make.o make_malloc.o metachar.o parse.o sigcompat.o str.o \
suff.o targ.o trace.o var.o util.o"

LIB_OBJECTS=" ${LIBOBJDIR}stresep$U.o"

do_compile main.o ${MDEFS}

for o in ${BASE_OBJECTS} ${LIB_OBJECTS}
do
	do_compile "$o"
done

case "yes" in
yes)
	case "no" in
	no) MDEFS=;;
	*)
		MDEFS="-DUSE_FILEMON -DUSE_FILEMON_`echo no | toUpper`"
		case "no,no" in
		dev,*/filemon.h) FDEFS="-DHAVE_FILEMON_H -I`dirname no`";;
		*) FDEFS=;;
		esac
		do_compile filemon_no.o filemon/filemon_no.c ${FDEFS}
		BASE_OBJECTS="filemon_no.o $BASE_OBJECTS"
		;;
	esac
	do_compile meta.o ${MDEFS}
	BASE_OBJECTS="meta.o ${BASE_OBJECTS}"
	;;
esac
do_compile job.o ${MDEFS}

do_link bmake main.o job.o ${BASE_OBJECTS} ${LST_OBJECTS} ${LIB_OBJECTS}
