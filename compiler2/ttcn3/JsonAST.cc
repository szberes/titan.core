///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#include "JsonAST.hh"
#include "../../common/memory.h"
#include <cstddef>
#include <cstdio>

void JsonAST::init_JsonAST()
{
  omit_as_null = false;
  alias = NULL;
  as_value = false;
  default_value = NULL;
}

JsonAST::JsonAST(const JsonAST *other_val)
{
  init_JsonAST();
  if (NULL != other_val) {
    omit_as_null = other_val->omit_as_null;
    alias = (NULL != other_val->alias) ? mcopystr(other_val->alias) : NULL;
    as_value = other_val->as_value;
    default_value = (NULL != other_val->default_value) ? mcopystr(other_val->default_value) : NULL;
  }
}

JsonAST::~JsonAST()
{
  Free(alias);
  Free(default_value);
}

void JsonAST::print_JsonAST() const
{
  printf("\n\rOmit encoding: ");
  if (omit_as_null) {
    printf("as null value\n\r");
  } else {
    printf("skip field\n\r");
  }
  if (alias) {
    printf("Name as %s\n\r", alias);
  }
  if (as_value) {
    printf("Encoding unions as JSON value\n\r");
  }
  if (default_value) {
    printf("Default value: %s\n\r", default_value);
  }
}
