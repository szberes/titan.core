# Makefile for pseudoinstall

TOP := ..
include $(TOP)/Makefile.cfg

all run:
	$(ABS_SRC)/../pseudoinstall

# Should this delete ?
clean distclean: ;

# definitely do-nothing
dep install: ;

# All targets are phony
.PHONY: all run clean distclean dep install

export OPENSSL_DIR XMLDIR
