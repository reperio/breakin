VERSION?=0.0

CC?=gcc
DESTDIR?=/usr/local
CPPFLAGS=-DTHREADING -DPRODUCT_VERSION=\"$(VERSION)\" -DPREFIX=\"$(DESTDIR)\" -DBURNIN_SCRIPT_PATH=\"$(DESTDIR)/etc/breakin/tests\"
LDFLAGS=-DTHREADING
LIBS=-lpanel -lncurses -ltinfo -lcurl
OBJS=breakin.o util.o dmidecode.o bench_stream.o bench_disk.o

all: breakin hpl_calc_n cryptpasswd hpl_calc_pq hpl

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

# XXX: Requires libopenmpi-dev and libopenblas-dev
hpl:
	rm -f hpl-2.3.tar.gz
	wget https://netlib.org/benchmark/hpl/hpl-2.3.tar.gz
	tar -xf hpl-2.3.tar.gz
	cd hpl-2.3 && ./configure && make
	cp hpl-2.3/testing/xhpl hpl

clean:
	rm -f *.o breakin breakin.static hpl_calc_n cryptpasswd .buildfoo hpl_calc_pq
	rm -rf hpl hpl-2.3 hpl-2.3.tar.gz

install: breakin hpl_calc_n cryptpasswd hpl
	mkdir -p $(DESTDIR)/bin
	install -m 0750 breakin $(DESTDIR)/bin/breakin
	install -m 0750 hpl_calc_n $(DESTDIR)/bin/hpl_calc_n
	install -m 0750 hpl_calc_pq $(DESTDIR)/bin/hpl_calc_pq
	install -m 0750 cryptpasswd $(DESTDIR)/bin/cryptpasswd
	install -m 0750 hpl $(DESTDIR)/bin/hpl
	mkdir -p $(DESTDIR)/etc/breakin/tests
	cp -av scripts/* $(DESTDIR)/etc/breakin
	sed -i -e 's;@DESTDIR@;$(DESTDIR);g' $(DESTDIR)/etc/breakin/startup.sh
	sed -i -e 's;@DESTDIR@;$(DESTDIR);g' $(DESTDIR)/etc/breakin/stop.sh
	sed -i -e 's;@DESTDIR@;$(DESTDIR);g' $(DESTDIR)/etc/breakin/tests/hpl

