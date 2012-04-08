# Use the same version as the library in
# github/promovicy/smack-util
# But since we not only fixed bugs, but also
# implement a function differently, we use a different
# library name
V_MAJOR = 0
V_MINOR = 2
V ?= 0

PREFIX := /usr
PAMPREFIX := /
DESTDIR :=

STATIC := 0

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

PAM_SMACK = pam_wbsmack.so
PAM_SMACKSRC = pam/pam_wbsmack.c
PAM_SMACKOBJ = $(patsubst %.c,%.o,${PAM_SMACKSRC})

UCHSMACK = uchsmack
UCHSMACKSRC = src/uchsmack.c
UCHSMACKOBJ = $(patsubst %.c,%.o,${UCHSMACKSRC})

all: $(LIB_SHARED) $(LIB_STATIC) $(PAM_SMACK) $(UCHSMACK)

$(LIB_SHARED): $(LIB_OBJECTS)
	$(CC) $(LDFLAGS) -shared -Xlinker -soname -Xlinker $(LIB_SONAME) -o $@ $^

$(LIB_STATIC): $(LIB_OBJECTS)
	$(AR) crs $@ $^
	$(RANLIB) $@

$(PAM_SMACK): $(PAM_SMACKOBJ) $(LIB_SHARED)
	$(CC) $(LDFLAGS) -shared -lpam -Xlinker -soname -Xlinker $(PAM_SMACK) -o $@ $(PAM_SMACKOBJ) -L. -lwbsmack

$(UCHSMACK): $(UCHSMACKOBJ) $(LIB_SHARED)
ifeq ($(STATIC), 1)
	$(CC) $(LDFLAGS) -lcap -static -o $@ $(UCHSMACKOBJ) libwbsmack.a
else
	$(CC) $(LDFLAGS) -lcap -o $@ $(UCHSMACKOBJ) -L. -lwbsmack
endif

%.o: %.c
ifeq ($(V), 0)
	@echo CC $*.c
	@$(CC) $(CFLAGS) -c -o $*.o $*.c -MMD -MT $@ -MF $*.d
else
	$(CC) $(CFLAGS) -c -o $*.o $*.c -MMD -MT $@ -MF $*.d
endif

install: all
	install -d -m755               $(DESTDIR)$(PREFIX)/lib
	install    -m755 $(LIB_STATIC) $(DESTDIR)$(PREFIX)/lib/
	install    -m755 $(LIB_SHARED) $(DESTDIR)$(PREFIX)/lib/
	ln -sf $(LIB_SHARED) $(DESTDIR)$(PREFIX)/lib/$(LIB_SONAME)
	ln -sf $(LIB_SONAME) $(DESTDIR)$(PREFIX)/lib/$(LIB_SO)
	install -d -m755               $(DESTDIR)$(PREFIX)/include
	install    -m644 $(LIB_HEADER) $(DESTDIR)$(PREFIX)/include/
	install -d -m755               $(DESTDIR)$(PAMPREFIX)/lib/security
	install    -m755 $(PAM_SMACK)  $(DESTDIR)$(PAMPREFIX)/lib/security/
	install -d -m755               $(DESTDIR)$(PREFIX)/bin
	install    -m755 $(UCHSMACK)   $(DESTDIR)$(PREFIX)/bin/

clean:
	-rm -f src/*.d pam/*.d
	-rm -f $(LIB_SHARED) $(LIB_STATIC) $(PAM_SMACK) $(UCHSMACK)
	-rm -f $(LIB_OBJECTS) $(UCHSMACKOBJ) $(PAM_SMACKOBJ)

-include src/*.d
-include pam/*.d
