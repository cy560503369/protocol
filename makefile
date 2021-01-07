VPATH += ./

LDFLAGS += -lm -pthread

CC = $(CROSS_COMPILE)gcc

BIN_PATH := ../bin

target = protocol

all: $(target)

obj-protocol = cJSON.o uart.o protocol103.o protocol.o

protocol: $(obj-protocol)
	$(CC) $(LDFLAGS) $(obj-protocol) -o $(BIN_PATH)/protocol

.PHONY:clean
clean:
	rm *.o
	rm $(BIN_PATH)/protocol
