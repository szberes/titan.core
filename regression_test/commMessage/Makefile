###############################################################################
# Copyright (c) 2000-2014 Ericsson Telecom AB
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the Eclipse Public License v1.0
# which accompanies this distribution, and is available at
# http://www.eclipse.org/legal/epl-v10.html
###############################################################################
TOPDIR := ..
include $(TOPDIR)/Makefile.regression

.SUFFIXES: .ttcn .asn .hh
.PHONY: all clean dep run

TTCN3_LIB = ttcn3$(RT2_SUFFIX)-parallel$(DYNAMIC_SUFFIX)
TTCN3_LIB_SINGLE = ttcn3$(RT2_SUFFIX)$(DYNAMIC_SUFFIX)

TTCN3_MODULES = TcommMessage.ttcn HS41022.ttcn

ASN1_MODULES = ImplMsgEncData.asn

GENERATED_SOURCES = $(TTCN3_MODULES:.ttcn=.cc) $(ASN1_MODULES:.asn=.cc)
GENERATED_HEADERS = $(TTCN3_MODULES:.ttcn=.hh) $(ASN1_MODULES:.asn=.hh)
ifdef CODE_SPLIT
GENERATED_SOURCES := $(foreach file, $(GENERATED_SOURCES:.cc=), $(addprefix $(file), .cc _seq.cc _set.cc  _seqof.cc _setof.cc _union.cc))
endif

OBJECTS = $(GENERATED_SOURCES:.cc=.o)

TARGET = TcommMessage$(EXESUFFIX)
TARGET_SINGLE = TcommMessage_single$(EXESUFFIX)

all: $(TARGET) $(TARGET_SINGLE)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) -L$(TTCN3_DIR)/lib -l$(TTCN3_LIB) -L$(OPENSSL_DIR)/lib -lcrypto $($(PLATFORM)_LIBS)

$(TARGET_SINGLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJECTS) -L$(TTCN3_DIR)/lib -l$(TTCN3_LIB_SINGLE) -L$(OPENSSL_DIR)/lib -lcrypto $($(PLATFORM)_LIBS)

.cc.o:
	$(CXX) -c $(CPPFLAGS) $(CXXFLAGS) $<

$(GENERATED_SOURCES) $(GENERATED_HEADERS): $(TTCN3_MODULES) $(ASN1_MODULES)
	$(TTCN3_COMPILER) $^

clean distclean:
	-rm -f $(TARGET) $(TARGET_SINGLE) $(OBJECTS) $(GENERATED_HEADERS) \
	$(GENERATED_SOURCES) *.log Makefile.bak

dep: $(GENERATED_SOURCES)
	makedepend $(CPPFLAGS) $(GENERATED_SOURCES)

run: run-single run-parallel

run-single: $(TARGET_SINGLE) config.cfg
	./$^

run-parallel: $(TARGET)
	$(TTCN3_DIR)/bin/ttcn3_start $(TARGET) config_parallel.cfg

