# Generated automatically from Makefile.in by configure.
# set up some stupid things
SHELL = /bin/sh
.SUFFIXES:
.SUFFIXES: .c .o

# set the MAKE variable if necessary


all: 
	( cd plug-ins ; $(MAKE) )
	( cd app ; $(MAKE) )

depend:
	( cd plug-ins ; $(MAKE) depend )
	( cd app ; $(MAKE) depend )

clean:
	rm -f config.cache
	rm -f config.log
	rm -f config.status
	rm -f `find . -name '*~' -print`
	( cd plug-ins ; $(MAKE) clean )
	( cd app ; $(MAKE) clean )
