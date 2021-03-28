
FLAGS  = -Wall -g -pthread
CC     = gcc
PROG   = corridas
OBJS   = corridas.o

all:	${PROG}

clean:
	rm ${OBJS} ${PROG} *~
  
${PROG}:	${OBJS}
	${CC} ${FLAGS} ${OBJS} -o $@

.c.o:
	${CC} ${FLAGS} $< -c

##########################

corridas.o: corridas.h corridas.c

corridas: corridas.o

