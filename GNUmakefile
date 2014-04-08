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

CC         = gcc
CXXFLAGS   = -c -g -W -Waggregate-return -Wall -Werror -Wcast-align -Wcast-qual -Wchar-subscripts -Wcomment -Wformat -Wmissing-declarations -Wparentheses -Wpointer-arith -Wredundant-decls -Wreturn-type -Wshadow -Wswitch -Wtrigraphs -Wwrite-strings -O -fno-inline-functions-called-once -fPIC -Wuninitialized -Wunused -march=x86-64 -I.
CFLAGS     = -Wimplicit -Wmissing-prototypes -Wnested-externs -Wstrict-prototypes -std=gnu99
ifneq ($(filter debug,$(MAKECMDGOALS)),)
BUILD_TYPE = debug
CXXFLAGS  += -DSHF_DEBUG_VERSION
else
BUILD_TYPE = release
CXXFLAGS  += -O3
endif
DEPS_H        = $(wildcard *.h)
DEPS_HPP      = $(wildcard *.hpp)
PROD_SRCS_C   =                              $(filter-out test%,$(wildcard *.c))
PROD_OBJS_C   = $(patsubst %,$(BUILD_TYPE)/%,$(filter-out test%,$(patsubst %.c,%.o,$(PROD_SRCS_C))))
PROD_SRCS_CPP =                              $(filter-out test%,$(wildcard *.cpp))
PROD_OBJS_CPP = $(patsubst %,$(BUILD_TYPE)/%,$(filter-out test%,$(patsubst %.cpp,%.o,$(PROD_SRCS_CPP))))
TEST_SRCS_C   =                              $(filter     test%,$(wildcard *.c))
TEST_OBJS_C   = $(patsubst %,$(BUILD_TYPE)/%,$(filter     test%,$(patsubst %.c,%.o,$(TEST_SRCS))))
TEST_SRCS_CPP =                              $(filter     test%,$(wildcard *.cpp))
TEST_OBJS_CPP = $(patsubst %,$(BUILD_TYPE)/%,$(filter     test%,$(patsubst %.cpp,%.o,$(TEST_SRCS))))
TEST_EXES     = $(patsubst %,$(BUILD_TYPE)/%,$(filter     test%,$(patsubst %.c,%.t,$(TEST_SRCS_C)))) \
                $(patsubst %,$(BUILD_TYPE)/%,$(filter     test%,$(patsubst %.cpp,%.t,$(TEST_SRCS_CPP))))

ifneq ($(filter clean,$(MAKECMDGOALS)),)
else
DUMMY := $(shell mkdir $(BUILD_TYPE) 2>&1)
$(info make: variable: DEPS_H=$(DEPS_H))
$(info make: variable: DEPS_HPP=$(DEPS_HPP))
$(info make: variable: PROD_SRCS_C=$(PROD_SRCS_C))
$(info make: variable: PROD_OBJS_C=$(PROD_OBJS_C))
$(info make: variable: PROD_SRCS_CPP=$(PROD_SRCS_CPP))
$(info make: variable: PROD_OBJS_CPP=$(PROD_OBJS_CPP))
$(info make: variable: TEST_SRCS_C=$(TEST_SRCS_C))
$(info make: variable: TEST_OBJS_C=$(TEST_OBJS_C))
$(info make: variable: TEST_SRCS_CPP=$(TEST_SRCS_CPP))
$(info make: variable: TEST_OBJS_CPP=$(TEST_OBJS_CPP))
$(info make: variable: TEST_EXES=$(TEST_EXES))
endif

$(BUILD_TYPE)/%.o: %.c $(DEPS_H)
	@echo make: compiling: $@
	@$(CC) -o $@ $< $(CXXFLAGS) $(CFLAGS)

$(BUILD_TYPE)/%.o: %.cpp $(DEPS_H) $(DEPS_HPP)
	@echo make: compiling: $@
	@$(CC) -o $@ $< $(CXXFLAGS)

$(BUILD_TYPE)/%.t: $(BUILD_TYPE)/%.o $(PROD_OBJS_C) $(PROD_OBJS_CPP)
	@echo make: linking: $@
	@g++ -o $@ $^
	@echo make: running: $@
	@./$@

all: $(TEST_EXES)
	@echo make: built and tested $(BUILD_TYPE) version

debug: all

.PHONY: all clean debug

clean:
	rm -rf release debug

