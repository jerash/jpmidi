#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT(jpmidi, 0.3.1, rmouneyres@gmail.com)
AC_CONFIG_AUX_DIR(config)
AC_CANONICAL_TARGET
AC_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE(jpmidi, 0.3.1)
AC_PREFIX_DEFAULT(/usr/local)

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET

# Prepend alternate dependencies paths.
ac_path=$PATH
for X in $ac_with_paths; do
  if test -d $X/bin; then
    ac_path="$X/bin:$ac_path"
  fi
  if test -d $X/include; then
    for Y in qt qt4; do
      if test -d $X/include/$Y; then
        CFLAGS="-I$X/include/$Y $CFLAGS"
        CPPFLAGS="-I$X/include/$Y $CPPFLAGS"
        ac_incpath="$X/include/$Y $ac_incpath"
      fi
    done
    CFLAGS="-I$X/include $CFLAGS"
    CPPFLAGS="-I$X/include $CPPFLAGS"
    ac_incpath="$X/include $ac_incpath"
  fi
  if test -d $X/lib64; then
    LIBS="-L$X/lib64 $LIBS"
    ac_libs="-L$X/lib64 $ac_libs"
  fi
  if test -d $X/lib; then
    LIBS="-L$X/lib $LIBS"
    ac_libs="-L$X/lib $ac_libs"
  fi
done

# Checks for libraries.
AC_CHECK_LIB(jack, main, [ac_jack_lib="yes"], [ac_jack_lib="no"])
if test "x$ac_jack_lib" = "xno"; then
   AC_MSG_ERROR([JACK library not found.])
fi
ac_libs="$ac_libs -ljack"


# Checks for header files.

# Check for JACK headers.
AC_CHECK_HEADER(jack/jack.h, [ac_jack_h="yes"], [ac_jack_h="no"])

# Check for some JACK MIDI headers.
AC_CHECK_HEADER(jack/midiport.h, [ac_jack_midi="yes"], [ac_jack_midi="no"])

if test "x$ac_jack_h" = "xno" -o "x$ac_jack_midi" = "xno"; then
   AC_MSG_ERROR([JACK headers not found (jack.h, midiport.h).])
fi

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AM_PATH_GLIB_2_0(2.0.0,,AC_MSG_ERROR(glib not found or too old),gmodule)

# On some systems, readline depends on termcap or ncurses.  But, the
# MacOSX linker complains bitterly if these libraries are explicitly
# referenced.  
#
# AC_CHECK_LIB() foolishly assumes that checking a library for an entry
# point always returns the same result regardless of any dependent
# libraries specified.  The `unset ac_cv_lib_readline_readline' erases
# the cached result to work around this problem.
READLINE_DEPS=""
HAVE_READLINE=true
AC_CHECK_LIB(readline, readline, [:],
    [unset ac_cv_lib_readline_readline
     AC_CHECK_LIB(readline, readline, [READLINE_DEPS="-ltermcap"],
        [unset ac_cv_lib_readline_readline
	 AC_CHECK_LIB(readline, readline,
	    [READLINE_DEPS="-lncurses"], [HAVE_READLINE=false], "-lncurses")],
	"-ltermcap")])
AC_CHECK_HEADER(readline/chardefs.h, [], [HAVE_READLINE=false])
if test x$HAVE_READLINE = xfalse; then
	AC_MSG_ERROR([readline support not found])
fi
AC_SUBST(READLINE_DEPS)


AM_CONDITIONAL(HAVE_READLINE, $HAVE_READLINE)

AC_OUTPUT(
Makefile
config/Makefile
src/Makefile
)


dnl
dnl Output summary message
dnl

echo
echo $PACKAGE $VERSION :
echo
echo \| Compiler flags.................... : $CFLAGS
echo \| Install prefix.................... : $prefix
echo
