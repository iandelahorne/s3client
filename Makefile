CFLAGS=-g -Wall -I/opt/local/include
LDFLAGS=-L/opt/local/lib -lcrypto -lcurl -lssl -lexpat

OBJS=s3test.o s3string.o

all: s3test

s3test: $(OBJS)

clean:
	rm -f $(OBJS)