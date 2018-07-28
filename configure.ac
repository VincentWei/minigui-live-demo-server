dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.60)
AC_INIT(mgremoteserver,0.8.0)
AC_CONFIG_SRCDIR(src/websocket.c)

dnl ========================================================================
dnl Need for cross-compiling
AC_CANONICAL_SYSTEM

dnl ========================================================================
dnl Init automake
AM_INIT_AUTOMAKE

AC_CONFIG_HEADER(src/config.h)

dnl ========================================================================
dnl Checks for programs.
AC_PROG_MAKE_SET
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LD
AC_PROG_RANLIB
AC_PROG_LIBTOOL

dnl ========================================================================
dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t])

dnl ========================================================================
dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_HEADER_TIME
AC_CHECK_HEADERS([arpa/inet.h fcntl.h limits.h netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/socket.h sys/time.h unistd.h])

dnl ========================================================================
dnl Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([gettimeofday memmove memset mkfifo select socket strcasecmp strchr strerror strpbrk strstr])

devel_mode="no"
AC_ARG_ENABLE(develmode,
[   --enable-develmode      developer mode <default=no>],
devel_mode=$enableval)

openssl="no"
AC_ARG_WITH(openssl,
[   --enable-openssl,       build with OpenSSL support <default=no>],
openssl="$withval")

unixsocket="yes"
AC_ARG_WITH(unixsocket,
[   --enable-unixsocket,    use UNIX socket instead of FIFO <default=yes>],
unixsocket="$withval")

if test "x$devel_mode" = "xyes"; then
    CPPFLAGS="$CPPFLAGS -DDEBUG -D_DEBUG"
    if test "$ac_cv_prog_gcc" = "yes"; then
        CPPFLAGS="$CPPFLAGS -Wall -Werror"
    fi
fi

AC_CHECK_LIB([png], [png_check_sig], DEP_LIBS="$DEP_LIBS -lpng", [AC_MSG_ERROR([png library missing])])

# Build with OpenSSL
if test "$openssl" = 'yes'; then
    AC_CHECK_LIB([ssl], [SSL_library_init],
        DEP_LIBS="$DEP_LIBS -lssl",
        [AC_MSG_ERROR([ssl library missing])])
    AC_CHECK_LIB([crypto], [CRYPTO_num_locks],
        DEP_LIBS="$DEP_LIBS -lcrypto",
        [AC_MSG_ERROR([crypto library missing])])
fi

# Use UNIX socket 
if test "$unixsocket" = 'yes'; then
    AC_DEFINE(UNIXSOCKET, 1,
                [Define if use UNIX socket instead of named pipe.])
fi

if test "$ac_cv_prog_gcc" = "yes"; then
    CFLAGS="$CFLAGS -Wstrict-prototypes -pipe"
fi

dnl ========================================================================
dnl Write Output

AC_SUBST(DEP_LIBS)

AC_OUTPUT(
Makefile
m4/Makefile
src/Makefile
producer/Makefile
)
