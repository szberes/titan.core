///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#ifndef JSON_HH_
#define	JSON_HH_

#include "Types.h"

/** Descriptor for JSON encoding/decoding during runtime */
struct TTCN_JSONdescriptor_t 
{
  /** Encoding only. 
    * true  : use the null literal to encode omitted fields in records or sets
    *         example: { field1 : value1, field2 : null, field3 : value3 } 
    * false : skip both the field name and the value if a field is omitted
    *         example: { field1 : value1, field3 : value3 }
    * The decoder will always accept both variants. */
  boolean omit_as_null;
  
  /** An alias for the name of the field (in a record, set or union). 
    * Encoding: this alias will appear instead of the name of the field
    * Decoding: the decoder will look for this alias instead of the field's real name*/
  const char* alias;
  
  /** If set, the union will be encoded as a JSON value instead of a JSON object
    * with one name-value pair. 
    * Since the field name is no longer present, the decoder will determine the
    * selected field based on the type of the value. The first field (in the order
    * of declaration) that can successfully decode the value will be the selected one. */
  boolean as_value;
  
  /** Decoding only.
    * Fields that don't appear in the JSON code will have this value assigned to
    * them. */
  const char* default_value;
};

/** This macro makes sure that error and warning messages will only be displayed
  * if the silent flag is not set. */
#define JSON_ERROR if(!p_silent) TTCN_EncDec_ErrorContext::error

// JSON descriptors for base types
extern const TTCN_JSONdescriptor_t INTEGER_json_;
extern const TTCN_JSONdescriptor_t FLOAT_json_;
extern const TTCN_JSONdescriptor_t BOOLEAN_json_;
extern const TTCN_JSONdescriptor_t BITSTRING_json_;
extern const TTCN_JSONdescriptor_t HEXSTRING_json_;
extern const TTCN_JSONdescriptor_t OCTETSTRING_json_;
extern const TTCN_JSONdescriptor_t CHARSTRING_json_;
extern const TTCN_JSONdescriptor_t UNIVERSAL_CHARSTRING_json_;
extern const TTCN_JSONdescriptor_t VERDICTTYPE_json_;
extern const TTCN_JSONdescriptor_t GeneralString_json_;
extern const TTCN_JSONdescriptor_t NumericString_json_;
extern const TTCN_JSONdescriptor_t UTF8String_json_;
extern const TTCN_JSONdescriptor_t PrintableString_json_;
extern const TTCN_JSONdescriptor_t UniversalString_json_;
extern const TTCN_JSONdescriptor_t BMPString_json_;
extern const TTCN_JSONdescriptor_t GraphicString_json_;
extern const TTCN_JSONdescriptor_t IA5String_json_;
extern const TTCN_JSONdescriptor_t TeletexString_json_;
extern const TTCN_JSONdescriptor_t VideotexString_json_;
extern const TTCN_JSONdescriptor_t VisibleString_json_;

/** JSON decoder error codes */
enum json_decode_error {
  /** An unexpected JSON token was extracted. The token might still be valid and
    * useful for the caller structured type. */
  JSON_ERROR_INVALID_TOKEN = -1,
  /** The JSON tokeniser couldn't extract a valid token (JSON_TOKEN_ERROR) or the
    * format of the data extracted is invalid. In either case, this is a fatal 
    * error and the decoding cannot continue. 
    * @note This error code is always preceeded by a dynamic test case error, if the
    * caller receives this code, it means that decoding error behavior is (at least 
    * partially) set to warnings. */
  JSON_ERROR_FATAL = -2
};

// JSON decoding error messages
#define JSON_DEC_BAD_TOKEN_ERROR "Failed to extract valid token, invalid JSON format%s"
#define JSON_DEC_FORMAT_ERROR "Invalid JSON %s format, expecting %s value"
#define JSON_DEC_NAME_TOKEN_ERROR "Invalid JSON token, expecting JSON field name"
#define JSON_DEC_OBJECT_END_TOKEN_ERROR "Invalid JSON token, expecting JSON name-value pair or object end mark%s"
#define JSON_DEC_REC_OF_END_TOKEN_ERROR "Invalid JSON token, expecting JSON value or array end mark%s"
#define JSON_DEC_ARRAY_ELEM_TOKEN_ERROR "Invalid JSON token, expecting %d more JSON value%s"
#define JSON_DEC_ARRAY_END_TOKEN_ERROR "Invalid JSON token, expecting JSON array end mark%s"
#define JSON_DEC_FIELD_TOKEN_ERROR "Invalid JSON token found while decoding field '%s'"
#define JSON_DEC_INVALID_NAME_ERROR "Invalid field name '%s'"
#define JSON_DEC_MISSING_FIELD_ERROR "No JSON data found for field '%s'"
#define JSON_DEC_STATIC_OBJECT_END_TOKEN_ERROR "Invalid JSON token, expecting JSON object end mark%s"
#define JSON_DEC_AS_VALUE_ERROR "Extracted JSON %s could not be decoded by any field of the union"

#endif	/* JSON_HH_ */

