#ifndef TTCN2JSON_HH
#define	TTCN2JSON_HH

// forward declarations
namespace Common {
  class Modules;
}

class JSON_Tokenizer;

namespace Ttcn {
  
  /** TTCN-3 to JSON schema converter
    * Generates a JSON schema from the type and coding function definitions in
    * TTCN-3 modules */
  class Ttcn2Json {
    
  private:
    
    /** Input modules */
    Common::Modules* modules;
    
    Ttcn2Json(const Ttcn2Json&); // no copying
    Ttcn2Json& operator=(const Ttcn2Json&); // no assignment
    
    /** Inserts the JSON schema skeleton into the parameter JSON tokenizer and
      * passes the tokenizer on to the TTCN-3 modules to insert the schemas for
      * their types and coding functions */
    void create_schema(JSON_Tokenizer& json);
    
    /** Inserts a JSON schema to the end of another schema
      * @param to contains the destination schema
      * @param from contains the inserted (source) schema; its contents will be
      * altered (ruined), do not use after this call */
    void insert_schema(JSON_Tokenizer& to, JSON_Tokenizer& from);
    
  public:
    
    /** Initializes this object with the input modules, calls create_schema() to
      * generate the JSON schema and writes the schema into the given file 
      * @param p_modules input TTCN-3 modules
      * @param p_schema_name JSON schema file name */
    Ttcn2Json(Common::Modules* p_modules, const char* p_schema_name);
    
  };
}

#endif	/* TTCN2JSON_HH */

