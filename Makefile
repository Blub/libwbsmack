# Use the same version as the library in
# github/promovicy/smack-util
# But since we not only fixed bugs, but also
# implement a function differently, we use a different
# library name
V_MAJOR = 0
V_MINOR = 2
V ?= 0

AR ?= ar
RANLIB ?= ranlib

CFLAGS = -Wall -Werror -g -O2 -fPIC
LDFLAGS = -fPIC

LIB_STATIC = libarchsmack.a
LIB_SONAME = libarchsmack.so.$(V_MAJOR)
LIB_SHARED = libarchsmack.so.$(V_MAJOR).$(V_MINOR)

LIB_SOURCES = src/getsmack.c \
              src/getsmackuser.c \
              src/setsmack.c \
              src/smackaccess.c \
              src/smackenabled.c
LIB_OBJECTS = $(patsubst %.c,%.o,${LIB_SOURCES})

all: $(LIB_SONAME) $(LIB_STATIC)

$(LIB_SONAME): $(LIB_OBJECTS)
	$(CC) $(LDFLAGS) -shared -Xlinker -soname -Xlinker $@ -o $@ $^

$(LIB_STATIC): $(LIB_OBJECTS)
	$(AR) crs $@ $^
	$(RANLIB) $@

%.o: %.c
ifeq ($(V), 0)
	@echo CC $*.c
	@$(CC) $(CFLAGS) -c -o $*.o $*.c -MMD -MT $@ -MF $*.d
else
	$(CC) $(CFLAGS) -c -o $*.o $*.c -MMD -MT $@ -MF $*.d
endif

clean:
	-rm -f .deps.*.d
	-rm -f $(LIB_SONAME) $(LIB_STATIC)
	-rm -f $(LIB_OBJECTS)

-include src/*.d
