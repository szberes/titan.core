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
.PHONY: all compile clean dep run

TTCN3_LIB = ttcn3$(RT2_SUFFIX)$(DYNAMIC_SUFFIX)

ASN1_MODULES = Test283.asn
TTCN3_MODULES = Test283T.ttcn

GENERATED_SOURCES = $(ASN1_MODULES:.asn=.cc) $(TTCN3_MODULES:.ttcn=.cc)
GENERATED_HEADERS = $(GENERATED_SOURCES:.cc=.hh)
ifdef CODE_SPLIT
GENERATED_SOURCES := $(foreach file, $(GENERATED_SOURCES:.cc=), $(addprefix $(file), .cc _seq.cc _set.cc  _seqof.cc _setof.cc _union.cc))
endif

all: $(GENERATED_SOURCES)

$(GENERATED_SOURCES) $(GENERATED_HEADERS): compile
	@if [ ! -f $@ ]; then $(RM) compile; $(MAKE) compile; fi

compile: $(TTCN3_MODULES) $(ASN1_MODULES)
	$(TTCN3_COMPILER) $(COMPILER_FLAGS) $^ - $?
	touch $@

clean distclean:
	$(RM) $(GENERATED_HEADERS) $(GENERATED_SOURCES) Makefile.bak keyword_verdict_file

dep:

run: $(GENERATED_SOURCES)
	sh $(ABS_SRC)/keyword
