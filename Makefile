#
# Copyright (C) 2014-2021 Canonical, Ltd.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
VERSION=0.00.01

CFLAGS += -Wall -Wextra -DVERSION='"$(VERSION)"' -O2

#
# Pedantic flags
#
ifeq ($(PEDANTIC),1)
CFLAGS += -Wabi -Wcast-qual -Wfloat-equal -Wmissing-declarations \
	-Wmissing-format-attribute -Wno-long-long -Wpacked \
	-Wredundant-decls -Wshadow -Wno-missing-field-initializers \
	-Wno-missing-braces -Wno-sign-compare -Wno-multichar
endif

BINDIR=/usr/bin
MANDIR=/usr/share/man/man1

tiny-smart: tiny-smart.o
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

tiny-smart.1.gz: tiny-smart.1
	gzip -c $< > $@

dist:
	rm -rf tiny-smart-$(VERSION)
	mkdir tiny-smart-$(VERSION)
	cp -rp Makefile tiny-smart.c \
		tiny-smart-$(VERSION)
	tar -zcf tiny-smart-$(VERSION).tar.gz tiny-smart-$(VERSION)
	rm -rf tiny-smart-$(VERSION)

clean:
	rm -f tiny-smart tiny-smart.o
	rm -f tiny-smart-$(VERSION).tar.gz

install: tiny-smart tiny-smart.1.gz
	mkdir -p ${DESTDIR}${BINDIR}
	cp tiny-smart ${DESTDIR}${BINDIR}
