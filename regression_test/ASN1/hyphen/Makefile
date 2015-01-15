###############################################################################
# Copyright (c) 2000-2014 Ericsson Telecom AB
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the Eclipse Public License v1.0
# which accompanies this distribution, and is available at
# http://www.eclipse.org/legal/epl-v10.html
###############################################################################
TOPDIR := ../..
include $(TOPDIR)/Makefile.regression

.SUFFIXES: .ttcn .asn .hh
.PHONY: all clean dep run

TTCN3_LIB = ttcn3$(RT2_SUFFIX)$(DYNAMIC_SUFFIX)

ASN1_MODULES = Test284.asn

GENERATED_SOURCES = $(ASN1_MODULES:.asn=.cc)
GENERATED_HEADERS = $(GENERATED_SOURCES:.cc=.hh)
ifdef CODE_SPLIT
GENERATED_SOURCES := $(foreach file, $(GENERATED_SOURCES:.cc=), $(addprefix $(file), .cc _seq.cc _set.cc  _seqof.cc _setof.cc _union.cc))
endif

all: $(GENERATED_SOURCES)

.asn.cc .asn.hh:
	$(TTCN3_COMPILER) $<

clean distclean:
	$(RM) $(GENERATED_HEADERS) $(GENERATED_SOURCES) Makefile.bak

dep:

run: $(GENERATED_SOURCES)
	$(ABS_SRC)/hyphen
