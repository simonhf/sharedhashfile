dnl lines starting with "dnl" are comments

PHP_ARG_ENABLE(sharedhashfile, whether to enable sharedhashfile extension, [  --enable-sharedhashfile   Enable sharedhashfile extension])

if test "$PHP_SHAREDHASHFILE" != "no"; then

  dnl this defines the extension
  PHP_NEW_EXTENSION(sharedhashfile, SharedHashFile.php.c shf.c murmurhash3.c, $ext_shared)
  PHP_ADD_INCLUDE(../../src)
  dnl fixme EXTRA_LDFLAGS="$EXTRA_LDFLAGS ../shf.lo ../murmurhash3.lo"
  CFLAGS="$CFLAGS -std=gnu99"

  dnl this is boilerplate to make the extension work on OS X
  case $build_os in
  darwin1*.*.*)
    AC_MSG_CHECKING([whether to compile for recent osx architectures])
    CFLAGS="$CFLAGS -arch i386 -arch x86_64 -mmacosx-version-min=10.5"
    AC_MSG_RESULT([yes])
    ;;
  darwin*)
    AC_MSG_CHECKING([whether to compile for every osx architecture ever])
    CFLAGS="$CFLAGS -arch i386 -arch x86_64 -arch ppc -arch ppc64"
    AC_MSG_RESULT([yes])
    ;;
  esac

fi
