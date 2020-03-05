# Makefile to build the SDL 2

CC      = ppc-morphos-gcc-9 -noixemul
INCLUDE = -I../../../../include -I../../../../sdk
CFLAGS  = -mcpu=750 -mtune=7450 -O2 $(INCLUDE) -Wall -Wno-pointer-sign -fno-strict-aliasing -D_LARGEFILE64_SOURCE
AR	= ar
RANLIB	= ranlib

ECHE = echo -e
BOLD = \033[1m
NRML = \033[22m

COMPILING = @$(ECHE) "compiling $(BOLD)$@$(NRML)..."
LINKING = @$(ECHE) "linking $(BOLD)$@$(NRML)..."
STRIPPING = @$(ECHE) "stripping $(BOLD)$@$(NRML)..."
ARCHIVING = @$(ECHE) "archiving $(BOLD)$@$(NRML)..."

all: lib/libSDL.a lib/libb32/libSDL.a lib/libb32/libSDL-nc.a

lib: all

install: all
install-iso: all

sdl-startup.o: sdl-startup.c sdl-stubs.c
	$(COMPILING)
	@$(CC) $(CFLAGS) -o $@ -c sdl-startup.c

sdl-startup-brel.o: sdl-startup.c sdl-stubs.c
	$(COMPILING)
	@$(CC) $(CFLAGS) -mresident32 -o $@ -c sdl-startup.c

sdl-startup-brel-nc.o: sdl-startup.c sdl-stubs.c
	$(COMPILING)
	@$(CC) $(CFLAGS) -mresident32 -o $@ -c sdl-startup.c -D__NO_SDL_CONSTRUCTORS

lib/libSDL.a: sdl-startup.o ../../../../sdk/fd/sdl2_lib.fd
	$(ARCHIVING)
	@mkdir -p lib
	@-rm -f $@
	@cvinclude.pl --fd ../../../../sdk/fd/sdl2_lib.fd --clib ../../../../sdk/clib/sdl2_protos.h --gluelib=$@
	@$(AR) cru $@ sdl-startup.o
	@ppc-morphos-ranlib $@

lib/libb32/libSDL.a: sdl-startup-brel.o   ../../../../sdk/fd/sdl2_lib.fd
	$(ARCHIVING)
	@mkdir -p lib/libb32
	@-rm -f $@
	@cvinclude.pl --fd  ../../../../sdk/fd/sdl2_lib.fd --clib ../../../../sdk/clib/sdl2_protos.h --brelgluelib=$@
	@$(AR) cru $@ sdl-startup-brel.o
	@ppc-morphos-ranlib $@

lib/libb32/libSDL-nc.a: sdl-startup-brel-nc.o   ../../../../sdk/fd/sdl2_lib.fd
	$(ARCHIVING)
	@mkdir -p lib/libb32
	@-rm -f $@
	@cvinclude.pl --fd  ../../../../sdk/fd/sdl2_lib.fd --clib ../../../../sdk/clib/sdl2_protos.h --brelgluelib=$@
	@$(AR) cru $@ sdl-startup-brel-nc.o
	@ppc-morphos-ranlib $@

install-iso:
	@echo "Nothing to install-iso here."

clean:
	@-rm -rf lib *.s *.o

dump:
	objdump --disassemble-all --reloc lib/libSDL.a >lib/libSDL.s
