###############################################################################
# Copyright (c) 2000-2014 Ericsson Telecom AB
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the Eclipse Public License v1.0
# which accompanies this distribution, and is available at
# http://www.eclipse.org/legal/epl-v10.html
###############################################################################
TOPDIR := ..
include $(TOPDIR)/Makefile.regression

CODIRS := dynamicTemplate styleGuide topLevelPdu \
	circularImport typeInstantiation \
	compareImported compoundif \
	centralstorage mfgen-tpd \
	openType optionalAssignCompare portConstructor \
	isbound namedActualParameters assignmentNotation \
	attribQualif

all dep clean distclean:
	for dir in $(CODIRS); do $(MAKE) -C $$dir $@ || exit; done
