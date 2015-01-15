###############################################################################
# Copyright (c) 2000-2014 Ericsson Telecom AB
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the Eclipse Public License v1.0
# which accompanies this distribution, and is available at
# http://www.eclipse.org/legal/epl-v10.html
###############################################################################
DIRS := \
JUnitLogger TSTLogger

all run dep clean distclean install:
	for d in $(DIRS); do $(MAKE) -C $$d $@ || exit 1; done
