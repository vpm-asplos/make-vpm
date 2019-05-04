# GNU -*-Makefile-*- to build GNU make on Amiga
#
# Amiga overrides for use with Basic.mk.
#
# Copyright (C) 2017 Free Software Foundation, Inc.
# This file is part of GNU Make.
#
# GNU Make is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License as published by the Free Software
# Foundation; either version 3 of the License, or (at your option) any later
# version.
#
# GNU Make is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
# details.
#
# You should have received a copy of the GNU General Public License along with
# this program.  If not, see <http://www.gnu.org/licenses/>.

CC = sc
LD = $(CC) Link

RM = delete
MKDIR = makedir
CP = copy
CP.cmd = $(CP) $< To $@

CPPFLAGS =
CFLAGS =
LDFLAGS =

prog_SOURCES += $(alloca_SOURCES) $(loadavg_SOURCES) $(glob_SOURCES) $(amiga_SOURCES)

extra_CPPFLAGS = IDir $(OUTDIR)src IDir $(SRCDIR)/src IDir $(SRCDIR)/glob

C_SOURCE =
OUTPUT_OPTION =
LDFLAGS = From LIB:cres.o
LDLIBS = Lib LIB:sc.lib LIB:amiga.lib
LINK_OUTPUT = To $@

$(OUTDIR)src/config.h: $(SRCDIR)/src/config.ami
	$(CP.cmd)
