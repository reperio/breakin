VERSION?=0.0

CC?=gcc
DESTDIR?=/usr/local
CPPFLAGS=-DTHREADING -DPRODUCT_VERSION=\"$(VERSION)\" -DPREFIX=\"$(DESTDIR)\" -DBURNIN_SCRIPT_PATH=\"$(DESTDIR)/etc/breakin/tests\"
LDFLAGS=-DTHREADING
LIBS=-lpanel -lncurses -ltinfo -lcurl
OBJS=breakin.o util.o dmidecode.o bench_stream.o bench_disk.o

all: breakin hpl_calc_n cryptpasswd hpl_calc_pq

breakin: ${OBJS}
	${CC} ${LDFLAGS} -pthread $^ -o $@  ${LIBS}

breakin.static: ${OBJS}
	${CC} ${LDFLAGS} -pthread $^ ${LIBS} -static -o $@  

hpl_calc_n: hpl_calc_n.o
	${CC} ${LDFLAGS} $^ -lm -o $@  

hpl_calc_pq: hpl_calc_pq.o
	${CC} ${LDFLAGS} $^ -lm -o $@  

cryptpasswd: cryptpasswd.o
	${CC} ${LDFLAGS} $^ -lcrypt -o $@

clean:
	rm -f *.o breakin breakin.static hpl_calc_n cryptpasswd .buildfoo hpl_calc_pq

install: breakin hpl_calc_n cryptpasswd
	mkdir -p $(DESTDIR)/bin
	cp -p breakin $(DESTDIR)/bin/breakin
	cp -p hpl_calc_n $(DESTDIR)/bin/hpl_calc_n
	cp -p hpl_calc_pq $(DESTDIR)/bin/hpl_calc_pq
	cp -p cryptpasswd $(DESTDIR)/bin/cryptpasswd
	mkdir -p $(DESTDIR)/etc/breakin/tests
	cp -av scripts/* $(DESTDIR)/etc/breakin
	sed -i -e 's;@DESTDIR@;$(DESTDIR);g' $(DESTDIR)/etc/breakin/startup.sh

