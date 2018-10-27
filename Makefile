# $Id: Makefile,v 1.5 2003/04/03 18:04:31 folkert Exp folkert $
# $Log: Makefile,v $
# Revision 1.5  2003/04/03 18:04:31  folkert
# *** empty log message ***
#
# Revision 1.4  2003/01/28 20:18:44  folkert
# added package target
#
# Revision 1.3  2003/01/28 18:34:50  folkert
# added locale-stuff
#
# Revision 1.2  2003/01/27 17:27:04  folkert
# *** empty log message ***
#

# please set here the path to the locale-files
LOCALE_PATH=/usr/local/share/locale

CC=gcc

CFLAGS=-O2 -Wall
# CFLAGS=-Wall -Wshadow -g
LDFLAGS=
VERSION=1.9

OBJS=logintop10.o io.o val.o mem.o
LANGUAGE_FILES=logintop10-nl.mo logintop10-fr.mo logintop10-es.mo logintop10-ar.mo logintop10-de.mo

all: logintop10

logintop10-ar.mo: logintop10-ar.po
	msgfmt -c -f --statistics -v -o logintop10-ar.mo logintop10-ar.po

logintop10-es.mo: logintop10-es.po
	msgfmt -c -f --statistics -v -o logintop10-es.mo logintop10-es.po

logintop10-fr.mo: logintop10-fr.po
	msgfmt -c -f --statistics -v -o logintop10-fr.mo logintop10-fr.po

logintop10-nl.mo: logintop10-nl.po
	msgfmt -c -f --statistics -v -o logintop10-nl.mo logintop10-nl.po

logintop10-de.mo: logintop10-de.po
	msgfmt -c -f --statistics -v -o logintop10-de.mo logintop10-de.po

logintop10: $(OBJS) $(LANGUAGE_FILES)
	$(CC) -Wall -W $(OBJS) $(LDFLAGS) $(LDFLAGSf) -o logintop10
	strip logintop10

install: logintop10 $(LANGUAGE_FILES)
	cp logintop10 /usr/local/bin
	install -D logintop10.1 /usr/local/man/man1/logintop10.1
	gzip -9 /usr/local/man/man1/logintop10.1
	install -D logintop10-nl.mo $(LOCALE_PATH)/nl/LC_MESSAGES/logintop10.mo
	install -D logintop10-fr.mo $(LOCALE_PATH)/fr/LC_MESSAGES/logintop10.mo
	install -D logintop10-es.mo $(LOCALE_PATH)/es/LC_MESSAGES/logintop10.mo
	install -D logintop10-ar.mo $(LOCALE_PATH)/ar/LC_MESSAGES/logintop10.mo
	install -D logintop10-de.mo $(LOCALE_PATH)/de/LC_MESSAGES/logintop10.mo

uninstall: clean
	rm -f /usr/local/bin/logintop10
	rm -f /usr/local/man/man1/logintop10.1.gz
	rm -f $(LOCALE_PATH)/nl/LC_MESSAGES/logintop10.mo
	rm -f $(LOCALE_PATH)/fr/LC_MESSAGES/logintop10.mo
	rm -f $(LOCALE_PATH)/es/LC_MESSAGES/logintop10.mo
	rm -f $(LOCALE_PATH)/ar/LC_MESSAGES/logintop10.mo
	rm -f $(LOCALE_PATH)/de/LC_MESSAGES/logintop10.mo

clean:
	rm -f $(OBJS) logintop10 core $(LANGUAGE_FILES) logintop10-$(VERSION).tgz

package: clean
	mkdir logintop10-$(VERSION)
	cp *.c *.h *.1 Makefile *.po readme.txt license.txt logintop10-$(VERSION)
	tar czf logintop10-$(VERSION).tgz logintop10-$(VERSION)
	rm -rf logintop10-$(VERSION)
