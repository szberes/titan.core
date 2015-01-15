///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#ifndef ILOGGER_PLUGIN_HH
#define ILOGGER_PLUGIN_HH

#include "Types.h"
#include "Logger.hh"
#include "TTCN3.hh"
#include "Charstring.hh"

/// Forward declarations.
namespace TitanLoggerApi
{
  class TitanLogEvent;
}

class ILoggerPlugin
{
public:
  ILoggerPlugin() :
    major_version_(0), minor_version_(0), name_(NULL), help_(NULL), is_configured_(false) { }
  virtual ~ILoggerPlugin() { }

  virtual bool is_static() = 0;
  virtual void init(const char *options = NULL) = 0;
  virtual void fini() = 0;
  virtual void reset() { }
  virtual void fatal_error(const char */*err_msg*/, ...) { }

  virtual bool is_log2str_capable() { return false; }
  virtual CHARSTRING log2str(const TitanLoggerApi::TitanLogEvent& /*event*/)
    { return CHARSTRING(); }

  inline unsigned int major_version() const { return this->major_version_; }
  inline unsigned int minor_version() const { return this->minor_version_; }
  inline const char *plugin_name() const { return this->name_; }
  inline const char *plugin_help() const { return this->help_; }
  inline bool is_configured() const { return this->is_configured_; }
  inline void set_configured(bool configured) { this->is_configured_ = configured; }

  virtual void log(const TitanLoggerApi::TitanLogEvent& event, bool log_buffered,
      bool separate_file, bool use_emergency_mask) = 0;

  /// Backward compatibility functions.
  virtual void open_file(bool /*is_first*/) { }
  virtual void close_file() { }
  virtual void set_file_name(const char */*new_filename_skeleton*/,
                             bool /*from_config*/) { }
  virtual void set_append_file(bool /*new_append_file*/) { }
  virtual bool set_file_size(int /*size*/) { return false; }
  virtual bool set_file_number(int /*number*/) { return false; }
  virtual bool set_disk_full_action(TTCN_Logger::disk_full_action_t /*disk_full_action*/) { return false; }
  virtual void set_parameter(const char */*parameter_name*/, const char */*parameter_value*/) { }

protected:
  unsigned int major_version_;
  unsigned int minor_version_;
  char *name_;
  char *help_;
  bool is_configured_;
};

#endif  // ILOGGER_PLUGIN_HH
