AUTOMAKE_OPTIONS = subdir-objects
SUBDIRS = lib
bin_PROGRAMS = hlpt

hlpt_SOURCES = ./src/main.c
hlpt_SOURCES += ./src/aa_tree.c
hlpt_SOURCES += ./src/bitonic_sort.c
hlpt_SOURCES += ./src/command/dbin_command.c
hlpt_SOURCES += ./src/command/hex.c
hlpt_SOURCES += ./src/search/dbin_random.c
hlpt_SOURCES += ./src/search/hlp_random.c
hlpt_SOURCES += ./src/solver/dbin_solve.c
hlpt_SOURCES += ./src/solver/hlp_solve.c
hlpt_SOURCES += ./src/vector_tools.c
hlpt_SOURCES += ./src/redstone.c

EXTRA_DIST = m4/gnulib-cache.m4

ACLOCAL_AMFLAGS = -I m4
CCAS = nasm
AM_CFLAGS = -march=native
AM_CPPFLAGS = -I$(top_builddir)/lib -I$(top_srcdir)/lib
LDADD = lib/libgnu.a

# AM_LDFLAGS = -z noexecstack 
# AM_CCASFLAGS = -felf64

debug: AM_CFLAGS += -DDEBUG -g
debug: AM_CCASFLAGS += -dDEBUG -g

src/asm_parts.o: src/asm_parts.s
	@$(CCAS) $(AM_CCASFLAGS) $< -o $@

