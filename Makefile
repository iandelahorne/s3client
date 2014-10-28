CFLAGS=-g -Wall -I/usr/include/libxml2 -DLINUX -D_GNU_SOURCE=1
LDFLAGS=-lcrypto -lcurl -lssl -lxml2 -lbsd

OBJS=s3test.o s3string.o s3digest.o s3ops.o s3xml.o

all: s3test

s3test: $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

valgrind: s3test
	valgrind --leak-check=full ./s3test

clean:
	rm -f $(OBJS)
