V_MAJOR = 0
V_MINOR = 2
V_PATCHLEVEL = 1
V ?= 0
NODOC ?= 0

PREFIX := /usr
ETCDIR := /etc
MANDIR := /usr/share/man
PAMPREFIX := /

DESTDIR :=

STATIC := 0

AR ?= ar
RANLIB ?= ranlib

CFLAGS = -Wall -Werror -fPIC
ifeq ($(DEBUG), 1)
	CFLAGS += -g -O0
else
	CFLAGS += -O2
endif
LDFLAGS = -fPIC

LIBNAME = wbsmack

LIB_STATIC = lib$(LIBNAME).a
LIB_ACCESS = lib$(LIBNAME)_access.a
LIB_SO     = lib$(LIBNAME).so
LIB_SONAME = lib$(LIBNAME).so.$(V_MAJOR)
LIB_SHARED = lib$(LIBNAME).so.$(V_MAJOR).$(V_MINOR)
LIB_PATCH  = lib$(LIBNAME).so.$(V_MAJOR).$(V_MINOR).$(V_PATCHLEVEL)
LIB_HEADER = src/smack.h

LIB_SOURCES = src/getsmack.c \
              src/getsmackuser.c \
              src/opensmackentry.c \
              src/setsmack.c \
              src/smackenabled.c
LIB_SOURCES_S = \
              src/smackaccess.c \
              src/smacktransition.c
LIB_OBJECTS = $(patsubst %.c,%.o,${LIB_SOURCES})
LIB_OBJECTS_S = $(patsubst %.c,%.o,${LIB_SOURCES_S})

PAM_SMACK = pam_wbsmack.so
PAM_SMACKSRC = pam/pam_wbsmack.c
PAM_SMACKOBJ = $(patsubst %.c,%.o,${PAM_SMACKSRC})

UCHSMACK = uchsmack
UCHSMACKSRC = src/uchsmack.c
UCHSMACKOBJ = $(patsubst %.c,%.o,${UCHSMACKSRC})

USMACKEXEC = usmackexec
USMACKEXECSRC = src/usmackexec.c
USMACKEXECOBJ = $(patsubst %.c,%.o,${USMACKEXECSRC})

UNROOT = unroot
UNROOTSRC = src/unroot.c
UNROOTOBJ = $(patsubst %.c,%.o,${UNROOTSRC})

BINARIES := $(UCHSMACK) $(USMACKEXEC) $(UNROOT)
PAMLIBS := $(PAM_SMACK)
LIBRAREIS := $(LIB_SHARED) $(LIB_STATIC) $(LIB_ACCESS)

INSTALL := doc header $(BINARIES) $(PAM_SMACK)
SUBST_INSTALL = $(patsubst %,install-%,${INSTALL})

all: $(LIBRARIES) $(PAMLIBS) $(BINARIES)

$(LIB_SHARED): $(LIB_OBJECTS)
	$(CC) $(LDFLAGS) -shared -Xlinker -soname -Xlinker $(LIB_SONAME) -o $@ $^

$(LIB_STATIC): $(LIB_OBJECTS) $(LIB_OBJECTS_S)
	$(AR) crs $@ $^
	$(RANLIB) $@

$(LIB_ACCESS): $(LIB_OBJECTS_S)
	$(AR) crs $@ $^
	$(RANLIB) $@

$(PAM_SMACK): $(PAM_SMACKOBJ) $(LIB_SHARED) $(LIB_ACCESS)
	$(CC) $(LDFLAGS) -shared -lpam -Xlinker -soname -Xlinker $(PAM_SMACK) -o $@ $(PAM_SMACKOBJ) $(LIB_ACCESS) $(LIB_SHARED)

$(UCHSMACK): $(UCHSMACKOBJ) $(LIB_SHARED) $(LIB_ACCESS) $(LIB_STATIC)
ifeq ($(STATIC), 1)
	$(CC) $(LDFLAGS) -lcap -static -o $@ $(UCHSMACKOBJ) $(LIB_STATIC)
else
	$(CC) $(LDFLAGS) -lcap -o $@ $(UCHSMACKOBJ) $(LIB_STATIC)
endif

$(USMACKEXEC): $(USMACKEXECOBJ) $(LIB_SHARED) $(LIB_ACCESS) $(LIB_STATIC)
ifeq ($(STATIC), 1)
	$(CC) $(LDFLAGS) -lcap -static -o $@ $(USMACKEXECOBJ) $(LIB_STATIC)
else
	$(CC) $(LDFLAGS) -lcap -o $@ $(USMACKEXECOBJ) $(LIB_STATIC)
endif

$(UNROOT): $(UNROOTOBJ)
ifeq ($(STATIC), 1)
	$(CC) $(LDFLAGS) -lcap -static -o $@ $(UNROOTOBJ)
else
	$(CC) $(LDFLAGS) -lcap -o $@ $(UNROOTOBJ)
endif

%.o: %.c
ifeq ($(V), 0)
	@echo CC $*.c
	@$(CC) $(CFLAGS) -c -o $*.o $*.c -MMD -MT $@ -MF $*.d
else
	$(CC) $(CFLAGS) -c -o $*.o $*.c -MMD -MT $@ -MF $*.d
endif

install: all $(SUBST_INSTALL)
	install -d -m755               $(DESTDIR)$(PREFIX)/lib
	install    -m755 $(LIB_STATIC) $(DESTDIR)$(PREFIX)/lib/
	install    -m755 $(LIB_ACCESS) $(DESTDIR)$(PREFIX)/lib/
	install    -m755 $(LIB_SHARED) $(DESTDIR)$(PREFIX)/lib/$(LIB_PATCH)
	ln -sf $(LIB_PATCH)  $(DESTDIR)$(PREFIX)/lib/$(LIB_SHARED)
	ln -sf $(LIB_SHARED) $(DESTDIR)$(PREFIX)/lib/$(LIB_SONAME)
	ln -sf $(LIB_SONAME) $(DESTDIR)$(PREFIX)/lib/$(LIB_SO)
	install -d -m755               $(DESTDIR)$(ETCDIR)/smack/transition.d
install-header:
	install -d -m755               $(DESTDIR)$(PREFIX)/include
	install    -m644 $(LIB_HEADER) $(DESTDIR)$(PREFIX)/include/
install-bindir:
	install -d -m755               $(DESTDIR)$(PREFIX)/bin
install-$(PAM_SMACK):
	install -d -m755               $(DESTDIR)$(PAMPREFIX)/lib/security
	install    -m755 $(PAM_SMACK)  $(DESTDIR)$(PAMPREFIX)/lib/security/
install-$(UCHSMACK): install-bindir
	install    -m755 $(UCHSMACK)   $(DESTDIR)$(PREFIX)/bin/
install-$(USMACKEXEC): install-bindir
	install    -m755 $(USMACKEXEC) $(DESTDIR)$(PREFIX)/bin/
install-$(UNROOT): install-bindir
	install    -m755 $(UNROOT)     $(DESTDIR)$(PREFIX)/bin/
install-doc:
ifneq ($(NODOC), 1)
	@echo Installing documentation
	install -d -m755                      $(DESTDIR)$(MANDIR)/man1
	install    -m644 doc/uchsmack.1       $(DESTDIR)$(MANDIR)/man1/
	install    -m644 doc/usmackexec.1     $(DESTDIR)$(MANDIR)/man1/
	install -d -m755                      $(DESTDIR)$(MANDIR)/man8
	install    -m644 doc/unroot.8         $(DESTDIR)$(MANDIR)/man8/
	install -d -m755                      $(DESTDIR)$(MANDIR)/man3
	install    -m644 doc/getsmack.3       $(DESTDIR)$(MANDIR)/man3/
	install    -m644 doc/setsmack.3       $(DESTDIR)$(MANDIR)/man3/
	install    -m644 doc/opensmackentry.3 $(DESTDIR)$(MANDIR)/man3/
	install    -m644 doc/smackaccess.3    $(DESTDIR)$(MANDIR)/man3/
	ln -sf smackaccess.3 $(DESTDIR)$(MANDIR)/man3/smackchecktrans.3
	install    -m644 doc/smackenabled.3   $(DESTDIR)$(MANDIR)/man3/
	ln -sf opensmackentry.3 $(DESTDIR)$(MANDIR)/man3/smackentryget.3
	ln -sf opensmackentry.3 $(DESTDIR)$(MANDIR)/man3/smackentrycontains.3
	ln -sf opensmackentry.3 $(DESTDIR)$(MANDIR)/man3/closesmackentry.3
endif

clean:
	-rm -f src/*.d pam/*.d
	-rm -f $(LIB_SHARED) $(LIB_STATIC) $(LIB_ACCESS)
	-rm -f $(PAM_SMACK) $(UCHSMACK) $(USMACKEXEC) $(UNROOT)
	-rm -f pam/*.o src/*.o

-include src/*.d
-include pam/*.d
