CC=gcc
CFLAG_INCLUDE=-I. -Iinclude -Iprotobuf
CFLAGS=-Wall $(CFLAG_INCLUDE) -Wextra -std=gnu99 -ggdb
LDLIBS=-lprotobuf-c -lcrypto -lm
SRC=src
VPATH= $(SRC) include protobuf

all: example_hash chord_protobuf chord

example_hash: hash.o example_hash.o
	$(CC) $(CFLAGS) $(SRC)/hash.c $(SRC)/example_hash.c -o example_hash $(LDLIBS)

chord_protobuf:
	protoc-c --c_out=. protobuf/chord.proto

chord: outstanding_request.o request_queue.o time_utils.o hash.o chord_arg_parser.o comms.o entry.o entry_list.o  protobuf/chord.pb-c.o chord.o
	$(CC) $(CFLAGS) $(SRC)/outstanding_request.c $(SRC)/request_queue.c $(SRC)/time_utils.c $(SRC)/hash.c $(SRC)/chord_arg_parser.c $(SRC)/comms.c $(SRC)/entry.c $(SRC)/entry_list.c protobuf/chord.pb-c.c $(SRC)/chord.c -o chord $(LDLIBS)
clean:
	rm -rf protobuf/*.pb-c.* *~ chord *.o example_hash

.PHONY : clean all
