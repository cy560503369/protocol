INC = ../include/
LIB = -lpthread
SOURCE = test.c
OBJECT = test.o
TARGET = demo_test

CC = $(CROSS_COMPILE)gcc
CFLAGS =  -Wall -I${INC}

${TARGET}:${OBJECT}
	$(CC)  -o $@ ${OBJECT}  ${LIB}

${OBJ}/%.o:${SRC}/%.c
	$(CC) -c $<  -o  $@ $(CFLAGS)  ${LIB} 


.PHONY:clean
clean:
	find $(OBJ) -name *.o -exec rm -rf {} \;
	rm $(TARGET) -f

