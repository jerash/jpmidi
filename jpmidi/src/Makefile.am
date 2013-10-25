AUTOMAKE_OPTIONS = foreign

CFLAGS = -Wall 	$(GLIB_CFLAGS) -g
LDFLAGS = $(GLIB_LIBS) -ljack -lreadline $(READLINE_DEPS) -g

include_HEADERS =  \
	elements.h \
	except.h \
	md.h \
	midi.h \
	jpmidi.h \
	main.h \
	jackclient.h \
	cmdline.h \
	dump.h

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
	dump.c

