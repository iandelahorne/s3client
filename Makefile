CFLAGS=-g -Wall -I/opt/local/include  -I/opt/local/include/libxml2
LDFLAGS=-L/opt/local/lib -lcrypto -lcurl -lssl -lexpat -lxml2

OBJS=s3test.o s3string.o

all: s3test

s3test: $(OBJS)

clean:
	rm -f $(OBJS)