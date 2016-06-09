#!smake
#.SHELL = /bin/sh
PROGDESTDIR = /usr/local/trapfwd



STARTUPROOTDIR = /etc
MAININITDIR =  $(STARTUPROOTDIR)/init.d
KILLSTATEDIRS = rc0.d 
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
		if test ! -f $(STARTUPROOTDIR)/$$i/K19trapfwd; then \
			ln -s $(MAININITDIR)/trapforwarder $(STARTUPROOTDIR)/$$i/K19trapfwd; \
		fi; \
	done;
	@list='$(STARTSTATEDIRS)'; for i in $$list; do \
		if test ! -f $(STARTUPROOTDIR)/$$i/S89trapfwd; then \
			ln -s $(MAININITDIR)/trapforwarder $(STARTUPROOTDIR)/$$i/S89trapfwd; \
		fi; \
	done;




