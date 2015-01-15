###############################################################################
# Copyright (c) 2000-2014 Ericsson Telecom AB
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the Eclipse Public License v1.0
# which accompanies this distribution, and is available at
# http://www.eclipse.org/legal/epl-v10.html
###############################################################################
TOPDIR := ..
include $(TOPDIR)/Makefile.regression

.SUFFIXES: .ttcn .hh
.PHONY: all clean dep run run-single run-parallel

TTCN3_MODULES = erc.ttcn

GENERATED_SOURCES = $(TTCN3_MODULES:.ttcn=.cc)
GENERATED_HEADERS = $(GENERATED_SOURCES:.cc=.hh)
ifdef CODE_SPLIT
GENERATED_SOURCES := $(foreach file, $(GENERATED_SOURCES:.cc=), $(addprefix $(file), .cc _seq.cc _set.cc  _seqof.cc _setof.cc _union.cc))
endif

OBJECTS = $(GENERATED_SOURCES:.cc=.o)

SINGLE_TARGET = erc_single$(EXESUFFIX)
PARALLEL_TARGET = erc_parallel$(EXESUFFIX)

all: $(SINGLE_TARGET) $(PARALLEL_TARGET)

$(SINGLE_TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) -L$(TTCN3_DIR)/lib -lttcn3$(RT2_SUFFIX)$(DYNAMIC_SUFFIX) -L$(OPENSSL_DIR)/lib -lcrypto $($(PLATFORM)_LIBS)

$(PARALLEL_TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) -L$(TTCN3_DIR)/lib -lttcn3$(RT2_SUFFIX)-parallel$(DYNAMIC_SUFFIX) -L$(OPENSSL_DIR)/lib -lcrypto $($(PLATFORM)_LIBS)

.cc.o:
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $<

.ttcn.cc .ttcn.hh:
	$(TTCN3_COMPILER) $<

clean distclean:
	-rm -f $(SINGLE_TARGET) $(PARALLEL_TARGET) $(OBJECTS) $(GENERATED_HEADERS) \
	$(GENERATED_SOURCES) *.log Makefile.bak

dep: $(GENERATED_SOURCES)
	makedepend $(CPPFLAGS) $(GENERATED_SOURCES)

run: run-single run-parallel

run-single: $(SINGLE_TARGET) config.cfg
	env env_test="variable" ./$^

run-parallel: $(PARALLEL_TARGET) config.cfg
	env env_test="variable" '$(TTCN3_DIR)/bin/ttcn3_start' $^

.NOTPARALLEL:

