///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#ifndef JSONAST_HH_
#define	JSONAST_HH_

#include "../datatypes.h"

class JsonAST {
  private:
    void init_JsonAST();
    JsonAST(const JsonAST&);
    JsonAST& operator=(const JsonAST&);
  public:
    boolean omit_as_null;
    char* alias;
    boolean as_value;
    char* default_value;
  
    JsonAST() { init_JsonAST(); }
    JsonAST(const JsonAST *other_val);
    ~JsonAST();
    
    void print_JsonAST() const;
};

#endif	/* JSONAST_HH_ */

