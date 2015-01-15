###############################################################################
# Copyright (c) 2000-2014 Ericsson Telecom AB
# All rights reserved. This program and the accompanying materials
# are made available under the terms of the Eclipse Public License v1.0
# which accompanies this distribution, and is available at
# http://www.eclipse.org/legal/epl-v10.html
###############################################################################
TOPDIR := ../../../
include   $(TOPDIR)/Makefile.regression

FILES := concat.ttcn concat.cfg
RUNNABLE := concat
CFG := concat.cfg
DIR_SINGLE := dir_single_mode
DIR_PARALLEL := dir_parallel_mode
GENERATED_DIRS := $(DIR_SINGLE) $(DIR_PARALLEL) 

MAKE_PROG := $(MAKE)

# List of fake targets:
.PHONY: all clean run run_single run_parallel runall

all: $(GENERATED_DIRS)

dir_single_mode:
	mkdir $@
	cd $@ && for file in $(FILES); do ln -s ../$$file || exit; done
	cd $@ && $(TTCN3_DIR)/bin/ttcn3_makefilegen -s ./* && $(MAKE_PROG)
	
dir_parallel_mode:
	mkdir $@
	cd $@ && for file in $(FILES); do ln -s ../$$file || exit; done
	cd $@ && $(TTCN3_DIR)/bin/ttcn3_makefilegen ./* && $(MAKE_PROG)

run: $(GENERATED_DIRS)
	cd $(DIR_SINGLE) && ./$(RUNNABLE) $(CFG) 
	cd $(DIR_PARALLEL) && $(TTCN3_DIR)/bin/ttcn3_start $(RUNNABLE) $(CFG) 

# To run all tests, possibly in parallel
run_single: $(DIR_SINGLE) 
	cd $(DIR_SINGLE) && ./$(RUNNABLE) $(CFG) 
run_parallel: $(DIR_PARALLEL) 
	cd $(DIR_PARALLEL) && $(TTCN3_DIR)/bin/ttcn3_start $(RUNNABLE) $(CFG) 
runall: run_single run_parallel 

clean distclean:
	rm -rf $(GENERATED_DIRS)

