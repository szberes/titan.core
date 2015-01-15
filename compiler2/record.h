///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#ifndef RECORD_H
#define RECORD_H

#include "datatypes.h"
#include "ttcn3/compiler.h"

#ifdef __cplusplus
extern "C" {
#endif

void defRecordClass(const struct_def *sdef, output_struct *output);
void defRecordTemplate(const struct_def *sdef, output_struct *output);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
