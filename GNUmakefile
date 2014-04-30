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
CXXFLAGS       += -fno-inline-functions-called-once -fPIC -Wuninitialized -Wunused -march=x86-64 -I.
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
DEPS_H          = $(wildcard *.h)
DEPS_HPP        = $(wildcard *.hpp)
NODE_SRCS       =                              $(filter-out %build,$(wildcard ./wrappers/nodejs/*))
PROD_SRCS_C     =                              $(filter-out test%,$(wildcard *.c))
PROD_OBJS_C     = $(patsubst %,$(BUILD_TYPE)/%,$(filter-out test%,$(patsubst %.c,%.o,$(PROD_SRCS_C))))
PROD_SRCS_CPP   =                              $(filter-out test%,$(wildcard *.cpp))
PROD_OBJS_CPP   = $(patsubst %,$(BUILD_TYPE)/%,$(filter-out test%,$(patsubst %.cpp,%.o,$(PROD_SRCS_CPP))))
TEST_SRCS_C     =                              $(filter     test%,$(wildcard *.c))
TEST_OBJS_C     = $(patsubst %,$(BUILD_TYPE)/%,$(filter     test%,$(patsubst %.c,%.o,$(TEST_SRCS))))
TEST_SRCS_CPP   =                              $(filter     test%,$(wildcard *.cpp))
TEST_OBJS_CPP   = $(patsubst %,$(BUILD_TYPE)/%,$(filter     test%,$(patsubst %.cpp,%.o,$(TEST_SRCS))))
TEST_EXES       = $(patsubst %,$(BUILD_TYPE)/%,$(filter     test%,$(patsubst %.c,%.t,$(TEST_SRCS_C)))) \
                  $(patsubst %,$(BUILD_TYPE)/%,$(filter     test%,$(patsubst %.cpp,%.t,$(TEST_SRCS_CPP))))

ifneq ($(filter clean,$(MAKECMDGOALS)),)
else
DUMMY := $(shell mkdir $(BUILD_TYPE) 2>&1)
NODEJS := $(shell which nodejs 2>&1)
NODE_GYP := $(shell which node-gyp 2>&1)
ifdef SHF_DEBUG_MAKE
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
$(info make: variable: BUILD_TYPE=$(BUILD_TYPE))
$(info make: variable: BUILD_TYPE_NODE=$(BUILD_TYPE_NODE))
$(info make: variable: NODEJS=$(NODEJS))
$(info make: variable: NODE_GYP=$(NODE_GYP))
$(info make: variable: NODE_SRCS=$(NODE_SRCS))
endif
endif

all: $(TEST_EXES) $(BUILD_TYPE)/SharedHashFile.a $(BUILD_TYPE)/SharedHashFile.node
	@ls -al /dev/shm/ | egrep test | perl -lane 'print $$_; $$any.= $$_; sub END{if(length($$any) > 0){print qq[make: unwanted /dev/shm/test* files detected after testing!]; exit 1}}'
	@echo "make: note: prefix make with SHF_DEBUG_MAKE=1 to debug this make file"
	@echo "make: note: prefix make with SHF_ENABLE_PERFORMANCE_TEST=1 to run perf test"
	@echo "make: built and tested $(BUILD_TYPE) version"

$(BUILD_TYPE)/%.o: %.c $(DEPS_H)
	@echo "make: compiling: $@"
	@$(CC) -o $@ $< $(CXXFLAGS) $(CFLAGS)

$(BUILD_TYPE)/%.o: %.cpp $(DEPS_H) $(DEPS_HPP)
	@echo "make: compiling: $@"
	@$(CC) -o $@ $< $(CXXFLAGS)

$(BUILD_TYPE)/%.a: $(PROD_OBJS_C) $(PROD_OBJS_CPP)
	@echo "make: archiving: $@"
	@ar ruv $@ $^

$(BUILD_TYPE)/%.t: $(BUILD_TYPE)/%.o $(PROD_OBJS_C) $(PROD_OBJS_CPP)
	@echo "make: linking: $@"
	@g++ -o $@ $^
	@echo "make: running: $@"
	@./$@

$(BUILD_TYPE)/SharedHashFile.node: $(TEST_EXES) $(BUILD_TYPE)/SharedHashFile.a $(NODE_SRCS)
	@echo "make: building: $@"
ifneq ($(findstring node-gyp,$(NODE_GYP)),)
	@cd ./wrappers/nodejs && SHF_BUILD_TYPE=$(BUILD_TYPE) NODE_DEBUG=mymod node-gyp --$(BUILD_TYPE) rebuild
	@echo "make: copying node wrapper & test program to $(BUILD_TYPE) build folder"
	@cp ./wrappers/nodejs/build/$(BUILD_TYPE_NODE)/SharedHashFile.node $(BUILD_TYPE)/.
	@cp ./wrappers/nodejs/SharedHashFile*.js $(BUILD_TYPE)/.
	@echo "make: running test"
	@cd $(BUILD_TYPE) && NODE_DEBUG=mymod nodejs ./SharedHashFile.js
	@echo "make: running test: perf test calling dummy C++ functions"
	@cd $(BUILD_TYPE) && NODE_DEBUG=mymod nodejs ./SharedHashFileDummy.js
	@echo "make: building and running test: IPC: Unix Domain Socket"
	@cd $(BUILD_TYPE) && cp ../wrappers/nodejs/TestIpcSocket.* .
	@cd $(BUILD_TYPE) && gcc -o TestIpcSocket.o $(CFLAGS) $(CXXFLAGS) -I .. TestIpcSocket.c
	@cd $(BUILD_TYPE) && gcc -o TestIpcSocket TestIpcSocket.o shf.o murmurhash3.o
	@cd $(BUILD_TYPE) && ./TestIpcSocket
	@echo "make: building and running test: IPC: SharedHashFile Queue"
	@cd $(BUILD_TYPE) && cp ../wrappers/nodejs/TestIpcQueue.* .
	@cd $(BUILD_TYPE) && gcc -o TestIpcQueue.o $(CFLAGS) $(CXXFLAGS) -I .. TestIpcQueue.c
	@cd $(BUILD_TYPE) && gcc -o TestIpcQueue TestIpcQueue.o shf.o murmurhash3.o tap.o
	@cd $(BUILD_TYPE) && ./TestIpcQueue c2js
	@cd $(BUILD_TYPE) && ./TestIpcQueue c2c
else
	@echo "make: note: !!! node-gyp not found; cannot build nodejs interface; e.g. install via: sudo apt-get install nodejs && sudo apt-get install node-gyp !!!"
endif

debug: all

fixme:
	find -type f | egrep -v "/(release|debug)/" | egrep "\.(c|cc|cpp|h|hpp|js|md|txt)" | xargs egrep -i fixme | perl -lane 'print $$_; $$any.= $$_; sub END{if(length($$any) > 0){exit 1}else{print qq[make: fixme not found in any source file!]}}'

.PHONY: all clean debug fixme

clean:
	rm -rf release debug wrappers/nodejs/build
