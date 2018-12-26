CC=gcc
CFLAGS=-Wno-implicit-int -Wno-implicit-function-declaration

all:: ncc nld nobj
	make CC="$(CC)" CFLAGS="$(CFLAGS)" -C ncpp 
	make CC="$(CC)" CFLAGS="$(CFLAGS)" -C ncc1
	make CC="$(CC)" CFLAGS="$(CFLAGS)" -C nas

ncc: ncc.c
nld: nld.c
nobj: nobj.c

install:: all
	mkdir -p ~/bin
	cp ncc nld nobj ncpp/ncpp ncc1/ncc1 nas/nas ~/bin

clean::
	rm -f nld ncc nobj
	make -C ncpp clean
	make -C ncc1 clean
	make -C nas clean

