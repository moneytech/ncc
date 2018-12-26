OBJS=ncpp.o input.o directive.o token.o macro.o vstring.o

ncpp: $(OBJS)
	$(CC) $(CFLAGS) -o ncpp $(OBJS)

clean::
	rm -f *.o ncpp
