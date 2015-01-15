///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#ifndef OCTETSTRING_HH
#define OCTETSTRING_HH

#include "Basetype.hh"
#include "Template.hh"
#include "Optional.hh"
#include "Error.hh"

class INTEGER;
class BITSTRING;
class HEXSTRING;
class CHARSTRING;
class OCTETSTRING_ELEMENT;
class Module_Param;

// octetstring value class

class OCTETSTRING : public Base_Type {

  friend class OCTETSTRING_ELEMENT;
  friend class OCTETSTRING_template;
  friend class TTCN_Buffer;

  friend OCTETSTRING int2oct(int value, int length);
  friend OCTETSTRING int2oct(const INTEGER& value, int length);
  friend OCTETSTRING str2oct(const CHARSTRING& value);
  friend OCTETSTRING bit2oct(const BITSTRING& value);
  friend OCTETSTRING hex2oct(const HEXSTRING& value);
  friend OCTETSTRING unichar2oct(const UNIVERSAL_CHARSTRING& invalue);
  friend OCTETSTRING unichar2oct(const UNIVERSAL_CHARSTRING& invalue, const CHARSTRING& string_encoding);
  friend OCTETSTRING replace(const OCTETSTRING& value, int index, int len,
                             const OCTETSTRING& repl);

protected: // for ASN_ANY which is derived from OCTETSTRING
  struct octetstring_struct;
  octetstring_struct *val_ptr;

  void init_struct(int n_octets);
  void copy_value();
  OCTETSTRING(int n_octets);

public:
  OCTETSTRING();
  OCTETSTRING(int n_octets, const unsigned char* octets_ptr);
  OCTETSTRING(const OCTETSTRING_ELEMENT& other_value);
  OCTETSTRING(const OCTETSTRING& other_value);
  ~OCTETSTRING();

  OCTETSTRING& operator=(const OCTETSTRING& other_value);
  OCTETSTRING& operator=(const OCTETSTRING_ELEMENT& other_value);

  boolean operator==(const OCTETSTRING& other_value) const;
  boolean operator==(const OCTETSTRING_ELEMENT& other_value) const;

  inline boolean operator!=(const OCTETSTRING& other_value) const
    { return !(*this == other_value); }
  inline boolean operator!=(const OCTETSTRING_ELEMENT& other_value) const
    { return !(*this == other_value); }

  OCTETSTRING operator+(const OCTETSTRING& other_value) const;
  OCTETSTRING operator+(const OCTETSTRING_ELEMENT& other_value) const;

  OCTETSTRING& operator+=(const OCTETSTRING& other_value);
  OCTETSTRING& operator+=(const OCTETSTRING_ELEMENT& other_value);

  OCTETSTRING operator~() const;
  OCTETSTRING operator&(const OCTETSTRING& other_value) const;
  OCTETSTRING operator&(const OCTETSTRING_ELEMENT& other_value) const;
  OCTETSTRING operator|(const OCTETSTRING& other_value) const;
  OCTETSTRING operator|(const OCTETSTRING_ELEMENT& other_value) const;
  OCTETSTRING operator^(const OCTETSTRING& other_value) const;
  OCTETSTRING operator^(const OCTETSTRING_ELEMENT& other_value) const;

  OCTETSTRING operator<<(int shift_count) const;
  OCTETSTRING operator<<(const INTEGER& shift_count) const;
  OCTETSTRING operator>>(int shift_count) const;
  OCTETSTRING operator>>(const INTEGER& shift_count) const;
  OCTETSTRING operator<<=(int rotate_count) const;
  OCTETSTRING operator<<=(const INTEGER& rotate_count) const;
  OCTETSTRING operator>>=(int rotate_count) const;
  OCTETSTRING operator>>=(const INTEGER& rotate_count) const;

  OCTETSTRING_ELEMENT operator[](int index_value);
  OCTETSTRING_ELEMENT operator[](const INTEGER& index_value);
  const OCTETSTRING_ELEMENT operator[](int index_value) const;
  const OCTETSTRING_ELEMENT operator[](const INTEGER& index_value) const;

  inline boolean is_bound() const { return val_ptr != NULL; }
  inline boolean is_value() const { return val_ptr != NULL; }
  inline void must_bound(const char *err_msg) const
    { if (val_ptr == NULL) TTCN_error("%s", err_msg); }
  void clean_up();

  int lengthof() const;
  operator const unsigned char*() const;
  void dump () const;

#ifdef TITAN_RUNTIME_2
  boolean is_equal(const Base_Type* other_value) const { return *this == *(static_cast<const OCTETSTRING*>(other_value)); }
  void set_value(const Base_Type* other_value) { *this = *(static_cast<const OCTETSTRING*>(other_value)); }
  Base_Type* clone() const { return new OCTETSTRING(*this); }
  const TTCN_Typedescriptor_t* get_descriptor() const { return &OCTETSTRING_descr_; }
#else
  inline boolean is_present() const { return is_bound(); }
#endif

  void log() const;
  void set_param(Module_Param& param);

  void encode_text(Text_Buf& text_buf) const;
  void decode_text(Text_Buf& text_buf);

public:
  void encode(const TTCN_Typedescriptor_t& p_td, TTCN_Buffer& p_buf,
              TTCN_EncDec::coding_t p_coding, ...) const;

  void decode(const TTCN_Typedescriptor_t& p_td, TTCN_Buffer& p_buf,
              TTCN_EncDec::coding_t p_coding, ...);

  ASN_BER_TLV_t* BER_encode_TLV(const TTCN_Typedescriptor_t& p_td,
                                unsigned p_coding) const;
#ifdef TITAN_RUNTIME_2
  ASN_BER_TLV_t* BER_encode_negtest_raw() const;
  virtual int encode_raw(TTCN_Buffer& p_buf) const;
  virtual int RAW_encode_negtest_raw(RAW_enc_tree& p_myleaf) const;
#endif
  boolean BER_decode_TLV(const TTCN_Typedescriptor_t& p_td,
                         const ASN_BER_TLV_t& p_tlv, unsigned L_form);

  /** Encodes the value of the variable according to the
    * TTCN_Typedescriptor_t. It must be public because called by
    * another types during encoding. Returns the length of encoded data*/
  int RAW_encode(const TTCN_Typedescriptor_t&, RAW_enc_tree&) const;
  /** Decodes the value of the variable according to the
   * TTCN_Typedescriptor_t. It must be public because called by
   * another types during encoding. Returns the number of decoded
   * bits. */
  int RAW_decode(const TTCN_Typedescriptor_t&, TTCN_Buffer& buff, int limit,
                 raw_order_t top_bit_ord, boolean no_err=FALSE,
                 int sel_field=-1, boolean first_call=TRUE);
  int TEXT_encode(const TTCN_Typedescriptor_t&,
                 TTCN_Buffer&) const;
  int TEXT_decode(const TTCN_Typedescriptor_t&, TTCN_Buffer&,  Limit_Token_List&,
                 boolean no_err=FALSE, boolean first_call=TRUE);
  int XER_encode(const XERdescriptor_t& p_td, TTCN_Buffer& p_buf,
                 unsigned int flavor, int indent) const;
  int XER_decode(const XERdescriptor_t& p_td, XmlReaderWrap& reader,
                 unsigned int flavor);
  
  /** Encodes accordingly to the JSON encoding rules.
    * Returns the length of the encoded data. */
  int JSON_encode(const TTCN_Typedescriptor_t&, JSON_Tokenizer&) const;
  
  /** Decodes accordingly to the JSON encoding rules.
    * Returns the length of the decoded data. */
  int JSON_decode(const TTCN_Typedescriptor_t&, JSON_Tokenizer&, boolean);
};


class OCTETSTRING_ELEMENT {
  boolean bound_flag;
  OCTETSTRING& str_val;
  int octet_pos;

public:
  OCTETSTRING_ELEMENT(boolean par_bound_flag, OCTETSTRING& par_str_val,
    int par_octet_pos);

  OCTETSTRING_ELEMENT& operator=(const OCTETSTRING& other_value);
  OCTETSTRING_ELEMENT& operator=(const OCTETSTRING_ELEMENT& other_value);

  boolean operator==(const OCTETSTRING& other_value) const;
  boolean operator==(const OCTETSTRING_ELEMENT& other_value) const;
  inline boolean operator!=(const OCTETSTRING& other_value) const
    { return !(*this == other_value); }
  inline boolean operator!=(const OCTETSTRING_ELEMENT& other_value) const
    { return !(*this == other_value); }

  OCTETSTRING operator+(const OCTETSTRING& other_value) const;
  OCTETSTRING operator+(const OCTETSTRING_ELEMENT& other_value) const;

  OCTETSTRING operator~() const;
  OCTETSTRING operator&(const OCTETSTRING& other_value) const;
  OCTETSTRING operator&(const OCTETSTRING_ELEMENT& other_value) const;
  OCTETSTRING operator|(const OCTETSTRING& other_value) const;
  OCTETSTRING operator|(const OCTETSTRING_ELEMENT& other_value) const;
  OCTETSTRING operator^(const OCTETSTRING& other_value) const;
  OCTETSTRING operator^(const OCTETSTRING_ELEMENT& other_value) const;

  inline boolean is_bound() const { return bound_flag; }
  inline boolean is_present() const { return bound_flag; }
  inline boolean is_value() const { return bound_flag; }
  inline void must_bound(const char *err_msg) const
    { if (!bound_flag) TTCN_error("%s", err_msg); }

  unsigned char get_octet() const;

  void log() const;
};

// octetstring template class

class OCTETSTRING_template : public Restricted_Length_Template {
#ifdef __SUNPRO_CC
public:
#endif
  struct octetstring_pattern_struct;
private:
  OCTETSTRING single_value;
  union {
    struct {
      unsigned int n_values;
      OCTETSTRING_template *list_value;
    } value_list;
    octetstring_pattern_struct *pattern_value;
  };

  void copy_template(const OCTETSTRING_template& other_value);
  static boolean match_pattern(const octetstring_pattern_struct *string_pattern,
    const OCTETSTRING::octetstring_struct *string_value);

public:
  OCTETSTRING_template();
  OCTETSTRING_template(template_sel other_value);
  OCTETSTRING_template(const OCTETSTRING& other_value);
  OCTETSTRING_template(const OCTETSTRING_ELEMENT& other_value);
  OCTETSTRING_template(const OPTIONAL<OCTETSTRING>& other_value);
  OCTETSTRING_template(unsigned int n_elements,
    const unsigned short *pattern_elements);
  OCTETSTRING_template(const OCTETSTRING_template& other_value);
  ~OCTETSTRING_template();
  void clean_up();

  OCTETSTRING_template& operator=(template_sel other_value);
  OCTETSTRING_template& operator=(const OCTETSTRING& other_value);
  OCTETSTRING_template& operator=(const OCTETSTRING_ELEMENT& other_value);
  OCTETSTRING_template& operator=(const OPTIONAL<OCTETSTRING>& other_value);
  OCTETSTRING_template& operator=(const OCTETSTRING_template& other_value);

  OCTETSTRING_ELEMENT operator[](int index_value);
  OCTETSTRING_ELEMENT operator[](const INTEGER& index_value);
  const OCTETSTRING_ELEMENT operator[](int index_value) const;
  const OCTETSTRING_ELEMENT operator[](const INTEGER& index_value) const;

  boolean match(const OCTETSTRING& other_value) const;
  const OCTETSTRING& valueof() const;

  int lengthof() const;

  void set_type(template_sel template_type, unsigned int list_length);
  OCTETSTRING_template& list_item(unsigned int list_index);

  void log() const;
  void log_match(const OCTETSTRING& match_value) const;

  void set_param(Module_Param& param);

  void encode_text(Text_Buf& text_buf) const;
  void decode_text(Text_Buf& text_buf);

  boolean is_present() const;
  boolean match_omit() const;
#ifdef TITAN_RUNTIME_2
  void valueofv(Base_Type* value) const { *(static_cast<OCTETSTRING*>(value)) = valueof(); }
  void set_value(template_sel other_value) { *this = other_value; }
  void copy_value(const Base_Type* other_value) { *this = *(static_cast<const OCTETSTRING*>(other_value)); }
  Base_Template* clone() const { return new OCTETSTRING_template(*this); }
  const TTCN_Typedescriptor_t* get_descriptor() const { return &OCTETSTRING_descr_; }
  boolean matchv(const Base_Type* other_value) const { return match(*(static_cast<const OCTETSTRING*>(other_value))); }
  void log_matchv(const Base_Type* match_value) const  { log_match(*(static_cast<const OCTETSTRING*>(match_value))); }
#else
  void check_restriction(template_res t_res, const char* t_name=NULL) const;
#endif
};

#endif
