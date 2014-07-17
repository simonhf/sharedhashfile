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
BUILD_TYPE_NODE = Debug
CXXFLAGS       += -DSHF_DEBUG_VERSION
else
BUILD_TYPE = release
BUILD_TYPE_NODE = Release
CXXFLAGS       += -O3
endif
DEPS_H          = $(wildcard ./src/*.h)
DEPS_HPP        = $(wildcard ./src/*.hpp)
NODE_SRCS       =                                    $(filter-out %build,$(wildcard ./wrappers/nodejs/*))
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

ifneq ($(filter clean,$(MAKECMDGOALS)),)
else
DUMMY := $(shell mkdir $(BUILD_TYPE) 2>&1)
NODEJS := $(shell perl -e 'print `which node` || `which nodejs`' 2>&1)
NODE_GYP := $(shell which node-gyp 2>&1)
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
$(info make: variable: BUILD_TYPE_NODE=$(BUILD_TYPE_NODE))
$(info make: variable: NODEJS=$(NODEJS))
$(info make: variable: NODE_GYP=$(NODE_GYP))
$(info make: variable: NODE_SRCS=$(NODE_SRCS))
endif
endif

all: tab $(MAIN_EXES) $(TEST_EXES) $(BUILD_TYPE)/SharedHashFile.a $(BUILD_TYPE)/SharedHashFile.node
	@ls -al /dev/shm/ | egrep test | perl -lane 'print $$_; $$any.= $$_; sub END{if(length($$any) > 0){print qq[make: unwanted /dev/shm/test* files detected after testing!]; exit 1}}'
	@echo "make: note: prefix make with SHF_DEBUG_MAKE=1 to debug this make file"
	@echo "make: note: prefix make with SHF_PERFORMANCE_TEST_(ENABLE|CPUS|KEYS)=(1|4|10000000) to run perf test"
	@echo "make: built and tested $(BUILD_TYPE) version"

$(BUILD_TYPE)/%.o: ./src/%.c $(DEPS_H)
	@echo "make: compiling: $@"
	@$(CC) -o $@ $< $(CXXFLAGS) $(CFLAGS)

$(BUILD_TYPE)/%.o: ./src/%.cpp $(DEPS_H) $(DEPS_HPP)
	@echo "make: compiling: $@"
	@$(CC) -o $@ $< $(CXXFLAGS)

$(BUILD_TYPE)/%.a: $(PROD_OBJS_C) $(PROD_OBJS_CPP)
	@echo "make: archiving: $@"
	@ar ruv $@ $^

$(BUILD_TYPE)/%.t: $(BUILD_TYPE)/%.o $(PROD_OBJS_C) $(PROD_OBJS_CPP)
	@echo "make: linking: $@"
	@g++ -o $@ $^
	@echo "make: running: $@"
	@PATH=$$PATH:$(BUILD_TYPE) ./$@ 2>&1 | perl src/verbose-if-fail.pl $@.tout

$(BUILD_TYPE)/%: $(BUILD_TYPE)/main.%.o $(PROD_OBJS_C) $(PROD_OBJS_CPP)
	@echo "make: linking: $@"
	@g++ -o $@ $^

$(BUILD_TYPE)/SharedHashFile.node: $(MAIN_EXES) $(TEST_EXES) $(BUILD_TYPE)/SharedHashFile.a $(NODE_SRCS)
	@echo "make: building: $@"
ifneq ($(findstring node-gyp,$(NODE_GYP)),)
	@cd ./wrappers/nodejs && SHF_BUILD_TYPE=$(BUILD_TYPE) NODE_DEBUG=mymod node-gyp --$(BUILD_TYPE) rebuild
	@echo "make: copying node wrapper & test program to $(BUILD_TYPE) build folder"
	@cp ./wrappers/nodejs/build/$(BUILD_TYPE_NODE)/SharedHashFile.node $(BUILD_TYPE)/.
	@cp ./wrappers/nodejs/SharedHashFile*.js $(BUILD_TYPE)/.
	@echo "make: running test"
	@cd $(BUILD_TYPE) && PATH=$$PATH:. NODE_DEBUG=mymod $(NODEJS) ./SharedHashFile.js 2>&1 | perl ../src/verbose-if-fail.pl SharedHashFile.js.tout
	@echo "make: running test: perf test calling dummy C++ functions"
	@cd $(BUILD_TYPE) && NODE_DEBUG=mymod $(NODEJS) ./SharedHashFileDummy.js 2>&1 | perl ../src/verbose-if-fail.pl SharedHashFileDummy.js.tout
	@echo "make: building and running test: IPC: Unix Domain Socket"
	@cd $(BUILD_TYPE) && cp ../wrappers/nodejs/TestIpcSocket.* .
	@cd $(BUILD_TYPE) && gcc -o TestIpcSocket.o $(CFLAGS) $(CXXFLAGS) -I ../src TestIpcSocket.c
	@cd $(BUILD_TYPE) && gcc -o TestIpcSocket TestIpcSocket.o shf.o murmurhash3.o
	@cd $(BUILD_TYPE) && ./TestIpcSocket 2>&1 | perl ../src/verbose-if-fail.pl TestIpcSocket.tout
	@echo "make: running test: IPC: SharedHashFile Queue"
	@cd $(BUILD_TYPE) && cp ../wrappers/nodejs/TestIpcQueue.js .
	@cd $(BUILD_TYPE) && PATH=$$PATH:. ./test.q.shf.t c2js 2>&1 | perl ../src/verbose-if-fail.pl test.q.shf.t.tout
else
	@echo "make: note: !!! node-gyp not found; cannot build nodejs interface; e.g. install via: sudo apt-get install nodejs && sudo apt-get install node-gyp !!!"
endif

debug: all

fixme:
	 find -type f | egrep -v "/(release|debug)/" | egrep -v "/.html/" | egrep "\.(c|cc|cpp|h|hpp|js|md|txt)" | xargs egrep -i fixme | perl -lane 'print $$_; $$any+=length $$_>0; sub END{printf qq[make: %u line(s) with fixme\n],length $$any; exit(length $$any>0)}'

tab:
	@find -type f | egrep -v "/(release|debug)/" | egrep -v "/.html/" | egrep "\.(c|cc|cpp|h|hpp|js|md|txt)" | xargs  grep -P "\\t" | perl -lane 'print $$_; $$any+=length $$_>0; sub END{printf qq[make: %u line(s) with tab\n],length $$any; exit(length $$any>0)}'

.PHONY: all clean debug fixme tab

clean:
	rm -rf release debug wrappers/nodejs/build
