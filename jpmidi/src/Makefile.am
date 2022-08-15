AUTOMAKE_OPTIONS = foreign

CFLAGS = -Wall 	$(GLIB_CFLAGS) -g
LIBS = $(GLIB_LIBS) -ljack -lreadline $(READLINE_DEPS)
LDFLAGS = -g

include_HEADERS =  \
	elements.h \
	except.h \
	md.h \
	midi.h \
	jpmidi.h \
	main.h \
	jackclient.h \
	cmdline.h \
	dump.h \
	commands.h \
	tcpserver.h

bin_PROGRAMS = jpmidi
jpmidi_SOURCES =  \
	elements.c \
	except.c \
	mdutil.c \
	midiread.c \
	jpmidi.c \
	main.c \
	jackclient.c \
	cmdline.c \
	dump.c \
	commands.c \
	tcpserver.c

