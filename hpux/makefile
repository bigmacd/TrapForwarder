#!smake
#.SHELL = /bin/sh
PROGDESTDIR = /usr/local/trapfwd



STARTUPROOTDIR = /sbin
MAININITDIR =  $(STARTUPROOTDIR)/init.d
KILLSTATEDIRS = rc1.d 
STARTSTATEDIRS = rc2.d

default: install

install:	
	if [ ! -d $(PROGDESTDIR) ] ; then \
		mkdir $(PROGDESTDIR); \
	fi;
	./shtool install -c -s -m 0755 trapfwd $(PROGDESTDIR)/
	./shtool install -c -m 0666 trapfwd.cfg $(PROGDESTDIR)/
	./shtool install -c -m 0755 trapforwarder $(MAININITDIR)/
	@list='$(KILLSTATEDIRS)'; for i in $$list; do \
		if test ! -f $(STARTUPROOTDIR)/$$i/K787trapfwd; then \
			ln -s ../init.d/trapforwarder $(STARTUPROOTDIR)/$$i/K787trapfwd; \
		fi; \
	done;
	@list='$(STARTSTATEDIRS)'; for i in $$list; do \
		if test ! -f $(STARTUPROOTDIR)/$$i/S901trapfwd; then \
			ln -s ../init.d/trapforwarder $(STARTUPROOTDIR)/$$i/S227trapfwd; \
		fi; \
	done;




