###############################################################################
# Copyright (c) 2000-2014 Ericsson Telecom AB
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the Eclipse Public License v1.0
# which accompanies this distribution, and is available at
# http://www.eclipse.org/legal/epl-v10.html
###############################################################################
TOPDIR := ../../..
include $(TOPDIR)/Makefile.regression

all:
	@echo Howdy!
	-mkdir -p bin
	$(TTCN3_DIR)/bin/ttcn3_makefilegen -f -t counter.tpd && \
	if [ -h bin/counter.ttcn ] ; then \
	  echo success ; \
	else \
	  echo Symlink bin/counter.ttcn does not exist; exit 1; \
	fi ; \

clean:
	$(RM) bin/*
