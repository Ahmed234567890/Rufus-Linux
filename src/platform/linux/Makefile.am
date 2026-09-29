# Linux platform library
noinst_LIBRARIES = libplatform_linux.a

libplatform_linux_a_SOURCES = \
    platform_linux.c \
    device_linux.c \
    block_linux.c

libplatform_linux_a_CFLAGS = \
    -I$(top_srcdir)/src/core \
    -I$(top_srcdir)/src \
    $(LINUX_NATIVE_CFLAGS) \
    $(AM_CFLAGS)