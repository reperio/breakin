CC=gcc
CFLAGS= -DTHREADING
LDFLAGS= -DTHREADING 
LIBS=-lpanel -lncurses `curl-config --libs`
VERSION:=0.0
INSTALLPREFIX=SET_ME

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
	rm -f *.o breakin breakin.static hpl_calc_n cryptpasswd .buildfoo

install: breakin hpl_calc_n cryptpasswd

	mkdir -p ${INSTALLPREFIX}/usr/local/bin
	cp -p breakin ${INSTALLPREFIX}/usr/local/bin/breakin
	cp -p hpl_calc_n ${INSTALLPREFIX}/usr/local/bin/hpl_calc_n
	cp -p cryptpasswd ${INSTALLPREFIX}/usr/local/bin/cryptpasswd

	mkdir -p ${INSTALLPREFIX}/etc/breakin/tests

	cp -av scripts/* ${INSTALLPREFIX}/etc/breakin

