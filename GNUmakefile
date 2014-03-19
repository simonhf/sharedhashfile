# ==================================================================================
# This file is part of the SharedHashFile library.
# Copyright (c) 2014 - Hardy-Francis Enterprises Inc.
#
# Permission is granted to use this software under the terms of the GPL v3.
# Details can be found at: www.gnu.org/licenses
#
# SharedHashFile is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
# FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
# ------------------------------------------------------------------------------
# To release a closed-source product which uses SharedHashFile, commercial
# licenses are available; visit www.sharedhashfile.com for more information.
# ==================================================================================

# See http://www.electric-cloud.com/blog/2009/08/19/makefile-performance-built-in-rules/
# No builtin implicit rules:
.SUFFIXES:
MAKEFLAGS = --no-builtin-rules

# Preserve intermediate files:
.SECONDARY:

CC        = gcc
CFLAGS    = -c -g -W -Waggregate-return -Wall -Werror -Wcast-align -Wcast-qual -Wchar-subscripts -Wcomment -Wformat -Wimplicit -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wparentheses -Wpointer-arith -Wredundant-decls -Wreturn-type -Wshadow -Wstrict-prototypes -Wswitch -Wtrigraphs -Wwrite-strings -O -fno-inline-functions-called-once -fPIC -Wuninitialized -Wunused -march=x86-64 -I. -std=gnu99
ifneq ($(filter debug,$(MAKECMDGOALS)),)
BUILD_TYPE = debug
CFLAGS    += -DSHF_DEBUG_VERSION
else
BUILD_TYPE = release
CFLAGS    += -O3
endif
DEPS       = $(wildcard *.h)
PROD_SRCS  =                              $(filter-out test%,$(wildcard *.c))
PROD_OBJS  = $(patsubst %,$(BUILD_TYPE)/%,$(filter-out test%,$(patsubst %.c,%.o,$(PROD_SRCS))))
TEST_SRCS  =                              $(filter     test%,$(wildcard *.c))
TEST_OBJS  = $(patsubst %,$(BUILD_TYPE)/%,$(filter     test%,$(patsubst %.c,%.o,$(TEST_SRCS))))
TEST_EXES  = $(patsubst %,$(BUILD_TYPE)/%,$(filter     test%,$(patsubst %.c,%.t,$(TEST_SRCS))))

ifneq ($(filter clean,$(MAKECMDGOALS)),)
else
DUMMY := $(shell mkdir $(BUILD_TYPE) 2>&1)
$(info make: variable: PROD_SRCS=$(PROD_SRCS))
$(info make: variable: PROD_OBJS=$(PROD_OBJS))
$(info make: variable: TEST_SRCS=$(TEST_SRCS))
$(info make: variable: TEST_OBJS=$(TEST_OBJS))
$(info make: variable: TEST_EXES=$(TEST_EXES))
endif

$(BUILD_TYPE)/%.o: %.c $(DEPS)
	@echo make: compling: $@
	@$(CC) -o $@ $< $(CFLAGS)

$(BUILD_TYPE)/%.t: $(BUILD_TYPE)/%.o $(PROD_OBJS)
	@echo make: linking: $@
	@gcc -o $@ $^
	@echo make: running: $@
	@./$@

all: $(TEST_EXES)
	@echo make: built and tested $(BUILD_TYPE) version

debug: all

.PHONY: all clean debug

clean:
	rm -rf release debug

