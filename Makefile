.POSIX:

VERSION =0

PREFIX = /usr/local

PKG_CONFIG = pkg-config

PKGS = wayland-client fcft pixman-1
INCS = `$(PKG_CONFIG) --cflags $(PKGS)`
LIBS = `$(PKG_CONFIG) --libs $(PKGS)`

DAMCPPFLAGS = -DVERSION=\"$(VERSION)\" -D_GNU_SOURCE 
DAMCFLAGS   = -pedantic -Wall $(INCS) $(DAMCPPFLAGS) $(CPPFLAGS) $(CFLAGS)
LDLIBS      = $(LIBS)

SRC = dam.o xdg-shell-protocol.o wlr-layer-shell-unstable-v1-protocol.o \
      river-control-unstable-v1-protocol.o river-status-unstable-v1-protocol.o
OBJ = $(SRC:.c=.o)

all: dam

.c.o:
	$(CC) -o $@ -c $(DAMCFLAGS) -c $<

dam.o: wlr-layer-shell-unstable-v1-protocol.h \
       river-control-unstable-v1-protocol.h river-status-unstable-v1-protocol.h

dam: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ) $(LDLIBS)

WAYLAND_PROTOCOLS = `$(PKG_CONFIG) --variable=pkgdatadir wayland-protocols`
WAYLAND_SCANNER   = `$(PKG_CONFIG) --variable=wayland_scanner wayland-scanner`

xdg-shell-protocol.c:
	$(WAYLAND_SCANNER) private-code $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@
xdg-shell-protocol.h:
	$(WAYLAND_SCANNER) client-header $(WAYLAND_PROTOCOLS)/stable/xdg-shell/xdg-shell.xml $@
wlr-layer-shell-unstable-v1-protocol.c:
	$(WAYLAND_SCANNER) private-code wlr-layer-shell-unstable-v1.xml $@
wlr-layer-shell-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) client-header wlr-layer-shell-unstable-v1.xml $@
wlr-layer-shell-unstable-v1-protocol.o: xdg-shell-protocol.o
river-control-unstable-v1-protocol.c:
	$(WAYLAND_SCANNER) private-code river-control-unstable-v1.xml $@
river-control-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) client-header river-control-unstable-v1.xml $@
river-status-unstable-v1-protocol.c:
	$(WAYLAND_SCANNER) private-code river-status-unstable-v1.xml $@
river-status-unstable-v1-protocol.h:
	$(WAYLAND_SCANNER) client-header river-status-unstable-v1.xml $@

clean:
	rm -f dam *.o *-protocol.*

install: all
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f dam $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/dam

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/dam
	
.PHONY: all clean install uninstall
