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

ifneq ($(filter debug,$(MAKECMDGOALS)),)
BUILD_TYPE      = debug
BUILD_TYPE_NODE = Debug
else
BUILD_TYPE = release
BUILD_TYPE_NODE = Release
endif

DEPS_H          = $(wildcard *.h)
DEPS_HPP        = $(wildcard *.hpp)
TEST_SRCS_C     =                              $(filter     test%,$(wildcard *.c))
TEST_OBJS_C     = $(patsubst %,$(BUILD_TYPE)/%,$(filter     test%,$(patsubst %.c,%.o,$(TEST_SRCS))))
TEST_EXES       = $(patsubst %,$(BUILD_TYPE)/%,$(filter     test%,$(patsubst %.c,%.t,$(TEST_SRCS_C)))) \
                  $(patsubst %,$(BUILD_TYPE)/%,$(filter     test%,$(patsubst %.cpp,%.t,$(TEST_SRCS_CPP))))

ifneq ($(filter clean,$(MAKECMDGOALS)),)
else
PYTHON := $(shell which python 2>&1)
$(info make: variable: TEST_SRCS_C=$(TEST_SRCS_C))
$(info make: variable: TEST_OBJS_C=$(TEST_OBJS_C))
$(info make: variable: TEST_EXES=$(TEST_EXES))
$(info make: variable: BUILD_TYPE=$(BUILD_TYPE))
$(info make: variable: PYTHON=$(PYTHON))
endif

all: $(BUILD_TYPE)/build/lib.linux-x86_64-2.7/SharedHashFile.so
	@echo "make: built $(BUILD_TYPE) version"

$(BUILD_TYPE)/build/lib.linux-x86_64-2.7/SharedHashFile.so:
	@echo "make: building: $@"
	@python setup.py build

debug: all

fixme:
	find -type f | egrep -v "/(release|debug)/" | egrep "\.(c|cc|cpp|h|hpp|js|md|txt|py)" | xargs egrep -i fixme | perl -lane 'print $$_; $$any.= $$_; sub END{if(length($$any) > 0){exit 1}else{print qq[make: fixme not found in any source file!]}}'

install: all
	@echo "make: Locally installing..."
	@echo "make: To remove: sudo pip uninstall SharedHashFile"
	@sudo python setup.py install
	@PATH=$$PATH:../../../$(BUILD_TYPE) python TestSharedHashFile.py
	
test: install
	@echo "make: built and $(BUILD_TYPE) version" 

.PHONY: all clean debug fixme install

clean:
	rm -rf dist/
	rm -rf build/