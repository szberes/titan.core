///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
/*
 * Common header file for the Base Library of the TTCN-3 Test Executor
 *
 * This file is included from the output files of the TTCN-3 compiler.
 * You should include this file instead of the individual header files of this
 * directory in your C++ modules that are linked to the executable test suite.
 *
 * Do not modify this file.
 * Code originally authored by Janos Zoltan Szabo
 *
 */

#ifndef TTCN3_HH
#define TTCN3_HH

#include "version.h"
#include <cversion.h>

#include "Vector.hh"
#include "Basetype.hh"
#include "Template.hh"
#include "Integer.hh"
#include "Float.hh"
#include "Boolean.hh"
#include "ASN_Null.hh"
#include "Objid.hh"
#include "Verdicttype.hh"
#include "Component.hh"
#include "Bitstring.hh"
#include "Hexstring.hh"
#include "Octetstring.hh"
#include "ASN_Any.hh"
#include "Charstring.hh"
#include "Universal_charstring.hh"
#include "Struct_of.hh"
#include "Optional.hh"
#include "Array.hh"
#include "ASN_CharacterString.hh"
#include "ASN_External.hh"
#include "ASN_EmbeddedPDV.hh"
#include "Addfunc.hh"

#include "Timer.hh"
#include "Port.hh"
#include "Logger.hh"

#ifdef TITAN_RUNTIME_2
#include "RT2/TitanLoggerApiSimple.hh"
#else
#include "RT1/TitanLoggerApiSimple.hh"
#endif

#include "Module_list.hh"
#include "Parameters.h"
#include "Snapshot.hh"
#include "Default.hh"
#include "Runtime.hh"
#include "Encdec.hh"
#include "BER.hh"
#include "RAW.hh"
#include "TEXT.hh"
#include "XER.hh"
#include "JSON.hh"
#include "Error.hh"
#include "XmlReader.hh"

#endif
