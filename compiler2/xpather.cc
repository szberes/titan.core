///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#include "xpather.h"

#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

#define LIBXML_SCHEMAS_ENABLED
#include <libxml/xmlschemastypes.h>

#include "../common/memory.h"
#include "vector.hh"
// Do _NOT_ #include "string.hh", it drags in ustring.o, common/Quadruple.o,
// Int.o, ttcn3/PatternString.o, and then the entire AST :(
#include "map.hh"
#include "../common/path.h"

// in makefile.c
void ERROR  (const char *fmt, ...);
void WARNING(const char *fmt, ...);
void NOTIFY (const char *fmt, ...);
void DEBUG  (const char *fmt, ...);

// for vector and map
void fatal_error(const char * filename, int lineno, const char * fmt, ...)
__attribute__ ((__format__ (__printf__, 3, 4), __noreturn__));

void fatal_error(const char * filename, int lineno, const char * fmt, ...)
{
  fputs(filename, stderr);
  fprintf(stderr, ":%d: ", lineno);
  va_list va;
  va_start(va, fmt);
  vfprintf(stderr, fmt, va);
  va_end(va);
  abort();
}

/// Run an XPath query and return an xmlXPathObjectPtr, which must be freed
xmlXPathObjectPtr run_xpath(xmlXPathContextPtr xpathCtx, const char *xpathExpr)
{
  xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(
    (const xmlChar *)xpathExpr, xpathCtx);
  if(xpathObj == NULL) {
    fprintf(stderr,"Error: unable to evaluate xpath expression \"%s\"\n", xpathExpr);
    return 0;
  }

  return xpathObj;
}

// RAII classes

class XmlDoc {
public:
  explicit XmlDoc(xmlDocPtr p) : doc_(p) {}
  ~XmlDoc() {
    if (doc_ != NULL) xmlFreeDoc(doc_);
  }
  operator xmlDocPtr() const { return doc_; }
private:
  xmlDocPtr doc_;
};

class XPathContext {
public:
  explicit XPathContext(xmlXPathContextPtr c) : ctx_(c) {}
  ~XPathContext() {
    if (ctx_ != NULL) xmlXPathFreeContext(ctx_);
  }
  operator xmlXPathContextPtr() const { return ctx_; }
private:
  xmlXPathContextPtr ctx_;
};

class XPathObject {
public:
  explicit XPathObject(xmlXPathObjectPtr o) : xpo_(o) {}
  ~XPathObject() {
    if (xpo_ != NULL) xmlXPathFreeObject(xpo_);
  }
  operator   xmlXPathObjectPtr() const { return xpo_; }
  xmlXPathObjectPtr operator->() const { return xpo_; }
private:
  xmlXPathObjectPtr xpo_;
};

//------------------------------------------------------------------
/// compare-by-content wrapper of a plain C string
struct cstring {
  explicit cstring(const char *s) : str(s) {}
  void destroy() const;
  operator const char*() const { return str; }
protected:
  const char *str;
  friend boolean operator<(const cstring& l, const cstring& r);
  friend boolean operator==(const cstring& l, const cstring& r);
};

void cstring::destroy() const {
  Free(const_cast<char*>(str)); // assumes valid pointer or NULL
}

boolean operator<(const cstring& l, const cstring& r) {
  return strcmp(l.str, r.str) < 0;
}

boolean operator==(const cstring& l, const cstring& r) {
  return strcmp(l.str, r.str) == 0;
}

/// RAII for C string
struct autostring : public cstring {
  /// Constructor; takes over ownership
  explicit autostring(const char *s = 0) : cstring(s) {}
  ~autostring() {
    // He who can destroy a thing, controls that thing -- Paul Muad'Dib
    Free(const_cast<char*>(str)); // assumes valid pointer or NULL
  }
  /// %Assignment; takes over ownership
  const autostring& operator=(const char *s) {
    Free(const_cast<char*>(str)); // assumes valid pointer or NULL
    str = s;
    return *this;
  }
  /// Relinquish ownership
  const char *extract() {
    const char *retval = str;
    str = 0;
    return retval;
  }
private:
  autostring(const autostring&);
  autostring& operator=(const autostring&);
};


bool validate_tpd(const XmlDoc& xml_doc, const char* tpd_file_name, const char* xsd_file_name)
{
  xmlLineNumbersDefault(1);

  xmlSchemaParserCtxtPtr ctxt = xmlSchemaNewParserCtxt(xsd_file_name);
  if (ctxt==NULL) {
    ERROR("Unable to create xsd context for xsd file `%s'", xsd_file_name);
    return false;
  }
  xmlSchemaSetParserErrors(ctxt, (xmlSchemaValidityErrorFunc)fprintf, (xmlSchemaValidityWarningFunc)fprintf, stderr);

  xmlSchemaPtr schema = xmlSchemaParse(ctxt);
  if (schema==NULL) {
    ERROR("Unable to parse xsd file `%s'", xsd_file_name);
    xmlSchemaFreeParserCtxt(ctxt);
    return false;
  }

  xmlSchemaValidCtxtPtr xsd = xmlSchemaNewValidCtxt(schema);
  if (xsd==NULL) {
    ERROR("Schema validation error for xsd file `%s'", xsd_file_name);
    xmlSchemaFree(schema);
    xmlSchemaFreeParserCtxt(ctxt);
    return false;
  }
  xmlSchemaSetValidErrors(xsd, (xmlSchemaValidityErrorFunc) fprintf, (xmlSchemaValidityWarningFunc) fprintf, stderr);

  int ret = xmlSchemaValidateDoc(xsd, xml_doc);

  xmlSchemaFreeValidCtxt(xsd);
  xmlSchemaFree(schema);
  xmlSchemaFreeParserCtxt(ctxt);
  xmlSchemaCleanupTypes();

  if (ret==0) {
    return true; // successful validation
  } else if (ret>0) {
    ERROR("TPD file `%s' is invalid according to schema `%s'", tpd_file_name, xsd_file_name);
    return false;
  } else {
    ERROR("TPD validation of `%s' generated an internal error in libxml2", tpd_file_name);
    return false;
  }
}

/** Extract a boolean value from the XML, if it exists otherwise flag is unchanged
 *
 * @param xpathCtx XPath context object
 * @param actcfg name of the active configuration
 * @param option name of the value
 * @param flag pointer to the variable to receive the value
 */
void xsdbool2boolean(const XPathContext& xpathCtx, const char *actcfg,
  const char *option, boolean* flag)
{
  char *xpath = mprintf(
    "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
    "/ProjectProperties/MakefileSettings/%s[text()='true']",
    actcfg, option);
  XPathObject xpathObj(run_xpath(xpathCtx, xpath));
  Free(xpath);

  if (xpathObj->nodesetval && xpathObj->nodesetval->nodeNr > 0) {
    *flag = TRUE;
  }
}

// data structures and functions to manage excluded folders/files

map<cstring, void> excluded_files;

boolean is_excluded_file(const cstring& path) {
  return excluded_files.has_key(path);
}

vector<const char> excluded_folders;

// Unfortunately, when "docs" is excluded, we need to drop
// files in "docs/", "docs/pdf/", "docs/txt/", "docs/txt/old/" etc;
// so it's not as simple as using a map :(

/** Checks whether a file is under an excluded folder
 *
 * @param path (relative) path of the file
 * @return true if file is excluded, false otherwise
 */
boolean is_excluded_folder(const char *path) {
  boolean answer = FALSE;
  size_t pathlen = strlen(path);

  for (size_t i = 0, end = excluded_folders.size(); i < end; ++i) {
    const char *xdir = excluded_folders[i];
    size_t xdlen = strlen(xdir);
    if (pathlen > xdlen && path[xdlen] == '/') {
      // we may have a winner
      if ((answer = !strncmp(path, xdir, xdlen))) break;
    }
  }
  return answer;
}

// How do you treat a raw info? You cook it, of course!
// Returns a newly allocated string.
char *cook(const char *raw, const map<cstring, const char>& path_vars)
{
  const char *slash = strchr(raw, '/');
  if (!slash) { // Pretend that the slash is at the end of the string.
    slash = raw + strlen(raw);
  }

  // Assume that a path variable reference is the first (or only) component
  // of the path: ROOT in "ROOT/etc/issue".
  autostring prefix(mcopystrn(raw, slash - raw));
  if (path_vars.has_key(prefix)) {
    char *cooked = mcopystr(path_vars[prefix]);
    bool ends_with_slash = cooked[strlen(cooked)-1] == '/';
    if (ends_with_slash && *slash == '/') {
      // Avoid paths with two slashes at the start; Cygwin thinks it's UNC
      ++slash;
    }
    // If there was no '/' (only the path variable reference e.g "ROOT")
    // then slash points to a null byte and the mputstr is a no-op.
    cooked = mputstr(cooked, slash);
    return cooked;
  }

  // If the path variable could not be substituted,
  // return (a copy of) the original.
  return mcopystr(raw);
}

void replacechar(char** content) {

  std::string s= *content;
  size_t found = 0;
  while ((found = s.find('['))!= std::string::npos){
    s.replace(found,1, "${");
  }
  while ((found = s.find(']')) != std::string::npos){
    s.replace(found,1, "}");
  }
  *content = mcopystr(s.c_str());
}

static void clear_seen_tpd_files(map<cstring, int>& seen_tpd_files) {
  for (size_t i = 0, num = seen_tpd_files.size(); i < num; ++i) {
    const cstring& key = seen_tpd_files.get_nth_key(i);
    int *elem = seen_tpd_files.get_nth_elem(i);
    key.destroy();
    delete elem;
  }
  seen_tpd_files.clear();
}

static tpd_result process_tpd_internal(const char *p_tpd_name, const char *actcfg,
  const char *file_list_path, int *p_argc, char ***p_argv,
  int *p_optind, char **p_ets_name,
  boolean *p_gflag, boolean *p_sflag, boolean *p_cflag, boolean *p_aflag, boolean *preprocess,
  boolean *p_Rflag, boolean *p_lflag, boolean *p_mflag, boolean *p_Pflag,
  boolean *p_Lflag, boolean recursive, boolean force_overwrite, boolean gen_only_top_level,
  const char *output_file, char** abs_work_dir_p, struct string_list* sub_project_dirs,
  const char* program_name, FILE* prj_graph_fp, struct string2_list* create_symlink_list, struct string_list* ttcn3_prep_includes,
  struct string_list* ttcn3_prep_defines, struct string_list* prep_includes, struct string_list* prep_defines,
  boolean *p_csflag, boolean *p_quflag, boolean* p_dsflag, char** cxxcompiler,
  char** optlevel, char** optflags, boolean* p_dbflag, boolean* p_drflag, boolean* p_dtflag, boolean* p_dxflag, boolean* p_djflag,
  boolean* p_fxflag, boolean* p_doflag, boolean* p_gfflag, boolean* p_lnflag, boolean* p_isflag,
  boolean* p_asflag, boolean* p_swflag, boolean* p_Yflag, struct string_list* solspeclibs, struct string_list* sol8speclibs,
  struct string_list* linuxspeclibs, struct string_list* freebsdspeclibs, struct string_list* win32speclibs, char** ttcn3prep,
  struct string_list* linkerlibs, struct string_list* additionalObjects, struct string_list* linkerlibsearchp, boolean Vflag, boolean Dflag,
  char** generatorCommandOutput, struct string2_list* target_placement_list, boolean prefix_workdir, struct string2_list* run_command_list,
  map<cstring, int>& seen_tpd_files);

extern "C" tpd_result process_tpd(const char *p_tpd_name, const char *actcfg,
  const char *file_list_path, int *p_argc, char ***p_argv,
  int *p_optind, char **p_ets_name,
  boolean *p_gflag, boolean *p_sflag, boolean *p_cflag, boolean *p_aflag, boolean *preprocess,
  boolean *p_Rflag, boolean *p_lflag, boolean *p_mflag, boolean *p_Pflag,
  boolean *p_Lflag, boolean recursive, boolean force_overwrite, boolean gen_only_top_level,
  const char *output_file, char** abs_work_dir_p, struct string_list* sub_project_dirs,
  const char* program_name, FILE* prj_graph_fp, struct string2_list* create_symlink_list, struct string_list* ttcn3_prep_includes,
  struct string_list* ttcn3_prep_defines, struct string_list* prep_includes, struct string_list* prep_defines,
  boolean *p_csflag, boolean *p_quflag, boolean* p_dsflag, char** cxxcompiler,
  char** optlevel, char** optflags, boolean* p_dbflag, boolean* p_drflag, boolean* p_dtflag, boolean* p_dxflag, boolean* p_djflag,
  boolean* p_fxflag, boolean* p_doflag, boolean* p_gfflag, boolean* p_lnflag, boolean* p_isflag,
  boolean* p_asflag, boolean* p_swflag, boolean* p_Yflag, struct string_list* solspeclibs, struct string_list* sol8speclibs,
  struct string_list* linuxspeclibs, struct string_list* freebsdspeclibs, struct string_list* win32speclibs, char** ttcn3prep,
  string_list* linkerlibs, string_list* additionalObjects, string_list* linkerlibsearchp, boolean Vflag, boolean Dflag,
  char** generatorCommandOutput, struct string2_list* target_placement_list, boolean prefix_workdir, struct string2_list* run_command_list) {

  map<cstring, int> seen_tpd_files;

  tpd_result success = process_tpd_internal(p_tpd_name,
      actcfg, file_list_path, p_argc, p_argv, p_optind, p_ets_name,
      p_gflag, p_sflag, p_cflag, p_aflag, preprocess,
      p_Rflag, p_lflag, p_mflag, p_Pflag,
      p_Lflag, recursive, force_overwrite, gen_only_top_level,
      output_file, abs_work_dir_p, sub_project_dirs,
      program_name, prj_graph_fp, create_symlink_list, ttcn3_prep_includes,
      ttcn3_prep_defines, prep_includes, prep_defines,
      p_csflag, p_quflag, p_dsflag, cxxcompiler,
      optlevel, optflags, p_dbflag, p_drflag, p_dtflag, p_dxflag, p_djflag,
      p_fxflag, p_doflag, p_gfflag, p_lnflag, p_isflag,
      p_asflag, p_swflag, p_Yflag, solspeclibs, sol8speclibs,
      linuxspeclibs, freebsdspeclibs, win32speclibs, ttcn3prep,
      linkerlibs, additionalObjects, linkerlibsearchp, Vflag, Dflag,
      generatorCommandOutput, target_placement_list, prefix_workdir, run_command_list, seen_tpd_files);

  for (size_t i = 0, num = seen_tpd_files.size(); i < num; ++i) {
    const cstring& key = seen_tpd_files.get_nth_key(i);
    int *elem = seen_tpd_files.get_nth_elem(i);
    key.destroy();
    delete elem;
  }
  seen_tpd_files.clear();

  return success;
}

// optind is the index of the next element of argv to be processed.
// Return TPD_SUCESS if parsing successful, TPD_SKIPPED if the tpd was
// seen already, or TPD_FAILED.
//
// Note: if process_tpd() returns TPD_SUCCESS, it is expected that all strings
// (argv[], ets_name, other_files[], output_file) are allocated on the heap
// and need to be freed. On input, these strings point into argv.
// process_tpd() may alter these strings; new values will be on the heap.
// If process_tpd() preserves the content of such a string (e.g. ets_name),
// it must nevertheless make a copy on the heap via mcopystr().
static tpd_result process_tpd_internal(const char *p_tpd_name, const char *actcfg,
  const char *file_list_path, int *p_argc, char ***p_argv,
  int *p_optind, char **p_ets_name,
  boolean *p_gflag, boolean *p_sflag, boolean *p_cflag, boolean *p_aflag, boolean *preprocess,
  boolean *p_Rflag, boolean *p_lflag, boolean *p_mflag, boolean *p_Pflag,
  boolean *p_Lflag, boolean recursive, boolean force_overwrite, boolean gen_only_top_level,
  const char *output_file, char** abs_work_dir_p, struct string_list* sub_project_dirs,
  const char* program_name, FILE* prj_graph_fp, struct string2_list* create_symlink_list, struct string_list* ttcn3_prep_includes,
  struct string_list* ttcn3_prep_defines, struct string_list* prep_includes, struct string_list* prep_defines,
  boolean *p_csflag, boolean *p_quflag, boolean* p_dsflag, char** cxxcompiler,
  char** optlevel, char** optflags, boolean* p_dbflag, boolean* p_drflag, boolean* p_dtflag, boolean* p_dxflag, boolean* p_djflag,
  boolean* p_fxflag, boolean* p_doflag, boolean* p_gfflag, boolean* p_lnflag, boolean* p_isflag,
  boolean* p_asflag, boolean* p_swflag, boolean* p_Yflag, struct string_list* solspeclibs, struct string_list* sol8speclibs,
  struct string_list* linuxspeclibs, struct string_list* freebsdspeclibs, struct string_list* win32speclibs, char** ttcn3prep,
  string_list* linkerlibs, string_list* additionalObjects, string_list* linkerlibsearchp, boolean Vflag, boolean Dflag,
  char** generatorCommandOutput, struct string2_list* target_placement_list, boolean prefix_workdir, struct string2_list* run_command_list,
  map<cstring, int>& seen_tpd_files)
{
  // read-only non-pointer aliases
  //char** const& local_argv = *p_argv;
  int const& local_argc = *p_argc;
  int const& local_optind = *p_optind;
  *abs_work_dir_p = NULL;

  assert(local_optind >= 2 // at least '-ttpd_name' must be in the args
    || local_optind == 0); // if called for a referenced project

  assert(local_argc   >= local_optind);

  autostring tpd_dir(get_dir_from_path(p_tpd_name));
  autostring abs_tpd_dir(get_absolute_dir(tpd_dir, NULL));

  autostring tpd_filename(get_file_from_path(p_tpd_name));
  autostring abs_tpd_name(compose_path_name(abs_tpd_dir, tpd_filename));

  if (seen_tpd_files.has_key(abs_tpd_name)) {
    ++*seen_tpd_files[abs_tpd_name];
    return TPD_SKIPPED; // nothing to do
  }
  else {
    if (recursive && !prefix_workdir) {
      // check that this tpd file is not inside a directory of another tpd file
      for (size_t i = 0; i < seen_tpd_files.size(); ++i) {
        const cstring& other_tpd_name = seen_tpd_files.get_nth_key(i);
        autostring other_tpd_dir(get_dir_from_path((const char*)other_tpd_name));
        if (strcmp((const char*)abs_tpd_dir,(const char*)other_tpd_dir)==0) {
          ERROR("TPD files `%s' and `%s' are in the same directory! Use the `-W' option.", (const char*)abs_tpd_name, (const char*)other_tpd_name);
          return TPD_FAILED;
        }
      }
    }
    // mcopystr makes another copy for the map
    seen_tpd_files.add(cstring(mcopystr(abs_tpd_name)), new int(1));
  }

  vector<char> base_files; // values Malloc'd but we pass them to the caller

  XmlDoc doc(xmlParseFile(p_tpd_name));
  if (doc == NULL) {
    fprintf(stderr, "Error: unable to parse file \"%s\"\n", p_tpd_name);
    return TPD_FAILED;
  }

  if (!Vflag) {
    // try schema validation if tpd schema file was found
    bool tpd_is_valid = false;
    const char* ttcn3_dir = getenv("TTCN3_DIR");
    if (ttcn3_dir) {
      size_t ttcn3_dir_len = strlen(ttcn3_dir);
      bool ends_with_slash = (ttcn3_dir_len>0) && (ttcn3_dir[ttcn3_dir_len - 1]=='/');
      expstring_t xsd_file_name = mprintf("%s%setc/xsd/TPD.xsd", ttcn3_dir, ends_with_slash?"":"/");
      autostring xsd_file_name_as(xsd_file_name);
      if (get_path_status(xsd_file_name)==PS_FILE) {
        if (validate_tpd(doc, p_tpd_name, xsd_file_name)) {
          tpd_is_valid = true;
          NOTIFY("TPD file `%s' validated successfully with schema file `%s'", p_tpd_name, xsd_file_name);
        }
      } else {
        ERROR("Cannot find XSD for schema for validation of TPD on path `%s'", xsd_file_name);
      }
    } else {
      ERROR("Environment variable TTCN3_DIR not present, cannot find XSD for schema validation of TPD");
    }
    if (!tpd_is_valid) {
      return TPD_FAILED;
    }
  }

  // Source files.
  // Key is projectRelativePath, value is relativeURI or rawURI.
  map<cstring, const char> files;   // values Malloc'd
  map<cstring, const char> folders; // values Malloc'd
  // NOTE! files and folders must be local variables of process_tpd.
  // This is because the keys (not the values) are owned by the XmlDoc.

  map<cstring, const char> path_vars;

  XPathContext xpathCtx(xmlXPathNewContext(doc));
  if (xpathCtx == NULL) {
    fprintf(stderr,"Error: unable to create new XPath context\n");
    return TPD_FAILED;
  }
  // Collect path variables
  {
    XPathObject pathsObj(run_xpath(xpathCtx,
      "/TITAN_Project_File_Information/PathVariables/PathVariable"));
    xmlNodeSetPtr nodes = pathsObj->nodesetval;

    const char *name = 0, *value = 0;
    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      // nodes->nodeTab[i]->name === "PathVariable"
      for (xmlAttrPtr attr = nodes->nodeTab[i]->properties; attr; attr = attr->next) {
        if (!strcmp((const char*)attr->name, "name")) {
          name = (const char*)attr->children->content;
        }
        else if (!strcmp((const char*)attr->name, "value")) {
          value = (const char*)attr->children->content;
        }
        else {
          WARNING("Unknown attribute %s", (const char*)nodes->nodeTab[i]->name);
        }
      } // next attribute

      if (name && value) path_vars.add(cstring(name), value);
      else ERROR("A PathVariable must have both name and value");
    } // next PathVariable
  }

  // Collect folders
  {
    XPathObject foldersObj(run_xpath(xpathCtx,
      "/TITAN_Project_File_Information/Folders/FolderResource"));

    xmlNodeSetPtr nodes = foldersObj->nodesetval;
    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      // nodes->nodeTab[i]->name === "FolderResource"
      const char *uri = 0, *path = 0, *raw = 0;

      // projectRelativePath is the path as it appears in Project Explorer (illusion)
      // relativeURI is the actual location, relative to the project root (reality)
      // rawURI is present if the relative path can not be calculated
      //
      // Theoretically these attributes could be in any order, loop over them
      for (xmlAttrPtr attr = nodes->nodeTab[i]->properties; attr; attr = attr->next) {
        if (!strcmp((const char*)attr->name, "projectRelativePath")) {
          path = (const char*)attr->children->content;
        }
        else if (!strcmp((const char*)attr->name, "relativeURI")) {
          uri = (const char*)attr->children->content;
        }
        else if (!strcmp((const char*)attr->name, "rawURI")) {
          raw = (const char*)attr->children->content;
        }
        else {
          WARNING("Unknown attribute %s", (const char*)nodes->nodeTab[i]->name);
        }
      } // next attribute

      if (path == NULL) {
        ERROR("A FolderResource must have a projectRelativePath");
        continue;
      }

      if (uri == NULL && raw == NULL) {
        ERROR("A FolderResource must have either relativeURI or rawURI");
        continue;
      }
      // relativeURI wins over rawURI
      folders.add(cstring(path), uri ? mcopystr(uri) : cook(raw, path_vars));
      // TODO uri: cut "file:", complain on anything else
    } // next FolderResource
  }

  if (actcfg == NULL)
  {
    // Find out the active config
    XPathObject  activeConfig(run_xpath(xpathCtx,
      "/TITAN_Project_File_Information/ActiveConfiguration/text()"));
    if (activeConfig->nodesetval && activeConfig->nodesetval->nodeNr == 1) {
      // there is one node
      actcfg = (const char*)activeConfig->nodesetval->nodeTab[0]->content;
    }
  }

  if (actcfg == NULL) {
    ERROR("Can not find the active build configuration.");
    for (size_t i = 0; i < folders.size(); ++i) {
      Free(const_cast<char*>(folders.get_nth_elem(i)));
    }
    folders.clear();
    return TPD_FAILED;
  }

  { // check if the active configuration exists
    expstring_t xpathActCfg= mprintf(
      "/TITAN_Project_File_Information/Configurations/"
        "Configuration[@name='%s']/text()", actcfg);
    XPathObject theConfigEx(run_xpath(xpathCtx, xpathActCfg));
    Free(xpathActCfg);

    xmlNodeSetPtr nodes = theConfigEx->nodesetval;
    if (nodes == NULL) {
      ERROR("The active build configuration named '%s' does not exist",
          actcfg);
      for (size_t i = 0; i < folders.size(); ++i) {
        Free(const_cast<char*>(folders.get_nth_elem(i)));
      }
      folders.clear();
      return TPD_FAILED;
    }
  }

  /////////////////////////////////////////////////////////////////////////////
  // working directory stuff
  autostring workdir;
  {
    const char* workdirFromTpd = "bin"; // default value
    char *workdirXpath = mprintf(
      "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
      "/ProjectProperties/LocalBuildSettings/workingDirectory/text()",
      actcfg);
    XPathObject workdirObj(run_xpath(xpathCtx, workdirXpath));
    Free(workdirXpath);
    if (workdirObj->nodesetval && workdirObj->nodesetval->nodeNr > 0) {
      workdirFromTpd = (const char*)workdirObj->nodesetval->nodeTab[0]->content;
    }
    if (prefix_workdir) { // the working directory is: prjNameStr + "_" + workdirFromTpd
      const char* prjNameStr = "unnamedproject";
      XPathObject prjName(run_xpath(xpathCtx, "/TITAN_Project_File_Information/ProjectName/text()"));
      if (prjName->nodesetval && prjName->nodesetval->nodeNr == 1) {
        prjNameStr = (const char*)prjName->nodesetval->nodeTab[0]->content;
      }
      workdir = mprintf("%s_%s", prjNameStr, workdirFromTpd);
    } else {
      workdir = mcopystr(workdirFromTpd);
    }
  }
  if (!folders.has_key(workdir)) {
    // Maybe the tpd was saved with the option "No info about work dir"
    folders.add(workdir, mcopystr(workdir)); // fake it
  }
  const char *real_workdir = folders[workdir]; // This is relative to the location of the tpd file
  excluded_folders.add(real_workdir); // excluded by convention

  autostring abs_workdir;
  // If -D flag was specified then we ignore the workdir
  // in the TPD (the current dir is considered the work dir).
  if (!Dflag) {
    bool hasWorkDir = false;
    // if the working directory does not exist create it
    autostring saved_work_dir(get_working_dir());
    if (set_working_dir(abs_tpd_dir)) {
      ERROR("Could not change to project directory `%s'", (const char*)abs_tpd_dir);
    } else {
      switch (get_path_status(real_workdir)) {
      case PS_FILE:
        ERROR("Cannot create working directory `%s' in project directory `%s' because a file with the same name exists", (const char*)abs_tpd_dir, real_workdir);
        break;
      case PS_DIRECTORY:
        // already exists
        hasWorkDir = true;
        break;
      default:
        if (recursive || local_argc != 0) { // we only want to create workdir if necessary
          printf("Working directory `%s' in project `%s' does not exist, trying to create it...\n", real_workdir, (const char*)abs_tpd_dir);
          int rv = mkdir(real_workdir, 0755);
          if (rv) ERROR("Could not create working directory, mkdir() failed: %s", strerror(errno));
          else printf("Working directory created\n");
          hasWorkDir = true;
        }
      }
    }

    if (local_argc==0) { // if not top level
      set_working_dir(saved_work_dir); // restore working directory
    } else { // if top level
      set_working_dir(real_workdir); // go into the working dir
    }
    if (hasWorkDir) { //we created working directory, or its already been created (from a parent makefilegen process maybe)
      *abs_work_dir_p = get_absolute_dir(real_workdir, abs_tpd_dir);
      abs_workdir = (mcopystr(*abs_work_dir_p));
    }
  }
  /////////////////////////////////////////////////////////////////////////////

  // Gather the excluded folders in the active config
  {
    expstring_t xpathActCfgPaths = mprintf(
      "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
      "/FolderProperties/FolderResource/FolderProperties/ExcludeFromBuild[text()='true']"
      // This was the selection criterium, we need to go up and down for the actual information
      "/parent::*/parent::*/FolderPath/text()",
      actcfg);
    XPathObject theConfigEx(run_xpath(xpathCtx, xpathActCfgPaths));
    Free(xpathActCfgPaths);

    xmlNodeSetPtr nodes = theConfigEx->nodesetval;
    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {

      excluded_folders.add((const char*)nodes->nodeTab[i]->content);
    }
  }

  // Gather individual excluded files in the active config
  {
    expstring_t xpathActCfgPaths = mprintf(
      "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
      "/FileProperties/FileResource/FileProperties/ExcludeFromBuild[text()='true']"
      "/parent::*/parent::*/FilePath/text()",
      actcfg);
    XPathObject theConfigEx(run_xpath(xpathCtx, xpathActCfgPaths));
    Free(xpathActCfgPaths);

    xmlNodeSetPtr nodes = theConfigEx->nodesetval;
    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      xmlNodePtr curnode = nodes->nodeTab[i];

      cstring aa((const char*)curnode->content);
      excluded_files.add(aa, 0);
    }
  }

  // Collect files; filter out excluded ones
  {
    XPathObject  filesObj(run_xpath(xpathCtx,
      "TITAN_Project_File_Information/Files/FileResource"));

    xmlNodeSetPtr nodes = filesObj->nodesetval;
    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      // nodes->nodeTab[i]->name === "FileResource"
      const char *uri = 0, *path = 0, *raw = 0;

      // projectRelativePath is the path as it appears in Project Explorer (illusion)
      // relativeURI is the actual location, relative to the project root (reality)
      // rawURI is present if the relative path can not be calculated
      //
      // Theoretically these attributes could be in any order, loop over them
      for (xmlAttrPtr attr = nodes->nodeTab[i]->properties; attr; attr = attr->next) {
        if (!strcmp((const char*)attr->name, "projectRelativePath")) {
          path = (const char*)attr->children->content;
        }
        else if (!strcmp((const char*)attr->name, "relativeURI")) {
          uri = (const char*)attr->children->content;
        }
        else if (!strcmp((const char*)attr->name, "rawURI")) {
          raw = (const char*)attr->children->content;
        }
        else {
          WARNING("Unknown attribute %s", (const char*)nodes->nodeTab[i]->name);
        }
      } // next attribute

      if (path == NULL) {
        ERROR("A FileResource must have a projectRelativePath");
        continue;
      }

      if (uri == NULL && raw == NULL) {
        ERROR("A FileResource must have either relativeURI or rawURI");
        continue;
      }

      cstring cpath(path);
      if (!is_excluded_file(cpath) && !is_excluded_folder(path)) {
        // relativeURI wins over rawURI
        char *ruri = uri ? mcopystr(uri) : cook(raw, path_vars);

        if (files.has_key(cpath)) {
          ERROR("A FileResource %s must be unique!", (const char*)cpath);
        } else {
          files.add(cpath, ruri); // relativeURI to the TPD location
          { // set the *preprocess value if .ttcnpp file was found
            const size_t ttcnpp_extension_len = 7; // ".ttcnpp"
            const size_t ruri_len = strlen(ruri);
            if ( ruri_len>ttcnpp_extension_len && strcmp(ruri+(ruri_len-ttcnpp_extension_len),".ttcnpp")==0 ) {
              *preprocess = TRUE;
            }
          }
        }
      }
    } // next FileResource
  }

  // Check options
  xsdbool2boolean(xpathCtx, actcfg, "useAbsolutePath", p_aflag);
  xsdbool2boolean(xpathCtx, actcfg, "GNUMake", p_gflag);
  xsdbool2boolean(xpathCtx, actcfg, "dynamicLinking", p_lflag);
  xsdbool2boolean(xpathCtx, actcfg, "functiontestRuntime", p_Rflag);
  xsdbool2boolean(xpathCtx, actcfg, "singleMode", p_sflag);
  xsdbool2boolean(xpathCtx, actcfg, "codeSplitting", p_csflag);
  xsdbool2boolean(xpathCtx, actcfg, "quietly", p_quflag);
  xsdbool2boolean(xpathCtx, actcfg, "disableSubtypeChecking", p_dsflag);
  xsdbool2boolean(xpathCtx, actcfg, "disableBER", p_dbflag);
  xsdbool2boolean(xpathCtx, actcfg, "disableRAW", p_drflag);
  xsdbool2boolean(xpathCtx, actcfg, "disableTEXT", p_dtflag);
  xsdbool2boolean(xpathCtx, actcfg, "disableXER", p_dxflag);
  xsdbool2boolean(xpathCtx, actcfg, "disableJSON", p_djflag);
  xsdbool2boolean(xpathCtx, actcfg, "forceXERinASN.1", p_fxflag);
  xsdbool2boolean(xpathCtx, actcfg, "defaultasOmit", p_doflag);
  xsdbool2boolean(xpathCtx, actcfg, "gccMessageFormat", p_gfflag);
  xsdbool2boolean(xpathCtx, actcfg, "lineNumbersOnlyInMessages", p_lnflag);
  xsdbool2boolean(xpathCtx, actcfg, "includeSourceInfo", p_isflag);
  xsdbool2boolean(xpathCtx, actcfg, "addSourceLineInfo", p_asflag);
  xsdbool2boolean(xpathCtx, actcfg, "suppressWarnings", p_swflag);
  xsdbool2boolean(xpathCtx, actcfg, "outParamBoundness", p_Yflag);

  // Extract the "incremental dependencies" option
  {
    boolean incremental_deps = TRUE;
    xsdbool2boolean(xpathCtx, actcfg, "incrementalDependencyRefresh", &incremental_deps);

    // For makefilegen, "Use GNU make" implies incremental deps by default,
    // unless explicitly disabled by "use makedepend" (a.k.a. mflag).
    // For Eclipse, incremental deps must be explicitly specified,
    // even if GNU make is being used.
    
    if (incremental_deps) {
      if( !(*p_gflag) ) {
        WARNING("Incremental dependency ordered but it requires gnu make");
      }
    }
    else {
      if (*p_gflag) {
        // GNU make but no incremental deps
        *p_mflag = true;
      }
    }
  }

  // Extract the default target option
  // if it is not defined as a command line argument
  if (!(*p_Lflag)) {
    char *defTargetXpath = mprintf(
      "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
      "/ProjectProperties/MakefileSettings/defaultTarget/text()",
      actcfg);
    XPathObject defTargetObj(run_xpath(xpathCtx, defTargetXpath));
    Free(defTargetXpath);
    if (defTargetObj->nodesetval && defTargetObj->nodesetval->nodeNr > 0) {
      const char* content = (const char*)defTargetObj->nodesetval->nodeTab[0]->content;
      if (!strcmp(content, "library")) {
        *p_Lflag = true;
      } else if (!strcmp(content, "executable")) {
        *p_Lflag = false;
      } else {
        ERROR("Unknown default target: '%s'."
            " The available targets are: 'executable', 'library'", content);
      }
    }
  }

  // Executable name (don't care unless top-level invocation)
  if (local_argc != 0)
  {
    char *exeXpath = mprintf(
      "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
      "/ProjectProperties/MakefileSettings/targetExecutable/text()",
      actcfg);
    XPathObject exeObj(run_xpath(xpathCtx, exeXpath));
    Free(exeXpath);
    if (exeObj->nodesetval && exeObj->nodesetval->nodeNr > 0) {
      const char* target_executable = (const char*)exeObj->nodesetval->nodeTab[0]->content;
      autostring target_exe_dir(get_dir_from_path(target_executable));
      autostring target_exe_file(get_file_from_path(target_executable));
      if (target_exe_dir!=NULL) { // if it's not only a file name
        if (get_path_status(target_exe_dir)==PS_NONEXISTENT) {
          if (strcmp(real_workdir,target_exe_dir)!=0) {
            WARNING("Provided targetExecutable directory `%s' does not exist, only file name `%s' will be used", (const char*)target_exe_dir, (const char*)target_exe_file);
          }
          target_executable = target_exe_file;
        }
      }
      if (!*p_ets_name) { // Command line will win
        *p_ets_name = mcopystr(target_executable);
      }
    }
  }

  // create an xml element for the currently processed project
  if (prj_graph_fp) {
    const char* prjNameStr = "???";
    XPathObject  prjName(run_xpath(xpathCtx, "/TITAN_Project_File_Information/ProjectName/text()"));
    if (prjName->nodesetval && prjName->nodesetval->nodeNr == 1) {
      prjNameStr = (const char*)prjName->nodesetval->nodeTab[0]->content;
    }
    autostring tpd_rel_dir(get_relative_dir(tpd_dir, NULL));
    autostring tpd_rel_path(compose_path_name(tpd_rel_dir, (const char*)tpd_filename));
    fprintf(prj_graph_fp, "<project name=\"%s\" uri=\"%s\">\n", prjNameStr, (const char*)tpd_rel_path);
    XPathObject subprojects(run_xpath(xpathCtx, "/TITAN_Project_File_Information/ReferencedProjects/ReferencedProject/attribute::name"));
    xmlNodeSetPtr nodes = subprojects->nodesetval;
    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      const char* refd_name = "???";
      if (!strcmp((const char*)nodes->nodeTab[i]->name, "name")) {
        refd_name = (const char*)nodes->nodeTab[i]->children->content;
      }
      fprintf(prj_graph_fp, "<reference name=\"%s\"/>\n", refd_name);
    }
    fprintf(prj_graph_fp, "</project>\n");
  }

  // Tpd part of the MakefileSettings
  {
    //TTCN3preprocessorIncludes
    char *preincludeXpath = mprintf(
        "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
        "/ProjectProperties/MakefileSettings/TTCN3preprocessorIncludes/listItem/text()",
        actcfg);
    XPathObject preincludeObj(run_xpath(xpathCtx, preincludeXpath));
    Free(preincludeXpath);

    xmlNodeSetPtr nodes = preincludeObj->nodesetval;

    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      char* content = (char*)preincludeObj->nodesetval->nodeTab[i]->content;

      // add includes to the end of list
      if (ttcn3_prep_includes) {
        // go to last element
        struct string_list* last_elem = ttcn3_prep_includes;
        while (last_elem->next) last_elem = last_elem->next;
        // add string to last element if empty or create new last element and add it to that
        if (last_elem->str) {
          last_elem->next = (struct string_list*)Malloc(sizeof(struct string_list));
          last_elem = last_elem->next;
          last_elem->next = NULL;
        }
        replacechar(&content);
        last_elem->str = content;
      }
    }
  }
  {
    //TTCN3preprocessorDefines
    char *ttcn3predefinesXpath = mprintf(
        "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
        "/ProjectProperties/MakefileSettings/TTCN3preprocessorDefines/listItem/text()",
        actcfg);
    XPathObject ttcn3predefinesObj(run_xpath(xpathCtx, ttcn3predefinesXpath));
    Free(ttcn3predefinesXpath);

    xmlNodeSetPtr nodes = ttcn3predefinesObj->nodesetval;

    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      const char* content = (const char*)ttcn3predefinesObj->nodesetval->nodeTab[i]->content;

      // add includes to the end of list
      if (ttcn3_prep_defines) {
        // go to last element
        struct string_list* last_elem = ttcn3_prep_defines;
        while (last_elem->next) last_elem = last_elem->next;
        // add string to last element if empty or create new last element and add it to that
        if (last_elem->str) {
          last_elem->next = (struct string_list*)Malloc(sizeof(struct string_list));
          last_elem = last_elem->next;
          last_elem->next = NULL;
        }
        last_elem->str = mcopystr(content);
      }
    }
  }
  {
    //preprocessorIncludes
    char *preincludesXpath = mprintf(
        "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
        "/ProjectProperties/MakefileSettings/preprocessorIncludes/listItem/text()",
        actcfg);
    XPathObject preincludesObj(run_xpath(xpathCtx, preincludesXpath));
    Free(preincludesXpath);

    xmlNodeSetPtr nodes = preincludesObj->nodesetval;

    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      char* content = (char*)preincludesObj->nodesetval->nodeTab[i]->content;

      // add includes to the end of list
      if (prep_includes) {
        // go to last element
        struct string_list* last_elem = prep_includes;
        while (last_elem->next) last_elem = last_elem->next;
        // add string to last element if empty or create new last element and add it to that
        if (last_elem->str) {
          last_elem->next = (struct string_list*)Malloc(sizeof(struct string_list));
          last_elem = last_elem->next;
          last_elem->next = NULL;
        }
        replacechar(&content);
        last_elem->str = content;
      }
    }
  }
  {
    //preprocessorDefines
    char *predefinesXpath = mprintf(
        "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
        "/ProjectProperties/MakefileSettings/preprocessorDefines/listItem/text()",
        actcfg);
    XPathObject predefinesObj(run_xpath(xpathCtx, predefinesXpath));
    Free(predefinesXpath);

    xmlNodeSetPtr nodes = predefinesObj->nodesetval;

    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      const char* content = (const char*)predefinesObj->nodesetval->nodeTab[i]->content;

      // add includes to the end of list
      if (prep_defines) {
        // go to last element
        struct string_list* last_elem = prep_defines;
        while (last_elem->next) last_elem = last_elem->next;
        // add string to last element if empty or create new last element and add it to that
        if (last_elem->str) {
          last_elem->next = (struct string_list*)Malloc(sizeof(struct string_list));
          last_elem = last_elem->next;
          last_elem->next = NULL;
        }
        last_elem->str = mcopystr(content);
      }
    }
  }
  {
    char *cxxCompilerXpath = mprintf(
            "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
            "/ProjectProperties/MakefileSettings/CxxCompiler/text()",
            actcfg);
    XPathObject cxxCompilerObj(run_xpath(xpathCtx, cxxCompilerXpath));
    Free(cxxCompilerXpath);
    xmlNodeSetPtr nodes = cxxCompilerObj->nodesetval;
    if (nodes) {
      *cxxcompiler = mcopystr((const char*)cxxCompilerObj->nodesetval->nodeTab[0]->content);
    }
  }
  {
    char *optLevelXpath = mprintf(
        "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
        "/ProjectProperties/MakefileSettings/optimizationLevel/text()",
        actcfg);
    XPathObject optLevelObj(run_xpath(xpathCtx, optLevelXpath));
    Free(optLevelXpath);
    xmlNodeSetPtr nodes = optLevelObj->nodesetval;
    if (nodes) {
      *optlevel = mcopystr((const char*)optLevelObj->nodesetval->nodeTab[0]->content);
    }
  }
  {
    char *optFlagsXpath = mprintf(
        "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
        "/ProjectProperties/MakefileSettings/otherOptimizationFlags/text()",
        actcfg);
    XPathObject optFlagsObj(run_xpath(xpathCtx, optFlagsXpath));
    Free(optFlagsXpath);
    xmlNodeSetPtr nodes = optFlagsObj->nodesetval;
    if (nodes) {
      *optflags = mcopystr((const char*)optFlagsObj->nodesetval->nodeTab[0]->content);
    }
  }
  {
    //SolarisSpecificLibraries
    char *solspeclibXpath = mprintf(
        "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
        "/ProjectProperties/MakefileSettings/SolarisSpecificLibraries/listItem/text()",
        actcfg);
    XPathObject solspeclibObj(run_xpath(xpathCtx, solspeclibXpath));
    Free(solspeclibXpath);

    xmlNodeSetPtr nodes = solspeclibObj->nodesetval;

    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      char* content = (char*)solspeclibObj->nodesetval->nodeTab[i]->content;

      // add includes to the end of list
      if (solspeclibs) {
        // go to last element
        struct string_list* last_elem =solspeclibs;
        while (last_elem->next) last_elem = last_elem->next;
        // add string to last element if empty or create new last element and add it to that
        if (last_elem->str) {
          last_elem->next = (struct string_list*)Malloc(sizeof(struct string_list));
          last_elem = last_elem->next;
          last_elem->next = NULL;
        }
        replacechar(&content);
        last_elem->str = content;
      }
    }
  }
  {
    //Solaris8SpecificLibraries
    char *sol8speclibXpath = mprintf(
        "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
        "/ProjectProperties/MakefileSettings/Solaris8SpecificLibraries/listItem/text()",
        actcfg);
    XPathObject sol8speclibObj(run_xpath(xpathCtx, sol8speclibXpath));
    Free(sol8speclibXpath);

    xmlNodeSetPtr nodes = sol8speclibObj->nodesetval;

    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      char* content = (char*)sol8speclibObj->nodesetval->nodeTab[i]->content;

      // add includes to the end of list
      if (sol8speclibs) {
        // go to last element
        struct string_list* last_elem = sol8speclibs;
        while (last_elem->next) last_elem = last_elem->next;
        // add string to last element if empty or create new last element and add it to that
        if (last_elem->str) {
          last_elem->next = (struct string_list*)Malloc(sizeof(struct string_list));
          last_elem = last_elem->next;
          last_elem->next = NULL;
        }
        replacechar(&content);
        last_elem->str = content;
      }
    }
  }
  {
    //LinuxSpecificLibraries
    char *linuxspeclibXpath = mprintf(
        "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
        "/ProjectProperties/MakefileSettings/LinuxSpecificLibraries/listItem/text()",
        actcfg);
    XPathObject linuxspeclibObj(run_xpath(xpathCtx, linuxspeclibXpath));
    Free(linuxspeclibXpath);

    xmlNodeSetPtr nodes = linuxspeclibObj->nodesetval;

    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      char* content = (char*)linuxspeclibObj->nodesetval->nodeTab[i]->content;

      // add includes to the end of list
      if (linuxspeclibs) {
        // go to last element
        struct string_list* last_elem = linuxspeclibs;
        while (last_elem->next) last_elem = last_elem->next;
        // add string to last element if empty or create new last element and add it to that
        if (last_elem->str) {
          last_elem->next = (struct string_list*)Malloc(sizeof(struct string_list));
          last_elem = last_elem->next;
          last_elem->next = NULL;
        }
        replacechar(&content);
        last_elem->str = content;
      }
    }
  }
  {
    //FreeBSDSpecificLibraries
    char *freebsdspeclibXpath = mprintf(
        "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
        "/ProjectProperties/MakefileSettings/FreeBSDSpecificLibraries/listItem/text()",
        actcfg);
    XPathObject freebsdspeclibObj(run_xpath(xpathCtx, freebsdspeclibXpath));
    Free(freebsdspeclibXpath);

    xmlNodeSetPtr nodes = freebsdspeclibObj->nodesetval;

    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      char* content = (char*)freebsdspeclibObj->nodesetval->nodeTab[i]->content;

      // add includes to the end of list
      if (freebsdspeclibs) {
        // go to last element
        struct string_list* last_elem = freebsdspeclibs;
        while (last_elem->next) last_elem = last_elem->next;
        // add string to last element if empty or create new last element and add it to that
        if (last_elem->str) {
          last_elem->next = (struct string_list*)Malloc(sizeof(struct string_list));
          last_elem = last_elem->next;
          last_elem->next = NULL;
        }
        replacechar(&content);
        last_elem->str = content;
      }
    }
  }
  {
    //Win32SpecificLibraries
    char *win32speclibXpath = mprintf(
        "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
        "/ProjectProperties/MakefileSettings/Win32SpecificLibraries/listItem/text()",
        actcfg);
    XPathObject win32speclibObj(run_xpath(xpathCtx, win32speclibXpath));
    Free(win32speclibXpath);

    xmlNodeSetPtr nodes = win32speclibObj->nodesetval;

    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      char* content = (char*)win32speclibObj->nodesetval->nodeTab[i]->content;

      // add includes to the end of list
      if (win32speclibs) {
        // go to last element
        struct string_list* last_elem = win32speclibs;
        while (last_elem->next) last_elem = last_elem->next;
        // add string to last element if empty or create new last element and add it to that
        if (last_elem->str) {
          last_elem->next = (struct string_list*)Malloc(sizeof(struct string_list));
          last_elem = last_elem->next;
          last_elem->next = NULL;
        }
        replacechar(&content);
        last_elem->str = content;
      }

    }
  }
  {
    //TTCN3preprocessor
    char *ttcn3preproc = mprintf(
        "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
        "/ProjectProperties/MakefileSettings/TTCN3preprocessor/text()",
        actcfg);
    XPathObject ttcn3preprocObj(run_xpath(xpathCtx, ttcn3preproc));
    Free(ttcn3preproc);
    xmlNodeSetPtr nodes = ttcn3preprocObj->nodesetval;
    if (nodes) {
      *ttcn3prep = mcopystr((const char*)ttcn3preprocObj->nodesetval->nodeTab[0]->content);
    }
  }
  {
    // additionalObjects
    char *additionalObjectsXpath = mprintf(
        "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
        "/ProjectProperties/MakefileSettings/additionalObjects/listItem/text()",
        actcfg);
    XPathObject additionalObjectsObj(run_xpath(xpathCtx, additionalObjectsXpath));
    Free(additionalObjectsXpath);

    xmlNodeSetPtr nodes = additionalObjectsObj->nodesetval;

    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      char* content = (char*)additionalObjectsObj->nodesetval->nodeTab[i]->content;

      // add to the end of list
      if (additionalObjects) {
        // go to last element
        struct string_list* last_elem = additionalObjects;
        while (last_elem->next) last_elem = last_elem->next;
        // add string to last element if empty or create new last element and add it to that
        if (last_elem->str) {
          last_elem->next = (struct string_list*)Malloc(sizeof(struct string_list));
          last_elem = last_elem->next;
          last_elem->next = NULL;
        }
        replacechar(&content);
        last_elem->str = content;
      }
    }
  }
  {
    //linkerLibraries
    char *linkerlibsXpath = mprintf(
        "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
        "/ProjectProperties/MakefileSettings/linkerLibraries/listItem/text()",
        actcfg);
    XPathObject linkerlibsObj(run_xpath(xpathCtx, linkerlibsXpath));
    Free(linkerlibsXpath);

    xmlNodeSetPtr nodes = linkerlibsObj->nodesetval;

    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      char* content = (char*)linkerlibsObj->nodesetval->nodeTab[i]->content;

      // add includes to the end of list
      if (linkerlibs) {
        // go to last element
        struct string_list* last_elem = linkerlibs;
        while (last_elem->next) last_elem = last_elem->next;
        // add string to last element if empty or create new last element and add it to that
        if (last_elem->str) {
          last_elem->next = (struct string_list*)Malloc(sizeof(struct string_list));
          last_elem = last_elem->next;
          last_elem->next = NULL;
        }
        replacechar(&content);
        last_elem->str = content;
      }
    }
  }
  {
    //linkerLibrarySearchPath
    char *linkerlibsearchXpath = mprintf(
        "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
        "/ProjectProperties/MakefileSettings/linkerLibrarySearchPath/listItem/text()",
        actcfg);
    XPathObject linkerlibsearchObj(run_xpath(xpathCtx, linkerlibsearchXpath));
    Free(linkerlibsearchXpath);

    xmlNodeSetPtr nodes = linkerlibsearchObj->nodesetval;

    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      char* content = (char*)linkerlibsearchObj->nodesetval->nodeTab[i]->content;

      // add includes to the end of list
      if (linkerlibsearchp) {
        // go to last element
        struct string_list* last_elem = linkerlibsearchp;
        while (last_elem->next) last_elem = last_elem->next;
        // add string to last element if empty or create new last element and add it to that
        if (last_elem->str) {
          last_elem->next = (struct string_list*)Malloc(sizeof(struct string_list));
          last_elem = last_elem->next;
          last_elem->next = NULL;
        }
        replacechar(&content);
        last_elem->str = content;
      }
    }
  }

  if (generatorCommandOutput && local_argc != 0) { // only in case of top-level invocation
    char* generatorCommandXpath = mprintf(
      "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
      "/ProjectProperties/MakefileSettings/ProjectSpecificRulesGenerator/GeneratorCommand/text()",
      actcfg);
    XPathObject generatorCommandObj(run_xpath(xpathCtx, generatorCommandXpath));
    Free(generatorCommandXpath);
    autostring generatorCommand;
    if (generatorCommandObj->nodesetval && generatorCommandObj->nodesetval->nodeNr > 0) {
      generatorCommand = mcopystr((const char*)generatorCommandObj->nodesetval->nodeTab[0]->content);
      // run the command and capture the output
      printf("Executing generator command `%s' specified in `%s'...\n", (const char*)generatorCommand, (const char*)abs_tpd_name);
      FILE* gc_fp = popen(generatorCommand, "r");
      if (!gc_fp) {
        ERROR("Could not execute command `%s'", (const char*)generatorCommand);
      } else {
        char buff[1024];
        while (fgets(buff, sizeof(buff), gc_fp)!=NULL) {
          *generatorCommandOutput = mputstr(*generatorCommandOutput, buff);
        }
        pclose(gc_fp);
      }
    }
  }

  if (target_placement_list && local_argc != 0) { // only in case of top-level invocation
    char* targetPlacementXpath = mprintf(
      "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
      "/ProjectProperties/MakefileSettings/ProjectSpecificRulesGenerator/Targets/Target/attribute::*",
      actcfg);
    XPathObject targetPlacementObj(run_xpath(xpathCtx, targetPlacementXpath));
    Free(targetPlacementXpath);
    xmlNodeSetPtr nodes = targetPlacementObj->nodesetval;
    const char* targetName = NULL;
    const char* targetPlacement = NULL;
    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      if (!strcmp((const char*)nodes->nodeTab[i]->name, "name")) {
        targetName = (const char*)nodes->nodeTab[i]->children->content;
      }
      else if (!strcmp((const char*)nodes->nodeTab[i]->name,"placement")) {
        targetPlacement = (const char*)nodes->nodeTab[i]->children->content;
      }
      if (targetName && targetPlacement) { // collected both
        if (target_placement_list) {
          // go to last element
          struct string2_list* last_elem = target_placement_list;
          while (last_elem->next) last_elem = last_elem->next;
          // add strings to last element if empty or create new last element and add it to that
          if (last_elem->str1) {
            last_elem->next = (struct string2_list*)Malloc(sizeof(struct string2_list));
            last_elem = last_elem->next;
            last_elem->next = NULL;
          }
          last_elem->str1 = mcopystr(targetName);
          last_elem->str2 = mcopystr(targetPlacement);
        }
        targetName = targetPlacement = NULL; // forget both
      }
    }
  }

  // Referenced projects
  {
    XPathObject subprojects(run_xpath(xpathCtx,
      "/TITAN_Project_File_Information/ReferencedProjects/ReferencedProject/attribute::*"));
    xmlNodeSetPtr nodes = subprojects->nodesetval;
    const char *name = NULL, *projectLocationURI = NULL;
    if (nodes) for (int i = 0; i < nodes->nodeNr; ++i) {
      // FIXME: this assumes every ReferencedProject has name and URI.
      // This is not necessarily so if the referenced project was closed
      // when the project was exported to TPD.
      // Luckily, the name from the closed project will be overwritten
      // by the name from the next ReferencedProject. However, if some pervert
      // changes the next ReferencedProject to have the projectLocationURI
      // as the first attribute, it will be joined to the name
      // of the previous, closed, ReferencedProject.
      if (!strcmp((const char*)nodes->nodeTab[i]->name, "name")) {
        name = (const char*)nodes->nodeTab[i]->children->content;
      }
      else if (!strcmp((const char*)nodes->nodeTab[i]->name,"projectLocationURI")) {
        projectLocationURI = (const char*)nodes->nodeTab[i]->children->content;
      }

      if (name && projectLocationURI) { // collected both
        // see if there is a specified configuration for the project
        const char *my_actcfg = NULL;
        autostring req_xpath(mprintf(
          "/TITAN_Project_File_Information/Configurations/Configuration[@name='%s']"
          "/ProjectProperties/ConfigurationRequirements/configurationRequirement"
          "/projectName[text()='%s']"
          // Up to this point, we selected the projectName node which contains
          // the name of the subproject. But we want its sibling.
          // So we go up one and down the other path.
          "/parent::*/rerquiredConfiguration/text()",
          //Yes, it's rerquiredConfiguration; the Designer misspells it :(
          actcfg, name));
        XPathObject reqcfgObj(run_xpath(xpathCtx, req_xpath));
        if (reqcfgObj->nodesetval && reqcfgObj->nodesetval->nodeNr == 1) {
          my_actcfg = (const char*)reqcfgObj->nodesetval->nodeTab[0]->content;
        }

        int my_argc = 0;
        char *my_args[] = { NULL };
        char **my_argv = my_args + 0;
        int my_optind = 0;
        boolean my_gflag = *p_gflag, my_aflag = *p_aflag, my_cflag = *p_cflag, // pass down
          my_Rflag = *p_Rflag, my_Pflag = *p_Pflag,
          my_sflag =  0, my_Lflag =  0, my_lflag =  0, my_mflag =  0, my_csflag = 0,
          my_quflag = 0, my_dsflag = 0, my_dbflag = 0, my_drflag = 0,
          my_dtflag = 0, my_dxflag = 0, my_djflag = 0, my_fxflag = 0, my_doflag = 0, 
          my_gfflag = 0, my_lnflag = 0, my_isflag = 0, my_asflag = 0, 
          my_swflag = 0, my_Yflag = 0;

        char *my_ets = NULL;

        autostring abs_projectLocationURI(
          compose_path_name(abs_tpd_dir, projectLocationURI));

        char* sub_proj_abs_work_dir = NULL;
        tpd_result success = process_tpd_internal((const char*)abs_projectLocationURI,
          my_actcfg, file_list_path, &my_argc, &my_argv, &my_optind, &my_ets,
          &my_gflag, &my_sflag, &my_cflag, &my_aflag, preprocess, &my_Rflag, &my_lflag,
          &my_mflag, &my_Pflag, &my_Lflag, recursive, force_overwrite, gen_only_top_level, NULL, &sub_proj_abs_work_dir,
          sub_project_dirs, program_name, prj_graph_fp, create_symlink_list, ttcn3_prep_includes, ttcn3_prep_defines, prep_includes, prep_defines, &my_csflag,
          &my_quflag, &my_dsflag, cxxcompiler, optlevel, optflags, &my_dbflag, &my_drflag,
          &my_dtflag, &my_dxflag, &my_djflag, &my_fxflag, &my_doflag,
          &my_gfflag, &my_lnflag, &my_isflag, &my_asflag, &my_swflag, &my_Yflag, solspeclibs, sol8speclibs, linuxspeclibs, freebsdspeclibs, win32speclibs,
          ttcn3prep, linkerlibs, additionalObjects, linkerlibsearchp, Vflag, FALSE, NULL, NULL, prefix_workdir, run_command_list, seen_tpd_files);
        autostring sub_proj_abs_work_dir_as(sub_proj_abs_work_dir); // ?!

        if (success == TPD_SUCCESS) {

          if (recursive) { // call ttcn3_makefilegen on referenced project's tpd file
            // -r is not needed any more because top level process traverses all projects recursively
            expstring_t command = mprintf("%s -cVD", program_name);
            if (force_overwrite) command = mputc(command, 'f');
            if (prefix_workdir) command = mputc(command, 'W');
            if (*p_gflag) command = mputc(command, 'g');
            if (*p_sflag) command = mputc(command, 's');
            if (*p_aflag) command = mputc(command, 'a');
            if (*p_Rflag) command = mputc(command, 'R');
            if (*p_lflag) command = mputc(command, 'l');
            if (*p_mflag) command = mputc(command, 'm');
            command = mputstr(command, " -t ");
            command = mputstr(command, (const char*)abs_projectLocationURI);
            if (my_actcfg) {
              command = mputstr(command, " -b ");
              command = mputstr(command, my_actcfg);
            }

            autostring sub_tpd_dir(get_dir_from_path((const char*)abs_projectLocationURI));
            const char * sub_proj_effective_work_dir = sub_proj_abs_work_dir ? sub_proj_abs_work_dir : (const char*)sub_tpd_dir;
            if (!gen_only_top_level) {
              if (run_command_list) {
                // go to last element
                struct string2_list* last_elem = run_command_list;
                while (last_elem->next) last_elem = last_elem->next;
                // add strings to last element if empty or create new last element and add it to that
                if (last_elem->str1 || last_elem->str2) {
                  last_elem->next = (struct string2_list*)Malloc(sizeof(struct string2_list));
                  last_elem = last_elem->next;
                  last_elem->next = NULL;
                }
                last_elem->str1 = mcopystr(sub_proj_effective_work_dir);
                last_elem->str2 = command;
              } else {
                ERROR("Internal error: cannot add command to list");
              }
            }
            // add working dir to the end of list
            if (sub_project_dirs) {
              // go to last element
              struct string_list* last_elem = sub_project_dirs;
              while (last_elem->next) last_elem = last_elem->next;
              // add string to last element if empty or create new last element and add it to that
              if (last_elem->str) {
                last_elem->next = (struct string_list*)Malloc(sizeof(struct string_list));
                last_elem = last_elem->next;
                last_elem->next = NULL;
              }
              autostring cwd_as(get_working_dir());
              last_elem->str = (*p_aflag) ? mcopystr(sub_proj_effective_work_dir) : get_relative_dir(sub_proj_effective_work_dir, (const char*)cwd_as);
            }
          }

          for (int z = 0; z < my_argc; ++z) {
            if (*p_cflag) {
              // central storage, keep in separate container
              base_files.add(my_argv[z]); // string was allocated with new
            }
            else {
              const cstring tmp(my_argv[z]);
              if (!files.has_key(tmp)){
                files.add(tmp, my_argv[z]);
              } else {
                Free(my_argv[z]);
              }
            }
          }

          Free(my_argv); // free the array; we keep the pointers
          Free(my_ets);
        }
        else if (success == TPD_FAILED) {
          ERROR("Failed to process %s", (const char*)abs_projectLocationURI);
        }
        // else TPD_SKIPPED, keep quiet

        name = projectLocationURI = NULL; // forget both
      }
    } // next referenced project
  }

  if (output_file) {
    if (get_path_status(output_file) == PS_DIRECTORY) {
      // points to existing dir; use as-is
    }
    else { // we assume it points to a file: not our problem
      output_file = NULL;
    }
  }

  // (argc - optind) is the number of non-option arguments (assumed to be files)
  // given on the command line.
  int new_argc = local_argc - local_optind + files.size() + base_files.size();
  char ** new_argv = (char**)Malloc(sizeof(char*) * new_argc);

  int n = 0;

  // First, copy the filenames gathered from the TPD
  //
  // We symlink the files into the working directory
  // and pass only the filename to the makefile generator
  for (int nf = files.size(); n < nf; ++n) {
    const char *fn = files.get_nth_elem(n); // relativeURI to the TPD location
    autostring  dir_n (get_dir_from_path (fn));
    autostring  file_n(get_file_from_path(fn));
    autostring  rel_n (get_absolute_dir(dir_n, abs_tpd_dir));
    autostring  abs_n (compose_path_name(rel_n, file_n));

    if (local_argc == 0) {
      // We are being invoked recursively, for a referenced TPD.
      // Do not symlink; just return absolute paths to the files.
      if (*fn == '/') {
        if (*p_cflag) {
          // compose with workdir
          new_argv[n] = compose_path_name(abs_workdir, file_n);
        } else {
          // it's an absolute path, copy verbatim
          new_argv[n] = mcopystr(fn); // fn will be destroyed, pass a copy
        }
      }
      else { // relative path
        if (*p_cflag) {
          // compose with workdir
          new_argv[n] = compose_path_name(abs_workdir, file_n);
          // Do not call file_n.extract() : the composed path will be returned,
          // its component will need to be deallocated here.
        }
        else {
          // compose with tpd dir
          new_argv[n] = const_cast<char*>(abs_n.extract());
        }
      }
    }
    else { // we are processing the top-level TPD
#ifndef MINGW
      if (!*p_Pflag) {
        int fd = open(abs_n, O_RDONLY);
        if (fd >= 0) { // successfully opened
          close(fd);
          if (output_file) {
            file_n = compose_path_name(output_file, file_n);
          }
//TODO ! compose with output_file
          // save into list: add symlink data to the end of list
          if (create_symlink_list) {
            // go to last element
            struct string2_list* last_elem = create_symlink_list;
            while (last_elem->next) last_elem = last_elem->next;
            // add strings to last element if empty or create new last element and add it to that
            if (last_elem->str1) {
              last_elem->next = (struct string2_list*)Malloc(sizeof(struct string2_list));
              last_elem = last_elem->next;
              last_elem->next = NULL;
            }
            last_elem->str1 = mcopystr(abs_n);
            last_elem->str2 = mcopystr(file_n);
          } 
        }
        else {
          ERROR("%s does not exist", (const char*)abs_n);
        }
      }
#endif
      if (*p_Pflag) {
        if (*p_aflag) {
          puts((const char *)abs_n);
        } else {
          autostring dir_part(get_dir_from_path(abs_n));
          autostring file_part(get_file_from_path(abs_n));
          autostring rel_dir_part(get_relative_dir((const char *)dir_part, file_list_path ? file_list_path : (const char *)abs_tpd_dir));
          autostring rel_dir_file_part(compose_path_name((const char *)rel_dir_part, (const char *)file_part));
          puts((const char *)rel_dir_file_part);
        }
      }
      new_argv[n] = const_cast<char *>(file_n.extract());
    }
  }
  // Print the TPD too.
  if (*p_Pflag) {
    autostring dir_part(get_dir_from_path(p_tpd_name));
    autostring file_part(get_file_from_path(p_tpd_name));
    if (*p_aflag) {
      puts((const char *)abs_tpd_name);
    } else {
      autostring rel_dir_part(get_relative_dir(dir_part, file_list_path ? file_list_path : abs_tpd_dir));
      autostring rel_dir_file_part(compose_path_name(rel_dir_part, file_part));
      const char *rel_tpd_name = (const char *)rel_dir_file_part;
      puts(rel_tpd_name);
    }
  }

  // base_files from referenced projects
  for (size_t bf = 0, bs = base_files.size(); bf < bs; ++bf, ++n) {
    new_argv[n] = base_files[bf];
  }
  base_files.clear(); // string ownership transfered

  // Then, copy the filenames from the command line.
  for (int a = *p_optind; a < *p_argc; ++a, ++n) {
    // String may be from main's argv; copy to the heap.
    new_argv[n] = mcopystr((*p_argv)[a]);
  }

  if (local_argc > 0) { // it is the outermost call
    clear_seen_tpd_files(seen_tpd_files);
  }
  // replace argv
  *p_argv = new_argv;
  *p_argc = new_argc;
  *p_optind = 0;

  // finally...
  for (size_t i = 0, e = files.size(); i < e; ++i) {
    Free(const_cast<char*>(files.get_nth_elem(i)));
  }
  files.clear();

  for (size_t i = 0, e = folders.size(); i < e; ++i) {
    Free(const_cast<char*>(folders.get_nth_elem(i)));
  }
  folders.clear();

  excluded_files.clear();
  excluded_folders.clear();
  path_vars.clear();

  xmlCleanupParser();
  // ifdef debug
    xmlMemoryDump();
  return TPD_SUCCESS;
}
