# Use the same version as the library in
# github/promovicy/smack-util
# But since we not only fixed bugs, but also
# implement a function differently, we use a different
# library name
V_MAJOR = 0
V_MINOR = 2
V ?= 0

PREFIX := /usr
DESTDIR :=

AR ?= ar
RANLIB ?= ranlib

CFLAGS = -Wall -Werror -g -O2 -fPIC
LDFLAGS = -fPIC

LIBNAME = wbsmack

LIB_STATIC = lib$(LIBNAME).a
LIB_SO     = lib$(LIBNAME).so
LIB_SONAME = lib$(LIBNAME).so.$(V_MAJOR)
LIB_SHARED = lib$(LIBNAME).so.$(V_MAJOR).$(V_MINOR)
LIB_HEADER = src/smack.h

LIB_SOURCES = src/getsmack.c \
              src/getsmackuser.c \
              src/setsmack.c \
              src/smackaccess.c \
              src/smackenabled.c
LIB_OBJECTS = $(patsubst %.c,%.o,${LIB_SOURCES})

all: $(LIB_SHARED) $(LIB_STATIC)

$(LIB_SHARED): $(LIB_OBJECTS)
	$(CC) $(LDFLAGS) -shared -Xlinker -soname -Xlinker $(LIB_SONAME) -o $@ $^

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

install:
	install -d -m755               $(DESTDIR)$(PREFIX)/lib
	install    -m644 $(LIB_STATIC) $(DESTDIR)$(PREFIX)/lib/
	install    -m644 $(LIB_SHARED) $(DESTDIR)$(PREFIX)/lib/
	ln -sf $(LIB_SHARED) $(DESTDIR)$(PREFIX)/lib/$(LIB_SONAME)
	ln -sf $(LIB_SONAME) $(DESTDIR)$(PREFIX)/lib/$(LIB_SO)
	install -d -m755               $(DESTDIR)$(PREFIX)/include
	install    -m644 $(LIB_HEADER) $(DESTDIR)$(PREFIX)/include/

clean:
	-rm -f src/*.d
	-rm -f $(LIB_SONAME) $(LIB_STATIC)
	-rm -f $(LIB_OBJECTS)

-include src/*.d
