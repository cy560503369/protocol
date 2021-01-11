BIN = ./bin
SRC = ./src
INC = ./include/
LIB = -L./lib -l60870 -lpthread
OBJ = ./obj
SOURCE = $(wildcard ${SRC}/*.c)
OBJECT = $(patsubst %.c,${OBJ}/%.o,$(notdir ${SOURCE}))

TARGET = protocol
BIN_TARGET = ${BIN}/${TARGET}

CC = $(CROSS_COMPILE)gcc
CFLAGS =  -Wall -I${INC}

${BIN_TARGET}:${OBJECT}
	$(CC)  -o $@ ${OBJECT}  ${LIB}

${OBJ}/%.o:${SRC}/%.c
	$(CC) -c $<  -o  $@ $(CFLAGS)  ${LIB} 


.PHONY:clean
clean:
	find $(OBJ) -name *.o -exec rm -rf {} \;
	rm $(BIN_TARGET) -f
	
