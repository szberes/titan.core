///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#ifndef DBGNEW_HH
#define DBGNEW_HH

#ifndef _Common_memory_H
#include "memory.h"
#endif

#ifdef MEMORY_DEBUG

class debug_new_counter_t
{
  static int count;
  static const char * progname;
public:
  debug_new_counter_t();
  ~debug_new_counter_t();
  void set_program_name(const char *pgn);
};
// implementation in new.cc

// An instance for every translation unit. Because each instance is constructed
// before main() and probably before any other global object,
// it is destroyed after main() ends and all global objects are destroyed.
// The last destructor runs check_mem_leak().
static debug_new_counter_t debug_new_counter;

// Custom placement new for memory tracking
void* operator new(size_t size, const char* file, int line);
void* operator new[](size_t size, const char* file, int line);

// Redirect "normal" new to memory-tracking placement new.
#define new new(__FILE__, __LINE__)

#endif // MEMORY_DEBUG

#endif
