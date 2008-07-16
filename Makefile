CC=gcc
CFLAGS= -DTHREADING
LDFLAGS= -DTHREADING 
LIBS=-lpanel -lncurses `curl-config --libs`
VERSION:=0.0

OBJS=breakin.o util.o dmidecode.o bench_stream.o bench_disk.o

all: breakin hpl_calc_n cryptpasswd

%.o: %.c
	$(CC) -DPRODUCT_VERSION=\"${VERSION}\" ${CFLAGS} -c $<

breakin: ${OBJS}
	${CC} ${LDFLAGS} -pthread $^ -o $@  ${LIBS}

breakin.static: ${OBJS}
	${CC} ${LDFLAGS} -pthread $^ ${LIBS} -static -o $@  

hpl_calc_n: hpl_calc_n.o
	${CC} ${LDFLAGS} $^ -lm -o $@  

cryptpasswd: cryptpasswd.o
	${CC} ${LDFLAGS} $^ -lcrypt -o $@

clean:
	rm -f *.o breakin breakin.static hpl_calc_n cryptpasswd
