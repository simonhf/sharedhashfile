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

CC              = gcc
CXXFLAGS        = -c -g -W -Waggregate-return -Wall -Werror -Wcast-align -Wcast-qual -Wchar-subscripts
CXXFLAGS       += -Wcomment -Wformat -Wmissing-declarations -Wparentheses -Wpointer-arith -Wredundant-decls
CXXFLAGS       +=  -Wreturn-type -Wshadow -Wswitch -Wtrigraphs -Wwrite-strings -O
CXXFLAGS       += -fno-inline-functions-called-once -fPIC -Wuninitialized -Wunused -march=x86-64 -I. -Isrc
CFLAGS          = -Wimplicit -Wmissing-prototypes -Wnested-externs -Wstrict-prototypes -std=gnu99
ifneq ($(filter debug,$(MAKECMDGOALS)),)
BUILD_TYPE      = debug
CXXFLAGS       += -DSHF_DEBUG_VERSION
else
BUILD_TYPE      = release
CXXFLAGS       += -O3
endif
DEPS_H          = $(wildcard ./src/*.h)
DEPS_HPP        = $(wildcard ./src/*.hpp)
PROD_SRCS_C     =                                    $(filter-out ./src/test%,$(wildcard ./src/*.c))
PROD_SRCS_C     =                                    $(filter-out ./src/main%,$(wildcard ./src/*.c))
PROD_OBJS_C     = $(patsubst ./src/%,$(BUILD_TYPE)/%,$(filter-out ./src/test%,$(patsubst %.c,%.o,$(PROD_SRCS_C))))
PROD_SRCS_CPP   =                                    $(filter-out ./src/test%,$(wildcard ./src/*.cpp))
PROD_OBJS_CPP   = $(patsubst ./src/%,$(BUILD_TYPE)/%,$(filter-out ./src/test%,$(patsubst %.cpp,%.o,$(PROD_SRCS_CPP))))
MAIN_SRCS_C     =                                    $(filter     ./src/main%,$(wildcard ./src/*.c))
MAIN_OBJS_C     = $(patsubst ./src/%,$(BUILD_TYPE)/%,$(filter     ./src/main%,$(patsubst %.c,%.o,$(MAIN_SRCS_C))))
MAIN_EXES       = $(patsubst       %,$(BUILD_TYPE)/%,$(filter               %,$(patsubst ./src/main.%.c,%,$(MAIN_SRCS_C))))
TEST_SRCS_C     =                                    $(filter     ./src/test%,$(wildcard ./src/*.c))
TEST_OBJS_C     = $(patsubst ./src/%,$(BUILD_TYPE)/%,$(filter     ./src/test%,$(patsubst %.c,%.o,$(TEST_SRCS_C))))
TEST_SRCS_CPP   =                                    $(filter     ./src/test%,$(wildcard ./src/*.cpp))
TEST_OBJS_CPP   = $(patsubst ./src/%,$(BUILD_TYPE)/%,$(filter     ./src/test%,$(patsubst %.cpp,%.o,$(TEST_SRCS_CPP))))
TEST_EXES       = $(patsubst ./src/%,$(BUILD_TYPE)/%,$(filter     ./src/test%,$(patsubst %.c,%.t,$(TEST_SRCS_C)))) \
                  $(patsubst ./src/%,$(BUILD_TYPE)/%,$(filter     ./src/test%,$(patsubst %.cpp,%.t,$(TEST_SRCS_CPP))))
ifdef SHF_SKIP_TESTS
TEST_EXE_SKIP   = "without testing"
else
TEST_EXE_SKIP   = "and tested"
endif

ifneq ($(filter clean,$(MAKECMDGOALS)),)
else
DUMMY := $(shell mkdir $(BUILD_TYPE) 2>&1)
ifdef SHF_DEBUG_MAKE
$(info make: variable: DEPS_H=$(DEPS_H))
$(info make: variable: DEPS_HPP=$(DEPS_HPP))
$(info make: variable: PROD_SRCS_C=$(PROD_SRCS_C))
$(info make: variable: PROD_OBJS_C=$(PROD_OBJS_C))
$(info make: variable: PROD_SRCS_CPP=$(PROD_SRCS_CPP))
$(info make: variable: PROD_OBJS_CPP=$(PROD_OBJS_CPP))
$(info make: variable: MAIN_SRCS_C=$(MAIN_SRCS_C))
$(info make: variable: MAIN_OBJS_C=$(MAIN_OBJS_C))
$(info make: variable: MAIN_EXES=$(MAIN_EXES))
$(info make: variable: TEST_SRCS_C=$(TEST_SRCS_C))
$(info make: variable: TEST_OBJS_C=$(TEST_OBJS_C))
$(info make: variable: TEST_SRCS_CPP=$(TEST_SRCS_CPP))
$(info make: variable: TEST_OBJS_CPP=$(TEST_OBJS_CPP))
$(info make: variable: TEST_EXES=$(TEST_EXES))
$(info make: variable: BUILD_TYPE=$(BUILD_TYPE))
endif
endif

all: eolws tab $(MAIN_EXES) $(TEST_EXES) $(BUILD_TYPE)/SharedHashFile.a
	@ls -al /dev/shm/ | egrep test | perl -lane 'print $$_; $$any.= $$_; sub END{if(length($$any) > 0){print qq[make: unwanted /dev/shm/test* files detected after testing!]; exit 1}}'
	@echo "make: note: prefix make with SHF_DEBUG_MAKE=1 to debug this make file"
	@echo "make: note: prefix make with SHF_SKIP_TESTS=1 to build but do not run tests"
	@echo "make: note: prefix make with SHF_PERFORMANCE_TEST_(ENABLE|LOCK|MIX|CPUS|KEYS|FIXED|DEBUG)=(1|1|2|4|10000000|0|0) to run perf test"
	@echo "make: built $(TEST_EXE_SKIP) $(BUILD_TYPE) version"

$(BUILD_TYPE)/%.o: ./src/%.c $(DEPS_H)
	@echo "make: compiling: $@"
	@$(CC) -o $@ $< $(CXXFLAGS) $(CFLAGS)

$(BUILD_TYPE)/%.o: ./src/%.cpp $(DEPS_H) $(DEPS_HPP)
	@echo "make: compiling: $@"
	@$(CC) -o $@ $< $(CXXFLAGS)

$(BUILD_TYPE)/%.a: $(PROD_OBJS_C) $(PROD_OBJS_CPP)
	@echo "make: archiving: $@"
	@ar rv $@ $^

$(BUILD_TYPE)/%.t: $(BUILD_TYPE)/%.o $(PROD_OBJS_C) $(PROD_OBJS_CPP)
	@echo "make: linking: $@"
	@g++ -o $@ $^ -pthread -lm
ifndef SHF_SKIP_TESTS
	@echo "make: running: $@"
	@PATH=$$PATH:$(BUILD_TYPE) ./$@ 2>&1 | perl src/verbose-if-fail.pl $@.tout
endif

$(BUILD_TYPE)/%: $(BUILD_TYPE)/main.%.o $(PROD_OBJS_C) $(PROD_OBJS_CPP)
	@echo "make: linking: $@"
	@g++ -o $@ $^ -pthread -lm

release: all

debug: all

fixme:
	 find -type f | egrep -v "/(release|debug)/" | egrep -v "/.html/" | egrep "\.(c|cc|cpp|h|hpp|js|md|txt)" | xargs egrep --line-number -i fixme | perl -lane 'print qq[>].$$_.qq[<]; $$any+=length($$_)>0; sub END{printf qq[make: %u line(s) with fixme\n],$$any; exit($$any>0)}'

tab:
	@find -type f | egrep -v "/(release|debug)/" | egrep -v "/.html/" | egrep "\.(c|cc|cpp|h|hpp|js|md|txt)" | xargs  grep --line-number -P "\\t" | perl -lane 'print qq[>].$$_.qq[<]; $$any+=length($$_)>0; sub END{printf qq[make: %u line(s) with tab\n],$$any; exit($$any>0)}'

eolws:
	@find -type f | egrep -v "/(release|debug)/" | egrep -v "/.html/" | egrep "\.(c|cc|cpp|h|hpp|js|md|txt)" | xargs  grep --line-number -P "\\s+$$" | perl -lane 'print qq[>].$$_.qq[<]; $$any+=length($$_)>0; sub END{printf qq[make: %u line(s) with eol whitespace\n],$$any; exit($$any>0)}'

.PHONY: all clean release debug fixme tab eolws

clean:
	rm -rf release debug
