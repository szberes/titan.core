###############################################################################
# Copyright (c) 2000-2014 Ericsson Telecom AB
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the Eclipse Public License v1.0
# which accompanies this distribution, and is available at
# http://www.eclipse.org/legal/epl-v10.html
###############################################################################
# Makefile for the TITAN User Guide

TOP  := ..
include ../Makefile.cfg

.SUFFIXES: .tex .labels .dvi .ps .pdf

TITAN_DOCS := apiguide.doc userguide.doc installationguide.doc referenceguide.doc releasenotes.doc

TITAN_PDFS := $(TITAN_DOCS:.doc=.pdf)

ifeq ($(GEN_PDF), yes)

install: $(TITAN_PDFS)
ifdef MINGW
$(info Skipped ${CURDIR} for MinGW)
else
	mkdir -p $(DOCDIR)
	cp $(TITAN_PDFS) $(DOCDIR)
endif

%.pdf : %.doc
	@for doc in $(TITAN_DOCS:.doc=); do \
	  java -jar ../../titan_playground/jar/doc2pdf.jar $(addsuffix .doc, $$doc) $(addsuffix .pdf, $$doc) || exit; \
	done

else
install:
endif

release:
	$(MAKE) install GEN_PDF=yes

all run:

clean:

distclean:clean

dep:
