///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#ifndef XPATHER_H_
#define XPATHER_H_

#include <stdio.h>

typedef int boolean;
#define TRUE 1
#define FALSE 0

enum tpd_result { TPD_FAILED = -1, TPD_SKIPPED = 0, TPD_SUCCESS = 1 };

struct string_list {
char* str;
struct string_list* next;
};

struct string2_list {
char* str1;
char* str2;
struct string2_list* next;
};

/**
 *
 * @param p_tpd_name filename
 * @param actcfg actual configuration name
 * @param file_list_path the argument of the -P switch
 * @param argc pointer to argv
 * @param argv pointer to argc
 * @param optind pointer to optind
 * @param ets_name from the -e switch
 * @param gnu_make from the -g switch
 * @param single_mode -s switch
 * @param central_storage -c switch
 * @param absolute_paths -a switch
 * @param preprocess -p switch
 * @param use_runtime_2 -R switch
 * @param dynamic dynamic linking, -l switch
 * @param makedepend -m switch
 * @param filelist -P switch
 * @param recursive -r switch
 * @param force_overwrite -F switch
 * @param output_file from the -o switch
 * @param abs_work_dir workingDirectory element of TPD will be stored here, must be Free()'d
 * @param sub_project_dirs directories of sub-projects that need to be built before this one,
                           if set to NULL when calling this function then it won't be set, otherwise it must be deallocated after the call
 * @param program_name original value of argv[0], which is the executable path used to start this program
 * @param prj_graph_fp write project graph xml data into this file if not NULL
 * @create_symlink_list a list of symlinks to be created
 * @param ttcn3_prep_includes TTCN3preprocessorIncludes
 * @param ttcn3_prep_defines preprocessorDefines
 * @param prep_includes preprocessorIncludes
 * @param prep_defines preprocessorDefines
 * @param codesplit codeSplitting
 * @param quietly quietly
 * @param disablesubtypecheck disableSubtypeChecking
 * @param cxxcompiler CxxCompiler
 * @param optlevel optimizationLevel
 * @param optflags otherOptimizationFlags
 * @param disableber disableBER -b
 * @param disableraw disableRAW -r
 * @param disabletext disableTEXT -x
 * @param disablexer disableXER -X
 * @param disablejson disableJSON -j
 * @param forcexerinasn forceXERinASN.1 -a
 * @param defaultasomit defaultasOmit -d
 * @param gccmessageformat gccMessageFormat -g
 * @param linenumber lineNumbersOnlyInMessages -i
 * @param includesourceinfo includeSourceInfo -L
 * @param addsourcelineinfo addSourceLineInfo -l
 * @param suppresswarnings suppressWarnings -S
 * @param outparamboundness  outParamBoundness -Y
 * @param solspeclibs SolarisSpecificLibraries
 * @param sol8speclibs Solaris8SpecificLibraries
 * @param linuxspeclibs LinuxSpecificLibraries
 * @param freebsdspeclibs FreeBSDSpecificLibraries
 * @param win32speclibs Win32SpecificLibraries
 * @param ttcn3preprocessor ttcn3preprocessor
 * @param linkerlibs linkerlibs
 * @param linkerlibrarysearchpath linkerlibrarysearchpath
 * @param Vflag -V switch
 * @param Dflag -D switch
 * @param generatorCommandOutput the output of the command that generates a Makefile snippet
 * @param target_placement_list a list of (target,placement) strings pairs from the TPD
 * @param prefix_workdir prefix working directory with project name
 * @param run_command_list contains the working directories and the makefilegen commands to be called there
 * @return TPD_SUCESS if parsing successful, TPD_SKIPPED if the tpd
 *         was seen already, or TPD_FAILED on error.
 */
#ifdef __cplusplus
extern "C"
#else
enum
#endif
tpd_result process_tpd(const char *p_tpd_name, const char *actcfg,
  const char *file_list_path,
  int *argc, char ***argv,
  int *optind, char **ets_name,
  boolean *gnu_make, boolean *single_mode,
  boolean *central_storage, boolean *absolute_paths,
  boolean *preprocess, boolean *use_runtime_2,
  boolean *dynamic, boolean *makedepend, boolean *filelist,
  boolean *library, boolean recursive, boolean force_overwrite, boolean gen_only_top_level,
  const char *output_file, char** abs_work_dir, struct string_list* sub_project_dirs,
  const char* program_name, FILE* prj_graph_fp, struct string2_list* create_symlink_list, struct string_list* ttcn3_prep_includes,
  struct string_list* ttcn3_prep_defines, struct string_list* prep_includes, struct string_list* prep_defines,
  boolean *codesplit, boolean *quietly, boolean *disablesubtypecheck, char** cxxcompiler,
  char** optlevel, char** optflags, boolean *disableber, boolean *disableraw, boolean *disabletext, boolean *disablexer,
  boolean *disablejson, boolean *forcexerinasn, boolean *defaultasomit, boolean *gccmessageformat,
  boolean *linenumber, boolean *includesourceinfo, boolean *addsourcelineinfo, boolean *suppresswarnings,
  boolean *outparamboundness, struct string_list* solspeclibs, struct string_list* sol8speclibs,
  struct string_list* linuxspeclibs, struct string_list* freebsdspeclibs, struct string_list* win32speclibs,
  char** ttcn3preprocessor, struct string_list* linkerlibs, struct string_list* additionalObjects, struct string_list* linkerlibsearchpath, boolean Vflag, boolean Dflag,
  char** generatorCommandOutput, struct string2_list* target_placement_list, boolean prefix_workdir, struct string2_list* run_command_list);

#endif /* XPATHER_H_ */
