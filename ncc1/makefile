HDRS=ncc1.h token.h symbol.h type.h tree.h block.h reg.h
OBJS=ncc1.o lex.o symbol.o type.o decl.o init.o stmt.o block.o \
	opt.o reg.o tree.o output.o gen.o

ncc1: $(OBJS)
	$(CC) $(CFLAGS) -o ncc1 $(OBJS) 

clean::
	rm -f *.o ncc1
