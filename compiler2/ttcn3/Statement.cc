///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2000-2014 Ericsson Telecom AB
// All rights reserved. This program and the accompanying materials
// are made available under the terms of the Eclipse Public License v1.0
// which accompanies this distribution, and is available at
// http://www.eclipse.org/legal/epl-v10.html
///////////////////////////////////////////////////////////////////////////////
#include "../../common/dbgnew.hh"
#include "Statement.hh"
#include "Ttcnstuff.hh"
#include "../TypeCompat.hh"
#include "../CompType.hh"
#include "../CompField.hh"
#include "../SigParam.hh"
#include "TtcnTemplate.hh"
#include "ILT.hh"
#include "ArrayDimensions.hh"
#include "../Int.hh"
#include "../main.hh"
#include "Attributes.hh"

namespace Ttcn {

  // =================================
  // ===== StatementBlock
  // =================================

  StatementBlock::StatementBlock()
    : Scope(), checked(false), labels_checked(false), my_sb(0), my_def(0), exception_handling(EH_NONE)
  {
  }

  StatementBlock::~StatementBlock()
  {
    for(size_t i=0; i<stmts.size(); i++)
      delete stmts[i];
    stmts.clear();
    defs.clear();
    labels.clear();
  }

  StatementBlock *StatementBlock::clone() const
  {
    FATAL_ERROR("StatementBlock::clone");
  }

  void StatementBlock::dump(unsigned int level) const
  {
    size_t n = stmts.size();
    DEBUG(level, "StatementBlock at %p with %lu", (const void*)this,
      (unsigned long) n);
    for (size_t i = 0; i < n; ++i) {
      stmts[i]->dump(level+1);
    }
  }

  void StatementBlock::add_stmt(Statement *p_stmt, bool to_front)
  {
    if(!p_stmt)
      FATAL_ERROR("StatementBlock::add_stmt()");
    if (to_front) {
      stmts.add_front(p_stmt);
    } else {
      stmts.add(p_stmt);
    }
    p_stmt->set_my_scope(this);
    p_stmt->set_my_sb(this, stmts.size()-1);
  }

  void StatementBlock::set_my_scope(Scope *p_scope)
  {
    set_parent_scope(p_scope);
    for(size_t i=0; i<stmts.size(); i++)
      stmts[i]->set_my_scope(this);
  }

  void StatementBlock::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    for(size_t i=0; i<stmts.size(); i++)
      stmts[i]->set_fullname(p_fullname+".stmt_"+Int2string(i+1));
  }

  /** \todo handle loops and conditional statements */
  Statement *StatementBlock::get_first_stmt() const
  {
    size_t nof_stmts = stmts.size();
    for (size_t i = 0; i < nof_stmts; i++) {
      Statement *stmt = stmts[i];
      switch (stmt->get_statementtype()) {
      case Statement::S_LABEL:
	// ignore and go to the next statement
	break;
      case Statement::S_BLOCK: {
	// if the block is not empty return its first statement
	// otherwise go to the next statement
	Statement *first_stmt = stmt->get_block()->get_first_stmt();
	if (first_stmt) return first_stmt;
	else break; }
      case Statement::S_DOWHILE: {
	// if the block is not empty return its first statement
	// otherwise return the whole do-while statement
	Statement *first_stmt = stmt->get_block()->get_first_stmt();
	if (first_stmt) return first_stmt;
	else return stmt; }
      default:
        return stmt;
      }
    }
    return 0;
  }

  void StatementBlock::register_def(Definition *p_def)
  {
    if(!p_def) FATAL_ERROR("StatementBlock::register_def()");
    const Identifier& id=p_def->get_id();
    if (defs.has_key(id)) {
      const char *dispname = id.get_dispname().c_str();
      p_def->error("Duplicate definition with identifier `%s'", dispname);
      defs[id]->note("Previous definition with identifier `%s' is here",
	dispname);
    } else {
      defs.add(id, p_def);
      if (parent_scope) {
	if (parent_scope->has_ass_withId(id)) {
	  const char *dispname = id.get_dispname().c_str();
	  p_def->error("Definition with identifier `%s' is not unique"
                       " in the scope hierarchy", dispname);
	  Reference ref(0, id.clone());
	  Common::Assignment *ass = parent_scope->get_ass_bySRef(&ref);
	  if (!ass) FATAL_ERROR("StatementBlock::register_def()");
	  ass->note("Previous definition with identifier `%s' in higher "
                     "scope unit is here", dispname);
	} else if (parent_scope->is_valid_moduleid(id)) {
	  p_def->warning("Definition with name `%s' hides a module identifier",
	    id.get_dispname().c_str());
	}
      }
    }
  }

  bool StatementBlock::has_ass_withId(const Identifier& p_id)
  {
    return defs.has_key(p_id) || get_parent_scope()->has_ass_withId(p_id);
  }

  Common::Assignment* StatementBlock::get_ass_bySRef(Ref_simple *p_ref)
  {
    if(p_ref->get_modid()) return get_parent_scope()->get_ass_bySRef(p_ref);
    const Identifier& id=*p_ref->get_id();
    if(defs.has_key(id)) return defs[id];
    else return get_parent_scope()->get_ass_bySRef(p_ref);
  }

  Type *StatementBlock::get_mtc_system_comptype(bool is_system, bool is_connecting)
  {
    // return NULL outside test cases
    if (!my_def || ((is_system || !is_connecting) &&
                    my_def->get_asstype() != Common::Assignment::A_TESTCASE)) {
      return 0;
    }
    if (is_system) {
      Def_Testcase *t_tc = dynamic_cast<Def_Testcase*>(my_def);
      if (!t_tc) FATAL_ERROR("StatementBlock::get_mtc_system_comptype()");
      Type *system_ct = t_tc->get_SystemType();
      if (system_ct) return system_ct;
      // otherwise (if the testcase has no system clause) the type of 'system'
      // is the same as the type of 'mtc' (which is given in 'runs on' clause)
    }
    return my_def->get_RunsOnType();
  }

  Ttcn::StatementBlock *StatementBlock::get_statementblock_scope()
  {
    return this;
  }

  void StatementBlock::set_my_sb(StatementBlock *p_sb, size_t p_index)
  {
    my_sb=p_sb;
    my_sb_index=p_index;
    for(size_t i=0; i<stmts.size(); i++)
      stmts[i]->set_my_sb(this, i);
  }

  size_t StatementBlock::get_my_sb_index() const
  {
    if(!my_sb) FATAL_ERROR("StatementBlock::get_my_sb_index()");
    return my_sb_index;
  }

  void StatementBlock::set_my_def(Definition *p_def)
  {
    my_def=p_def;
    for(size_t i=0; i<stmts.size(); i++)
      stmts[i]->set_my_def(p_def);
  }

  void StatementBlock::set_my_ags(AltGuards *p_ags)
  {
    for(size_t i=0; i<stmts.size(); i++)
      stmts[i]->set_my_ags(p_ags);
  }

  void StatementBlock::set_my_laic_stmt(AltGuards *p_ags,
                                        Statement *p_loop_stmt)
  {
    for(size_t i=0; i<stmts.size(); i++)
      stmts[i]->set_my_laic_stmt(p_ags, p_loop_stmt);
  }

  StatementBlock::returnstatus_t StatementBlock::has_return() const
  {
    returnstatus_t ret_val = RS_NO;
    size_t nof_stmts = stmts.size();
    for (size_t i = 0; i < nof_stmts; i++) {
      Statement *stmt = stmts[i];
      if (stmt->get_statementtype() == Statement::S_GOTO) {
	if (stmt->goto_jumps_forward()) {
	  // heuristics without deep analysis of the control flow graph:
	  // skip over the next statements until a (used) label is found
	  // the behaviour will be sound (i.e. no false errors will be reported)
	  for (i++; i < nof_stmts; i++) {
	    stmt = stmts[i];
	    if (stmt->get_statementtype() == Statement::S_LABEL &&
		stmt->label_is_used()) break;
	  }
	} else return RS_YES;
      } else if (stmt->get_statementtype()==Statement::S_BLOCK && stmt->get_block()->exception_handling!=EH_NONE) {
        switch (stmt->get_block()->exception_handling) {
        case EH_TRY: // the i-th statement is a try{} statement block, the (i+1)-th must be a catch{} block
          if ((i+1)<nof_stmts && stmts[i+1]->get_statementtype()==Statement::S_BLOCK && stmts[i+1]->get_block()->exception_handling==EH_CATCH) {
            returnstatus_t try_block_rs = stmt->has_return();
            returnstatus_t catch_block_rs = stmts[i+1]->has_return();
            // 3 x 3 combinations
            if (try_block_rs==catch_block_rs) {
              switch (try_block_rs) {
              case RS_YES:
                return RS_YES;
              case RS_MAYBE:
                ret_val = RS_MAYBE;
              default:
                break;
              }
            } else {
              ret_val = RS_MAYBE;
            }
          } else { // if next statement is not a catch{} block then that error has already been reported
            ret_val = RS_MAYBE; // assume the catch block was an RS_MAYBE
          }
          break;
        case EH_CATCH:
          // logically this is part of the preceding try{} block, handle it as part of it, see above case EH_TRY
          break;
        default:
          FATAL_ERROR("StatementBlock::has_return()");
        }
      } else {
	switch (stmt->has_return()) {
	case RS_YES:
	  return RS_YES;
	case RS_MAYBE:
	  ret_val = RS_MAYBE;
	default:
	  break;
	}
      }
    }
    return ret_val;
  }

  bool StatementBlock::has_receiving_stmt(size_t start_i) const
  {
    for(size_t i=start_i; i<stmts.size(); i++)
      if(stmts[i]->has_receiving_stmt()) return true;
    return false;
  }

  bool StatementBlock::has_def_stmt_i(size_t start_i) const
  {
    for(size_t i=start_i; i<stmts.size(); i++)
      if(stmts[i]->get_statementtype()==Statement::S_DEF) return true;
    return false;
  }

  size_t StatementBlock::last_receiving_stmt_i() const
  {
    size_t nof_stmts = stmts.size();
    if (nof_stmts > 0) {
      size_t i = nof_stmts;
      do {
	if (stmts[--i]->has_receiving_stmt()) return i;
      } while (i > 0);
    }
    return nof_stmts;
  }

  StatementBlock *StatementBlock::get_parent_block() const
  {
    for (Scope *s = get_parent_scope(); s; s=s->get_parent_scope()) {
      StatementBlock *sb = dynamic_cast<StatementBlock*>(s);
      if (sb) return sb;
    }
    return 0;
  }

  void StatementBlock::chk_trycatch_blocks(Statement* s1, Statement* s2) {
    if (s1 && s1->get_statementtype()==Statement::S_BLOCK && s1->get_block()->get_exception_handling()==StatementBlock::EH_TRY) {
      if (!(s2 && s2->get_statementtype()==Statement::S_BLOCK && s2->get_block()->get_exception_handling()==StatementBlock::EH_CATCH)) {
        s1->error("`try' statement block must be followed by a `catch' block");
      }
    }
    if (s2 && s2->get_statementtype()==Statement::S_BLOCK && s2->get_block()->get_exception_handling()==StatementBlock::EH_CATCH) {
      if (!(s1 && s1->get_statementtype()==Statement::S_BLOCK && s1->get_block()->get_exception_handling()==StatementBlock::EH_TRY)) {
        s2->error("`catch' statement block must be preceded by a `try' block");
      }
    }
  }

  void StatementBlock::chk()
  {
    if (checked) return;
    chk_labels();
    bool unreach_found = false;
    size_t nof_stmts = stmts.size();
    Statement *prev_stmt = 0;
    for (size_t i = 0; i < nof_stmts; i++) {
      Statement *stmt = stmts[i];
      stmt->chk();
      if (!unreach_found && stmt->get_statementtype() != Statement::S_LABEL &&
	  prev_stmt && prev_stmt->is_terminating()) {
	// a statement is unreachable if:
	// - it is not a label (i.e. goto cannot jump to it)
	// - it is not the first statement of the block
	// - the previous statement terminates the control flow
	stmt->warning("Control never reaches this statement");
	unreach_found = true;
      }
      // check try-catch statement block usage
      chk_trycatch_blocks(prev_stmt, stmt);
      //
      prev_stmt = stmt;
    }
    chk_trycatch_blocks(prev_stmt, 0);
    chk_unused_labels();
    checked = true;
  }

  void StatementBlock::chk_allowed_interleave()
  {
    size_t nof_stmts = stmts.size();
    for (size_t i = 0; i < nof_stmts; i++)
      stmts[i]->chk_allowed_interleave();
  }

  void StatementBlock::chk_labels()
  {
    if(labels_checked) return;
    for(size_t i=0; i<stmts.size(); i++) {
      Statement *st=stmts[i];
      if(st->get_statementtype()!=Statement::S_LABEL) continue;
      const Identifier& labelid=st->get_labelid();
      if(has_label(labelid)) {
        const char *name=labelid.get_dispname().c_str();
        st->error("Duplicate label `%s'", name);
        Statement *st2=get_label(labelid);
        st2->note("Previous definition of label `%s' is here", name);
      }
      else labels.add(labelid, st);
    }
    labels_checked=true;
  }

  void StatementBlock::chk_unused_labels()
  {
    size_t nof_stmts = stmts.size();
    for (size_t i = 0; i < nof_stmts; i++) {
      Statement *stmt = stmts[i];
      if (stmt->get_statementtype() == Statement::S_LABEL &&
	  !stmt->label_is_used())
	stmt->warning("Label `%s' is defined, but not used",
	  stmt->get_labelid().get_dispname().c_str());
    }
  }

  bool StatementBlock::has_label(const Identifier& p_id) const
  {
    for (const StatementBlock *sb = this; sb; sb = sb->get_parent_block())
      if (sb->labels.has_key(p_id)) return true;
    return false;
  }

  Statement *StatementBlock::get_label(const Identifier& p_id) const
  {
    for (const StatementBlock *sb = this; sb; sb = sb->get_parent_block())
      if (sb->labels.has_key(p_id)) return sb->labels[p_id];
    FATAL_ERROR("StatementBlock::get_label()");
    return 0;
  }

  void StatementBlock::set_code_section(
    GovernedSimple::code_section_t p_code_section)
  {
    for(size_t i = 0; i < stmts.size(); i++)
      stmts[i]->set_code_section(p_code_section);
  }

  char* StatementBlock::generate_code(char *str)
  {
    if (exception_handling==EH_TRY) {
      str = mputstr(str, "TTCN_TryBlock try_block;\n");
    }
    if (stmts.size()>0) {
      Statement* first_stmt = stmts[0];
      str = first_stmt->generate_code(str);
      if (exception_handling==EH_CATCH) {
        if (first_stmt->get_statementtype()!=Statement::S_DEF) FATAL_ERROR("StatementBlock::generate_code()");
        Definition* error_msg_def = first_stmt->get_def();
        string error_msg_name = error_msg_def->get_id().get_name();
        str = mputprintf(str, "%s = ttcn_error.get_message();\n", error_msg_name.c_str());
      }
    }
    for(size_t i=1; i<stmts.size(); i++) {
      str = stmts[i]->generate_code(str);
    }
    return str;
  }

  void StatementBlock::ilt_generate_code(ILT *ilt)
  {
    size_t nof_stmts = stmts.size();
    if (nof_stmts == 0) return;
    char*& str=ilt->get_out_branches();
    size_t last_recv_stmt_i=last_receiving_stmt_i();
    // has no receiving stmt
    if (last_recv_stmt_i == nof_stmts) {
      bool has_def=has_def_stmt_i();
      if(has_def) str=mputstr(str, "{\n");
      for(size_t i=0; i<nof_stmts; i++)
        str=stmts[i]->generate_code(str);
      if(has_def) str=mputstr(str, "}\n");
      return;
    }
    for(size_t i=0; i<=last_recv_stmt_i; i++)
      stmts[i]->ilt_generate_code(ilt);
    // the last part which does not contain receiving stmt
    if(last_recv_stmt_i==nof_stmts-1) return;
    bool has_def=has_def_stmt_i(last_recv_stmt_i+1);
    if(has_def) str=mputstr(str, "{\n");
    for(size_t i=last_recv_stmt_i+1; i<nof_stmts; i++)
      str=stmts[i]->generate_code(str);
    if(has_def) str=mputstr(str, "}\n");
  }

  void StatementBlock::set_parent_path(WithAttribPath* p_path)
  {
    for (size_t i = 0; i < stmts.size(); i++)
      stmts[i]->set_parent_path(p_path);
  }

  // =================================
  // ===== Statement
  // =================================

  void Statement::clean_up()
  {
    switch (statementtype) {
    case S_ERROR:
    case S_BREAK:
    case S_CONTINUE:
    case S_STOP_EXEC:
    case S_REPEAT:
      break;
    case S_START_UNDEF:
    case S_STOP_UNDEF:
      delete undefstartstop.ref;
      delete undefstartstop.val;
      break;
    case S_UNKNOWN_INSTANCE:
    case S_FUNCTION_INSTANCE:
    case S_ALTSTEP_INSTANCE:
    case S_ACTIVATE:
      delete ref_pard;
      break;
    case S_DEF:
      delete def;
      break;
    case S_ASSIGNMENT:
      delete ass;
      break;
    case S_BLOCK:
      delete block;
      break;
    case S_LOG:
    case S_ACTION:
    case S_STOP_TESTCASE:
      delete logargs;
      break;
    case S_LABEL:
      delete label.id;
      delete label.clabel;
      break;
    case S_GOTO:
      delete go_to.id;
      break;
    case S_IF:
      delete if_stmt.ics;
      delete if_stmt.elseblock;
      delete if_stmt.elseblock_location;
      break;
    case S_SELECT:
      delete select.expr;
      delete select.scs;
      break;
    case S_FOR:
      if(loop.for_stmt.varinst)
        delete loop.for_stmt.init_varinst;
      else
        delete loop.for_stmt.init_ass;
      delete loop.for_stmt.finalexpr;
      delete loop.for_stmt.step;
      delete loop.block;
      if (loop.label_next)
        delete loop.label_next;
      if (loop.il_label_end)
        delete loop.il_label_end;
      break;
    case S_WHILE:
    case S_DOWHILE:
      delete loop.expr;
      delete loop.block;
      if (loop.label_next)
        delete loop.label_next;
      if (loop.il_label_end)
        delete loop.il_label_end;
      break;
    case S_ALT:
    case S_INTERLEAVE:
      delete ags;
      break;
    case S_RETURN:
      delete returnexpr.v;
      delete returnexpr.t;
      break;
    case S_DEACTIVATE:
      delete deactivate;
      break;
    case S_SEND:
      delete port_op.portref;
      delete port_op.s.sendpar;
      delete port_op.s.toclause;
      break;
    case S_CALL:
      delete port_op.portref;
      delete port_op.s.sendpar;
      delete port_op.s.call.timer;
      delete port_op.s.toclause;
      delete port_op.s.call.body;
      break;
    case S_REPLY:
      delete port_op.portref;
      delete port_op.s.sendpar;
      delete port_op.s.replyval;
      delete port_op.s.toclause;
      break;
    case S_RAISE:
      delete port_op.portref;
      delete port_op.s.raise.signature_ref;
      delete port_op.s.sendpar;
      delete port_op.s.toclause;
      break;
    case S_RECEIVE:
    case S_CHECK_RECEIVE:
    case S_TRIGGER:
      delete port_op.portref;
      delete port_op.r.rcvpar;
      delete port_op.r.fromclause;
      delete port_op.r.redirect.value;
      delete port_op.r.redirect.sender;
      break;
    case S_GETCALL:
    case S_CHECK_GETCALL:
      delete port_op.portref;
      delete port_op.r.rcvpar;
      delete port_op.r.fromclause;
      delete port_op.r.redirect.param;
      delete port_op.r.redirect.sender;
      break;
    case S_GETREPLY:
    case S_CHECK_GETREPLY:
      delete port_op.portref;
      delete port_op.r.rcvpar;
      delete port_op.r.getreply_valuematch;
      delete port_op.r.fromclause;
      delete port_op.r.redirect.value;
      delete port_op.r.redirect.param;
      delete port_op.r.redirect.sender;
      break;
    case S_CATCH:
    case S_CHECK_CATCH:
      delete port_op.portref;
      delete port_op.r.ctch.signature_ref;
      delete port_op.r.rcvpar;
      delete port_op.r.fromclause;
      delete port_op.r.redirect.value;
      delete port_op.r.redirect.sender;
      break;
    case S_CHECK:
      delete port_op.portref;
      delete port_op.r.fromclause;
      delete port_op.r.redirect.sender;
      break;
    case S_CLEAR:
    case S_START_PORT:
    case S_STOP_PORT:
    case S_HALT:
      delete port_op.portref;
      break;
    case S_START_COMP:
      delete comp_op.compref;
      delete comp_op.funcinstref;
      break;
    case S_START_COMP_REFD:
      delete comp_op.compref;
      delete comp_op.derefered.value;
      delete comp_op.derefered.ap_list2;
      break;
    case S_STOP_COMP:
    case S_KILL:
    case S_KILLED:
      delete comp_op.compref;
      break;
    case S_DONE:
      if (comp_op.compref) {
        delete comp_op.compref;
        delete comp_op.donereturn.donematch;
        delete comp_op.donereturn.redirect;
      }
      break;
    case S_CONNECT:
    case S_MAP:
    case S_DISCONNECT:
    case S_UNMAP:
      delete config_op.compref1;
      delete config_op.portref1;
      delete config_op.compref2;
      delete config_op.portref2;
      break;
    case S_START_TIMER:
      delete timer_op.timerref;
      delete timer_op.value;
      break;
    case S_STOP_TIMER:
    case S_TIMEOUT:
      delete timer_op.timerref;
      break;
    case S_SETVERDICT:
      delete setverdict.verdictval;
      delete setverdict.logargs;
      break;
    case S_TESTCASE_INSTANCE:
      delete testcase_inst.tcref;
      delete testcase_inst.timerval;
      break;
    case S_TESTCASE_INSTANCE_REFD:
      delete execute_refd.value;
      delete execute_refd.ap_list2;
      delete execute_refd.timerval;
      break;
    case S_ACTIVATE_REFD:
    case S_UNKNOWN_INVOKED:
    case S_FUNCTION_INVOKED:
    case S_ALTSTEP_INVOKED:
      delete fau_refd.value;
      delete fau_refd.ap_list2;
      break;
    case S_STRING2TTCN:
      delete str2ttcn.val;
      delete str2ttcn.ref;
      break;
    default:
      FATAL_ERROR("Statement::clean_up()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_REPEAT:
      ags=0;
    case S_ERROR:
    case S_STOP_EXEC:
      break;
    case S_BREAK:
    case S_CONTINUE:
      brk_cnt.loop_stmt=0;
      brk_cnt.ags=0;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Ref_base *p_ref, Value *p_val)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_START_UNDEF:
      if (!p_ref) FATAL_ERROR("Statement::Statement()");
      undefstartstop.ref=p_ref;
      undefstartstop.val=p_val;
      break;
    case S_STOP_UNDEF:
      if (!p_ref || p_val) FATAL_ERROR("Statement::Statement()");
      undefstartstop.ref=p_ref;
      undefstartstop.val=0;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Ref_pard *p_ref)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_UNKNOWN_INSTANCE:
    case S_FUNCTION_INSTANCE:
    case S_ALTSTEP_INSTANCE:
    case S_ACTIVATE:
      if(!p_ref)
        FATAL_ERROR("Statement::Statement()");
      ref_pard=p_ref;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Value *p_derefered_value,
                       ParsedActualParameters *p_ap_list)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_ACTIVATE_REFD:
    case S_UNKNOWN_INVOKED:
    case S_FUNCTION_INVOKED:
    case S_ALTSTEP_INVOKED:
      if(!p_derefered_value || !p_ap_list)
        FATAL_ERROR("Statement::Statement()");
      fau_refd.value = p_derefered_value;
      fau_refd.t_list1 = p_ap_list;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    }
  }

  Statement::Statement(statementtype_t p_st, Definition *p_def)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_DEF:
      if(!p_def)
        FATAL_ERROR("Statement::Statement()");
      def=p_def;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Assignment *p_ass)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_ASSIGNMENT:
      if(!p_ass)
        FATAL_ERROR("Statement::Statement()");
      ass=p_ass;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, StatementBlock *p_block)
    : statementtype(p_st), my_sb(0)
  {
    switch (statementtype) {
    case S_BLOCK:
      if (!p_block) FATAL_ERROR("Statement::Statement()");
      block = p_block;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, LogArguments *p_logargs)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_LOG:
    case S_ACTION:
    case S_STOP_TESTCASE:
      logargs=p_logargs;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Identifier *p_id)
    : statementtype(p_st), my_sb(0)
  {
    if (!p_id)
      FATAL_ERROR("Statement::Statement()");
    switch (statementtype) {
    case S_LABEL:
      label.id = p_id;
      label.stmt_idx = 0;
      label.clabel = 0;
      label.used = false;
      break;
    case S_GOTO:
      go_to.id = p_id;
      go_to.stmt_idx = 0;
      go_to.label = 0;
      go_to.jumps_forward = false;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, IfClauses *p_ics,
                       StatementBlock *p_block, Location *p_loc)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_IF:
      if(!p_ics)
        FATAL_ERROR("Statement::Statement()");
      if_stmt.ics=p_ics;
      if (p_block) {
	if (!p_loc) FATAL_ERROR("Statement::Statement()");
	if_stmt.elseblock = p_block;
	if_stmt.elseblock_location = p_loc;
      } else {
	if (p_loc) FATAL_ERROR("Statement::Statement()");
	if_stmt.elseblock = 0;
	if_stmt.elseblock_location = 0;
      }
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Value *p_expr, SelectCases *p_scs)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_SELECT:
      if(!p_expr || !p_scs)
        FATAL_ERROR("Statement::Statement()");
      select.expr=p_expr;
      select.scs=p_scs;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Definitions *p_defs,
                       Assignment *p_ass, Value *p_final,
                       Assignment *p_step, StatementBlock *p_block)
    : statementtype(p_st), my_sb(0)
  {
    switch (statementtype) {
    case S_FOR: // precisely one of p_defs, p_ass allowed
      if (p_defs) {
        // it came from a for loop which looked like this:
        // for (var integer foo:=1; foo<10; foo:=foo+1) ;
	if (p_ass) FATAL_ERROR("Statement::Statement()");
	loop.for_stmt.varinst = true;
	loop.for_stmt.init_varinst = p_defs;
      } else {
        // it came from a for loop which looked like this:
        // for (foo:=1; foo<10; foo:=foo+1) ;
	if (!p_ass) FATAL_ERROR("Statement::Statement()");
	loop.for_stmt.varinst = false;
	loop.for_stmt.init_ass = p_ass;
      }
      if(!p_final || !p_step || !p_block)
        FATAL_ERROR("Statement::Statement()");
      loop.for_stmt.finalexpr=p_final;
      loop.for_stmt.step=p_step;
      loop.block=p_block;
      loop.label_next=0;
      loop.il_label_end=0;
      loop.has_cnt=false;
      loop.has_brk=false;
      loop.has_cnt_in_ags=false;
      loop.iterate_once=false; // not used by for
      loop.is_ilt=false;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Value *p_val,
                       StatementBlock *p_block)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_WHILE:
    case S_DOWHILE:
      if(!p_val || !p_block)
        FATAL_ERROR("Statement::Statement()");
      loop.expr=p_val;
      loop.block=p_block;
      loop.label_next=0;
      loop.il_label_end=0;
      loop.has_cnt=false;
      loop.has_brk=false;
      loop.has_cnt_in_ags=false;
      loop.iterate_once=false; // used only by do-while
      loop.is_ilt=false;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, AltGuards *p_ags)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_ALT:
    case S_INTERLEAVE:
      if(!p_ags)
        FATAL_ERROR("Statement::Statement()");
      ags=p_ags;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Template *p_temp)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_RETURN:
      returnexpr.v=0;
      returnexpr.t=p_temp;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Value *p_val)
    : statementtype(p_st), my_sb(0)
  {
    switch (statementtype) {
    case S_KILLED:
      if (!p_val)
        FATAL_ERROR("Statement::Statement()");
      // no break
    case S_STOP_COMP:
    case S_KILL:
      comp_op.compref=p_val;
      break;
    case S_DEACTIVATE:
      deactivate=p_val;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Value *p_val,
                       LogArguments *p_logargs)
    : statementtype(p_st), my_sb(0)
  {
    if (!p_val || statementtype != S_SETVERDICT)
      FATAL_ERROR("Statement::Statement()");
    setverdict.verdictval = p_val;
    setverdict.logargs = p_logargs;
  }

  Statement::Statement(statementtype_t p_st, Reference *p_ref,
                       TemplateInstance *p_templinst, Value *p_val)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_SEND:
      if(!p_ref || !p_templinst)
        FATAL_ERROR("Statement::Statement()");
      port_op.portref=p_ref;
      port_op.s.sendpar=p_templinst;
      port_op.s.toclause=p_val;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Reference *p_ref,
              TemplateInstance *p_templinst, Value *p_timerval,
              bool p_nowait, Value *p_toclause, AltGuards *p_callbody)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_CALL:
      if(!p_ref || !p_templinst || (p_timerval && p_nowait))
        FATAL_ERROR("Statement::Statement()");
      port_op.portref=p_ref;
      port_op.s.sendpar=p_templinst;
      port_op.s.call.timer=p_timerval;
      port_op.s.call.nowait=p_nowait;
      port_op.s.toclause=p_toclause;
      port_op.s.call.body=p_callbody;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Reference *p_ref,
                       TemplateInstance *p_templinst, Value *p_replyval,
                       Value *p_toclause)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_REPLY:
      if(!p_ref || !p_templinst)
        FATAL_ERROR("Statement::Statement()");
      port_op.portref=p_ref;
      port_op.s.sendpar=p_templinst;
      port_op.s.replyval=p_replyval;
      port_op.s.toclause=p_toclause;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Reference *p_ref,
                       Reference *p_sig, TemplateInstance *p_templinst,
                       Value *p_toclause)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_RAISE:
      if(!p_ref || !p_templinst || !p_sig)
        FATAL_ERROR("Statement::Statement()");
      port_op.portref=p_ref;
      port_op.s.raise.signature_ref=p_sig;
      port_op.s.raise.signature=0;
      port_op.s.sendpar=p_templinst;
      port_op.s.toclause=p_toclause;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Reference *p_ref,
                       TemplateInstance *p_templinst,
                       TemplateInstance *p_fromclause,
                       Reference *p_redirectval, Reference *p_redirectsender)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_RECEIVE:
    case S_CHECK_RECEIVE:
    case S_TRIGGER:
      port_op.portref=p_ref;
      port_op.r.rcvpar=p_templinst;
      port_op.r.fromclause=p_fromclause;
      port_op.r.redirect.value=p_redirectval;
      port_op.r.redirect.param=0;
      port_op.r.redirect.sender=p_redirectsender;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Reference *p_ref,
                       TemplateInstance *p_templinst,
                       TemplateInstance *p_fromclause,
                       ParamRedirect *p_redirectparam,
		       Reference *p_redirectsender)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_GETCALL:
    case S_CHECK_GETCALL:
      port_op.portref=p_ref;
      port_op.r.rcvpar=p_templinst;
      port_op.r.fromclause=p_fromclause;
      port_op.r.redirect.value=0;
      port_op.r.redirect.param=p_redirectparam;
      port_op.r.redirect.sender=p_redirectsender;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Reference *p_ref,
                       TemplateInstance *p_templinst,
                       TemplateInstance *p_valuematch,
                       TemplateInstance *p_fromclause,
                       Reference *p_redirectval, ParamRedirect *p_redirectparam,
                       Reference *p_redirectsender)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_GETREPLY:
    case S_CHECK_GETREPLY:
      if (!p_templinst && p_valuematch) FATAL_ERROR("Statement::Statement()");
      port_op.portref=p_ref;
      port_op.r.rcvpar=p_templinst;
      port_op.r.getreply_valuematch=p_valuematch;
      port_op.r.fromclause=p_fromclause;
      port_op.r.redirect.value=p_redirectval;
      port_op.r.redirect.param=p_redirectparam;
      port_op.r.redirect.sender=p_redirectsender;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Reference *p_ref,
                       Reference *p_sig, TemplateInstance *p_templinst,
                       bool p_timeout, TemplateInstance *p_fromclause,
                       Reference *p_redirectval, Reference *p_redirectsender)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_CATCH:
    case S_CHECK_CATCH:
      if (((p_sig || p_templinst) && p_timeout) ||
        (p_sig && !p_templinst) || (!p_sig && p_templinst))
	FATAL_ERROR("Statement::Statement()");
      port_op.portref=p_ref;
      port_op.r.ctch.signature_ref=p_sig;
      port_op.r.ctch.signature=0;
      port_op.r.rcvpar=p_templinst;
      port_op.r.ctch.timeout=p_timeout;
      port_op.r.ctch.in_call=false;
      port_op.r.ctch.call_has_timer=false;
      port_op.r.fromclause=p_fromclause;
      port_op.r.redirect.value=p_redirectval;
      port_op.r.redirect.param=0;
      port_op.r.redirect.sender=p_redirectsender;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Reference *p_ref,
                       TemplateInstance *p_fromclause,
                       Reference *p_redirectsender)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_CHECK:
      port_op.portref=p_ref; // may be NULL for "any port.check"
      port_op.r.fromclause=p_fromclause;
      port_op.r.redirect.value=0;
      port_op.r.redirect.param=0;
      port_op.r.redirect.sender=p_redirectsender;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Reference *p_ref)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_CLEAR:
    case S_START_PORT:
    case S_STOP_PORT:
    case S_HALT:
      port_op.portref=p_ref;
      break;
    case S_STOP_TIMER:
    case S_TIMEOUT:
      timer_op.timerref=p_ref;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Value *p_compref,
                       Ref_pard *p_funcinst)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_START_COMP:
      if(!p_compref || !p_funcinst)
        FATAL_ERROR("Statement::Statement()");
      comp_op.compref = p_compref;
      comp_op.funcinstref = p_funcinst;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Value *p_compref,
                        Value *p_derefered_value, ParsedActualParameters *p_ap_list)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_START_COMP_REFD:
      if(!p_compref || !p_derefered_value || !p_ap_list)
        FATAL_ERROR("Statement::Statement()");
      comp_op.compref = p_compref;
      comp_op.derefered.value = p_derefered_value;
      comp_op.derefered.t_list1 = p_ap_list;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    }
  }

  Statement::Statement(statementtype_t p_st, Value *p_compref,
                       TemplateInstance *p_donematch, Reference *p_redirect)
    : statementtype(p_st), my_sb(0)
  {
    switch (statementtype) {
    case S_DONE:
      if (!p_compref) FATAL_ERROR("Statement::Statement()");
      comp_op.compref = p_compref;
      comp_op.donereturn.donematch = p_donematch;
      comp_op.donereturn.redirect = p_redirect;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, component_t p_anyall)
    : statementtype(p_st), my_sb(0)
  {
    switch (statementtype) {
    case S_DONE:
    case S_KILLED:
      comp_op.compref = 0;
      comp_op.any_or_all = p_anyall;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st,
                       Value *p_compref1, Reference *p_portref1,
                       Value *p_compref2, Reference *p_portref2)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_CONNECT:
    case S_MAP:
    case S_DISCONNECT:
    case S_UNMAP:
      if(!p_compref1 || !p_portref1 || !p_compref2 || !p_portref2)
        FATAL_ERROR("Statement::Statement()");
      config_op.compref1=p_compref1;
      config_op.portref1=p_portref1;
      config_op.compref2=p_compref2;
      config_op.portref2=p_portref2;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Ref_pard *p_ref, Value *p_val)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_TESTCASE_INSTANCE:
      if(!p_ref)
        FATAL_ERROR("Statement::Statement()");
      testcase_inst.tcref=p_ref;
      testcase_inst.timerval=p_val;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    } // switch statementtype
  }

  Statement::Statement(statementtype_t p_st, Value *p_derefered_value,
                        TemplateInstances *p_ap_list, Value *p_val)
    : statementtype(p_st), my_sb(0)
  {
    switch(statementtype) {
    case S_TESTCASE_INSTANCE_REFD:
      if(!p_derefered_value) FATAL_ERROR("Statement::Statement()");
      execute_refd.value = p_derefered_value;
      execute_refd.t_list1 = p_ap_list;
      execute_refd.timerval = p_val;
      break;
    default:
      FATAL_ERROR("Statement::Statement()");
    }
  }

  Statement::Statement(statementtype_t p_st, Value* p_val, Reference* p_ref): statementtype(p_st), my_sb(0)
  {
    if (p_st!=S_STRING2TTCN || p_val==NULL || p_ref==NULL) FATAL_ERROR("Statement::Statement()");
    str2ttcn.val = p_val;
    str2ttcn.ref = p_ref;
  }

  Statement::~Statement()
  {
    clean_up();
  }

  Statement *Statement::clone() const
  {
    FATAL_ERROR("Statement::clone");
  }

  void Statement::dump(unsigned int level) const
  {
    DEBUG(level, "Statement at %p, a(n) %s", (const void *)this,
          get_stmt_name());
    switch (statementtype) {
    case S_TESTCASE_INSTANCE:
    case S_TESTCASE_INSTANCE_REFD: {
      Common::Value *v = execute_refd.value;
      v->dump(level + 1);
    } break;
    case S_DEF:
      def->dump(level + 1);
      break;
    case S_ASSIGNMENT:
      ass->dump(level + 1);
      break;
    case S_BLOCK:
      block->dump(level + 1);
      break;
    case S_IF:
      if_stmt.ics->dump(level + 1);
      if (if_stmt.elseblock) if_stmt.elseblock->dump(level + 1);
      break;
    default:
      break;
    }
  }

  size_t Statement::get_my_sb_index() const
  {
    switch (statementtype) {
    case S_LABEL:
      return label.stmt_idx;
    case S_GOTO:
      return go_to.stmt_idx;
    default:
      FATAL_ERROR("Statement::get_my_sb_index()");
      return 0;
    }
  }

  const char *Statement::get_stmt_name() const
  {
    switch(statementtype) {
    case S_ERROR: return "<erroneous statement>";
    case S_START_UNDEF: return "start";
    case S_STOP_UNDEF: return "stop";
    case S_UNKNOWN_INSTANCE: return "function or altstep instance";
    case S_UNKNOWN_INVOKED: return "function or altstep type invocation";
    case S_DEF: return "definition";
    case S_ASSIGNMENT: return "assignment";
    case S_FUNCTION_INSTANCE: return "function instance";
    case S_FUNCTION_INVOKED: return "function type invocation";
    case S_BLOCK: return "statement block";
    case S_LOG: return "log";
    case S_LABEL: return "label";
    case S_GOTO: return "goto";
    case S_IF: return "if";
    case S_SELECT: return "select-case";
    case S_FOR: return "for";
    case S_WHILE: return "while";
    case S_DOWHILE: return "do-while";
    case S_BREAK: return "break";
    case S_CONTINUE: return "continue";
    case S_STOP_EXEC: return "stop";
    case S_STOP_TESTCASE: return "testcase.stop";
    case S_ALT: return "alt";
    case S_REPEAT: return "repeat";
    case S_INTERLEAVE: return "interleave";
    case S_ALTSTEP_INSTANCE: return "altstep instance";
    case S_ALTSTEP_INVOKED: return "altstep type invocation";
    case S_RETURN: return "return";
    case S_ACTIVATE:
    case S_ACTIVATE_REFD:
      return "activate";
    case S_DEACTIVATE: return "deactivate";
    case S_SEND: return "send";
    case S_CALL: return "call";
    case S_REPLY: return "reply";
    case S_RAISE: return "raise";
    case S_RECEIVE: return "receive";
    case S_TRIGGER: return "trigger";
    case S_GETCALL: return "getcall";
    case S_GETREPLY: return "getreply";
    case S_CATCH: return "catch";
    case S_CHECK: return "check";
    case S_CHECK_RECEIVE: return "check-receive";
    case S_CHECK_GETCALL: return "check-getcall";
    case S_CHECK_GETREPLY: return "check-getreply";
    case S_CHECK_CATCH: return "check-catch";
    case S_CLEAR: return "clear";
    case S_START_PORT: return "start port";
    case S_STOP_PORT: return "stop port";
    case S_HALT: return "halt";
    case S_START_COMP:
    case S_START_COMP_REFD:
      return "start test component";
    case S_STOP_COMP: return "stop test component";
    case S_DONE: return "done";
    case S_KILL: return "kill";
    case S_KILLED: return "killed";
    case S_CONNECT: return "connect";
    case S_MAP: return "map";
    case S_DISCONNECT: return "disconnect";
    case S_UNMAP: return "unmap";
    case S_START_TIMER: return "start timer";
    case S_STOP_TIMER: return "stop timer";
    case S_TIMEOUT: return "timeout";
    case S_SETVERDICT: return "setverdict";
    case S_ACTION: return "action";
    case S_TESTCASE_INSTANCE:
    case S_TESTCASE_INSTANCE_REFD:
      return "execute";
    case S_STRING2TTCN: return "string2ttcn";
    default:
      FATAL_ERROR("Statement::get_stmt_name()");
      return "";
    } // switch statementtype
  }

  const Identifier& Statement::get_labelid() const
  {
    switch (statementtype) {
    case S_LABEL:
      return *label.id;
    case S_GOTO:
      return *go_to.id;
    default:
      FATAL_ERROR("Statement::get_labelid()");
      return *label.id;
    }
  }

  bool Statement::label_is_used() const
  {
    if (statementtype != S_LABEL) FATAL_ERROR("Statement::label_is_used()");
    return label.used;
  }

  bool Statement::goto_jumps_forward() const
  {
    if (statementtype != S_GOTO) FATAL_ERROR("Statement::goto_jumps_forward()");
    return go_to.jumps_forward;
  }

  const string& Statement::get_clabel()
  {
    if (statementtype != S_LABEL || !my_sb)
      FATAL_ERROR("Statement::get_clabel()");
    if (!label.clabel) label.clabel =
	new string(my_sb->get_scope_mod_gen()->get_temporary_id());
    return *label.clabel;
  }

  Definition *Statement::get_def() const
  {
    if (statementtype != S_DEF) FATAL_ERROR("Statement::get_def()");
    return def;
  }

  AltGuards *Statement::get_ags() const
  {
    if (statementtype != S_ALT && statementtype != S_INTERLEAVE)
      FATAL_ERROR("Statement::get_ags()");
    return ags;
  }

  StatementBlock *Statement::get_block() const
  {
    switch (statementtype) {
    case S_BLOCK:
      return block;
    case S_FOR:
    case S_WHILE:
    case S_DOWHILE:
      return loop.block;
    default:
      FATAL_ERROR("Statement::get_block()");
      return 0;
    }
  }

  void Statement::set_my_scope(Scope *p_scope)
  {
    switch(statementtype) {
    case S_ERROR:
    case S_LABEL:
    case S_GOTO:
    case S_BREAK:
    case S_CONTINUE:
    case S_STOP_EXEC:
    case S_REPEAT:
      break;
    case S_START_UNDEF:
    case S_STOP_UNDEF:
      undefstartstop.ref->set_my_scope(p_scope);
      if (undefstartstop.val) undefstartstop.val->set_my_scope(p_scope);
      break;
    case S_UNKNOWN_INSTANCE:
    case S_FUNCTION_INSTANCE:
    case S_ALTSTEP_INSTANCE:
    case S_ACTIVATE:
      ref_pard->set_my_scope(p_scope);
      break;
    case S_DEF:
      def->set_my_scope(p_scope);
      break;
    case S_ASSIGNMENT:
      ass->set_my_scope(p_scope);
      break;
    case S_BLOCK:
      block->set_my_scope(p_scope);
      break;
    case S_LOG:
    case S_ACTION:
    case S_STOP_TESTCASE:
      if (logargs) logargs->set_my_scope(p_scope);
      break;
    case S_IF:
      if_stmt.ics->set_my_scope(p_scope);
      if(if_stmt.elseblock) if_stmt.elseblock->set_my_scope(p_scope);
      break;
    case S_SELECT:
      select.expr->set_my_scope(p_scope);
      select.scs->set_my_scope(p_scope);
      break;
    case S_FOR:
      if (loop.for_stmt.varinst) {
        loop.for_stmt.init_varinst->set_parent_scope(p_scope);
        loop.for_stmt.finalexpr->set_my_scope(loop.for_stmt.init_varinst);
        loop.for_stmt.step->set_my_scope(loop.for_stmt.init_varinst);
        loop.block->set_my_scope(loop.for_stmt.init_varinst);
      } else {
        loop.for_stmt.init_ass->set_my_scope(p_scope);
        loop.for_stmt.finalexpr->set_my_scope(p_scope);
        loop.for_stmt.step->set_my_scope(p_scope);
        loop.block->set_my_scope(p_scope);
      }
      break;
    case S_WHILE:
    case S_DOWHILE:
      loop.expr->set_my_scope(p_scope);
      loop.block->set_my_scope(p_scope);
      break;
    case S_ALT:
    case S_INTERLEAVE:
      ags->set_my_scope(p_scope);
      break;
    case S_RETURN:
      if (returnexpr.v) returnexpr.v->set_my_scope(p_scope);
      if (returnexpr.t) returnexpr.t->set_my_scope(p_scope);
      break;
    case S_DEACTIVATE:
      if (deactivate) deactivate->set_my_scope(p_scope);
      break;
    case S_SEND:
      port_op.portref->set_my_scope(p_scope);
      port_op.s.sendpar->set_my_scope(p_scope);
      if(port_op.s.toclause) port_op.s.toclause->set_my_scope(p_scope);
      break;
    case S_CALL:
      port_op.portref->set_my_scope(p_scope);
      port_op.s.sendpar->set_my_scope(p_scope);
      if(port_op.s.toclause) port_op.s.toclause->set_my_scope(p_scope);
      if(port_op.s.call.timer) port_op.s.call.timer->set_my_scope(p_scope);
      if(port_op.s.call.body) port_op.s.call.body->set_my_scope(p_scope);
      break;
    case S_REPLY:
      port_op.portref->set_my_scope(p_scope);
      port_op.s.sendpar->set_my_scope(p_scope);
      if(port_op.s.replyval) port_op.s.replyval->set_my_scope(p_scope);
      if(port_op.s.toclause) port_op.s.toclause->set_my_scope(p_scope);
      break;
    case S_RAISE:
      port_op.portref->set_my_scope(p_scope);
      port_op.s.raise.signature_ref->set_my_scope(p_scope);
      port_op.s.sendpar->set_my_scope(p_scope);
      if(port_op.s.toclause) port_op.s.toclause->set_my_scope(p_scope);
      break;
    case S_RECEIVE:
    case S_CHECK_RECEIVE:
    case S_TRIGGER:
      if(port_op.portref) port_op.portref->set_my_scope(p_scope);
      if(port_op.r.rcvpar) port_op.r.rcvpar->set_my_scope(p_scope);
      if(port_op.r.fromclause) port_op.r.fromclause->set_my_scope(p_scope);
      if(port_op.r.redirect.value)
        port_op.r.redirect.value->set_my_scope(p_scope);
      if(port_op.r.redirect.sender)
        port_op.r.redirect.sender->set_my_scope(p_scope);
      break;
    case S_GETCALL:
    case S_CHECK_GETCALL:
      if(port_op.portref) port_op.portref->set_my_scope(p_scope);
      if(port_op.r.rcvpar) port_op.r.rcvpar->set_my_scope(p_scope);
      if(port_op.r.fromclause) port_op.r.fromclause->set_my_scope(p_scope);
      if(port_op.r.redirect.param)
        port_op.r.redirect.param->set_my_scope(p_scope);
      if(port_op.r.redirect.sender)
        port_op.r.redirect.sender->set_my_scope(p_scope);
      break;
    case S_GETREPLY:
    case S_CHECK_GETREPLY:
      if(port_op.portref) port_op.portref->set_my_scope(p_scope);
      if(port_op.r.rcvpar) port_op.r.rcvpar->set_my_scope(p_scope);
      if(port_op.r.getreply_valuematch)
        port_op.r.getreply_valuematch->set_my_scope(p_scope);
      if(port_op.r.fromclause) port_op.r.fromclause->set_my_scope(p_scope);
      if(port_op.r.redirect.value)
        port_op.r.redirect.value->set_my_scope(p_scope);
      if(port_op.r.redirect.param)
        port_op.r.redirect.param->set_my_scope(p_scope);
      if(port_op.r.redirect.sender)
        port_op.r.redirect.sender->set_my_scope(p_scope);
      break;
    case S_CATCH:
    case S_CHECK_CATCH:
      if(port_op.portref) port_op.portref->set_my_scope(p_scope);
      if(port_op.r.ctch.signature_ref)
        port_op.r.ctch.signature_ref->set_my_scope(p_scope);
      if(port_op.r.rcvpar) port_op.r.rcvpar->set_my_scope(p_scope);
      if(port_op.r.fromclause) port_op.r.fromclause->set_my_scope(p_scope);
      if(port_op.r.redirect.value)
        port_op.r.redirect.value->set_my_scope(p_scope);
      if(port_op.r.redirect.sender)
        port_op.r.redirect.sender->set_my_scope(p_scope);
      break;
    case S_CHECK:
      if(port_op.portref) port_op.portref->set_my_scope(p_scope);
      if(port_op.r.fromclause) port_op.r.fromclause->set_my_scope(p_scope);
      if(port_op.r.redirect.sender)
        port_op.r.redirect.sender->set_my_scope(p_scope);
      break;
    case S_CLEAR:
    case S_START_PORT:
    case S_STOP_PORT:
    case S_HALT:
      if (port_op.portref) port_op.portref->set_my_scope(p_scope);
      break;
    case S_START_COMP:
      comp_op.compref->set_my_scope(p_scope);
      comp_op.funcinstref->set_my_scope(p_scope);
      break;
    case S_START_COMP_REFD:
      comp_op.compref->set_my_scope(p_scope);
      comp_op.derefered.value->set_my_scope(p_scope);
      comp_op.derefered.t_list1->set_my_scope(p_scope);
      break;
    case S_STOP_COMP:
    case S_KILL:
    case S_KILLED:
      if (comp_op.compref) comp_op.compref->set_my_scope(p_scope);
      break;
    case S_DONE:
      if (comp_op.compref) {
        comp_op.compref->set_my_scope(p_scope);
        if (comp_op.donereturn.donematch)
          comp_op.donereturn.donematch->set_my_scope(p_scope);
        if (comp_op.donereturn.redirect)
          comp_op.donereturn.redirect->set_my_scope(p_scope);
      }
      break;
    case S_CONNECT:
    case S_MAP:
    case S_DISCONNECT:
    case S_UNMAP:
      config_op.compref1->set_my_scope(p_scope);
      config_op.portref1->set_my_scope(p_scope);
      config_op.compref2->set_my_scope(p_scope);
      config_op.portref2->set_my_scope(p_scope);
      break;
    case S_START_TIMER:
      timer_op.timerref->set_my_scope(p_scope);
      if (timer_op.value) timer_op.value->set_my_scope(p_scope);
      break;
    case S_STOP_TIMER:
    case S_TIMEOUT:
      if (timer_op.timerref) timer_op.timerref->set_my_scope(p_scope);
      break;
    case S_SETVERDICT:
      setverdict.verdictval->set_my_scope(p_scope);
      if (setverdict.logargs)
        setverdict.logargs->set_my_scope(p_scope);
      break;
    case S_TESTCASE_INSTANCE:
      testcase_inst.tcref->set_my_scope(p_scope);
      if (testcase_inst.timerval) testcase_inst.timerval->set_my_scope(p_scope);
      break;
    case S_TESTCASE_INSTANCE_REFD:
      execute_refd.value->set_my_scope(p_scope);
      execute_refd.t_list1->set_my_scope(p_scope);
      if(execute_refd.timerval) execute_refd.timerval->set_my_scope(p_scope);
      break;
    case S_ACTIVATE_REFD:
    case S_UNKNOWN_INVOKED:
    case S_FUNCTION_INVOKED:
    case S_ALTSTEP_INVOKED:
      fau_refd.value->set_my_scope(p_scope);
      fau_refd.t_list1->set_my_scope(p_scope);
      break;
    case S_STRING2TTCN:
      str2ttcn.val->set_my_scope(p_scope);
      str2ttcn.ref->set_my_scope(p_scope);
      break;
    default:
      FATAL_ERROR("Statement::set_my_scope()");
    } // switch statementtype
  }

  void Statement::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    switch (statementtype) {
    case S_ERROR:
    case S_LABEL:
    case S_GOTO:
    case S_BREAK:
    case S_CONTINUE:
    case S_STOP_EXEC:
    case S_REPEAT:
      break;
    case S_START_UNDEF:
    case S_STOP_UNDEF:
      undefstartstop.ref->set_fullname(p_fullname+".ref");
      if (undefstartstop.val)
        undefstartstop.val->set_fullname(p_fullname+".val");
      break;
    case S_UNKNOWN_INSTANCE:
    case S_FUNCTION_INSTANCE:
    case S_ALTSTEP_INSTANCE:
    case S_ACTIVATE:
      ref_pard->set_fullname(p_fullname+".ref");
      break;
    case S_DEF:
      def->set_fullname(p_fullname+".def");
      break;
    case S_ASSIGNMENT:
      ass->set_fullname(p_fullname+".ass");
      break;
    case S_BLOCK:
      block->set_fullname(p_fullname+".block");
      break;
    case S_LOG:
    case S_ACTION:
    case S_STOP_TESTCASE:
      if (logargs) logargs->set_fullname(p_fullname+".logargs");
      break;
    case S_IF:
      if_stmt.ics->set_fullname(p_fullname+".ifclauses");
      if (if_stmt.elseblock)
        if_stmt.elseblock->set_fullname(p_fullname+".elseblock");
      break;
    case S_SELECT:
      select.expr->set_fullname(p_fullname+".expr");
      select.scs->set_fullname(p_fullname+".scs");
      break;
    case S_FOR:
      if(loop.for_stmt.varinst)
        loop.for_stmt.init_varinst->set_fullname(p_fullname+".init");
      else
        loop.for_stmt.init_ass->set_fullname(p_fullname+".init");
      loop.for_stmt.finalexpr->set_fullname(p_fullname+".final");
      loop.for_stmt.step->set_fullname(p_fullname+".step");
      loop.block->set_fullname(p_fullname+".block");
      break;
    case S_WHILE:
    case S_DOWHILE:
      loop.expr->set_fullname(p_fullname+".expr");
      loop.block->set_fullname(p_fullname+".block");
      break;
    case S_ALT:
    case S_INTERLEAVE:
      ags->set_fullname(p_fullname+".ags");
      break;
    case S_RETURN:
      if (returnexpr.v) returnexpr.v->set_fullname(p_fullname+".returnexpr");
      if (returnexpr.t) returnexpr.t->set_fullname(p_fullname+".returnexpr");
      break;
    case S_DEACTIVATE:
      if (deactivate) deactivate->set_fullname(p_fullname+".deact");
      break;
    case S_SEND:
      port_op.portref->set_fullname(p_fullname+".portref");
      port_op.s.sendpar->set_fullname(p_fullname+".sendpar");
      if(port_op.s.toclause)
        port_op.s.toclause->set_fullname(p_fullname+".to");
      break;
    case S_CALL:
      port_op.portref->set_fullname(p_fullname+".portref");
      port_op.s.sendpar->set_fullname(p_fullname+".sendpar");
      if(port_op.s.toclause)
        port_op.s.toclause->set_fullname(p_fullname+".to");
      if(port_op.s.call.timer)
        port_op.s.call.timer->set_fullname(p_fullname+".timer");
      if(port_op.s.call.body)
        port_op.s.call.body->set_fullname(p_fullname+".body");
      break;
    case S_REPLY:
      port_op.portref->set_fullname(p_fullname+".portref");
      port_op.s.sendpar->set_fullname(p_fullname+".sendpar");
      if(port_op.s.replyval)
        port_op.s.replyval->set_fullname(p_fullname+".replyval");
      if(port_op.s.toclause)
        port_op.s.toclause->set_fullname(p_fullname+".to");
      break;
    case S_RAISE:
      port_op.portref->set_fullname(p_fullname+".portref");
      port_op.s.raise.signature_ref->set_fullname(p_fullname+".sign");
      port_op.s.sendpar->set_fullname(p_fullname+".sendpar");
      if(port_op.s.toclause)
        port_op.s.toclause->set_fullname(p_fullname+".to");
      break;
    case S_RECEIVE:
    case S_CHECK_RECEIVE:
    case S_TRIGGER:
      if(port_op.portref) port_op.portref->set_fullname(p_fullname+".portref");
      if(port_op.r.rcvpar)
        port_op.r.rcvpar->set_fullname(p_fullname+".rcvpar");
      if(port_op.r.fromclause)
        port_op.r.fromclause->set_fullname(p_fullname+".from");
      if(port_op.r.redirect.value)
        port_op.r.redirect.value->set_fullname(p_fullname+".redirval");
      if(port_op.r.redirect.sender)
        port_op.r.redirect.sender->set_fullname(p_fullname+".redirsender");
      break;
    case S_GETCALL:
    case S_CHECK_GETCALL:
      if(port_op.portref) port_op.portref->set_fullname(p_fullname+".portref");
      if(port_op.r.rcvpar)
        port_op.r.rcvpar->set_fullname(p_fullname+".rcvpar");
      if(port_op.r.fromclause)
        port_op.r.fromclause->set_fullname(p_fullname+".from");
      if(port_op.r.redirect.param)
        port_op.r.redirect.param->set_fullname(p_fullname+".pars");
      if(port_op.r.redirect.sender)
        port_op.r.redirect.sender->set_fullname(p_fullname+".redirsender");
      break;
    case S_GETREPLY:
    case S_CHECK_GETREPLY:
      if(port_op.portref) port_op.portref->set_fullname(p_fullname+".portref");
      if(port_op.r.rcvpar)
        port_op.r.rcvpar->set_fullname(p_fullname+".rcvpar");
      if(port_op.r.getreply_valuematch)
        port_op.r.getreply_valuematch->set_fullname(p_fullname+".valmatch");
      if(port_op.r.fromclause)
        port_op.r.fromclause->set_fullname(p_fullname+".from");
      if(port_op.r.redirect.value)
        port_op.r.redirect.value->set_fullname(p_fullname+".redirval");
      if(port_op.r.redirect.param)
        port_op.r.redirect.param->set_fullname(p_fullname+".pars");
      if(port_op.r.redirect.sender)
        port_op.r.redirect.sender->set_fullname(p_fullname+".redirsender");
      break;
    case S_CATCH:
    case S_CHECK_CATCH:
      if(port_op.portref) port_op.portref->set_fullname(p_fullname+".portref");
      if(port_op.r.ctch.signature_ref)
        port_op.r.ctch.signature_ref->set_fullname(p_fullname+".sign");
      if(port_op.r.rcvpar)
        port_op.r.rcvpar->set_fullname(p_fullname+".rcvpar");
      if(port_op.r.fromclause)
        port_op.r.fromclause->set_fullname(p_fullname+".from");
      if(port_op.r.redirect.value)
        port_op.r.redirect.value->set_fullname(p_fullname+".redirval");
      if(port_op.r.redirect.sender)
        port_op.r.redirect.sender->set_fullname(p_fullname+".redirsender");
      break;
    case S_CHECK:
      if(port_op.portref) port_op.portref->set_fullname(p_fullname+".portref");
      if(port_op.r.fromclause)
        port_op.r.fromclause->set_fullname(p_fullname+".from");
      if(port_op.r.redirect.sender)
        port_op.r.redirect.sender->set_fullname(p_fullname+".redirsender");
      break;
    case S_CLEAR:
    case S_START_PORT:
    case S_STOP_PORT:
    case S_HALT:
      if(port_op.portref) port_op.portref->set_fullname(p_fullname+".portref");
      break;
    case S_START_COMP:
      comp_op.compref->set_fullname(p_fullname+".compref");
      comp_op.funcinstref->set_fullname(p_fullname+".funcref");
      break;
    case S_START_COMP_REFD:
      comp_op.compref->set_fullname(p_fullname+".compref");
      comp_op.derefered.value->set_fullname(p_fullname+".funcref");
      comp_op.derefered.t_list1->set_fullname(p_fullname+".<parameters>");
      break;
    case S_STOP_COMP:
    case S_KILL:
    case S_KILLED:
      if (comp_op.compref) comp_op.compref->set_fullname(p_fullname+".compref");
      break;
    case S_DONE:
      if(comp_op.compref) {
        comp_op.compref->set_fullname(p_fullname+".compref");
        if(comp_op.donereturn.donematch)
          comp_op.donereturn.donematch->set_fullname(p_fullname+".donematch");
        if(comp_op.donereturn.redirect)
          comp_op.donereturn.redirect->set_fullname(p_fullname+".redir");
      }
      break;
    case S_CONNECT:
    case S_MAP:
    case S_DISCONNECT:
    case S_UNMAP:
      config_op.compref1->set_fullname(p_fullname+".compref1");
      config_op.portref1->set_fullname(p_fullname+".portref1");
      config_op.compref2->set_fullname(p_fullname+".compref2");
      config_op.portref2->set_fullname(p_fullname+".portref2");
      break;
    case S_START_TIMER:
      timer_op.timerref->set_fullname(p_fullname+".timerref");
      if(timer_op.value) timer_op.value->set_fullname(p_fullname+".timerval");
      break;
    case S_STOP_TIMER:
    case S_TIMEOUT:
      if (timer_op.timerref)
        timer_op.timerref->set_fullname(p_fullname+".timerref");
      break;
    case S_SETVERDICT:
      setverdict.verdictval->set_fullname(p_fullname+".verdictval");
      if (setverdict.logargs)
        setverdict.logargs->set_fullname(p_fullname+".verdictreason");
      break;
    case S_TESTCASE_INSTANCE:
      testcase_inst.tcref->set_fullname(p_fullname+".tcref");
      if (testcase_inst.timerval)
        testcase_inst.timerval->set_fullname(p_fullname+".timerval");
      break;
    case S_TESTCASE_INSTANCE_REFD:
      execute_refd.value->set_fullname(p_fullname+".tcref");
      execute_refd.t_list1->set_fullname(p_fullname+".<parameters>");
      if(execute_refd.timerval)
        execute_refd.timerval->set_fullname(p_fullname+".timerval");
      break;
    case S_ACTIVATE_REFD:
    case S_UNKNOWN_INVOKED:
    case S_FUNCTION_INVOKED:
    case S_ALTSTEP_INVOKED:
      fau_refd.value->set_fullname(p_fullname+".ref");
      fau_refd.t_list1->set_fullname(p_fullname+".<parameters>");
      break;
    case S_STRING2TTCN:
      str2ttcn.val->set_fullname(p_fullname+".ti");
      str2ttcn.ref->set_fullname(p_fullname+".ref");
      break;
    default:
      FATAL_ERROR("Statement::set_fullname()");
    } // switch statementtype
  }

  void Statement::set_my_sb(StatementBlock *p_sb, size_t p_index)
  {
    my_sb=p_sb;
    switch(statementtype) {
    case S_BLOCK:
      block->set_my_sb(p_sb, p_index);
      break;
    case S_LABEL:
      label.stmt_idx = p_index;
      break;
    case S_GOTO:
      go_to.stmt_idx = p_index;
      break;
    case S_IF:
      if_stmt.ics->set_my_sb(p_sb, p_index);
      if(if_stmt.elseblock) if_stmt.elseblock->set_my_sb(p_sb, p_index);
      break;
    case S_SELECT:
      select.scs->set_my_sb(p_sb, p_index);
      break;
    case S_FOR:
    case S_WHILE:
    case S_DOWHILE:
      loop.block->set_my_sb(p_sb, p_index);
      break;
    case S_ALT:
    case S_INTERLEAVE:
      ags->set_my_sb(p_sb, p_index);
      break;
    case S_CALL:
      if(port_op.s.call.body) port_op.s.call.body->set_my_sb(p_sb, p_index);
      break;
    default:
      break;
    } // switch statementtype
  }

  void Statement::set_my_def(Definition *p_def)
  {
    switch (statementtype) {
    case S_BLOCK:
      block->set_my_def(p_def);
      break;
    case S_IF:
      if_stmt.ics->set_my_def(p_def);
      if (if_stmt.elseblock) if_stmt.elseblock->set_my_def(p_def);
      break;
    case S_SELECT:
      select.scs->set_my_def(p_def);
      break;
    case S_FOR:
    case S_WHILE:
    case S_DOWHILE:
      loop.block->set_my_def(p_def);
      break;
    case S_ALT:
    case S_INTERLEAVE:
      ags->set_my_def(p_def);
      break;
    case S_CALL:
      if (port_op.s.call.body) port_op.s.call.body->set_my_def(p_def);
      break;
    default:
      break;
    } // switch statementtype
  }

  void Statement::set_my_ags(AltGuards *p_ags)
  {
    switch (statementtype) {
    case S_BLOCK:
      block->set_my_ags(p_ags);
      break;
    case S_IF:
      if_stmt.ics->set_my_ags(p_ags);
      if (if_stmt.elseblock) if_stmt.elseblock->set_my_ags(p_ags);
      break;
    case S_SELECT:
      select.scs->set_my_ags(p_ags);
      break;
    case S_FOR:
    case S_WHILE:
    case S_DOWHILE:
      loop.block->set_my_ags(p_ags);
      break;
    case S_REPEAT:
      ags = p_ags;
      break;
    default:
      break;
    } // switch statementtype
  }

  void Statement::set_my_laic_stmt(AltGuards *p_ags, Statement *p_loop_stmt)
  {
    switch (statementtype) {
    case S_BLOCK:
      block->set_my_laic_stmt(p_ags, p_loop_stmt);
      break;
    case S_IF:
      if_stmt.ics->set_my_laic_stmt(p_ags, p_loop_stmt);
      if (if_stmt.elseblock)
        if_stmt.elseblock->set_my_laic_stmt(p_ags, p_loop_stmt);
      break;
    case S_SELECT:
      select.scs->set_my_laic_stmt(p_ags, p_loop_stmt);
      break;
    case S_ALT:
    case S_INTERLEAVE:
      if (p_loop_stmt)
        ags->set_my_laic_stmt(0, p_loop_stmt); // ags is set later
      break;
    case S_CALL:
      if (p_loop_stmt && port_op.s.call.body)
        port_op.s.call.body->set_my_laic_stmt(0, p_loop_stmt);
         // ags is set later
      break;
    case S_BREAK:
    case S_CONTINUE:
      if (p_loop_stmt)
        brk_cnt.loop_stmt=p_loop_stmt;
      brk_cnt.ags=p_ags;
      break;
    default:
      break;
    } // switch statementtype
  }

  /** \todo handle blocks, loops and conditional statements
   * (i.e. investigate their last statements within the block) */
  bool Statement::is_terminating() const
  {
    switch (statementtype) {
    case S_GOTO:
    case S_BREAK:
    case S_CONTINUE:
    case S_STOP_EXEC:
    case S_STOP_TESTCASE:
    case S_REPEAT:
    case S_RETURN:
      return true;
    case S_STOP_COMP:
    case S_KILL:
      // checking for self.stop, self.kill, mtc.stop and mtc.kill
      if (comp_op.compref) {
	Value *v_last = comp_op.compref->get_value_refd_last();
	if (v_last->get_valuetype() == Value::V_EXPR) {
	  switch (v_last->get_optype()) {
	  case Value::OPTYPE_COMP_SELF:
	  case Value::OPTYPE_COMP_MTC:
	    return true;
	  default:
	    break;
	  }
	}
      }
      return false;
    case S_WHILE:
    case S_DOWHILE:
      if(!loop.expr->is_unfoldable() && loop.expr->get_val_bool()) {
        return !loop.has_brk; // not endless loop if it has a break
      }
      return false;
    default:
      return false;
    }
  }

  StatementBlock::returnstatus_t Statement::has_return() const
  {
    switch (statementtype) {
    case S_BLOCK:
      return block->has_return();
    case S_IF:
      return if_stmt.ics->has_return(if_stmt.elseblock);
    case S_SELECT:
      return select.scs->has_return();
    case S_FOR:
    case S_WHILE:
      if (loop.block->has_return() == StatementBlock::RS_NO)
	return StatementBlock::RS_NO;
      else return StatementBlock::RS_MAYBE;
    case S_DOWHILE:
      return loop.block->has_return();
    case S_ALT: {
      StatementBlock::returnstatus_t ret_val = ags->has_return();
      if (ret_val == StatementBlock::RS_YES && !ags->has_else()) {
	// the invoked defaults may skip the entire statement
	ret_val = StatementBlock::RS_MAYBE;
      }
      return ret_val; }
    case S_CALL:
      if (port_op.s.call.body) return port_op.s.call.body->has_return();
      else return StatementBlock::RS_NO;
    default:
      if (is_terminating()) return StatementBlock::RS_YES;
      else return StatementBlock::RS_NO;
    }
  }

  bool Statement::is_receiving_stmt() const
  {
    switch (statementtype) {
    case S_ALT:
    case S_INTERLEAVE:
    case S_ALTSTEP_INSTANCE:
    case S_ALTSTEP_INVOKED:
    case S_RECEIVE:
    case S_CHECK_RECEIVE:
    case S_TRIGGER:
    case S_GETCALL:
    case S_CHECK_GETCALL:
    case S_GETREPLY:
    case S_CHECK_GETREPLY:
    case S_CATCH:
    case S_CHECK_CATCH:
    case S_CHECK:
    case S_DONE:
    case S_KILLED:
    case S_TIMEOUT:
      return true;
    default:
      return false;
    } // switch statementtype
  }

  bool Statement::has_receiving_stmt() const
  {
    switch (statementtype) {
    case S_DEF:
    case S_ASSIGNMENT:
    case S_FUNCTION_INSTANCE:
    case S_FUNCTION_INVOKED:
    case S_LOG:
    case S_STRING2TTCN:
    case S_ACTION:
    case S_LABEL:
    case S_GOTO:
    case S_BREAK:
    case S_CONTINUE:
    case S_STOP_EXEC:
    case S_STOP_TESTCASE:
    case S_REPEAT:
    case S_RETURN:
    case S_ACTIVATE:
    case S_ACTIVATE_REFD:
    case S_DEACTIVATE:
    case S_SEND:
    case S_REPLY:
    case S_RAISE:
    case S_CLEAR:
    case S_START_PORT:
    case S_STOP_PORT:
    case S_HALT:
    case S_START_COMP:
    case S_START_COMP_REFD:
    case S_STOP_COMP:
    case S_KILL:
    case S_CONNECT:
    case S_DISCONNECT:
    case S_MAP:
    case S_UNMAP:
    case S_START_TIMER:
    case S_STOP_TIMER:
    case S_SETVERDICT:
    case S_TESTCASE_INSTANCE:
    case S_TESTCASE_INSTANCE_REFD:
      return false;
    case S_ALT:
    case S_INTERLEAVE:
    case S_ALTSTEP_INSTANCE:
    case S_ALTSTEP_INVOKED:
    case S_RECEIVE:
    case S_CHECK_RECEIVE:
    case S_TRIGGER:
    case S_GETCALL:
    case S_CHECK_GETCALL:
    case S_GETREPLY:
    case S_CHECK_GETREPLY:
    case S_CATCH:
    case S_CHECK_CATCH:
    case S_CHECK:
    case S_DONE:
    case S_KILLED:
    case S_TIMEOUT:
      return true;
    case S_BLOCK:
      return block->has_receiving_stmt();
    case S_IF:
      return if_stmt.ics->has_receiving_stmt()
        || (if_stmt.elseblock && if_stmt.elseblock->has_receiving_stmt());
    case S_SELECT:
      return select.scs->has_receiving_stmt();
    case S_FOR:
    case S_WHILE:
    case S_DOWHILE:
      return loop.block->has_receiving_stmt();
    case S_CALL:
      return port_op.s.call.body && port_op.s.call.body->has_receiving_stmt();
    case S_ERROR:
    case S_START_UNDEF:
    case S_STOP_UNDEF:
    case S_UNKNOWN_INSTANCE:
    case S_UNKNOWN_INVOKED:
    default:
      FATAL_ERROR("Statement::has_receiving_stmt()");
    } // switch statementtype
  }

  bool Statement::can_repeat() const
  {
    switch (statementtype) {
    case S_TRIGGER:
    case S_DONE:
    case S_KILLED:
      return true;
    case S_RECEIVE:
    case S_CHECK_RECEIVE:
    case S_GETCALL:
    case S_CHECK_GETCALL:
    case S_GETREPLY:
    case S_CHECK_GETREPLY:
    case S_CATCH:
    case S_CHECK_CATCH:
    case S_CHECK:
    case S_TIMEOUT:
      return false;
    default:
      FATAL_ERROR("Statement::can_repeat()");
    } // switch statementtype
  }

  void Statement::chk()
  {
    switch (statementtype) {
    case S_ERROR:
      break;
    case S_START_UNDEF:
      chk_start_undef();
      break;
    case S_STOP_UNDEF:
      chk_stop_undef();
      break;
    case S_UNKNOWN_INSTANCE:
      chk_unknown_instance();
      break;
    case S_UNKNOWN_INVOKED:
    case S_FUNCTION_INVOKED:
    case S_ALTSTEP_INVOKED:
      chk_unknown_invoke();
      break;
    case S_DEF:
      def->chk();
      my_sb->register_def(def);
      break;
    case S_ASSIGNMENT:
      chk_assignment();
      break;
    case S_FUNCTION_INSTANCE:
      chk_function();
      break;
    case S_BLOCK:
      chk_block();
      break;
    case S_LOG:
    case S_ACTION:
    case S_STOP_TESTCASE:
      chk_log_action(logargs);
      break;
    case S_LABEL:
      // do nothing
      break;
    case S_GOTO:
      chk_goto();
      break;
    case S_IF:
      chk_if();
      break;
    case S_SELECT:
      chk_select();
      break;
    case S_FOR:
      chk_for();
      break;
    case S_WHILE:
      chk_while();
      break;
    case S_DOWHILE:
      chk_do_while();
      break;
    case S_BREAK:
      chk_break();
      break;
    case S_CONTINUE:
      chk_continue();
      break;
    case S_STOP_EXEC:
      // do nothing
      break;
    case S_ALT:
      chk_alt();
      break;
    case S_REPEAT:
      chk_repeat();
      break;
    case S_INTERLEAVE:
      chk_interleave();
      break;
    case S_ALTSTEP_INSTANCE:
      chk_altstep();
      break;
    case S_RETURN:
      chk_return();
      break;
    case S_ACTIVATE:
      chk_activate();
      break;
    case S_ACTIVATE_REFD:
      chk_activate_refd();
      break;
    case S_DEACTIVATE:
      chk_deactivate();
      break;
    case S_SEND:
      chk_send();
      break;
    case S_CALL:
      chk_call();
      break;
    case S_REPLY:
      chk_reply();
      break;
    case S_RAISE:
      chk_raise();
      break;
    case S_RECEIVE:
    case S_CHECK_RECEIVE:
    case S_TRIGGER:
      chk_receive();
      break;
    case S_GETCALL:
    case S_CHECK_GETCALL:
      chk_getcall();
      break;
    case S_GETREPLY:
    case S_CHECK_GETREPLY:
      chk_getreply();
      break;
    case S_CATCH:
    case S_CHECK_CATCH:
      chk_catch();
      break;
    case S_CHECK:
      chk_check();
      break;
    case S_CLEAR:
      chk_clear();
      break;
    case S_START_PORT:
    case S_STOP_PORT:
    case S_HALT:
      chk_start_stop_port();
      break;
    case S_START_COMP:
      chk_start_comp();
      break;
    case S_START_COMP_REFD:
      chk_start_comp_refd();
      break;
    case S_STOP_COMP:
    case S_KILL:
      chk_stop_kill_comp();
      break;
    case S_DONE:
      chk_done();
      break;
    case S_KILLED:
      chk_killed();
      break;
    case S_CONNECT:
    case S_DISCONNECT:
      chk_connect();
      break;
    case S_MAP:
    case S_UNMAP:
      chk_map();
      break;
    case S_START_TIMER:
      chk_start_timer();
      break;
    case S_STOP_TIMER:
    case S_TIMEOUT:
      chk_stop_timer_timeout();
      break;
    case S_SETVERDICT:
      chk_setverdict();
      chk_log_action(setverdict.logargs); // for checking verdictreason
      break;
    case S_TESTCASE_INSTANCE:
      chk_execute();
      break;
    case S_TESTCASE_INSTANCE_REFD:
      chk_execute_refd();
      break;
    case S_STRING2TTCN:
      chk_string2ttcn();
      break;
    default:
      FATAL_ERROR("Statement::chk()");
    } // switch statementtype
  }

  void Statement::chk_string2ttcn()
  {
    Error_Context cntxt(this, "In string2ttcn() statement");
    str2ttcn.val->chk_expr_type(Type::T_CSTR, "charstring", Type::EXPECTED_DYNAMIC_VALUE);
    ///
    Common::Assignment* refd_ass = str2ttcn.ref->get_refd_assignment();
    if (refd_ass==NULL) {
      error("Could not determine the assignment for second parameter");
      goto error;
    }
    switch (refd_ass->get_asstype()) {
    case Definition::A_PAR_VAL_IN:
    case Definition::A_PAR_TEMPL_IN:
      refd_ass->use_as_lvalue(*str2ttcn.ref);
    case Definition::A_VAR:
    case Definition::A_VAR_TEMPLATE:
    case Definition::A_PAR_VAL_OUT:
    case Definition::A_PAR_VAL_INOUT:
    case Definition::A_PAR_TEMPL_OUT:
    case Definition::A_PAR_TEMPL_INOUT: 
      // valid assignment types
      break;
    default:
      str2ttcn.ref->error("Reference to '%s' cannot be used as the second parameter", refd_ass->get_assname());
      goto error;
    }
    return;
  error:
    delete str2ttcn.val;
    delete str2ttcn.ref;
    statementtype = S_ERROR;
  }

  void Statement::chk_allowed_interleave()
  {
    switch (statementtype) {
    case S_BLOCK:
      block->chk_allowed_interleave();
      break;
    case S_LABEL:
      error("Label statement is not allowed within an interleave statement");
      break;
    case S_GOTO:
      error("Goto statement is not allowed within an interleave statement");
      break;
    case S_IF:
      if_stmt.ics->chk_allowed_interleave();
      if (if_stmt.elseblock) if_stmt.elseblock->chk_allowed_interleave();
      break;
    case S_SELECT:
      select.scs->chk_allowed_interleave();
      break;
    case S_FOR:
    case S_WHILE:
    case S_DOWHILE:
      loop.block->chk_allowed_interleave();
      break;
    case S_ALT:
      ags->chk_allowed_interleave();
      break;
    case S_REPEAT:
      error("Repeat statement is not allowed within an interleave statement");
      break;
    case S_ALTSTEP_INSTANCE:
      error("Invocation of an altstep is not allowed within an interleave "
	"statement");
      break;
    case S_ALTSTEP_INVOKED:
      error("Invocation of an altstep type is not allowed within an interleave"
        "statement");
      break;
    case S_RETURN:
      error("Return statement is not allowed within an interleave statement");
      break;
    case S_CALL:
      if (port_op.s.call.body) port_op.s.call.body->chk_allowed_interleave();
      break;
    default:
      // the other statements are allowed
      break;
    }
  }

  void Statement::chk_start_undef()
  {
    Ref_base *t_ref = undefstartstop.ref;
    Value *t_val = undefstartstop.val;
    Common::Assignment *t_ass;
    {
      Error_Context cntxt(this, "In start statement");
      t_ass = t_ref->get_refd_assignment();
    }
    if (!t_ass) goto error;
    switch (t_ass->get_asstype()) {
    case Definition::A_PORT:
    case Definition::A_PAR_PORT:
      statementtype = S_START_PORT;
      port_op.portref = dynamic_cast<Reference*>(t_ref);
      if (!port_op.portref) goto error;
      if (t_val) {
	t_val->error("Start port operation cannot have argument");
	delete t_val;
      }
      chk_start_stop_port();
      break;
    case Definition::A_TIMER:
    case Definition::A_PAR_TIMER:
      statementtype = S_START_TIMER;
      timer_op.timerref = dynamic_cast<Reference*>(t_ref);
      if (!timer_op.timerref) goto error;
      timer_op.value = t_val;
      chk_start_timer();
      break;
    case Definition::A_CONST:
    case Definition::A_EXT_CONST:
    case Definition::A_MODULEPAR:
    case Definition::A_VAR:
    case Definition::A_FUNCTION_RVAL:
    case Definition::A_EXT_FUNCTION_RVAL:
    case Definition::A_PAR_VAL_IN:
    case Definition::A_PAR_VAL_OUT:
    case Definition::A_PAR_VAL_INOUT:
      statementtype = S_START_COMP;
      if (!t_val) {
        error("The argument of start operation is missing, although it cannot "
	  "be a start timer or start port operation");
	goto error;
      } else if (t_val->get_valuetype() != Value::V_REFD) {
        t_val->error("The argument of start operation is not a function, "
	  "although it cannot be a start timer or start port operation");
	goto error;
      } else {
        comp_op.funcinstref = t_val->steal_ttcn_ref_base();
	delete t_val;
      }
      comp_op.compref = new Value(Value::V_REFD, t_ref);
      comp_op.compref->set_my_scope(t_ref->get_my_scope());
      comp_op.compref->set_fullname(t_ref->get_fullname());
      comp_op.compref->set_location(*t_ref);
      chk_start_comp();
      break;
    default:
      t_ref->error("Port, timer or component reference was expected as the "
	"operand of start operation instead of %s",
	t_ass->get_description().c_str());
      goto error;
    }
    return;
  error:
    delete t_ref;
    delete t_val;
    statementtype = S_ERROR;
  }

  void Statement::chk_stop_undef()
  {
    Ref_base *t_ref = undefstartstop.ref;
    Common::Assignment *t_ass;
    {
      Error_Context cntxt(this, "In stop statement");
      t_ass = t_ref->get_refd_assignment();
    }
    if (!t_ass) goto error;
    // Determine what it is that we are trying to stop; change statementtype
    switch (t_ass->get_asstype()) {
    case Definition::A_PORT:
    case Definition::A_PAR_PORT:
      statementtype = S_STOP_PORT;
      port_op.portref = dynamic_cast<Reference*>(t_ref);
      if (!port_op.portref) goto error;
      chk_start_stop_port();
      break;
    case Definition::A_TIMER:
    case Definition::A_PAR_TIMER:
      statementtype = S_STOP_TIMER;
      timer_op.timerref = dynamic_cast<Reference*>(t_ref);
      if (!timer_op.timerref) goto error;
      timer_op.value = 0;
      chk_stop_timer_timeout();
      break;
    case Definition::A_CONST:
    case Definition::A_EXT_CONST:
    case Definition::A_MODULEPAR:
    case Definition::A_VAR:
    case Definition::A_FUNCTION_RVAL:
    case Definition::A_EXT_FUNCTION_RVAL:
    case Definition::A_PAR_VAL_IN:
    case Definition::A_PAR_VAL_OUT:
    case Definition::A_PAR_VAL_INOUT:
      statementtype = S_STOP_COMP;
      comp_op.compref = new Value(Value::V_REFD, t_ref);
      comp_op.compref->set_my_scope(t_ref->get_my_scope());
      comp_op.compref->set_fullname(t_ref->get_fullname());
      comp_op.compref->set_location(*t_ref);
      chk_stop_kill_comp();
      break;
    default:
      t_ref->error("Port, timer or component reference was expected as the "
	"operand of stop operation instead of %s",
	t_ass->get_description().c_str());
      goto error;
    }
    return;
  error:
    delete t_ref;
    statementtype = S_ERROR;
  }

  void Statement::chk_unknown_instance()
  {
    Common::Assignment *t_ass;
    {
      Error_Context cntxt(this, "In function or altstep instance");
      t_ass = ref_pard->get_refd_assignment(false);
    }
    if (!t_ass) goto error;
    switch (t_ass->get_asstype()) {
    case Common::Assignment::A_FUNCTION:
    case Common::Assignment::A_FUNCTION_RVAL:
    case Common::Assignment::A_FUNCTION_RTEMP:
    case Common::Assignment::A_EXT_FUNCTION:
    case Common::Assignment::A_EXT_FUNCTION_RVAL:
    case Common::Assignment::A_EXT_FUNCTION_RTEMP:
      statementtype = S_FUNCTION_INSTANCE;
      chk_function();
      break;
    case Common::Assignment::A_ALTSTEP:
      statementtype = S_ALTSTEP_INSTANCE;
      chk_altstep();
      break;
    default:
      ref_pard->error("Reference to a function or altstep was expected "
	"instead of %s, which cannot be invoked",
	t_ass->get_description().c_str());
      goto error;
    }
    return;
  error:
    clean_up();
    statementtype = S_ERROR;
  }

  void Statement::chk_unknown_invoke()
  {
    Error_Context cntxt(this, "In apply operation");
    Type *t = fau_refd.value->get_expr_governor_last();
    if (!t) goto error;
    switch (t->get_typetype()) {
    case Type::T_ERROR:
      goto error;
    case Type::T_FUNCTION:
      statementtype = S_FUNCTION_INVOKED;
      if (t->get_function_return_type()) warning("The value returned by "
	"function type `%s' is not used", t->get_typename().c_str());
      break;
    case Type::T_ALTSTEP:
      statementtype = S_ALTSTEP_INVOKED;
      break;
    default:
      fau_refd.value->error("A value of type function or altstep was "
	"expected instead of `%s'", t->get_typename().c_str());
      goto error;
    }
    my_sb->chk_runs_on_clause(t, *this, "call");
    {
      ActualParList *parlist = new Ttcn::ActualParList;
      Ttcn::FormalParList *fp_list = t->get_fat_parameters();
      bool is_erroneous = fp_list->fold_named_and_chk(fau_refd.t_list1,
	parlist);
      delete fau_refd.t_list1;
      if(is_erroneous) {
        delete parlist;
        fau_refd.ap_list2 = 0;
      } else {
	parlist->set_fullname(get_fullname());
	parlist->set_my_scope(my_sb);
        fau_refd.ap_list2 = parlist;
      }
    }
    return;
error:
    clean_up();
    statementtype = S_ERROR;
  }

  void Statement::chk_assignment()
  {
    Error_Context cntxt(this, "In variable assignment");
    ass->chk();
  }

  void Statement::chk_function()
  {
    Error_Context cntxt(this, "In function instance");
    Common::Assignment *t_ass = ref_pard->get_refd_assignment();
    my_sb->chk_runs_on_clause(t_ass, *ref_pard, "call");
    if (t_ass->get_Type())
      ref_pard->warning("The value returned by %s is not used",
	t_ass->get_description().c_str());
  }

  void Statement::chk_block()
  {
    Error_Context cntxt(this, "In statement block");
    block->chk();
  }

  void Statement::chk_log_action(LogArguments *lga)
  {
    Error_Context cntxt(this, "In %s statement", get_stmt_name());
    if (lga) {
      lga->chk();
      if (!semantic_check_only) lga->join_strings();
    }
  }

  void Statement::chk_goto()
  {
    Error_Context cntxt(this, "In goto statement");
    if (!my_sb->has_label(*go_to.id)) {
      error("Label `%s' is used, but not defined",
	go_to.id->get_dispname().c_str());
      go_to.label = 0;
      go_to.jumps_forward = false;
      return;
    }
    Statement *label_stmt = my_sb->get_label(*go_to.id);
    label_stmt->label.used = true;
    StatementBlock *label_sb = label_stmt->get_my_sb();
    // the index of the label in its own statement block
    size_t label_idx = label_stmt->get_my_sb_index();
    // the index of the goto statement (or its parent statement) in the
    // statement block of the label
    size_t goto_idx;
    if (my_sb == label_sb) goto_idx = go_to.stmt_idx;
    else {
      // the goto statement is within a nested statement block
      StatementBlock *goto_sb = my_sb, *parent_sb = my_sb->get_my_sb();
      while (parent_sb != label_sb) {
	// go up until the block of the label is found
	if (!parent_sb) FATAL_ERROR("Statement::chk_goto()");
	goto_sb = parent_sb;
	parent_sb = parent_sb->get_my_sb();
      }
      goto_idx = goto_sb->get_my_sb_index();
    }
    if (label_idx > goto_idx) {
      bool error_flag = false;
      for (size_t i = goto_idx + 1; i < label_idx; i++) {
	Statement *stmt = label_sb->get_stmt_byIndex(i);
	if (stmt->get_statementtype() != S_DEF) continue;
	if (!error_flag) {
	  error("Jump to label `%s' crosses local definition",
	    go_to.id->get_dispname().c_str());
	  error_flag = true;
	}
	stmt->note("Definition of %s is here",
	  stmt->get_def()->get_description().c_str());
      }
      if (error_flag)
	label_stmt->note("Label `%s' is here",
	  go_to.id->get_dispname().c_str());
      go_to.jumps_forward = true;
    } else go_to.jumps_forward = false;
    go_to.label = label_stmt;
  }

  void Statement::chk_if()
  {
    bool unreach=false;
    if_stmt.ics->chk(unreach);
    if(if_stmt.elseblock) {
      Error_Context cntxt(if_stmt.elseblock_location, "In else statement");
      if(unreach) if_stmt.elseblock_location->warning
        ("Control never reaches this code because of previous effective"
         " condition(s)");
      if_stmt.elseblock->chk();
    }
  }

  /** \todo review */
  void Statement::chk_select()
  {
    Error_Context cntxt(this, "In select case statement");
    Type *t_gov=0;
    for(int turn=0; turn<2; turn++) {
      if(turn) select.expr->set_lowerid_to_ref();
      Type::typetype_t tt=select.expr->get_expr_returntype();
      t_gov=select.expr->get_expr_governor(Type::EXPECTED_DYNAMIC_VALUE);
      if(!t_gov || tt==Type::T_ERROR) {
        SelectCases *scs=select.scs;
        for(size_t i=0; i<scs->get_nof_scs(); i++) {
          TemplateInstances *tis=scs->get_sc_byIndex(i)->get_tis();
          if(!tis) continue;
          for(size_t j=0; j<tis->get_nof_tis(); j++) {
            TemplateInstance *ti=tis->get_ti_byIndex(j);
            if(turn) ti->get_Template()->set_lowerid_to_ref();
            t_gov=ti->get_expr_governor(Type::EXPECTED_TEMPLATE);
            tt=ti->get_expr_returntype(Type::EXPECTED_TEMPLATE);
            if(t_gov && tt!=Type::T_ERROR) break;
          } // for j
          if(t_gov) break;
        } // for i
      }
      else t_gov=select.expr->get_expr_governor(Type::EXPECTED_DYNAMIC_VALUE);
    } // for turn
    if(!t_gov) {
      select.expr->error("Cannot determine the type of the expression");
      t_gov=Type::get_pooltype(Type::T_ERROR);
    }
    select.expr->set_my_governor(t_gov);
    t_gov->chk_this_value_ref(select.expr);
    t_gov->chk_this_value(select.expr, 0, Type::EXPECTED_DYNAMIC_VALUE,
      INCOMPLETE_NOT_ALLOWED, OMIT_NOT_ALLOWED, SUB_CHK);
    select.scs->chk(t_gov);
  }

  void Statement::chk_for()
  {
    Error_Context cntxt(this, "In for statement");
    if (loop.for_stmt.varinst) loop.for_stmt.init_varinst->chk_for();
    else loop.for_stmt.init_ass->chk();
    loop.for_stmt.finalexpr->chk_expr_bool(Type::EXPECTED_DYNAMIC_VALUE);
    if(!loop.for_stmt.finalexpr->is_unfoldable()
       && !loop.for_stmt.finalexpr->get_val_bool())
      loop.for_stmt.finalexpr->warning
        ("Control never reaches this code because the"
         " final conditional expression evals to false");
    loop.for_stmt.step->chk();
    loop.block->set_my_laic_stmt(0, this);
    loop.block->chk();
  }

  void Statement::chk_while()
  {
    Error_Context cntxt(this, "In while statement");
    loop.expr->chk_expr_bool(Type::EXPECTED_DYNAMIC_VALUE);
    if(!loop.expr->is_unfoldable() && !loop.expr->get_val_bool())
      loop.expr->warning("Control never reaches this code because the"
                         " conditional expression evals to false");
    loop.block->set_my_laic_stmt(0, this);
    loop.block->chk();
  }

  void Statement::chk_do_while()
  {
    Error_Context cntxt(this, "In do-while statement");
    loop.block->set_my_laic_stmt(0, this);
    loop.block->chk();
    loop.expr->chk_expr_bool(Type::EXPECTED_DYNAMIC_VALUE);
  }

  void Statement::chk_break()
  {
    Error_Context cntxt(this, "In break statement");
    if (!brk_cnt.loop_stmt && !brk_cnt.ags)
      error("Break statement cannot be used outside loops, alt or interleave"
        " statements, altsteps or response and exception handling part of call"
        " operations");
    if (brk_cnt.loop_stmt)
      brk_cnt.loop_stmt->loop.has_brk=true;
  }

  void Statement::chk_continue()
  {
    Error_Context cntxt(this, "In continue statement");
    if (brk_cnt.loop_stmt) {
      brk_cnt.loop_stmt->loop.has_cnt=true;
      if (brk_cnt.ags) brk_cnt.loop_stmt->loop.has_cnt_in_ags=true;
    } else
      error("Continue statement cannot be used outside loops");
  }

  void Statement::chk_alt()
  {
    Error_Context cntxt(this, "In alt construct");
    ags->set_my_ags(ags);
    ags->set_my_laic_stmt(ags, 0);
    ags->chk();
  }

  void Statement::chk_repeat()
  {
    Error_Context cntxt(this, "In repeat statement");
    if (ags) ags->repeat_found();
    else error("Repeat statement cannot be used outside alt statements, "
      "altsteps or response and exception handling part of call operations");
  }

  void Statement::chk_interleave()
  {
    Error_Context cntxt(this, "In interleave statement");
    ags->set_my_laic_stmt(ags, 0);
    ags->chk();
    ags->chk_allowed_interleave();
  }

  void Statement::chk_altstep()
  {
    Error_Context cntxt(this, "In altstep instance");
    Common::Assignment *t_ass = ref_pard->get_refd_assignment();
    my_sb->chk_runs_on_clause(t_ass, *ref_pard, "call");
  }

  void Statement::chk_return()
  {
    Error_Context cntxt(this, "In return statement");
    Definition *my_def = my_sb->get_my_def();
    if (!my_def) {
      error("Return statement cannot be used in the control part. "
            "It is allowed only in functions and altsteps");
      goto error;
    }
    switch (my_def->get_asstype()) {
    case Definition::A_FUNCTION:
      if (returnexpr.t) {
        returnexpr.t->error("Unexpected return value. The function does not "
                            "have return type");
	goto error;
      }
      break;
    case Definition::A_FUNCTION_RVAL:
      if (!returnexpr.t) {
	error("Missing return value. The function should return a value of "
	  "type `%s'", my_def->get_Type()->get_typename().c_str());
	goto error;
      } else if (!returnexpr.t->is_Value()) {
	returnexpr.t->error("A specific value without matching symbols was "
                            "expected as return value");
	goto error;
      } else {
	returnexpr.v = returnexpr.t->get_Value();
	delete returnexpr.t;
	returnexpr.t = 0;
	Type *return_type = my_def->get_Type();
	returnexpr.v->set_my_governor(return_type);
	return_type->chk_this_value_ref(returnexpr.v);
	return_type->chk_this_value(returnexpr.v, 0, Type::EXPECTED_DYNAMIC_VALUE,
	  INCOMPLETE_NOT_ALLOWED, OMIT_NOT_ALLOWED, SUB_CHK);
      }
      break;
    case Definition::A_FUNCTION_RTEMP:
      if (!returnexpr.t) {
	error("Missing return template. The function should return a template "
	  "of type `%s'", my_def->get_Type()->get_typename().c_str());
	goto error;
      } else {
	Type *return_type = my_def->get_Type();
	returnexpr.t->set_my_governor(return_type);
	return_type->chk_this_template_ref(returnexpr.t);
	return_type->chk_this_template_generic(returnexpr.t, INCOMPLETE_NOT_ALLOWED,
	  OMIT_ALLOWED, ANY_OR_OMIT_ALLOWED, SUB_CHK, NOT_IMPLICIT_OMIT, 0);
        Def_Function_Base* dfb = dynamic_cast<Def_Function_Base*>(my_def);
        if (!dfb) FATAL_ERROR("Statement::chk_return()");
        returnexpr.gen_restriction_check =
          returnexpr.t->chk_restriction("return template",
            dfb->get_template_restriction());
      }
      break;
    case Definition::A_ALTSTEP:
      if (returnexpr.t) {
	returnexpr.t->error("An altstep cannot return a value");
	goto error;
      }
      break;
    default:
      error("Return statement cannot be used in a %s. It is allowed only in "
            "functions and altsteps", my_def->get_assname());
      goto error;
    }
    return;
  error:
    delete returnexpr.t;
    returnexpr.t = 0;
  }

  void Statement::chk_activate()
  {
    Error_Context cntxt(this, "In activate statement");
    if (!ref_pard->chk_activate_argument()) {
      clean_up();
      statementtype = S_ERROR;
    }
  }

  void Statement::chk_activate_refd()
  {
    Error_Context cntxt(this, "In activate statement");
    Type *t = fau_refd.value->get_expr_governor_last();
    if (!t) goto error;
    switch (t->get_typetype()) {
    case Type::T_ERROR:
      goto error;
    case Type::T_ALTSTEP:
      break;
    default:
      fau_refd.value->error("A value of type altstep was expected in the "
	"argument of `derefers()' instead of `%s'", t->get_typename().c_str());
      goto error;
    }
    if (t->get_fat_runs_on_self()) {
      fau_refd.value->error("The argument of `derefers()' cannot be an altstep "
        "reference with 'runs on self' clause");
      goto error;
    }
    my_sb->chk_runs_on_clause(t, *this, "activate");
    {
      ActualParList *parlist = new ActualParList;
      Ttcn::FormalParList *fp_list = t->get_fat_parameters();
      bool is_erroneous = fp_list->fold_named_and_chk(fau_refd.t_list1,
	parlist);
      delete fau_refd.t_list1;
      if(is_erroneous) {
        delete parlist;
        fau_refd.ap_list2 = 0;
        goto error;
      } else {
        parlist->set_fullname(get_fullname());
	parlist->set_my_scope(my_sb);
        fau_refd.ap_list2 = parlist;
        if (!fp_list->chk_activate_argument(parlist,get_fullname().c_str()))
          goto error;
      }
    }
    return;
error:
    clean_up();
    statementtype = S_ERROR;
  }

  void Statement::chk_deactivate()
  {
    if (deactivate) {
      Error_Context cntxt(this, "In deactivate statement");
      deactivate->chk_expr_default(Type::EXPECTED_DYNAMIC_VALUE);
    }
  }

  void Statement::chk_send()
  {
    Error_Context cntxt(this, "In send statement");
    // checking the port reference
    Type *port_type = chk_port_ref(port_op.portref);
    // determining the message type
    Type *msg_type = 0;
    bool msg_type_determined = false;
    if (port_type) {
      Ttcn::PortTypeBody *port_type_body = port_type->get_PortBody();
      TypeSet *out_msgs = port_type_body->get_out_msgs();
      if (out_msgs) {
        if (out_msgs->get_nof_types() == 1) {
	  // there is only one outgoing message type
	  msg_type = out_msgs->get_type_byIndex(0);
	} else {
	  // there are more than one outgoing message types
	  msg_type = get_outgoing_type(port_op.s.sendpar);
	  if (msg_type) {
	    size_t nof_comp_types =
	      out_msgs->get_nof_compatible_types(msg_type);
	    if (nof_comp_types == 0) {
	      port_op.s.sendpar->error("Message type `%s' is not present on "
		"the outgoing list of port type `%s'",
		msg_type->get_typename().c_str(),
		port_type->get_typename().c_str());
	    } else if (nof_comp_types > 1) {
	      port_op.s.sendpar->error("Type of the message is ambiguous: "
		"`%s' is compatible with more than one outgoing message types "
		"of port type `%s'", msg_type->get_typename().c_str(),
		port_type->get_typename().c_str());
	    }
	  } else {
	    port_op.s.sendpar->error("Cannot determine the type of the "
	      "outgoing message");
	  }
	}
	msg_type_determined = true;
      } else if (port_type_body->get_operation_mode() ==
		 PortTypeBody::PO_PROCEDURE) {
        port_op.portref->error("Message-based operation `send' is not "
	  "applicable to a procedure-based port of type `%s'",
	  port_type->get_typename().c_str());
      } else {
        port_op.portref->error("Port type `%s' does not have any outgoing "
	  "message types", port_type->get_typename().c_str());
      }
    }
    // determining the message type if it is not done so far
    if (!msg_type_determined) {
      msg_type = get_outgoing_type(port_op.s.sendpar);
    }
    if (!msg_type) msg_type = Type::get_pooltype(Type::T_ERROR);
    // checking the parameter (template instance)
    port_op.s.sendpar->chk(msg_type);
    // checking for invalid message types
    msg_type = msg_type->get_type_refd_last();
    switch (msg_type->get_typetype()) {
    case Type::T_SIGNATURE:
      port_op.s.sendpar->error("The type of send parameter is signature `%s', "
	"which cannot be a message type", msg_type->get_typename().c_str());
      break;
    case Type::T_PORT:
      port_op.s.sendpar->error("The type of send parameter is port type `%s', "
	"which cannot be a message type", msg_type->get_typename().c_str());
      break;
    case Type::T_DEFAULT:
      port_op.s.sendpar->error("The type of send parameter is the `default' "
	"type, which cannot be a message type");
    default:
      break;
    }
    // checking for presence of wildcards in the template body
    port_op.s.sendpar->get_Template()->chk_specific_value(false);
    // checking to clause
    chk_to_clause(port_type);
  }

  void Statement::chk_call()
  {
    Error_Context cntxt(this, "In call statement");
    // checking the port reference
    Type *port_type = chk_port_ref(port_op.portref);
    // determining the signature of the argument
    Type *signature = 0;
    bool signature_determined = false;
    if (port_type) {
      PortTypeBody *port_type_body = port_type->get_PortBody();
      TypeSet *out_sigs = port_type_body->get_out_sigs();
      if (out_sigs) {
        if (out_sigs->get_nof_types() == 1) {
	  // there is only one outgoing signature
	  signature = out_sigs->get_type_byIndex(0);
	} else {
	  // there are more than one outgoing signatures
	  signature = get_outgoing_type(port_op.s.sendpar);
	  if (signature) {
	    if (!out_sigs->has_type(signature)) {
	      port_op.s.sendpar->error("Signature `%s' is not present on the "
		"outgoing list of port type `%s'",
		signature->get_typename().c_str(),
		port_type->get_typename().c_str());
	    }
	  } else {
	    port_op.s.sendpar->error("Cannot determine the type of the "
	      "signature");
	  }
	}
	signature_determined = true;
      } else if (port_type_body->get_operation_mode() ==
		 PortTypeBody::PO_MESSAGE) {
        port_op.portref->error("Procedure-based operation `call' is not "
	  "applicable to a message-based port of type `%s'",
	  port_type->get_typename().c_str());
      } else {
        port_op.portref->error("Port type `%s' does not have any outgoing "
	  "signatures", port_type->get_typename().c_str());
      }
    }
    if (!signature_determined)
      signature = get_outgoing_type(port_op.s.sendpar);
    if (!signature) signature = Type::get_pooltype(Type::T_ERROR);
    // checking the parameter (template instance)
    port_op.s.sendpar->chk(signature);
    signature = signature->get_type_refd_last();
    bool is_noblock_sig = false;
    Type::typetype_t tt = signature->get_typetype();
    switch (tt) {
    case Type::T_SIGNATURE:
      // the signature is known and correct
      is_noblock_sig = signature->is_nonblocking_signature();
    case Type::T_ERROR:
      break;
    default:
	port_op.s.sendpar->error("The type of parameter is `%s', which is not "
	  "a signature", signature->get_typename().c_str());
    }
    // checking presence/absence of optional parts
    if (is_noblock_sig) {
      if (port_op.s.call.timer) {
	port_op.s.call.timer->error("A call of non-blocking signature `%s' "
	  "cannot have call timer", signature->get_typename().c_str());
      } else if (port_op.s.call.nowait) {
	error("A call of non-blocking signature `%s' cannot use the "
	  "`nowait' keyword", signature->get_typename().c_str());
      }
      if (port_op.s.call.body) {
	error("A call of non-blocking signature `%s' cannot have "
	  "response and exception handling part",
	  signature->get_typename().c_str());
      }
    } else if (port_op.s.call.nowait) {
      if (port_op.s.call.body) {
	error("A call with `nowait' keyword cannot have response and "
	  "exception handling part");
      }
    } else {
      // do not issue any error if the signature is erroneous
      // because it could have been a non-blocking one
      if (tt == Type::T_SIGNATURE && !port_op.s.call.body) {
	error("Response and exception handling part is missing from "
	  "blocking call operation");
      }
    }
    // checking call timer
    if (port_op.s.call.timer) {
      Error_Context cntxt2(port_op.s.call.timer, "In call timer value");
      port_op.s.call.timer->chk_expr_float(Type::EXPECTED_DYNAMIC_VALUE);
      Value *t_val = port_op.s.call.timer->get_value_refd_last();
      if (t_val->get_valuetype() == Value::V_REAL) {
        ttcn3float v_real = t_val->get_val_Real();
        if (v_real < 0.0) {
          port_op.s.call.timer->error("The call timer has "
            "negative duration: `%s'", Real2string(v_real).c_str());
        } else if (isSpecialFloatValue(v_real)) {
          port_op.s.call.timer->error("The call timer duration cannot be %s",
                                      Real2string(v_real).c_str());
        }
      }
    }
    // checking to clause
    chk_to_clause(port_type);
    // checking response and exception handling part
    if (port_op.s.call.body) chk_call_body(port_type, signature);
  }

  void Statement::chk_reply()
  {
    Error_Context cntxt(this, "In reply statement");
    // checking the port reference
    Type *port_type = chk_port_ref(port_op.portref);
    // determining the signature of the argument
    Type *signature = 0;
    bool signature_determined = false;
    if (port_type) {
      PortTypeBody *port_type_body = port_type->get_PortBody();
      TypeSet *in_sigs = port_type_body->get_in_sigs();
      if (in_sigs) {
        if (in_sigs->get_nof_types() == 1) {
	  // there is only one incoming signature
	  signature = in_sigs->get_type_byIndex(0);
	} else {
	  // there are more than one incoming signatures
	  signature = get_outgoing_type(port_op.s.sendpar);
	  if (signature) {
	    if (!in_sigs->has_type(signature)) {
	      port_op.s.sendpar->error("Signature `%s' is not present on the "
		"incoming list of port type `%s'",
		signature->get_typename().c_str(),
		port_type->get_typename().c_str());
	    }
	  } else {
	    port_op.s.sendpar->error("Cannot determine the type of the "
	      "signature");
	  }
	}
	signature_determined = true;
      } else if (port_type_body->get_operation_mode() ==
		 PortTypeBody::PO_MESSAGE) {
	port_op.portref->error("Procedure-based operation `reply' is not "
	  "applicable to a message-based port of type `%s'",
	  port_type->get_typename().c_str());
      } else {
	port_op.portref->error("Port type `%s' does not have any incoming "
	  "signatures", port_type->get_typename().c_str());
      }
    }
    if (!signature_determined)
      signature = get_outgoing_type(port_op.s.sendpar);
    if (!signature) signature = Type::get_pooltype(Type::T_ERROR);
    // checking the parameter (template instance)
    port_op.s.sendpar->chk(signature);
    signature = signature->get_type_refd_last();
    Type *return_type = 0;
    switch (signature->get_typetype()) {
    case Type::T_SIGNATURE:
      // the signature is known and correct
      if (signature->is_nonblocking_signature())
	error("Operation `reply' is not applicable to non-blocking signature "
	  "`%s'", signature->get_typename().c_str());
      else return_type = signature->get_signature_return_type();
      // checking the presence/absence of reply value
      if (port_op.s.replyval) {
	if (!return_type) {
	  port_op.s.replyval->error("Unexpected return value. Signature "
	    "`%s' does not have return type",
	    signature->get_typename().c_str());
	}
      } else if (return_type) {
	error("Missing return value. Signature `%s' returns type `%s'",
	  signature->get_typename().c_str(),
	  return_type->get_typename().c_str());
      }
    case Type::T_ERROR:
      break;
    default:
      port_op.s.sendpar->error("The type of parameter is `%s', which is not a "
	"signature", signature->get_typename().c_str());
    }
    // checking the reply value if present
    if (port_op.s.replyval) {
      Error_Context cntxt2(port_op.s.replyval, "In return value");
      if (!return_type) return_type = Type::get_pooltype(Type::T_ERROR);
      port_op.s.replyval->set_my_governor(return_type);
      return_type->chk_this_value_ref(port_op.s.replyval);
      return_type->chk_this_value(port_op.s.replyval, 0,
        Type::EXPECTED_DYNAMIC_VALUE, INCOMPLETE_NOT_ALLOWED, OMIT_NOT_ALLOWED,
        SUB_CHK);
    }
    // checking to clause
    chk_to_clause(port_type);
  }

  void Statement::chk_raise()
  {
    Error_Context cntxt(this, "In raise statement");
    // checking the port reference
    Type *port_type = chk_port_ref(port_op.portref);
    // determining the signature of the exception
    port_op.s.raise.signature =
      chk_signature_ref(port_op.s.raise.signature_ref);
    // checking whether the signature is present on the incoming list
    // of the respective port type
    if (port_type) {
      PortTypeBody *port_type_body = port_type->get_PortBody();
      TypeSet *in_sigs = port_type_body->get_in_sigs();
      if (port_type_body->get_operation_mode() ==
		 PortTypeBody::PO_MESSAGE) {
	port_op.portref->error("Procedure-based operation `raise' is not "
	  "applicable to a message-based port of type `%s'",
	  port_type->get_typename().c_str());
      } else if (in_sigs) {
	if (port_op.s.raise.signature) {
	  if (!in_sigs->has_type(port_op.s.raise.signature)) {
	    port_op.s.raise.signature_ref->error("Signature `%s' is not "
	      "present on the incoming list of port type `%s'",
	      port_op.s.raise.signature->get_typename().c_str(),
	      port_type->get_typename().c_str());
	  }
	} else if (in_sigs->get_nof_types() == 1) {
	  // if the signature is unknown and the port type has exactly one
	  // incoming signature then use that for further checking
	  port_op.s.raise.signature =
	    in_sigs->get_type_byIndex(0)->get_type_refd_last();
	}
      } else {
	port_op.portref->error("Port type `%s' does not have any incoming "
	  "signatures", port_type->get_typename().c_str());
      }
    }
    // determining the type of exception
    Type *exc_type = 0;
    bool exc_type_determined = false;
    if (port_op.s.raise.signature) {
      // the signature is known
      SignatureExceptions *exceptions =
	port_op.s.raise.signature->get_signature_exceptions();
      if (exceptions) {
	if (exceptions->get_nof_types() == 1) {
	  // the signature has exactly one exception type
	  // use that for checking
	  exc_type = exceptions->get_type_byIndex(0);
	} else {
	  // the signature has more than one exception types
	  exc_type = get_outgoing_type(port_op.s.sendpar);
	  if (exc_type) {
	    size_t nof_comp_types =
	      exceptions->get_nof_compatible_types(exc_type);
	    if (nof_comp_types == 0) {
	      port_op.s.sendpar->error("Type `%s' is not present on the "
		"exception list of signature `%s'",
		exc_type->get_typename().c_str(),
		port_op.s.raise.signature->get_typename().c_str());
	    } else if (nof_comp_types > 1) {
	      port_op.s.sendpar->error("Type of the exception is ambiguous: "
		"`%s' is compatible with more than one exception types of "
		"signature `%s'", exc_type->get_typename().c_str(),
		port_op.s.raise.signature->get_typename().c_str());
	    }
	  } else {
	    port_op.s.sendpar->error("Cannot determine the type of the "
	      "exception");
	  }
	}
	exc_type_determined = true;
      } else {
	port_op.s.raise.signature_ref->error("Signature `%s' does not have "
	  "exceptions", port_op.s.raise.signature->get_typename().c_str());
      }
    }
    // determining the type of exception if it is not done so far
    if (!exc_type_determined) {
      exc_type = get_outgoing_type(port_op.s.sendpar);
    }
    if (!exc_type) exc_type = Type::get_pooltype(Type::T_ERROR);
    // checking the exception template
    port_op.s.sendpar->chk(exc_type);
    // checking for invalid exception types
    exc_type = exc_type->get_type_refd_last();
    switch (exc_type->get_typetype()) {
    case Type::T_SIGNATURE:
      port_op.s.sendpar->error("The type of raise parameter is signature `%s', "
	"which cannot be an exception type",
	exc_type->get_typename().c_str());
      break;
    case Type::T_PORT:
      port_op.s.sendpar->error("The type of raise parameter is port type `%s', "
	"which cannot be an exception type",
	exc_type->get_typename().c_str());
      break;
    case Type::T_DEFAULT:
      port_op.s.sendpar->error("The type of raise parameter is the `default' "
	"type, which cannot be an exception type");
    default:
      break;
    }
    // checking for presence of wildcards in the template body
    port_op.s.sendpar->get_Template()->chk_specific_value(false);
    // checking to clause
    chk_to_clause(port_type);
  }

  void Statement::chk_receive()
  {
    // determining statement type
    const char *stmt_name = get_stmt_name();
    Error_Context cntxt(this, "In %s statement", stmt_name);
    // checking the port reference
    Type *port_type = chk_port_ref(port_op.portref);
    // checking the parameter and/or value redirect
    if (port_op.r.rcvpar) {
      // the receive parameter (template instance) is present
      // trying to determine type of the incoming message
      Type *msg_type = 0;
      bool msg_type_determined = false, value_redirect_checked = false;
      if (port_type) {
	// the port reference is correct and the port type is known
	PortTypeBody *port_type_body = port_type->get_PortBody();
	TypeSet *in_msgs = port_type_body->get_in_msgs();
	if (in_msgs) {
          if (in_msgs->get_nof_types() == 1) {
	    // there is only one incoming message type
	    // use that for checking
	    msg_type = in_msgs->get_type_byIndex(0);
	  } else {
	    // there are more than one incoming message types
	    msg_type = get_incoming_type(port_op.r.rcvpar,
	      port_op.r.redirect.value, value_redirect_checked);
	    if (msg_type) {
	      size_t nof_comp_types =
		in_msgs->get_nof_compatible_types(msg_type);
	      if (nof_comp_types == 0) {
		port_op.r.rcvpar->error("Message type `%s' is not present on "
		  "the incoming list of port of type `%s'",
		  msg_type->get_typename().c_str(),
		  port_type->get_typename().c_str());
	      } else if (nof_comp_types > 1) {
		port_op.r.rcvpar->error("Type of the message is ambiguous: "
		  "`%s' is compatible with more than one incoming message "
		  "types of port type `%s'", msg_type->get_typename().c_str(),
		  port_type->get_typename().c_str());
	      }
	    } else {
	      port_op.r.rcvpar->error("Cannot determine the type of the "
	        "incoming message");
	    }
	  }
	  msg_type_determined = true;
	} else if (port_type_body->get_operation_mode() ==
	           PortTypeBody::PO_PROCEDURE) {
	  port_op.portref->error("Message-based operation `%s' is not "
	    "applicable to a procedure-based port of type `%s'", stmt_name,
	    port_type->get_typename().c_str());
	} else {
	  port_op.portref->error("Port type `%s' does not have any incoming "
	    "message types", port_type->get_typename().c_str());
	}
      } else if (!port_op.portref) {
	// the statement refers to 'any port'
	port_op.r.rcvpar->error("Operation `any port.%s' cannot have parameter",
	  stmt_name);
	if (port_op.r.redirect.value) {
	  port_op.r.redirect.value->error("Operation `any port.%s' cannot have "
	    "value redirect", stmt_name);
	}
      }
      if (!msg_type_determined) {
	msg_type = get_incoming_type(port_op.r.rcvpar, port_op.r.redirect.value,
	  value_redirect_checked);
      }
      if (!msg_type) msg_type = Type::get_pooltype(Type::T_ERROR);
      // check the template instance using the message type
      port_op.r.rcvpar->chk(msg_type);
      // check the value redirect if it is not done so far
      if (!value_redirect_checked)
	chk_value_redirect(port_op.r.redirect.value, msg_type);
    } else {
      // the statement does not have parameter
      if (port_type) {
        PortTypeBody *port_type_body = port_type->get_PortBody();
        if (!port_type_body->get_in_msgs()) {
          // the port type is known and it does not have incoming messages
	  if (port_type_body->get_operation_mode() ==
	      PortTypeBody::PO_PROCEDURE) {
	    port_op.portref->error("Message-based operation `%s' is not "
	      "applicable to a procedure-based port of type `%s'", stmt_name,
	      port_type->get_typename().c_str());
	  } else {
	    port_op.portref->error("Port type `%s' does not have any incoming "
	      "message types", port_type->get_typename().c_str());
	  }
	}
      }
      if (port_op.r.redirect.value) {
	port_op.r.redirect.value->error("Value redirect cannot be used without "
	  "receive parameter");
	chk_value_redirect(port_op.r.redirect.value, 0);
      }
    }
    // checking from clause and sender redirect
    chk_from_clause(port_type);
  }

  void Statement::chk_getcall()
  {
    // determining statement type
    const char *stmt_name = get_stmt_name();
    Error_Context cntxt(this, "In %s statement", stmt_name);
    // checking the port reference
    Type *port_type = chk_port_ref(port_op.portref);
    if (port_op.r.rcvpar) {
      // the parameter (signature template) is present
      // determining the signature of the argument
      Type *signature = 0;
      bool signature_determined = false;
      if (port_type) {
	// the port reference is correct and the port type is known
	PortTypeBody *port_type_body = port_type->get_PortBody();
	TypeSet *in_sigs = port_type_body->get_in_sigs();
	if (in_sigs) {
	  if (in_sigs->get_nof_types() == 1) {
	    // there is only one incoming signature
	    signature = in_sigs->get_type_byIndex(0);
	  } else {
	    // there are more than one incoming signatures
	    signature = get_outgoing_type(port_op.r.rcvpar);
	    if (signature) {
	      if (!in_sigs->has_type(signature)) {
		port_op.r.rcvpar->error("Signature `%s' is not present on the "
		  "incoming list of port type `%s'",
		  signature->get_typename().c_str(),
		  port_type->get_typename().c_str());
	      }
	    } else {
	      port_op.r.rcvpar->error("Cannot determine the type of the "
		"signature");
	    }
	  }
	  signature_determined = true;
	} else if (port_type_body->get_operation_mode() ==
		   PortTypeBody::PO_MESSAGE) {
	  port_op.portref->error("Procedure-based operation `%s' is not "
	    "applicable to a message-based port of type `%s'", stmt_name,
	    port_type->get_typename().c_str());
	} else {
	  port_op.portref->error("Port type `%s' does not have any incoming "
	    "signatures", port_type->get_typename().c_str());
	}
      } else if (!port_op.portref) {
	// the statement refers to 'any port'
	port_op.r.rcvpar->error("Operation `any port.%s' cannot have parameter",
	  stmt_name);
	if (port_op.r.redirect.param) {
	  port_op.r.redirect.param->error("Operation `any port.%s' cannot "
	    "have parameter redirect", stmt_name);
	}
      }
      if (!signature_determined)
	signature = get_outgoing_type(port_op.r.rcvpar);
      if (!signature) signature = Type::get_pooltype(Type::T_ERROR);
      // checking the parameter (template instance)
      port_op.r.rcvpar->chk(signature);
      // checking whether the argument is a signature template
      // and checking the parameter redirect if present
      signature = signature->get_type_refd_last();
      switch (signature->get_typetype()) {
      case Type::T_SIGNATURE:
	if (port_op.r.redirect.param)
	  port_op.r.redirect.param->chk(signature, false);
	break;
      case Type::T_ERROR:
	if (port_op.r.redirect.param)
	  port_op.r.redirect.param->chk_erroneous();
	break;
      default:
	port_op.r.rcvpar->error("The type of parameter is `%s', which is not "
	  "a signature", signature->get_typename().c_str());
	if (port_op.r.redirect.param)
	  port_op.r.redirect.param->chk_erroneous();
      }
    } else {
      // the statement does not have parameter
      if (port_type) {
        PortTypeBody *port_type_body = port_type->get_PortBody();
	if (!port_type_body->get_in_sigs()) {
          // the port type is known and it does not have incoming signatures
	  if (port_type_body->get_operation_mode() ==
	      PortTypeBody::PO_MESSAGE) {
	    port_op.portref->error("Procedure-based operation `%s' is not "
	      "applicable to a message-based port of type `%s'", stmt_name,
	      port_type->get_typename().c_str());
	  } else {
	    port_op.portref->error("Port type `%s' does not have any incoming "
	      "signatures", port_type->get_typename().c_str());
	  }
	}
      }
      if (port_op.r.redirect.param) {
	port_op.r.redirect.param->error("Parameter redirect cannot be used "
	  "without signature template");
	port_op.r.redirect.param->chk_erroneous();
      }
    }
    // checking from clause and sender redirect
    chk_from_clause(port_type);
  }

  void Statement::chk_getreply()
  {
    // determining statement type
    const char *stmt_name = get_stmt_name();
    Error_Context cntxt(this, "In %s statement", stmt_name);
    // checking the port reference
    Type *port_type = chk_port_ref(port_op.portref);
    if (port_op.r.rcvpar) {
      // the parameter (signature template) is present
      // determining the signature of the argument
      Type *signature = 0;
      bool signature_determined = false;
      if (port_type) {
	// the port reference is correct and the port type is known
	PortTypeBody *port_type_body = port_type->get_PortBody();
	if (port_type_body->getreply_allowed()) {
	  TypeSet *out_sigs = port_type_body->get_out_sigs();
	  if (out_sigs->get_nof_types() == 1) {
	    // there is only one outgoing signature
	    signature = out_sigs->get_type_byIndex(0);
	  } else {
	    // there are more than one outgoing signatures
	    signature = get_outgoing_type(port_op.r.rcvpar);
	    if (signature) {
	      if (!out_sigs->has_type(signature)) {
		port_op.r.rcvpar->error("Signature `%s' is not present on the "
		  "outgoing list of port type `%s'",
		  signature->get_typename().c_str(),
		  port_type->get_typename().c_str());
	      }
	    } else {
	      port_op.r.rcvpar->error("Cannot determine the type of the "
		"signature");
	    }
	  }
	  signature_determined = true;
	} else if (port_type_body->get_operation_mode() ==
		   PortTypeBody::PO_MESSAGE) {
	  port_op.portref->error("Procedure-based operation `%s' is not "
	    "applicable to a message-based port of type `%s'", stmt_name,
	    port_type->get_typename().c_str());
	} else {
	  port_op.portref->error("Port type `%s' does not have any outgoing "
	    "signatures that support reply", port_type->get_typename().c_str());
	}
      } else if (!port_op.portref) {
	// the statement refers to 'any port'
	port_op.r.rcvpar->error("Operation `any port.%s' cannot have parameter",
	  stmt_name);
	if (port_op.r.getreply_valuematch) {
	  port_op.r.getreply_valuematch->error("Operation `any port.%s' cannot "
	    "have value match", stmt_name);
	}
	if (port_op.r.redirect.value) {
	  port_op.r.redirect.value->error("Operation `any port.%s' cannot "
	    "have value redirect", stmt_name);
	}
	if (port_op.r.redirect.param) {
	  port_op.r.redirect.param->error("Operation `any port.%s' cannot "
	    "have parameter redirect", stmt_name);
	}
      }
      if (!signature_determined)
	signature = get_outgoing_type(port_op.r.rcvpar);
      if (!signature) signature = Type::get_pooltype(Type::T_ERROR);
      // checking the parameter (template instance)
      port_op.r.rcvpar->chk(signature);
      // checking whether the argument is a signature template
      // checking the parameter redirect if present
      // and determining the return type of the signature
      signature = signature->get_type_refd_last();
      Type *return_type = 0;
      switch (signature->get_typetype()) {
      case Type::T_SIGNATURE:
	if (signature->is_nonblocking_signature())
	  error("Operation `%s' is not applicable to non-blocking signature "
	    "`%s'", stmt_name, signature->get_typename().c_str());
	else return_type = signature->get_signature_return_type();
	if (port_op.r.redirect.param)
	  port_op.r.redirect.param->chk(signature, true);
	if (!return_type) {
	  if (port_op.r.getreply_valuematch) {
	    port_op.r.getreply_valuematch->error("Value match cannot be used "
	      "because signature `%s' does not have return type",
	      signature->get_typename().c_str());
	  }
	  if (port_op.r.redirect.value) {
	    port_op.r.redirect.value->error("Value redirect cannot be used "
	      "because signature `%s' does not have return type",
	      signature->get_typename().c_str());
	  }
	}
	break;
      case Type::T_ERROR:
	if (port_op.r.redirect.param)
	  port_op.r.redirect.param->chk_erroneous();
	break;
      default:
	port_op.r.rcvpar->error("The type of parameter is `%s', which is not "
	  "a signature", signature->get_typename().c_str());
	if (port_op.r.redirect.param)
	  port_op.r.redirect.param->chk_erroneous();
      }
      // checking the value match if present
      if (port_op.r.getreply_valuematch) {
	Error_Context cntxt2(port_op.s.replyval, "In value match");
	if (!return_type) return_type = Type::get_pooltype(Type::T_ERROR);
	port_op.r.getreply_valuematch->chk(return_type);
      }
      // checking the value redirect if present
      chk_value_redirect(port_op.r.redirect.value, return_type);
    } else {
      // the statement does not have parameter (value match is also omitted)
      if (port_type) {
        PortTypeBody *port_type_body = port_type->get_PortBody();
	if (!port_type_body->getreply_allowed()) {
          // the port type is known and it does not have outgoing signatures
	  if (port_type_body->get_operation_mode() ==
	      PortTypeBody::PO_MESSAGE) {
	    port_op.portref->error("Procedure-based operation `%s' is not "
	      "applicable to a message-based port of type `%s'", stmt_name,
	      port_type->get_typename().c_str());
	  } else {
	    port_op.portref->error("Port type `%s' does not have any outgoing "
	      "signatures that support reply",
	      port_type->get_typename().c_str());
	  }
	}
      }
      if (port_op.r.redirect.value) {
	port_op.r.redirect.value->error("Value redirect cannot be used "
	  "without signature template");
	chk_value_redirect(port_op.r.redirect.value, 0);
      }
      if (port_op.r.redirect.param) {
	port_op.r.redirect.param->error("Parameter redirect cannot be used "
	  "without signature template");
	port_op.r.redirect.param->chk_erroneous();
      }
    }
    // checking from clause and sender redirect
    chk_from_clause(port_type);
  }

  void Statement::chk_catch()
  {
    // determining statement type
    const char *stmt_name = get_stmt_name();
    Error_Context cntxt(this, "In %s statement", stmt_name);
    // checking the port reference
    Type *port_type = chk_port_ref(port_op.portref);
    // checking the signature reference, parameter and/or value redirect
    if (port_op.r.ctch.signature_ref) {
      // the signature reference is present
      port_op.r.ctch.signature =
	chk_signature_ref(port_op.r.ctch.signature_ref);
      // checking whether the signature is present on the incoming list
      // of the respective port type
      if (port_type) {
        PortTypeBody *port_type_body = port_type->get_PortBody();
	if (port_type_body->catch_allowed()) {
	  TypeSet *out_sigs = port_type_body->get_out_sigs();
	  if (port_op.r.ctch.signature) {
	    if (!out_sigs->has_type(port_op.r.ctch.signature)) {
	      port_op.r.ctch.signature_ref->error("Signature `%s' is not "
		"present on the outgoing list of port type `%s'",
		port_op.r.ctch.signature->get_typename().c_str(),
		port_type->get_typename().c_str());
	    }
	  } else if (out_sigs->get_nof_types() == 1) {
	    // if the signature is unknown and the port type has exactly one
	    // outgoing signature then use that for further checking
	    port_op.r.ctch.signature =
	      out_sigs->get_type_byIndex(0)->get_type_refd_last();
	  }
	} else if (port_type_body->get_operation_mode() ==
		   PortTypeBody::PO_MESSAGE) {
	  port_op.portref->error("Procedure-based operation `%s' is not "
	    "applicable to a message-based port of type `%s'", stmt_name,
	    port_type->get_typename().c_str());
	} else {
	  port_op.portref->error("Port type `%s' does not have any outgoing "
	    "signatures that support exceptions",
	    port_type->get_typename().c_str());
	}
      } else if (!port_op.portref) {
	// the statement refers to 'any port'
	port_op.r.rcvpar->error("Operation `any port.%s' cannot have parameter",
	  stmt_name);
	if (port_op.r.redirect.value) {
	  port_op.r.redirect.value->error("Operation `any port.%s' cannot have "
	    "value redirect", stmt_name);
	}
      }
      // the receive parameter (template instance) must be also present
      // trying to determine type of the exception
      Type *exc_type = 0;
      bool exc_type_determined = false, value_redirect_checked = false;
      if (port_op.r.ctch.signature) {
	// the signature is known
	SignatureExceptions *exceptions =
	  port_op.r.ctch.signature->get_signature_exceptions();
	if (exceptions) {
	  if (exceptions->get_nof_types() == 1) {
	    // the signature has exactly one exception type
	    // use that for checking
	    exc_type = exceptions->get_type_byIndex(0);
	  } else {
	    // the signature has more than one exception types
	    exc_type = get_incoming_type(port_op.r.rcvpar,
	      port_op.r.redirect.value, value_redirect_checked);
	    if (exc_type) {
	      size_t nof_comp_types =
		exceptions->get_nof_compatible_types(exc_type);
	      if (nof_comp_types == 0) {
		port_op.r.rcvpar->error("Type `%s' is not present on the "
		  "exception list of signature `%s'",
		  exc_type->get_typename().c_str(),
		  port_op.r.ctch.signature->get_typename().c_str());
	      } else if (nof_comp_types > 1) {
		port_op.r.rcvpar->error("Type of the exception is ambiguous: "
		  "`%s' is compatible with more than one exception types of "
		  "signature `%s'", exc_type->get_typename().c_str(),
		  port_op.r.ctch.signature->get_typename().c_str());
	      }
	    } else {
	      port_op.r.rcvpar->error("Cannot determine the type of the "
		"exception");
	    }
	  }
	  exc_type_determined = true;
	} else {
	  port_op.r.ctch.signature_ref->error("Signature `%s' does not have "
	    "exceptions", port_op.r.ctch.signature->get_typename().c_str());
	}
      }
      if (!exc_type_determined) {
	exc_type = get_incoming_type(port_op.r.rcvpar, port_op.r.redirect.value,
	  value_redirect_checked);
      }
      if (!exc_type) exc_type = Type::get_pooltype(Type::T_ERROR);
      // check the template instance using the exception type
      port_op.r.rcvpar->chk(exc_type);
      // check the value redirect if it is not done so far
      if (!value_redirect_checked)
	chk_value_redirect(port_op.r.redirect.value, exc_type);
      // checking for invalid exception types
      exc_type = exc_type->get_type_refd_last();
      switch (exc_type->get_typetype()) {
      case Type::T_SIGNATURE:
	port_op.r.rcvpar->error("The type of catch parameter is signature "
	  "`%s', which cannot be an exception type",
	  exc_type->get_typename().c_str());
	break;
      case Type::T_PORT:
	port_op.r.rcvpar->error("The type of catch parameter is port type "
	  "`%s', which cannot be an exception type",
	  exc_type->get_typename().c_str());
	break;
      case Type::T_DEFAULT:
	port_op.r.rcvpar->error("The type of catch parameter is the `default' "
	  "type, which cannot be an exception type");
      default:
	break;
      }
    } else {
      // the statement does not have signature reference
      if (port_op.r.ctch.timeout) {
	// the parameter is timeout
	if (port_op.portref) {
	  // the port reference is present
	  if (port_type) {
            PortTypeBody *port_type_body = port_type->get_PortBody();
	    if (!port_type_body->getreply_allowed()) {
              // the port type is known and it does not allow blocking calls
	      if (port_type_body->get_operation_mode() ==
		  PortTypeBody::PO_MESSAGE) {
		port_op.portref->error("Timeout exception cannot be caught on "
		  "a message-based port of type `%s'",
		  port_type->get_typename().c_str());
	      } else {
		port_op.portref->error("Timeout exception cannot be caught on "
		  "a port of type `%s', which does not have any outgoing "
		  "signatures that allow blocking calls",
		  port_type->get_typename().c_str());
	      }
	    }
	  }
	} else error("Timeout exception cannot be caught on `any port'");
	if (!port_op.r.ctch.in_call)
	  error("Catching of `timeout' exception is not allowed in this "
	    "context. It is permitted only in the response and exception "
	    "handling part of `call' operations");
	else if (!port_op.r.ctch.call_has_timer)
	  error("Catching of `timeout' exception is not allowed because the "
	    "previous `call' operation does not have timer");
	if (port_op.r.fromclause) port_op.r.fromclause->error(
	  "Operation `catch(timeout)' cannot have from clause");
	if (port_op.r.redirect.sender) port_op.r.redirect.sender->error(
	  "Operation `catch(timeout)' cannot have sender redirect");
      } else {
	// the operation does not have any parameter
	if (port_type) {
          PortTypeBody *port_type_body = port_type->get_PortBody();
	  if (!port_type_body->catch_allowed()) {
            // the port type is known and it does not have outgoing signatures
	    if (port_type_body->get_operation_mode() ==
		PortTypeBody::PO_MESSAGE) {
	      port_op.portref->error("Procedure-based operation `%s' is not "
		"applicable to a message-based port of type `%s'", stmt_name,
		port_type->get_typename().c_str());
	    } else {
	      port_op.portref->error("Port type `%s' does not have any "
		"outgoing signatures that support exceptions",
		port_type->get_typename().c_str());
	    }
	  }
	}
      }
      if (port_op.r.redirect.value) {
	// the statement does not have any parameter,
	// but the value redirect is present
	port_op.r.redirect.value->error("Value redirect cannot be used without "
	  "signature and parameter");
	chk_value_redirect(port_op.r.redirect.value, 0);
      }
    }
    // checking from clause and sender redirect
    chk_from_clause(port_type);
  }

  void Statement::chk_check()
  {
    Error_Context cntxt(this, "In check statement");
    Type *port_type = chk_port_ref(port_op.portref);
    if (port_type && !port_type->get_PortBody()->has_queue()) {
      // the port type is known and it does not have incoming queue
      port_op.portref->error("Port type `%s' does not have incoming queue "
	"because it has neither incoming messages nor incoming or outgoing "
	"signatures", port_type->get_typename().c_str());
    }
    // checking from clause and sender redirect
    chk_from_clause(port_type);
  }

  void Statement::chk_clear()
  {
    Error_Context cntxt(this, "In clear port statement");
    Type *port_type = chk_port_ref(port_op.portref);
    if (port_type && !port_type->get_PortBody()->has_queue()) {
      // the port type is known and it does not have incoming queue
      port_op.portref->warning("Port type `%s' does not have incoming queue "
	"because it has neither incoming messages nor incoming or outgoing "
	"signatures", port_type->get_typename().c_str());
    }
  }

  void Statement::chk_start_stop_port()
  {
    Error_Context cntxt(this, "In %s statement", get_stmt_name());
    chk_port_ref(port_op.portref);
  }

  void Statement::chk_start_comp()
  {
    Error_Context cntxt(this, "In start test component statement");
    Type *comp_type = chk_comp_ref(comp_op.compref, false, false);
    Common::Assignment *t_ass = comp_op.funcinstref->get_refd_assignment();
    if (!t_ass) return;
    // checking whether the referred definition is a function
    Common::Assignment::asstype_t asstype = t_ass->get_asstype();
    switch (asstype) {
    case Common::Assignment::A_FUNCTION:
    case Common::Assignment::A_FUNCTION_RVAL:
    case Common::Assignment::A_FUNCTION_RTEMP:
      break;
    default:
      comp_op.funcinstref->error("Reference to a function was expected in the "
	"argument instead of %s", t_ass->get_description().c_str());
      return;
    }
    Def_Function *t_func = dynamic_cast<Def_Function*>(t_ass);
    if (!t_func) FATAL_ERROR("Statement::chk_start_comp()");
    // checking startability
    if (!t_func->chk_startable()) return;
    // checking the 'runs on' clause against the type of component reference
    Type *runs_on_type = t_func->get_RunsOnType();
    if (!comp_type || !runs_on_type) return;
    if (!runs_on_type->is_compatible(comp_type, NULL))
      comp_op.compref->error("Component type mismatch: The component reference "
        "is of type `%s', but %s runs on `%s'",
	comp_type->get_typename().c_str(), t_func->get_description().c_str(),
	runs_on_type->get_typename().c_str());
    // checking the return type
    switch (asstype) {
    case Common::Assignment::A_FUNCTION_RTEMP:
      comp_op.funcinstref->warning("Function `%s' returns a template of type "
	"`%s', which cannot be retrieved when the test component terminates",
	t_func->get_fullname().c_str(),
	t_func->get_Type()->get_typename().c_str());
      break;
    case Common::Assignment::A_FUNCTION_RVAL: {
      bool return_type_correct = false;
      Type *return_type = t_func->get_Type();
      for (Type *t = return_type; ; t = t->get_type_refd()) {
	if (t->has_done_attribute()) {
	  return_type_correct = true;
	  break;
	} else if (!t->is_ref()) break;
      }
      if (!return_type_correct)
        comp_op.funcinstref->warning("The return type of %s is `%s', which "
	  "does not have the `done' extension attribute. When the test "
	  "component terminates the returned value cannot be retrieved with "
	  "a `done' operation", t_func->get_description().c_str(),
	  return_type->get_typename().c_str());
      }
    default:
      break;
    }
  }

  void Statement::chk_start_comp_refd()
  {
    Error_Context cntxt(this, "In start test component statement");
    Type *comp_type = chk_comp_ref(comp_op.compref, false, false);
    switch(comp_op.derefered.value->get_valuetype()){
    case Value::V_REFER:
      comp_op.derefered.value->error("A value of a function type was expected "
        "in the argument instead of a `refers' statement,"
        " which does not specify any function type");
      return;
    case Value::V_TTCN3_NULL:
      comp_op.derefered.value->error("A value of a function type was expected "
        "in the argument instead of a `null' value,"
        " which does not specify any function type");
      return;
    default:
      break;
    }
    Type *f_type = comp_op.derefered.value->get_expr_governor_last();
    if (!f_type) return;
    switch (f_type->get_typetype()) {
    case Type::T_ERROR:
      return;
    case Type::T_FUNCTION:
      break;
    default:
      comp_op.derefered.value->error("A value of type function was expected "
	"in the argument instead of `%s'", f_type->get_typename().c_str());
      return;
    }
    if (f_type->get_fat_runs_on_self()) {
      fau_refd.value->error("The argument cannot be a function reference with "
        "'runs on self' clause");
      return;
    }
    if(!f_type->chk_startability()) return;
    Type *runs_on_type = f_type->get_fat_runs_on_type();
    if (!comp_type || !runs_on_type) return;
    if (!runs_on_type->is_compatible(comp_type, NULL))
      comp_op.compref->error("Component type mismatch: The component reference "
        "is of type `%s', but functions of type `%s' run on `%s'",
        comp_type->get_typename().c_str(), f_type->get_typename().c_str(),
        runs_on_type->get_typename().c_str());
    Type *return_type = f_type->get_function_return_type();
    if (return_type) {
      if (f_type->get_returns_template()) {
	comp_op.derefered.value->warning("Functions of type `%s' return a "
	  "template of type `%s', which cannot be retrieved when the test "
	  "component terminates", f_type->get_typename().c_str(),
	  return_type->get_typename().c_str());
      } else {
	bool return_type_correct = false;
	for (Type *t = return_type; ; t = t->get_type_refd()) {
          if (t->has_done_attribute()) {
            return_type_correct = true;
            break;
          } else if (!t->is_ref()) break;
	}
	if (!return_type_correct)
	  comp_op.derefered.value->warning("The return type of function type "
	    "`%s' is `%s', which does not have the `done' extension attribute. "
	    "When the test component terminates the returned value cannot be "
	    "retrieved with a `done' operation", f_type->get_typename().c_str(),
	    return_type->get_typename().c_str());
      }
    }
    ActualParList *parlist = new ActualParList;
    Ttcn::FormalParList *fp_list = f_type->get_fat_parameters();
    if(fp_list->fold_named_and_chk(comp_op.derefered.t_list1, parlist)) {
      delete parlist;
      delete comp_op.derefered.t_list1;
      comp_op.derefered.ap_list2 = 0;
    } else {
      delete comp_op.derefered.t_list1;
      parlist->set_fullname(get_fullname());
      parlist->set_my_scope(my_sb);
      comp_op.derefered.ap_list2 = parlist;
    }
  }

  void Statement::chk_stop_kill_comp()
  {
    Error_Context cntxt(this, "In %s statement", get_stmt_name());
    chk_comp_ref(comp_op.compref, true, false);
  }

  void Statement::chk_done()
  {
    Error_Context cntxt(this, "In done statement");
    chk_comp_ref(comp_op.compref, false, false);
    if (!comp_op.compref) return;
    // value returning done can be used only when the statement contains a
    // specific component reference
    if (comp_op.donereturn.donematch) {
      bool value_redirect_checked = false;
      // try to determine the type of return value
      Type *return_type = get_incoming_type(comp_op.donereturn.donematch,
	comp_op.donereturn.redirect, value_redirect_checked);
      if (return_type) {
	bool return_type_correct = false;
	for (Type *t = return_type; ; t = t->get_type_refd()) {
	  if (t->has_done_attribute()) {
	    return_type_correct = true;
	    break;
	  } else if (!t->is_ref()) break;
	}
	if (!return_type_correct) {
		error("Return type `%s' does not have `done' extension attribute",
		return_type->get_typename().c_str());
		return_type = Type::get_pooltype(Type::T_ERROR);
	}
      } else {
	comp_op.donereturn.donematch->error("Cannot determine the return type "
	  "for value returning done");
	return_type = Type::get_pooltype(Type::T_ERROR);
      }
      comp_op.donereturn.donematch->chk(return_type);
      if (!value_redirect_checked)
	chk_value_redirect(comp_op.donereturn.redirect, return_type);
    } else if (comp_op.donereturn.redirect) {
      comp_op.donereturn.redirect->error("Redirect cannot be used for the "
	"return value without a matching template");
      chk_value_redirect(comp_op.donereturn.redirect, 0);
    }
  }

  void Statement::chk_killed()
  {
    Error_Context cntxt(this, "In killed statement");
    chk_comp_ref(comp_op.compref, false, false);
  }

  void Statement::chk_connect()
  {
    Error_Context cntxt(this, "In %s statement", get_stmt_name());
    // checking endpoints
    Type *pt1, *pt2;
    PortTypeBody *ptb1, *ptb2;
    {
      Error_Context cntxt2(config_op.compref1, "In first endpoint");
      pt1 = chk_conn_endpoint(config_op.compref1, config_op.portref1, false);
      ptb1 = pt1 ? pt1->get_PortBody() : 0;
    }
    {
      Error_Context cntxt2(config_op.compref2, "In second endpoint");
      pt2 = chk_conn_endpoint(config_op.compref2, config_op.portref2, false);
      ptb2 = pt2 ? pt2->get_PortBody() : 0;
    }
    // checking consistency
    if (!ptb1 || !ptb2) return;
    if (!ptb1->is_connectable(ptb2) ||
	(ptb1 != ptb2 && !ptb2->is_connectable(ptb1))) {
      error("The connection between port types `%s' and `%s' is not consistent",
        pt1->get_typename().c_str(), pt2->get_typename().c_str());
      ptb1->report_connection_errors(ptb2);
      if (ptb1 != ptb2) ptb2->report_connection_errors(ptb1);
    }
  }

  void Statement::chk_map()
  {
    Error_Context cntxt(this, "In %s statement", get_stmt_name());
    // checking endpoints
    Type *pt1, *pt2;
    PortTypeBody *ptb1, *ptb2;
    bool cref1_is_tc = false, cref1_is_system = false;
    bool cref2_is_tc = false, cref2_is_system = false;
    {
      Error_Context cntxt2(config_op.compref1, "In first endpoint");
      pt1 = chk_conn_endpoint(config_op.compref1, config_op.portref1, true);
      if (pt1) {
        ptb1 = pt1->get_PortBody();
	if (ptb1->is_internal()) {
	  config_op.portref1->warning("Port type `%s' was marked as `internal'",
            pt1->get_typename().c_str());
	}
      } else ptb1 = 0;
      Value *cref1 = config_op.compref1->get_value_refd_last();
      if (cref1->get_valuetype() == Value::V_EXPR) {
	switch (cref1->get_optype()) {
	case Value::OPTYPE_COMP_MTC:
	case Value::OPTYPE_COMP_SELF:
	case Value::OPTYPE_COMP_CREATE:
	  cref1_is_tc = true;
	  break;
	case Value::OPTYPE_COMP_SYSTEM:
          cref1_is_system = true;
	default:
          break;
	}
      }
    }
    {
      Error_Context cntxt2(config_op.compref2, "In second endpoint");
      pt2 = chk_conn_endpoint(config_op.compref2, config_op.portref2, true);
      if (pt2) {
        ptb2 = pt2->get_PortBody();
        if (ptb2->is_internal()) {
	  config_op.portref2->warning("Port type `%s' was marked as `internal'",
            pt2->get_typename().c_str());
	}
      } else ptb2 = 0;
      Value *cref2 = config_op.compref2->get_value_refd_last();
      if (cref2->get_valuetype() == Value::V_EXPR) {
	switch (cref2->get_optype()) {
	case Value::OPTYPE_COMP_MTC:
	case Value::OPTYPE_COMP_SELF:
	case Value::OPTYPE_COMP_CREATE:
	  cref2_is_tc = true;
	  break;
	case Value::OPTYPE_COMP_SYSTEM:
          cref2_is_system = true;
	default:
          break;
	}
      }
    }
    if (cref1_is_tc && cref2_is_tc) {
      error("Both endpoints of the mapping are test component ports");
      return;
    }
    if (cref1_is_system && cref2_is_system) {
      error("Both endpoints of the mapping are system ports");
      return;
    }
    // checking consistency
    if (!ptb1 || !ptb2) return;
    if (cref1_is_tc || cref2_is_system) {
      if (!ptb1->is_mappable(ptb2)) {
	error("The mapping between test component port type `%s' and system "
	  "port type `%s' is not consistent", pt1->get_typename().c_str(),
	  pt2->get_typename().c_str());
	ptb1->report_mapping_errors(ptb2);
      }
    } else if (cref2_is_tc || cref1_is_system) {
      if (!ptb2->is_mappable(ptb1)) {
	error("The mapping between system port type `%s' and test component "
	  "port type `%s' is not consistent", pt1->get_typename().c_str(),
	  pt2->get_typename().c_str());
	ptb2->report_mapping_errors(ptb1);
      }
    } else {
      // we have no idea which one is the system port
      if (!ptb1->is_mappable(ptb1) && !ptb2->is_mappable(ptb1)) {
	error("The mapping between port types `%s' and `%s' is not consistent",
          pt1->get_typename().c_str(), pt2->get_typename().c_str());
      }
    }
  }

  void Statement::chk_start_timer()
  {
    Error_Context cntxt(this, "In start timer statement");
    chk_timer_ref(timer_op.timerref);
    if (timer_op.value) {
      // check the actual duration
      timer_op.value->chk_expr_float(Type::EXPECTED_DYNAMIC_VALUE);
      Value *t_val = timer_op.value->get_value_refd_last();
      if (t_val->get_valuetype() == Value::V_REAL) {
        ttcn3float v_real = t_val->get_val_Real();
        if (v_real < 0.0) {
          timer_op.value->error("The timer duration is negative: `%s'",
                                Real2string(v_real).c_str());
        } else if (isSpecialFloatValue(v_real)) {
          timer_op.value->error("The timer duration cannot be %s",
                                Real2string(v_real).c_str());
        }
      }
    } else {
      // check whether the timer has default duration
      Common::Assignment *t_ass = timer_op.timerref->get_refd_assignment();
      if (t_ass && t_ass->get_asstype() == Common::Assignment::A_TIMER) {
        Def_Timer *t_def_timer = dynamic_cast<Def_Timer*>(t_ass);
	if (!t_def_timer) FATAL_ERROR("Statement::chk_start_timer()");
	if (!t_def_timer->has_default_duration(
	    timer_op.timerref->get_subrefs()))
	  error("Missing duration: %s does not have default duration",
	    t_ass->get_description().c_str());
      }
    }
  }

  void Statement::chk_stop_timer_timeout()
  {
    Error_Context cntxt(this, "In %s statement", get_stmt_name());
    chk_timer_ref(timer_op.timerref);
  }

  void Statement::chk_setverdict()
  {
    Error_Context cntxt(this, "In setverdict statement");
    if(!my_sb->get_my_def())
      error("Setverdict statement is not allowed in the control part");
    setverdict.verdictval->chk_expr_verdict(Type::EXPECTED_DYNAMIC_VALUE);
    Value *t_val = setverdict.verdictval->get_value_refd_last();
    if (t_val->get_valuetype() == Value::V_VERDICT &&
        t_val->get_val_verdict() == Value::Verdict_ERROR) {
      setverdict.verdictval->error("Error verdict cannot be set by the setverdict "
	"operation");
    }
  }

  void Statement::chk_execute()
  {
    Error_Context cntxt(this, "In execute statement");
    Ref_pard *ref=testcase_inst.tcref;
    Common::Assignment *t_ass=ref->get_refd_assignment();
    if(!t_ass) goto error;
    if(t_ass->get_asstype()!=Common::Assignment::A_TESTCASE) {
      ref->error("Reference to a testcase was expected in the argument"
                 " instead of %s", t_ass->get_description().c_str());
      goto error;
    }
    if(my_sb->get_scope_runs_on()) {
      ref->error("A definition that has `runs on' clause cannot "
                 "execute testcases");
      goto error;
    }
    if (testcase_inst.timerval) {
      testcase_inst.timerval->chk_expr_float(Type::EXPECTED_DYNAMIC_VALUE);
      Value *t_val = testcase_inst.timerval->get_value_refd_last();
      if (t_val->get_valuetype() == Value::V_REAL) {
        ttcn3float v_real = t_val->get_val_Real();
        if (v_real < 0.0) {
          testcase_inst.timerval->error("The testcase guard "
            "timer has negative duration: `%s'", Real2string(v_real).c_str());
        } else if (isSpecialFloatValue(v_real)) {
          testcase_inst.timerval->error("The testcase guard "
            "timer duration cannot be %s", Real2string(v_real).c_str());
        }
      }
    }
    return;
  error:
    clean_up();
    statementtype=S_ERROR;
  }

  void Statement::chk_execute_refd()
  {
    Error_Context cntxt(this, "In execute statement");
    switch(execute_refd.value->get_valuetype()){
    case Value::V_REFER:
      execute_refd.value->error("A value of a testcase type was expected "
        "in the argument instead of a `refers' statement,"
        " which does not specify any function type");
      return;
    case Value::V_TTCN3_NULL:
      execute_refd.value->error("A value of a testcase type was expected "
        "in the argument instead of a `null' value,"
        " which does not specify any function type");
      return;
    default:
      break;
    }
    Type *t = execute_refd.value->get_expr_governor_last();
    if (!t) goto error;
    switch (t->get_typetype()) {
    case Type::T_ERROR:
      goto error;
    case Type::T_TESTCASE:
      break;
    default:
      execute_refd.value->error("A value of type testcase was expected in the "
        "argument of `derefers()' instead of `%s'", t->get_typename().c_str());
      goto error;
    }
    if (my_sb->get_scope_runs_on()) {
      execute_refd.value->error("A definition that has `runs on' clause cannot "
        "execute testcases");
      goto error;
    } else {
      ActualParList *parlist = new ActualParList;
      Ttcn::FormalParList *fp_list = t->get_fat_parameters();
      bool is_erroneous = fp_list->chk_actual_parlist(execute_refd.t_list1,
	parlist);
      delete execute_refd.t_list1;
      if(is_erroneous) {
	delete parlist;
	execute_refd.ap_list2 = 0;
	goto error;
      }else {
	parlist->set_fullname(get_fullname());
	parlist->set_my_scope(my_sb);
	execute_refd.ap_list2 = parlist;
      }
      if(execute_refd.timerval) {
	execute_refd.timerval->chk_expr_float(Type::EXPECTED_DYNAMIC_VALUE);
	Value *t_val = execute_refd.timerval->get_value_refd_last();
	if(t_val->get_valuetype() == Value::V_REAL) {
	  ttcn3float v_real = t_val->get_val_Real();
          if(v_real < 0.0) {
            execute_refd.value->error("The testcase guard "
              "timer has negative duration: `%s'", Real2string(v_real).c_str());
          } else if (isSpecialFloatValue(v_real)) {
            execute_refd.value->error("The testcase guard "
              "timer duration cannot be %s", Real2string(v_real).c_str());
          }
	}
      }
    }
    return;
error:
    clean_up();
    statementtype=S_ERROR;
  }

  Type *Statement::chk_port_ref(Reference *p_ref)
  {
    if (!my_sb->get_my_def())
      error("Port operation is not allowed in the control part");
    if (!p_ref) return 0;
    Common::Assignment *t_ass = p_ref->get_refd_assignment();
    if (!t_ass) return 0;
    switch (t_ass->get_asstype()) {
    case Common::Assignment::A_PORT: {
      ArrayDimensions *t_dims = t_ass->get_Dimensions();
      if (t_dims) t_dims->chk_indices(p_ref, "port", false,
	Type::EXPECTED_DYNAMIC_VALUE);
      else if (p_ref->get_subrefs()) p_ref->error("Reference to single %s "
	"cannot have field or array sub-references",
	t_ass->get_description().c_str());
      break; }
    case Common::Assignment::A_PAR_PORT:
      if (p_ref->get_subrefs()) p_ref->error("Reference to %s cannot have "
        "field or array sub-references", t_ass->get_description().c_str());
      break;
    default:
      p_ref->error("Reference to a port or port parameter was expected "
        "instead of %s", t_ass->get_description().c_str());
      return 0;
    }
    Type *ret_val = t_ass->get_Type();
    if (!ret_val) return 0;
    ret_val = ret_val->get_type_refd_last();
    if (ret_val->get_typetype() == Type::T_PORT) return ret_val;
    else return 0;
  }

  void Statement::chk_to_clause(Type *port_type)
  {
    if (!port_op.s.toclause) return;
    // pointer to the address type
    Type *address_type;
    if (port_type) {
      // the port type is known
      address_type = port_type->get_PortBody()->get_address_type();
    } else {
      // the port type is unknown
      // address is permitted if it is visible from the current module
      address_type = my_sb->get_scope_mod()->get_address_type();
    }
    Error_Context cntxt(port_op.s.toclause, "In `to' clause");
    if (address_type) {
      // detect possible enumerated values (address may be an enumerated type)
      address_type->chk_this_value_ref(port_op.s.toclause);
      // try to figure out whether the argument is a component reference or
      // an SUT address
      bool is_address;
      Type *t_governor =
	port_op.s.toclause->get_expr_governor(Type::EXPECTED_DYNAMIC_VALUE);
      if (t_governor) is_address = address_type->is_compatible(t_governor, NULL);
      else is_address =
        port_op.s.toclause->get_expr_returntype(Type::EXPECTED_DYNAMIC_VALUE)
	  != Type::T_COMPONENT;
      if (is_address) {
	// the argument is an address value
	port_op.s.toclause->set_my_governor(address_type);
	address_type->chk_this_value(port_op.s.toclause, 0,
	  Type::EXPECTED_DYNAMIC_VALUE, INCOMPLETE_NOT_ALLOWED, OMIT_NOT_ALLOWED,
	  SUB_CHK);
      } else {
	// the argument is not an address value, treat as a component reference
	chk_comp_ref(port_op.s.toclause, true, true);
      }
    } else {
      // usage of address is not allowed
      chk_comp_ref(port_op.s.toclause, true, true);
    }
  }

  void Statement::chk_from_clause(Type *port_type)
  {
    if (!port_op.r.fromclause && !port_op.r.redirect.sender) return;
    // pointer to the address type
    Type *address_type;
    if (port_type) {
      // the port type is known
      address_type = port_type->get_PortBody()->get_address_type();
    } else if (port_op.portref) {
      // the operation refers to a specific port, but its type is unknown
      // address is permitted if it is visible from the current module
      address_type = my_sb->get_scope_mod()->get_address_type();
    } else {
      // the operation refers to 'any port'
      // address is not allowed
      address_type = 0;
    }
    bool sender_redirect_checked = false;
    Type *from_clause_type = 0;
    if (port_op.r.fromclause) {
      // the from clause is present
      Error_Context cntxt(port_op.r.fromclause, "In `from' clause");
      from_clause_type =
	port_op.r.fromclause->get_expr_governor(Type::EXPECTED_TEMPLATE);
      Template *templ_body = port_op.r.fromclause->get_Template();
      if (!from_clause_type) {
	// try to detect possible enumerated values
	if (address_type) address_type->chk_this_template_ref(templ_body);
	else templ_body->set_lowerid_to_ref();
	from_clause_type =
	  templ_body->get_expr_governor(Type::EXPECTED_TEMPLATE);
      }
      if (!from_clause_type) {
        // trying to determine the type of template in from clause
	// based on the sender redirect
	from_clause_type = chk_sender_redirect(address_type);
	sender_redirect_checked = true;
      }
      if (!from_clause_type) {
	// trying to figure out whether the template is a component reference
	// or an SUT address
        bool is_compref;
	if (templ_body->get_expr_returntype(Type::EXPECTED_TEMPLATE)
	    == Type::T_COMPONENT) is_compref = true;
	else {
	  switch (templ_body->get_templatetype()) {
	  case Template::SPECIFIC_VALUE:
	    // treat 'null' as component reference
	    if (templ_body->get_specific_value()->get_valuetype() ==
		Value::V_TTCN3_NULL) is_compref = true;
	    else is_compref = false;
	    break;
	  case Template::ANY_VALUE:
	  case Template::ANY_OR_OMIT:
	    // treat generic wildcards ? and * as component references
	    is_compref = true;
	    break;
	  default:
	    is_compref = false;
	  }
	}
	if (is_compref) {
	  // the argument is a component reference: get a pool type
	  from_clause_type = Type::get_pooltype(Type::T_COMPONENT);
	} else if (address_type) {
	  // the argument is not a component reference: try the address type
	  from_clause_type = address_type;
	}
      }
      if (from_clause_type) {
	// the type of from clause is known
	port_op.r.fromclause->chk(from_clause_type);
	if (!address_type
	    || !address_type->is_compatible(from_clause_type, NULL)) {
	  // from_clause_type must be a component type
	  switch (from_clause_type->get_type_refd_last()->get_typetype()) {
	  case Type::T_ERROR:
	    // to avoid further errors in sender redirect
	    from_clause_type = 0;
	  case Type::T_COMPONENT:
	    if (templ_body->get_templatetype() == Template::SPECIFIC_VALUE)
	      chk_comp_ref(templ_body->get_specific_value(), true, true);
	    break;
	  default:
	    port_op.r.fromclause->error("The type of the template should be a "
	      "component type %sinstead of `%s'",
	      address_type ? "or the `address' type " : "",
	      from_clause_type->get_typename().c_str());
	    // to avoid further errors in sender redirect
	    from_clause_type = 0;
	  }
	}
      } else {
	// the type of from clause is unknown
	port_op.r.fromclause->error("Cannot determine the type of the "
	  "template");
      }
    }
    if (!sender_redirect_checked) {
      Type *sender_redirect_type = chk_sender_redirect(address_type);
      if (from_clause_type && sender_redirect_type &&
          !from_clause_type->is_identical(sender_redirect_type)) {
	error("The types in `from' clause and `sender' redirect are not the "
	  "same: `%s' was expected instead of `%s'",
	  from_clause_type->get_typename().c_str(),
	  sender_redirect_type->get_typename().c_str());
      }
    }
  }

  void Statement::chk_call_body(Type *port_type, Type *signature)
  {
    bool has_catch_timeout = false;
    // setting the flags whether 'catch(timeout)' statements are allowed
    for (size_t i = 0; i < port_op.s.call.body->get_nof_ags(); i++) {
      AltGuard *t_ag = port_op.s.call.body->get_ag_byIndex(i);
      if (t_ag->get_type() != AltGuard::AG_OP)
	FATAL_ERROR("Statement::chk_call_body()");
      Statement *t_stmt = t_ag->get_guard_stmt();
      if (t_stmt->statementtype == S_CATCH) {
	t_stmt->port_op.r.ctch.in_call = true;
	if (port_op.s.call.timer)
	  t_stmt->port_op.r.ctch.call_has_timer = true;
	if (t_stmt->port_op.r.ctch.timeout) has_catch_timeout = true;
      }
    }
    Error_Context cntxt(this, "In response and exception handling part");
    port_op.s.call.body->set_my_laic_stmt(port_op.s.call.body, 0);
    port_op.s.call.body->set_my_ags(port_op.s.call.body);
    port_op.s.call.body->chk();
    if (port_type) {
      // checking whether getreply/catch operations refer to the same port
      // and same signature as the call operation
      for (size_t i = 0; i < port_op.s.call.body->get_nof_ags(); i++) {
	AltGuard *t_ag = port_op.s.call.body->get_ag_byIndex(i);
	if (t_ag->get_type() != AltGuard::AG_OP)
	  FATAL_ERROR("Statement::chk_call_body()");
	Statement *t_stmt = t_ag->get_guard_stmt();
	if (t_stmt->statementtype == S_ERROR) continue;
	// checking port reference
	if (!t_stmt->port_op.portref) {
	  t_stmt->error("The `%s' operation must refer to the same port as "
	    "the previous `call' statement: `%s' was expected instead of "
	    "`any port'", t_stmt->get_stmt_name(),
	    port_op.portref->get_id()->get_dispname().c_str());
	} else if (*port_op.portref->get_id() !=
		   *t_stmt->port_op.portref->get_id()) {
	  t_stmt->port_op.portref->error("The `%s' operation refers to a "
	    "different port than the previous `call' statement: `%s' was "
	    "expected instead of `%s'", t_stmt->get_stmt_name(),
	    port_op.portref->get_id()->get_dispname().c_str(),
	    t_stmt->port_op.portref->get_id()->get_dispname().c_str());
	}
	// checking the signature
	switch (t_stmt->statementtype) {
	case S_GETREPLY:
	  if (t_stmt->port_op.r.rcvpar) {
	    Type *t_sig = t_stmt->port_op.r.rcvpar
	      ->get_expr_governor(Type::EXPECTED_DYNAMIC_VALUE);
	    if (!signature->is_compatible(t_sig, NULL))
	      t_stmt->port_op.r.rcvpar->error("The `getreply' operation refers "
		"to a different signature than the previous `call' statement: "
		"`%s' was expected instead of `%s'",
		signature->get_typename().c_str(),
		t_sig->get_typename().c_str());
	  }
	  break;
	case S_CATCH:
	  if (t_stmt->port_op.r.ctch.signature
	      && !signature->is_compatible(t_stmt->port_op.r.ctch.signature, NULL))
	    t_stmt->port_op.r.ctch.signature_ref->error("The `catch' "
	      "operation refers to a different signature than the previous "
	      "`call' statement: `%s' was expected instead of `%s'",
	      signature->get_typename().c_str(),
	      t_stmt->port_op.r.ctch.signature->get_typename().c_str());
	  break;
	default:
	  FATAL_ERROR("Statement::chk_call_body()");
	}
      }
    }
    if (port_op.s.call.timer && !has_catch_timeout)
      warning("The call operation has a timer, but the timeout exception is "
	"not caught");
  }

  Type *Statement::get_outgoing_type(TemplateInstance *p_ti)
  {
    // first analyze the template instance as is
    Type *ret_val = p_ti->get_expr_governor(Type::EXPECTED_TEMPLATE);
    // return if this step was successful
    if (ret_val) return ret_val;
    // try to convert the undef identifier in the template instance to
    // a reference because it cannot be an enum value anymore
    Template *t_templ = p_ti->get_Template();
    t_templ->set_lowerid_to_ref();
    return t_templ->get_expr_governor(Type::EXPECTED_TEMPLATE);
  }

  Type *Statement::get_incoming_type(TemplateInstance *p_ti,
    Reference *p_val_redir, bool& p_val_redir_checked)
  {
    // first analyze the template instance
    Type *ret_val = p_ti->get_expr_governor(Type::EXPECTED_TEMPLATE);
    // return if this step was successful
    if (ret_val) return ret_val;
    // use the variable in value redirect in the next step
    ret_val = chk_value_redirect(p_val_redir, 0);
    p_val_redir_checked = true;
    // return if this step was successful
    if (ret_val) return ret_val;
    // finally try to convert the undef identifier in the template instance to
    // a reference because it cannot be an enum value anymore
    Template *t_templ = p_ti->get_Template();
    t_templ->set_lowerid_to_ref();
    return t_templ->get_expr_governor(Type::EXPECTED_TEMPLATE);
  }

  Type *Statement::chk_value_redirect(Reference *p_ref, Type *p_type)
  {
    if (!p_ref) return NULL;
    Error_Context cntxt(p_ref, "In `value' redirect");
    Type *t_var_type = p_ref->chk_variable_ref();
    if (p_type && t_var_type) {
      TypeCompatInfo info(my_sb->get_scope_mod(), p_type, t_var_type, true, false);
      if (p_ref->get_subrefs()) info.set_str2_elem(p_ref->get_subrefs()->refers_to_string_element());
      TypeChain l_chain;
      TypeChain r_chain;
      if (!p_type->is_compatible(t_var_type, &info, &l_chain, &r_chain)) {
        if (info.is_subtype_error()) {
          p_ref->error("%s", info.get_subtype_error().c_str());
        } else
        if (!info.is_erroneous()) {
          p_ref->error("Type mismatch in value redirect: "
                       "A variable of type `%s' was expected instead of `%s'",
                       p_type->get_typename().c_str(),
                       t_var_type->get_typename().c_str());
        } else {
          p_ref->error("%s", info.get_error_str_str().c_str());
        }
      }
    }
    return t_var_type;
  }

  Type *Statement::chk_sender_redirect(Type *address_type)
  {
    if (!port_op.r.redirect.sender) return 0;
    Error_Context cntxt(port_op.r.redirect.sender, "In `sender' redirect");
    Type *t_var_type = port_op.r.redirect.sender->chk_variable_ref();
    if (!t_var_type) return 0;
    if (!address_type || !address_type->is_identical(t_var_type)) {
      // t_var_type must be a component type
      switch (t_var_type->get_type_refd_last()->get_typetype()) {
      case Type::T_COMPONENT:
	break;
      case Type::T_ERROR:
        return 0;
      default:
	port_op.r.redirect.sender->error("The type of the variable should be "
	  "a component type %sinstead of `%s'",
	  address_type ? "or the `address' type " : "",
	  t_var_type->get_typename().c_str());
	return 0;
      }
    }
    return t_var_type;
  }

  Type *Statement::chk_signature_ref(Reference *p_ref)
  {
    if (!p_ref) FATAL_ERROR("Statement::chk_signature_ref()");
    Error_Context(p_ref, "In signature");
    Common::Assignment *t_ass = p_ref->get_refd_assignment();
    if (!t_ass) return 0;
    if (t_ass->get_asstype() != Common::Assignment::A_TYPE) {
      p_ref->error("Reference to a signature was expected instead of %s",
	t_ass->get_description().c_str());
      return 0;
    }
    Type *ret_val = t_ass->get_Type();
    if (!ret_val) return 0;
    ret_val = ret_val->get_field_type(p_ref->get_subrefs(),
      Type::EXPECTED_DYNAMIC_VALUE);
    if (!ret_val) return 0;
    ret_val = ret_val->get_type_refd_last();
    switch (ret_val->get_typetype()) {
    case Type::T_SIGNATURE:
      break;
    case Type::T_ERROR:
      return 0;
    case Type::T_PORT:
      p_ref->error("Reference to a signature was expected instead of port type "
	"`%s'", ret_val->get_typename().c_str());
      return 0;
    default:
      p_ref->error("Reference to a signature was expected instead of data type "
	"`%s'", ret_val->get_typename().c_str());
      return 0;
    }
    return ret_val;
  }

  void Statement::chk_timer_ref(Reference *p_ref)
  {
    if (!p_ref) return;
    Common::Assignment *t_ass = p_ref->get_refd_assignment();
    if (!t_ass) return;
    switch (t_ass->get_asstype()) {
    case Common::Assignment::A_TIMER: {
      ArrayDimensions *t_dims = t_ass->get_Dimensions();
      if (t_dims) t_dims->chk_indices(p_ref, "timer", false,
	Type::EXPECTED_DYNAMIC_VALUE);
      else if (p_ref->get_subrefs()) p_ref->error("Reference to single %s "
	"cannot have field or array sub-references",
	t_ass->get_description().c_str());
      break; }
    case Common::Assignment::A_PAR_TIMER:
      if (p_ref->get_subrefs()) p_ref->error("Reference to %s cannot have "
	"field or array sub-references", t_ass->get_description().c_str());
      break;
    default:
      p_ref->error("Reference to a timer or timer parameter was expected "
        "instead of %s", t_ass->get_description().c_str());
    }
  }

  Type *Statement::chk_comp_ref(Value *p_val, bool allow_mtc, bool allow_system, bool is_connecting)
  {
    if (!my_sb->get_my_def())
      error("Component operation is not allowed in the control part");
    if (!p_val) return 0;
    Value *v = p_val->get_value_refd_last();
    switch (v->get_valuetype()) {
    case Value::V_ERROR:
      return 0;
    case Value::V_REFD:
      break;
    case Value::V_INVOKE:
      if(p_val->get_expr_returntype() != Type::T_COMPONENT)
        p_val->error("A component reference was expected as return value");
      break;
    case Value::V_TTCN3_NULL:
      p_val->error("The `null' component reference shall not be used in `%s' operation", get_stmt_name());
      break;
    case Value::V_EXPR:
      switch (v->get_optype()) {
      case Value::OPTYPE_COMP_NULL:
	p_val->error("The `null' component reference shall not be used in `%s' operation", get_stmt_name());
        break;
      case Value::OPTYPE_COMP_MTC:
	if (!allow_mtc)
	  p_val->error("The `mtc' component reference shall not be used in `%s' operation", get_stmt_name());
        break;
      case Value::OPTYPE_COMP_SYSTEM:
	if (!allow_system)
	  p_val->error("The `system' component reference shall not be used in `%s' operation", get_stmt_name());
        break;
      case Value::OPTYPE_COMP_SELF:
      case Value::OPTYPE_COMP_CREATE:
        break;
      default:
	p_val->error("A component reference was expected as operand");
	return 0;
      }
      break;
    default:
      p_val->error("A component reference was expected as operand");
      return 0;
    }
    Type *ret_val = is_connecting ? p_val->get_component_governor() :
      p_val->get_expr_governor(Type::EXPECTED_DYNAMIC_VALUE);
    if (!ret_val) return 0;
    ret_val = ret_val->get_type_refd_last();
    switch (ret_val->get_typetype()) {
    case Type::T_ERROR:
      return 0;
    case Type::T_COMPONENT:
      return ret_val;
    default:
      p_val->error("Type mismatch: The type of the operand should be a "
	"component type instead of `%s'", ret_val->get_typename().c_str());
      return 0;
    }
  }

  Type *Statement::chk_conn_endpoint(Value *p_compref, Reference *p_portref,
    bool allow_system)
  {
    Type *comp_type = chk_comp_ref(p_compref, true, allow_system, true);
    if (comp_type) {
      ComponentTypeBody *comp_body = comp_type->get_CompBody();
      p_portref->set_base_scope(comp_body);
      const Identifier& t_portid = *p_portref->get_id();
      if (!comp_body->has_local_ass_withId(t_portid)) {
        p_portref->error("Component type `%s' does not have port with name "
	  "`%s'", comp_type->get_typename().c_str(),
	  t_portid.get_dispname().c_str());
	return 0;
      }
      Common::Assignment *t_ass = comp_body->get_local_ass_byId(t_portid);
      if (t_ass->get_asstype() != Common::Assignment::A_PORT) {
	p_portref->error("Definition `%s' in component type `%s' is a %s and "
	  "not a port", t_portid.get_dispname().c_str(),
	  comp_type->get_typename().c_str(), t_ass->get_assname());
	return 0;
      }
      ArrayDimensions *t_dims = t_ass->get_Dimensions();
      if (t_dims) t_dims->chk_indices(p_portref, "port", false,
	Type::EXPECTED_DYNAMIC_VALUE);
      else if (p_portref->get_subrefs()) {
	p_portref->error("Port `%s' is not an array. The "
	  "reference cannot have array indices",
	  t_portid.get_dispname().c_str());
      }
      Type *port_type = t_ass->get_Type();
      if (port_type) {
	// check whether the external interface is provided by another port type
	PortTypeBody *port_body = port_type->get_PortBody();
	if (port_body->get_type() == PortTypeBody::PT_USER) {
	  Type *provider_type = port_body->get_provider_type();
	  if (provider_type) port_type = provider_type;
	}
      }
      return port_type;
    } else {
      // the component type cannot be determined
      FieldOrArrayRefs *t_subrefs = p_portref->get_subrefs();
      if (t_subrefs) {
        // check the array indices: they should be integers
	for (size_t i = 0; i < t_subrefs->get_nof_refs(); i++) {
	  t_subrefs->get_ref(i)->get_val()
	    ->chk_expr_int(Type::EXPECTED_DYNAMIC_VALUE);
	}
      }
      return 0;
    }
  }

  void Statement::set_code_section(
    GovernedSimple::code_section_t p_code_section)
  {
    switch(statementtype) {
    case S_ERROR:
    case S_DEF:
    case S_LABEL:
    case S_GOTO:
    case S_BREAK:
    case S_CONTINUE:
    case S_STOP_EXEC:
    case S_REPEAT:
      break;
    case S_ASSIGNMENT:
      ass->set_code_section(p_code_section);
      break;
    case S_FUNCTION_INSTANCE:
    case S_ALTSTEP_INSTANCE:
    case S_ACTIVATE:
      ref_pard->set_code_section(p_code_section);
      break;
    case S_BLOCK:
      block->set_code_section(p_code_section);
      break;
    case S_LOG:
    case S_ACTION:
    case S_STOP_TESTCASE:
      if (logargs) logargs->set_code_section(p_code_section);
      break;
    case S_IF:
      if_stmt.ics->set_code_section(p_code_section);
      if (if_stmt.elseblock)
        if_stmt.elseblock->set_code_section(p_code_section);
      break;
    case S_FOR:
      if (!loop.for_stmt.varinst)
        loop.for_stmt.init_ass->set_code_section(p_code_section);
      loop.for_stmt.finalexpr->set_code_section(p_code_section);
      loop.for_stmt.step->set_code_section(p_code_section);
      loop.block->set_code_section(p_code_section);
      break;
    case S_WHILE:
    case S_DOWHILE:
      loop.expr->set_code_section(p_code_section);
      loop.block->set_code_section(p_code_section);
      break;
    case S_SELECT:
      select.expr->set_code_section(p_code_section);
      select.scs->set_code_section(p_code_section);
      break;
    case S_ALT:
    case S_INTERLEAVE:
      ags->set_code_section(p_code_section);
      break;
    case S_RETURN:
      if (returnexpr.v) returnexpr.v->set_code_section(p_code_section);
      if (returnexpr.t) returnexpr.t->set_code_section(p_code_section);
      break;
    case S_DEACTIVATE:
      if (deactivate) deactivate->set_code_section(p_code_section);
      break;
    case S_SEND:
      port_op.portref->set_code_section(p_code_section);
      port_op.s.sendpar->set_code_section(p_code_section);
      if (port_op.s.toclause)
        port_op.s.toclause->set_code_section(p_code_section);
      break;
    case S_CALL:
      port_op.portref->set_code_section(p_code_section);
      port_op.s.sendpar->set_code_section(p_code_section);
      if (port_op.s.toclause)
        port_op.s.toclause->set_code_section(p_code_section);
      if (port_op.s.call.timer)
        port_op.s.call.timer->set_code_section(p_code_section);
      if (port_op.s.call.body)
        port_op.s.call.body->set_code_section(p_code_section);
      break;
    case S_REPLY:
      port_op.portref->set_code_section(p_code_section);
      port_op.s.sendpar->set_code_section(p_code_section);
      if (port_op.s.replyval)
        port_op.s.replyval->set_code_section(p_code_section);
      if (port_op.s.toclause)
        port_op.s.toclause->set_code_section(p_code_section);
      break;
    case S_RAISE:
      port_op.portref->set_code_section(p_code_section);
      port_op.s.sendpar->set_code_section(p_code_section);
      if (port_op.s.toclause)
        port_op.s.toclause->set_code_section(p_code_section);
      break;
    case S_RECEIVE:
    case S_CHECK_RECEIVE:
    case S_TRIGGER:
      if (port_op.portref) port_op.portref->set_code_section(p_code_section);
      if (port_op.r.rcvpar) port_op.r.rcvpar->set_code_section(p_code_section);
      if (port_op.r.fromclause)
        port_op.r.fromclause->set_code_section(p_code_section);
      if (port_op.r.redirect.value)
        port_op.r.redirect.value->set_code_section(p_code_section);
      if (port_op.r.redirect.sender)
        port_op.r.redirect.sender->set_code_section(p_code_section);
      break;
    case S_GETCALL:
    case S_CHECK_GETCALL:
      if (port_op.portref) port_op.portref->set_code_section(p_code_section);
      if (port_op.r.rcvpar) port_op.r.rcvpar->set_code_section(p_code_section);
      if (port_op.r.fromclause)
        port_op.r.fromclause->set_code_section(p_code_section);
      if (port_op.r.redirect.param)
        port_op.r.redirect.param->set_code_section(p_code_section);
      if (port_op.r.redirect.sender)
        port_op.r.redirect.sender->set_code_section(p_code_section);
      break;
    case S_GETREPLY:
    case S_CHECK_GETREPLY:
      if (port_op.portref) port_op.portref->set_code_section(p_code_section);
      if (port_op.r.rcvpar) port_op.r.rcvpar->set_code_section(p_code_section);
      if (port_op.r.getreply_valuematch)
        port_op.r.getreply_valuematch->set_code_section(p_code_section);
      if (port_op.r.fromclause)
        port_op.r.fromclause->set_code_section(p_code_section);
      if (port_op.r.redirect.value)
        port_op.r.redirect.value->set_code_section(p_code_section);
      if (port_op.r.redirect.param)
        port_op.r.redirect.param->set_code_section(p_code_section);
      if (port_op.r.redirect.sender)
        port_op.r.redirect.sender->set_code_section(p_code_section);
      break;
    case S_CATCH:
    case S_CHECK_CATCH:
      if (port_op.portref) port_op.portref->set_code_section(p_code_section);
      if (port_op.r.rcvpar) port_op.r.rcvpar->set_code_section(p_code_section);
      if (port_op.r.fromclause)
        port_op.r.fromclause->set_code_section(p_code_section);
      if (port_op.r.redirect.value)
        port_op.r.redirect.value->set_code_section(p_code_section);
      if (port_op.r.redirect.sender)
        port_op.r.redirect.sender->set_code_section(p_code_section);
      break;
    case S_CHECK:
      if (port_op.portref) port_op.portref->set_code_section(p_code_section);
      if (port_op.r.fromclause)
        port_op.r.fromclause->set_code_section(p_code_section);
      if (port_op.r.redirect.sender)
        port_op.r.redirect.sender->set_code_section(p_code_section);
      break;
    case S_CLEAR:
    case S_START_PORT:
    case S_STOP_PORT:
    case S_HALT:
      if (port_op.portref) port_op.portref->set_code_section(p_code_section);
      break;
    case S_START_COMP:
      comp_op.compref->set_code_section(p_code_section);
      comp_op.funcinstref->set_code_section(p_code_section);
      break;
    case S_START_COMP_REFD:
      comp_op.compref->set_code_section(p_code_section);
      comp_op.derefered.value->set_code_section(p_code_section);
      break;
    case S_STOP_COMP:
    case S_KILL:
    case S_KILLED:
      if (comp_op.compref) comp_op.compref->set_code_section(p_code_section);
      break;
    case S_DONE:
      if (comp_op.compref) {
        comp_op.compref->set_code_section(p_code_section);
        if (comp_op.donereturn.donematch)
          comp_op.donereturn.donematch->set_code_section(p_code_section);
        if (comp_op.donereturn.redirect)
          comp_op.donereturn.redirect->set_code_section(p_code_section);
      }
      break;
    case S_CONNECT:
    case S_MAP:
    case S_DISCONNECT:
    case S_UNMAP:
      config_op.compref1->set_code_section(p_code_section);
      config_op.portref1->set_code_section(p_code_section);
      config_op.compref2->set_code_section(p_code_section);
      config_op.portref2->set_code_section(p_code_section);
      break;
    case S_START_TIMER:
      timer_op.timerref->set_code_section(p_code_section);
      if (timer_op.value) timer_op.value->set_code_section(p_code_section);
      break;
    case S_STOP_TIMER:
    case S_TIMEOUT:
      if (timer_op.timerref)
        timer_op.timerref->set_code_section(p_code_section);
      break;
    case S_SETVERDICT:
      setverdict.verdictval->set_code_section(p_code_section);
      if (setverdict.logargs)
        setverdict.logargs->set_code_section(p_code_section);
      break;
    case S_TESTCASE_INSTANCE:
      testcase_inst.tcref->set_code_section(p_code_section);
      if (testcase_inst.timerval)
        testcase_inst.timerval->set_code_section(p_code_section);
      break;
    case S_TESTCASE_INSTANCE_REFD:
      execute_refd.value->set_code_section(p_code_section);
      if(execute_refd.timerval)
        execute_refd.timerval->set_code_section(p_code_section);
      break;
    case S_ACTIVATE_REFD:
    case S_FUNCTION_INVOKED:
    case S_ALTSTEP_INVOKED:
      fau_refd.value->set_code_section(p_code_section);
      if(fau_refd.ap_list2)
        for(size_t i = 0; i < fau_refd.ap_list2->get_nof_pars(); i++)
            fau_refd.ap_list2->get_par(i)->set_code_section(p_code_section);
      break;
    case S_STRING2TTCN:
      str2ttcn.val->set_code_section(p_code_section);
      str2ttcn.ref->set_code_section(p_code_section);
      break;
    default:
      FATAL_ERROR("Statement::set_code_section()");
    } // switch statementtype
  }

  char *Statement::generate_code(char *str)
  {
    switch (statementtype) {
    case S_BLOCK:
    case S_IF:
    case S_SELECT:
    case S_FOR:
    case S_WHILE:
    case S_DOWHILE:
      // conditional and loop statements do not need single location setting
      // the embedded expressions, statements have their own locations
      break;
    default:
      str = update_location_object(str);
    }

    switch(statementtype) {
    case S_DEF:
      str=def->generate_code_str(str);
      break;
    case S_ASSIGNMENT:
      str=ass->generate_code(str);
      break;
    case S_FUNCTION_INSTANCE:
    case S_ALTSTEP_INSTANCE:
      str=generate_code_funcinst(str);
      break;
    case S_FUNCTION_INVOKED:
    case S_ALTSTEP_INVOKED:
      str=generate_code_invoke(str);
      break;
    case S_BLOCK:
      str=generate_code_block(str);
      break;
    case S_LOG:
      str=generate_code_log(str);
      break;
    case S_LABEL:
      str = generate_code_label(str);
      break;
    case S_GOTO:
      str = generate_code_goto(str);
      break;
    case S_IF:
      str=generate_code_if(str);
      break;
    case S_SELECT:
      str=generate_code_select(str);
      break;
    case S_FOR:
      str=generate_code_for(str);
      break;
    case S_WHILE:
      str=generate_code_while(str);
      break;
    case S_DOWHILE:
      str=generate_code_dowhile(str);
      break;
    case S_BREAK:
      str=generate_code_break(str);
      break;
    case S_CONTINUE:
      str=generate_code_continue(str);
      break;
    case S_STOP_EXEC:
      str=mputstr(str, "TTCN_Runtime::stop_execution();\n");
      break;
    case S_STOP_TESTCASE:
      str=generate_code_testcase_stop(str);
      break;
    case S_ALT:
      str=ags->generate_code_alt(str, *this);
      break;
    case S_REPEAT:
      str=generate_code_repeat(str);
      break;
    case S_INTERLEAVE:
      str=generate_code_interleave(str);
      break;
    case S_RETURN:
      str=generate_code_return(str);
      break;
    case S_ACTIVATE:
      str=generate_code_activate(str);
      break;
    case S_ACTIVATE_REFD:
      str=generate_code_activate_refd(str);
      break;
    case S_DEACTIVATE:
      str=generate_code_deactivate(str);
      break;
    case S_SEND:
      str = generate_code_send(str);
      break;
    case S_CALL:
      str = generate_code_call(str);
      break;
    case S_REPLY:
      str = generate_code_reply(str);
      break;
    case S_RAISE:
      str = generate_code_raise(str);
      break;
    case S_RECEIVE:
    case S_TRIGGER:
    case S_GETCALL:
    case S_GETREPLY:
    case S_CATCH:
    case S_CHECK:
    case S_CHECK_RECEIVE:
    case S_CHECK_GETCALL:
    case S_CHECK_GETREPLY:
    case S_CHECK_CATCH:
    case S_TIMEOUT:
    case S_DONE:
    case S_KILLED:
      str = generate_code_standalone(str);
      break;
    case S_CLEAR:
      str=generate_code_portop(str, "clear");
      break;
    case S_START_PORT:
      str=generate_code_portop(str, "start");
      break;
    case S_STOP_PORT:
      str=generate_code_portop(str, "stop");
      break;
    case S_HALT:
      str=generate_code_portop(str, "halt");
      break;
    case S_START_COMP:
      str=generate_code_startcomp(str);
      break;
    case S_START_COMP_REFD:
      str=generate_code_startcomp_refd(str);
      break;
    case S_STOP_COMP:
      str = generate_code_compop(str, "stop");
      break;
    case S_KILL:
      str = generate_code_compop(str, "kill");
      break;
    case S_CONNECT:
      str = generate_code_configop(str, "connect");
      break;
    case S_MAP:
      str = generate_code_configop(str, "map");
      break;
    case S_DISCONNECT:
      str = generate_code_configop(str, "disconnect");
      break;
    case S_UNMAP:
      str = generate_code_configop(str, "unmap");
      break;
    case S_START_TIMER:
      str=generate_code_starttimer(str);
      break;
    case S_STOP_TIMER:
      str=generate_code_stoptimer(str);
      break;
    case S_SETVERDICT:
      str=generate_code_setverdict(str);
      break;
    case S_ACTION:
      str=generate_code_action(str);
      break;
    case S_TESTCASE_INSTANCE:
      str=generate_code_testcaseinst(str);
      break;
    case S_TESTCASE_INSTANCE_REFD:
      str=generate_code_execute_refd(str);
      break;
    case S_STRING2TTCN:
      str=generate_code_string2ttcn(str);
      break;
    default:
      FATAL_ERROR("Statement::generate_code()");
    } // switch
    return str;
  }

  char* Statement::generate_code_string2ttcn(char *str)
  {
    expression_struct val_expr;
    Code::init_expr(&val_expr);
    str2ttcn.val->generate_code_expr(&val_expr);

    expression_struct ref_expr;
    Code::init_expr(&ref_expr);
    str2ttcn.ref->generate_code(&ref_expr);

    str = mputstr(str, val_expr.preamble);
    str = mputstr(str, ref_expr.preamble);

    str = mputprintf(str, "string_to_ttcn(%s,%s);\n", val_expr.expr, ref_expr.expr);

    str = mputstr(str, val_expr.postamble);
    str = mputstr(str, ref_expr.postamble);

    Code::free_expr(&val_expr);
    Code::free_expr(&ref_expr);

    return str;
  }

  void Statement::generate_code_expr(expression_struct *expr)
  {
    switch (statementtype) {
    case S_RECEIVE:
      generate_code_expr_receive(expr, "receive");
      break;
    case S_TRIGGER:
      generate_code_expr_receive(expr, "trigger");
      break;
    case S_CHECK_RECEIVE:
      generate_code_expr_receive(expr, "check_receive");
      break;
    case S_GETCALL:
      generate_code_expr_getcall(expr, "getcall");
      break;
    case S_CHECK_GETCALL:
      generate_code_expr_getcall(expr, "check_getcall");
      break;
    case S_GETREPLY:
      generate_code_expr_getreply(expr, "getreply");
      break;
    case S_CHECK_GETREPLY:
      generate_code_expr_getreply(expr, "check_getreply");
      break;
    case S_CATCH:
    case S_CHECK_CATCH:
      generate_code_expr_catch(expr);
      break;
    case S_CHECK:
      generate_code_expr_check(expr);
      break;
    case S_DONE:
      generate_code_expr_done(expr);
      break;
    case S_KILLED:
      generate_code_expr_killed(expr);
      break;
    case S_TIMEOUT:
      generate_code_expr_timeout(expr);
      break;
    default:
      FATAL_ERROR("Statement::generate_code_expr()");
    } //switch
  }

  void Statement::ilt_generate_code(ILT *ilt)
  {
    switch (statementtype) {
    case S_INTERLEAVE:
      ilt_generate_code_interleave(ilt);
      return;
    case S_ALT:
      ilt_generate_code_alt(ilt);
      return;
    case S_DEF:
      ilt_generate_code_def(ilt);
      return;
    default:
      break;
    } // switch
    if (is_receiving_stmt()) {
      ilt_generate_code_receiving(ilt);
      return;
    }
    if (!has_receiving_stmt()) {
      char*& str=ilt->get_out_branches();
      str=generate_code(str);
      return;
    }
    switch (statementtype) {
    case S_BLOCK:
      block->ilt_generate_code(ilt);
      break;
    case S_IF:
      ilt_generate_code_if(ilt);
      break;
    case S_SELECT:
      ilt_generate_code_select(ilt);
      break;
    case S_CALL:
      ilt_generate_code_call(ilt);
      break;
    case S_FOR:
      ilt_generate_code_for(ilt);
      break;
    case S_WHILE:
      ilt_generate_code_while(ilt);
      break;
    case S_DOWHILE:
      ilt_generate_code_dowhile(ilt);
      break;
    default:
      FATAL_ERROR("Statement::ilt_generate_code()");
    } // switch statementtype
  }

  char *Statement::generate_code_standalone(char *str)
  {
    const string& tmplabel = my_sb->get_scope_mod_gen()->get_temporary_id();
    const char *label_str = tmplabel.c_str();
    str = mputprintf(str, "{\n"
      "%s:\n", label_str);
    expression_struct expr;
    Code::init_expr(&expr);
    generate_code_expr(&expr);
    str = mputstr(str, expr.preamble);
    str = mputprintf(str, "alt_status alt_flag = ALT_UNCHECKED, "
	"default_flag = ALT_UNCHECKED;\n"
      "TTCN_Snapshot::take_new(FALSE);\n"
      "for ( ; ; ) {\n"
      "if (alt_flag != ALT_NO) {\n"
      "alt_flag = %s;\n"
      "if (alt_flag == ALT_YES) break;\n", expr.expr);
    if (can_repeat()) {
      str = mputprintf(str, "else if (alt_flag == ALT_REPEAT) goto %s;\n",
	label_str);
    }
    str = mputprintf(str, "}\n"
       "if (default_flag != ALT_NO) {\n"
       "default_flag = TTCN_Default::try_altsteps();\n"
       "if (default_flag == ALT_YES || default_flag == ALT_BREAK) break;\n"
       "else if (default_flag == ALT_REPEAT) goto %s;\n"
       "}\n", label_str);
    str = update_location_object(str);
    str = mputprintf(str, "if (alt_flag == ALT_NO && default_flag == ALT_NO) "
      "TTCN_error(\"Stand-alone %s statement failed in file ", get_stmt_name());
    str = Code::translate_string(str, get_filename());
    int first_line = get_first_line(), last_line = get_last_line();
    if (first_line < last_line) str = mputprintf(str,
      " between lines %d and %d", first_line, last_line);
    else str = mputprintf(str, ", line %d", first_line);
    str = mputstr(str, ".\");\n"
      "TTCN_Snapshot::take_new(TRUE);\n"
      "}\n");
    str = mputstr(str, expr.postamble);
    str = mputstr(str, "}\n");
    Code::free_expr(&expr);
    return str;
  }

  char *Statement::generate_code_funcinst(char *str)
  {
    expression_struct expr;
    Code::init_expr(&expr);
    ref_pard->generate_code_const_ref(&expr);
    str=Code::merge_free_expr(str, &expr);
    return str;
  }

  char* Statement::generate_code_invoke(char *str)
  {
    expression_struct expr;
    Code::init_expr(&expr);
    Value *last_v = fau_refd.value->get_value_refd_last();
    switch(last_v->get_valuetype()) {
    case Value::V_FUNCTION:
    case Value::V_ALTSTEP: {
      Common::Assignment *t_fat = last_v->get_refd_fat();
      expr.expr = mputprintf(expr.expr, "%s(",
	t_fat->get_genname_from_scope(my_sb).c_str());
      fau_refd.ap_list2->generate_code_alias(&expr, t_fat->get_FormalParList(),
	t_fat->get_RunsOnType(), false);
      break; }
    default: {
      fau_refd.value->generate_code_expr_mandatory(&expr);
      Type *t_governor = fau_refd.value->get_expr_governor_last();
      expr.expr = mputprintf(expr.expr, ".%s(",
	t_governor->get_typetype() == Type::T_ALTSTEP ?
	"invoke_standalone" : "invoke");
      fau_refd.ap_list2->generate_code_alias(&expr, 0,
	t_governor->get_fat_runs_on_type(),
        t_governor->get_fat_runs_on_self()); }
    }
    expr.expr = mputc(expr.expr, ')');
    str=Code::merge_free_expr(str, &expr);
    return str;
  }

  char *Statement::generate_code_block(char *str)
  {
    switch (block->get_exception_handling()) {
    case StatementBlock::EH_NONE:
      break;
    case StatementBlock::EH_TRY:
      str = mputstr(str, "try ");
      break;
    case StatementBlock::EH_CATCH:
      str = mputstr(str, "catch (const TTCN_Error& ttcn_error) ");
      break;
    default:
      FATAL_ERROR("Statement::generate_code_block()");
    }
    if (block->get_nof_stmts() > 0 || block->get_exception_handling()!=StatementBlock::EH_NONE) {
      str = mputstr(str, "{\n");
      str = block->generate_code(str);
      str = mputstr(str, "}\n");
    } else str = mputstr(str, "/* empty block */;\n");
    return str;
  }

  char *Statement::generate_code_log(char *str)
  {
    if (logargs) {
      bool buffered_mode = true;
      if (logargs->get_nof_logargs() == 1) {
	LogArgument *first_logarg = logargs->get_logarg_byIndex(0);
        switch (first_logarg->get_type()) {
	case LogArgument::L_STR:
	  // the argument is a simple string: use non-buffered mode
	  str = mputstr(str, "TTCN_Logger::log_str(TTCN_USER, \"");
	  str = Code::translate_string(str, first_logarg->get_str().c_str());
	  str = mputstr(str, "\");\n");
	  buffered_mode = false;
	  break;
	case LogArgument::L_MACRO: {
	  Value *t_val = first_logarg->get_val();
	  if (t_val->has_single_expr()) {
	    // the argument is a simple macro call: use non-buffered mode
	    str = mputprintf(str, "TTCN_Logger::log_str(TTCN_USER, %s);\n",
	      t_val->get_single_expr().c_str());
	    buffered_mode = false;
	  } }
	default:
	  break;
	}
      }
      if (buffered_mode) {
	// the argument is a complicated construct: use buffered mode
	str = mputstr(str, "try {\n"
	  "TTCN_Logger::begin_event(TTCN_USER);\n");
	str = logargs->generate_code(str);
	str = mputstr(str, "TTCN_Logger::end_event();\n"
	  "} catch (...) {\n"
	  "TTCN_Logger::finish_event();\n"
	  "throw;\n"
	  "}\n");
      }
    } else {
      // the argument is missing
      str = mputstr(str, "TTCN_Logger::log_str(TTCN_USER, "
	"\"<empty log statement>\");\n");
    }
    return str;
  }

  char *Statement::generate_code_testcase_stop(char *str)
  {
    if (logargs) str = generate_code_log(str);
    str = mputstr(str, "TTCN_error(\"testcase.stop\");\n");
    return str;
  }

  char *Statement::generate_code_label(char *str)
  {
    if (label.used) {
      return mputprintf(str, "%s: /* TTCN-3 label: %s */;\n",
	get_clabel().c_str(), label.id->get_dispname().c_str());
    } else {
      return mputprintf(str, "/* unused TTCN-3 label: %s */;\n",
	label.id->get_dispname().c_str());
    }
  }

  char *Statement::generate_code_goto(char *str)
  {
    if (!go_to.label) FATAL_ERROR("Statement::generate_code_goto()");
    return mputprintf(str, "goto %s; /* TTCN-3 label: %s */\n",
      go_to.label->get_clabel().c_str(), go_to.id->get_dispname().c_str());
    return str;
  }

  char* Statement::generate_code_if(char *str)
  {
    size_t blockcount=0;
    bool unreach=false, eachfalse=true;
    str=if_stmt.ics->generate_code(str, blockcount, unreach, eachfalse);
    if(if_stmt.elseblock && !unreach) {
      if(!eachfalse) str=mputstr(str, "else ");
      eachfalse=false;
      str=mputstr(str, "{\n");
      blockcount++;
      str=if_stmt.elseblock->generate_code(str);
    }
    while(blockcount-->0) str=mputstr(str, "}\n");
    if(eachfalse) str=mputstr(str, "/* never occurs */;\n");
    return str;
  }

  char* Statement::generate_code_select(char *str)
  {
    const string& tmp_prefix = my_sb->get_scope_mod_gen()->get_temporary_id();
    char *expr_init=memptystr();
    char *expr_name=select.expr->generate_code_tmp(0, expr_init);
    if (expr_init[0]) { // some init code was generated
      str = update_location_object(str);
      str = mputstr(str, "{\n");
      str = mputstr(str, expr_init);
    }
    str=select.scs->generate_code(str, tmp_prefix.c_str(), expr_name);
    Free(expr_name);
    if (expr_init[0]) str=mputstr(str, "}\n");
    Free(expr_init);
    return str;
  }

  char *Statement::generate_code_for(char *str)
  {
    /** \todo initial does not have its own location */
    // statements in initial may have side effects
    // generate code for them anyway
    if (loop.for_stmt.varinst) {
      str = mputstr(str, "{\n");
      str = loop.for_stmt.init_varinst->generate_code_str(str);
    } else {
      str = loop.for_stmt.init_ass->update_location_object(str);
      str = loop.for_stmt.init_ass->generate_code(str);
    }
    // check whether the final expression is constant
    bool final_is_true = false, final_is_false = false;
    if (!loop.for_stmt.finalexpr->is_unfoldable()) {
      if (loop.for_stmt.finalexpr->get_val_bool()) final_is_true = true;
      else final_is_false = true;
    }
    if (final_is_false) str = mputstr(str, "/* never occurs */;\n");
    else {
      if (loop.has_cnt)
        loop.label_next =
          new string(my_sb->get_scope_mod_gen()->get_temporary_id());
      str = update_location_object(str);
      str = mputstr(str, "for ( ; ; ) {\n");
      // do not generate the exit condition for infinite loops
      if (!final_is_true) {
	str = loop.for_stmt.finalexpr->update_location_object(str);
	size_t blockcount = 0;
	str = loop.for_stmt.finalexpr->generate_code_tmp(str, "if (!",
	  blockcount);
	str = mputstr(str, ") break;\n");
	while (blockcount-- > 0) str = mputstr(str, "}\n");
      }
      if (loop.label_next) str = mputstr(str, "{\n");
      str = loop.block->generate_code(str);
      if (loop.label_next)
        str = mputprintf(str, "}\n"
          "%s:\n", loop.label_next->c_str());
      str = loop.for_stmt.step->update_location_object(str);
      str = loop.for_stmt.step->generate_code(str);
      str = mputstr(str, "}\n");
    }
    if (loop.for_stmt.varinst) str = mputstr(str, "}\n");
    return str;
  }

  char *Statement::generate_code_while(char *str)
  {
    // check whether the expression is constant
    bool condition_always_true = false, condition_always_false = false;
    if (!loop.expr->is_unfoldable()) {
      if (loop.expr->get_val_bool()) condition_always_true = true;
      else condition_always_false = true;
    }
    if (condition_always_false) str = mputstr(str, "/* never occurs */;\n");
    else {
      str = mputstr(str, "for ( ; ; ) {\n");
      if (loop.has_cnt_in_ags) {
        loop.label_next =
          new string(my_sb->get_scope_mod_gen()->get_temporary_id());
        str = mputprintf(str, "%s:\n", loop.label_next->c_str());
      }
      // do not generate the exit condition for infinite loops
      if (!condition_always_true) {
	str = loop.expr->update_location_object(str);
	size_t blockcount = 0;
	str = loop.expr->generate_code_tmp(str, "if (!", blockcount);
	str = mputstr(str, ") break;\n");
	while(blockcount-- > 0) str = mputstr(str, "}\n");
      }
      str = loop.block->generate_code(str);
      str = mputstr(str, "}\n");
    }
    return str;
  }

  char *Statement::generate_code_dowhile(char *str)
  {
    // check whether the expression is constant
    bool expr_is_const = !loop.expr->is_unfoldable();
    bool is_infinite_loop = false;
    if (expr_is_const) {
      if (loop.expr->get_val_bool()) is_infinite_loop = true;
      else loop.iterate_once = true;
    }
    if (loop.iterate_once && !loop.has_brk && !loop.has_cnt) {
      str = mputstr(str, "{\n");
      str = loop.block->generate_code(str);
    } else {
      str = mputstr(str, "for ( ; ; ) {\n");
      if (loop.has_cnt_in_ags || (!expr_is_const && loop.has_cnt))
        loop.label_next =
          new string(my_sb->get_scope_mod_gen()->get_temporary_id());
      if (loop.label_next && is_infinite_loop)
        str = mputprintf(str, "%s:\n", loop.label_next->c_str());
      if (loop.label_next && !is_infinite_loop) str = mputstr(str, "{\n");
      str = loop.block->generate_code(str);
      // do not generate the exit condition for infinite loops
      if (!is_infinite_loop) {
        if (loop.label_next)
          str = mputprintf(str, "}\n"
            "%s:\n", loop.label_next->c_str());
        str = loop.expr->update_location_object(str);
        if (loop.iterate_once) str = mputstr(str, "break;\n");
        else {
          size_t blockcount = 0;
          str = loop.expr->generate_code_tmp(str, "if (!", blockcount);
          str = mputstr(str, ") break;\n");
          while (blockcount-- > 0) str = mputstr(str, "}\n");
        }
      }
    }
    str = mputstr(str, "}\n");
    return str;
  }

  char *Statement::generate_code_break(char *str)
  {
    // in altstep (2 (=2nd if branch))
    // in alt and loops - not inside interleave (4)
    // in loops without receiving statement embedded in interleave (4)
    // in loops with receiving statement embedded in interleave (1)
    // in interleave when not embedded in enclosed loop or alt/interleave (4)
    // in alt/interleave embedded in interleave (3)
    if (brk_cnt.loop_stmt && brk_cnt.loop_stmt->loop.il_label_end)
      str=mputprintf(str, "goto %s;\n",
            brk_cnt.loop_stmt->loop.il_label_end->c_str());
    else if (brk_cnt.ags && brk_cnt.ags->get_is_altstep())
      str=mputstr(str, "return ALT_BREAK;\n");
    else if (brk_cnt.ags && brk_cnt.ags->get_il_label_end())
      str=mputprintf(str, "goto %s;\n",
            brk_cnt.ags->get_il_label_end()->c_str());
    else
      str=mputstr(str, "break;\n");
    return str;
  }

  char *Statement::generate_code_continue(char *str)
  {
    // not inside interleave when continue is not inside embedded ags (2 or 1)
    // continue is inside ags enclosed in the loop (3)
    // in interleave (3, 2 or 1)
    if (brk_cnt.loop_stmt != 0) {
      if (brk_cnt.loop_stmt->loop.iterate_once && !brk_cnt.ags &&
          !brk_cnt.loop_stmt->loop.is_ilt)
        str=mputstr(str, "break;\n");
      else {
        if (!brk_cnt.loop_stmt->loop.label_next)
          str=mputstr(str, "continue;\n");
        else
          str=mputprintf(str, "goto %s;\n",
            brk_cnt.loop_stmt->loop.label_next->c_str());
      }
    } else
      FATAL_ERROR("Statement::generate_code_continue()");
    return str;
  }

  char *Statement::generate_code_repeat(char *str)
  {
    string *tmplabel=ags->get_label();
    if(!tmplabel) str=mputstr(str, "return ALT_REPEAT;\n");
    else str=mputprintf(str, "goto %s;\n", tmplabel->c_str());
    return str;
  }

  char* Statement::generate_code_interleave(char *str)
  {
    ILT_root ilt(this);
    str=ilt.generate_code(str);
    return str;
  }

  void Statement::ilt_generate_code_interleave(ILT *ilt)
  {
    const string& mytmpid=ilt->get_my_tmpid();
    bool toplevel=ilt->is_toplevel();
    size_t goto_label_num=toplevel?(size_t)-1:ilt->get_new_label_num();
    char*& out_branches=ilt->get_out_branches();
    out_branches=update_location_object(out_branches);
    string state_cond;
    if(toplevel) state_cond="";
    else {
      char *label_end = mprintf("%s_l%lu", mytmpid.c_str(),
        (unsigned long) goto_label_num);
      ags->set_il_label_end (label_end);
      Free(label_end);
      ILT_branch *branch=ilt->get_as_branch();
      size_t state_var=branch->get_my_state_var();
      size_t state_var_val=ilt->get_new_state_var_val(state_var);
      state_cond=branch->get_state_cond();
      if(!state_cond.empty()) state_cond+=" && ";
      char *s=mprintf("%s_state[%lu]==%lu", mytmpid.c_str(),
                      (unsigned long) state_var, (unsigned long) state_var_val);
      state_cond+=s;
      Free(s);
      out_branches=mputprintf(out_branches, "%s_state[%lu]=%lu;\n",
        mytmpid.c_str(),
        (unsigned long) state_var, (unsigned long) state_var_val);
    }
    for(size_t i=0; i<ags->get_nof_ags(); i++) {
      AltGuard *ag=ags->get_ag_byIndex(i);
      if(ag->get_type()!=AltGuard::AG_OP)
        FATAL_ERROR("Statement::ilt_generate_code_interleave()");
      size_t state_var=ilt->get_new_state_var(toplevel);
      size_t state_var_val=ilt->get_new_state_var_val(state_var);
      ilt->add_branch(new ILT_branch(ILT_branch::BT_IL, ag, state_cond,
                                     state_var, state_var_val,
                                     goto_label_num));
      if(!toplevel)
        out_branches=mputprintf(out_branches, "%s_state[%lu]=%lu;\n",
          mytmpid.c_str(),
          (unsigned long) state_var, (unsigned long) state_var_val);
    } // for
    if(!toplevel)
      out_branches=mputprintf(out_branches, "goto %s;\n"
                              "%s_l%lu:\n",
                              mytmpid.c_str(),
                              mytmpid.c_str(), (unsigned long) goto_label_num);
  }

  void Statement::ilt_generate_code_alt(ILT *ilt)
  {
    const string& mytmpid=ilt->get_my_tmpid();
    size_t goto_label_num=ilt->get_new_label_num();
    char *label_end = mprintf("%s_l%lu", mytmpid.c_str(),
      (unsigned long) goto_label_num);
    ags->set_il_label_end (label_end);
    ILT_branch *branch=ilt->get_as_branch();
    string state_cond=branch->get_state_cond();
    size_t state_var=branch->get_my_state_var();
    size_t state_var_val=ilt->get_new_state_var_val(state_var);
    char*& out_branches=ilt->get_out_branches();
    for(size_t i=0; i<ags->get_nof_ags(); i++) {
      AltGuard *ag=ags->get_ag_byIndex(i);
      if(ag->get_type()!=AltGuard::AG_OP)
        FATAL_ERROR("Statement::ilt_generate_code_alt()");
      ilt->add_branch(new ILT_branch(ILT_branch::BT_ALT, ag, state_cond,
                                     state_var, state_var_val,
                                     goto_label_num));
    } // for
    out_branches=update_location_object(out_branches);
    out_branches=mputprintf(out_branches, "%s_state[%lu]=%lu;\n"
                            "goto %s;\n"
                            "%s:\n",
                            mytmpid.c_str(), (unsigned long) state_var,
                            (unsigned long) state_var_val,
                            mytmpid.c_str(), label_end);
    Free(label_end);
  }

  void Statement::ilt_generate_code_receiving(ILT *ilt)
  {
    const string& mytmpid=ilt->get_my_tmpid();
    size_t goto_label_num=ilt->get_new_label_num();
    ILT_branch *branch=ilt->get_as_branch();
    string state_cond=branch->get_state_cond();
    size_t state_var=branch->get_my_state_var();
    size_t state_var_val=ilt->get_new_state_var_val(state_var);
    char*& out_branches=ilt->get_out_branches();
    ilt->add_branch(new ILT_branch(ILT_branch::BT_RECV, this, state_cond,
                                   state_var, state_var_val, goto_label_num));
    out_branches=update_location_object(out_branches);
    out_branches=mputprintf(out_branches, "%s_state[%lu]=%lu;\n"
                            "goto %s;\n"
                            "%s_l%lu:\n",
                            mytmpid.c_str(), (unsigned long) state_var,
                            (unsigned long) state_var_val,
                            mytmpid.c_str(), mytmpid.c_str(),
                            (unsigned long) goto_label_num);
  }

  void Statement::ilt_generate_code_def(ILT *ilt)
  {
    char*& str=ilt->get_out_branches();
    str=update_location_object(str);
    {
      char *genname=mprintf("%s_d%lu_%s", ilt->get_my_tmpid().c_str(),
                            (unsigned long) ilt->get_new_tmpnum(),
			    def->get_id().get_name().c_str());
      def->set_genname(string(genname));
      Free(genname);
    }
    def->ilt_generate_code(ilt);
  }

  void Statement::ilt_generate_code_if(ILT *ilt)
  {
    char *end_label=mprintf("%s_l%lu",
                             ilt->get_my_tmpid().c_str(),
                             (unsigned long) ilt->get_new_label_num());
    bool unreach=false;
    if_stmt.ics->ilt_generate_code(ilt, end_label, unreach);
    if(if_stmt.elseblock && !unreach)
      if_stmt.elseblock->ilt_generate_code(ilt);
    char*& str=ilt->get_out_branches();
    str=mputprintf(str, "%s:\n", end_label);
    Free(end_label);
  }

  void Statement::ilt_generate_code_select(ILT *ilt)
  {
    char*& str=ilt->get_out_branches();
    str=update_location_object(str);
    const string& tmp_prefix = my_sb->get_scope_mod_gen()->get_temporary_id();
    char *expr_init=memptystr();
    char *expr_name=select.expr->generate_code_tmp(0, expr_init);
    select.scs->ilt_generate_code(ilt, tmp_prefix.c_str(),
                                  expr_init, expr_name);
    Free(expr_name);
    Free(expr_init);
  }

  void Statement::ilt_generate_code_call(ILT *ilt)
  {
    char*& str=ilt->get_out_branches();
    str=update_location_object(str);
    expression_struct expr;
    Code::init_expr(&expr);
    port_op.portref->generate_code(&expr);
    expr.expr = mputstr(expr.expr, ".call(");
    port_op.s.sendpar->generate_code(&expr);
    if(port_op.s.toclause) {
      expr.expr = mputstr(expr.expr, ", ");
      port_op.s.toclause->generate_code_expr(&expr);
    }
    expr.expr = mputc(expr.expr, ')');
    str = Code::merge_free_expr(str, &expr);
    if (port_op.s.call.body) {
      str = mputstr(str, "{\n"); // (1)
      if (port_op.s.call.timer) {
	str = port_op.s.call.timer->update_location_object(str);
	str = mputstr(str, "TIMER call_timer;\n");
	Code::init_expr(&expr);
	expr.expr = mputstr(expr.expr, "call_timer.start(");
	port_op.s.call.timer->generate_code_expr(&expr);
	expr.expr = mputc(expr.expr, ')');
	str = Code::merge_free_expr(str, &expr);
      }
      // the label name is used for prefixing local variables
      if(!my_sb) FATAL_ERROR("Statement::generate_code_call()");
      const string& tmplabel = my_sb->get_scope_mod_gen()->get_temporary_id();
      str = port_op.s.call.body->generate_code_call_body(str, *this, tmplabel,
                                                       true);
      const char *label_str = tmplabel.c_str();
      str=mputprintf(str, "goto %s_end;\n"
                     "}\n", // (1)
                     label_str);
      port_op.s.call.body->ilt_generate_code_call_body(ilt, label_str);
      str=mputprintf(str, "%s_end:\n", label_str);
    }
  }

  void Statement::ilt_generate_code_for(ILT *ilt)
  {
    char*& str = ilt->get_out_branches();
    str = update_location_object(str);
    // statements in initial may have side effects
    // generate code for them anyway
    if (loop.for_stmt.varinst) {
      char *genname = mprintf("%s_d%lu_", ilt->get_my_tmpid().c_str(),
	(unsigned long) ilt->get_new_tmpnum());
      loop.for_stmt.init_varinst->set_genname(string(genname));
      Free(genname);
      loop.for_stmt.init_varinst->ilt_generate_code(ilt);
    } else str = loop.for_stmt.init_ass->generate_code(str);
    // check whether the final expression is constant
    bool final_is_true = false, final_is_false = false;
    if (!loop.for_stmt.finalexpr->is_unfoldable()) {
      if (loop.for_stmt.finalexpr->get_val_bool()) final_is_true = true;
      else final_is_false = true;
    }
    if (final_is_false) str = mputstr(str, "/* never occurs */;\n");
    else {
      char *label_prefix = mprintf("%s_l%lu_", ilt->get_my_tmpid().c_str(),
	(unsigned long) ilt->get_new_label_num());
      str = mputprintf(str, "%sbegin:\n", label_prefix);
      // do not generate the exit condition for infinite loops
      if (!final_is_true) {
	str = loop.for_stmt.finalexpr->update_location_object(str);
	size_t blockcount = 0;
	str = loop.for_stmt.finalexpr->generate_code_tmp(str, "if (!",
	  blockcount);
	str = mputprintf(str, ") goto %send;\n", label_prefix);
	while (blockcount-- > 0) str = mputstr(str, "}\n");
      }
      loop.is_ilt = true;
      if (loop.has_brk) {
        loop.il_label_end = new string(label_prefix);
        *loop.il_label_end += "end";
      }
      if (loop.has_cnt) {
        loop.label_next = new string(label_prefix);
        *loop.label_next += "next";
      }
      loop.block->ilt_generate_code(ilt);
      if (loop.label_next)
        str = mputprintf(str, "%snext:\n", label_prefix);
      str = update_location_object(str);
      str = loop.for_stmt.step->generate_code(str);
      str = mputprintf(str, "goto %sbegin;\n", label_prefix);
      if (!final_is_true || loop.has_brk)
        str = mputprintf(str, "%send:\n", label_prefix);
      Free(label_prefix);
    }
  }

  void Statement::ilt_generate_code_while(ILT *ilt)
  {
    char*& str = ilt->get_out_branches();
    // Location need not be set here; the location is set for the expression.
    // check whether the expression is constant
    bool expr_is_true = false, expr_is_false = false;
    if (!loop.expr->is_unfoldable()) {
      if (loop.expr->get_val_bool()) expr_is_true = true;
      else expr_is_false = true;
    }
    if (expr_is_false) str = mputstr(str, "/* never occurs */;\n");
    else {
      char *label_prefix = mprintf("%s_l%lu_", ilt->get_my_tmpid().c_str(),
	(unsigned long) ilt->get_new_label_num());
      str = mputprintf(str, "%sbegin:\n", label_prefix);
      loop.is_ilt = true;
      if (loop.has_brk) {
        loop.il_label_end = new string(label_prefix);
        *loop.il_label_end += "end";
      }
      if (loop.has_cnt) {
        loop.label_next = new string(label_prefix);
        *loop.label_next += "begin";
      }
      // do not generate the exit condition for infinite loops
      if (!expr_is_true) {
	str = loop.expr->update_location_object(str);
	size_t blockcount = 0;
	str = loop.expr->generate_code_tmp(str, "if (!", blockcount);
	str = mputprintf(str, ") goto %send;\n", label_prefix);
	while (blockcount-- > 0) str = mputstr(str, "}\n");
      }
      loop.block->ilt_generate_code(ilt);
      str = update_location_object(str);
      str = mputprintf(str, "goto %sbegin;\n", label_prefix);
      if (!expr_is_true || loop.has_brk)
        str = mputprintf(str, "%send:\n", label_prefix);
      Free(label_prefix);
    }
  }

  void Statement::ilt_generate_code_dowhile(ILT *ilt)
  {
    char*& str = ilt->get_out_branches();
    // Location need not be set here; there is only a label before the body.
    // check whether the expression is constant
    bool expr_is_true = false;
    if (!loop.expr->is_unfoldable()) {
      if (loop.expr->get_val_bool()) expr_is_true = true;
      else loop.iterate_once = true;
    }
    char *label_prefix = 0;
    if (!loop.iterate_once || loop.has_brk || loop.has_cnt)
      label_prefix = mprintf("%s_l%lu_", ilt->get_my_tmpid().c_str(),
	(unsigned long) ilt->get_new_label_num());
    loop.is_ilt = true;
    if (loop.has_brk) {
      loop.il_label_end = new string(label_prefix);
      *loop.il_label_end += "end";
    }
    if (loop.has_cnt)
      loop.label_next = new string(label_prefix);
    if (loop.iterate_once) {
      if (loop.label_next) *loop.label_next += "end";
      loop.block->ilt_generate_code(ilt);
    } else {
      str = mputprintf(str, "%sbegin:\n", label_prefix);
      if (loop.label_next)
        *loop.label_next += (expr_is_true ? "begin" : "next");
      loop.block->ilt_generate_code(ilt);
      if (expr_is_true) str = mputprintf(str, "goto %sbegin;\n", label_prefix);
      else {
        if (loop.label_next) str = mputprintf(str, "%snext:\n", label_prefix);
	str = loop.expr->update_location_object(str);
	size_t blockcount = 0;
	str = loop.expr->generate_code_tmp(str, "if (", blockcount);
	str = mputprintf(str, ") goto %sbegin;\n", label_prefix);
	while (blockcount-- > 0) str = mputstr(str, "}\n");
      }
    }
    if (loop.il_label_end || (loop.iterate_once && loop.label_next)) {
      str = mputprintf(str, "%send: ;\n", label_prefix);
    }
    Free(label_prefix);
  }

  char *Statement::generate_code_return(char *str)
  {
    expression_struct expr;
    Code::init_expr(&expr);
    expr.expr = mputstr(expr.expr, "return");
    Definition *my_def = my_sb->get_my_def();
    if (returnexpr.v) {
      expr.expr = mputc(expr.expr, ' ');
      returnexpr.v->generate_code_expr_mandatory(&expr);
    } else if (returnexpr.t) {
      expr.expr = mputc(expr.expr, ' ');
      if (!my_def) FATAL_ERROR("Statement::generate_code_return()");
      Def_Function_Base* dfb = dynamic_cast<Def_Function_Base*>(my_def);
      if (!dfb) FATAL_ERROR("Statement::generate_code_return()");
      if (dfb->get_template_restriction() != TR_NONE &&
          returnexpr.gen_restriction_check) {
        returnexpr.t->generate_code_expr(&expr,
          dfb->get_template_restriction());
      } else {
        returnexpr.t->generate_code_expr(&expr, TR_NONE);
      }
    } else {
      if (my_def && my_def->get_asstype() == Definition::A_ALTSTEP)
        expr.expr = mputstr(expr.expr, " ALT_YES");
      // else it's a return with no value: the only case a blank is unneeded
    }
    return Code::merge_free_expr(str, &expr);
  }

  char *Statement::generate_code_activate(char *str)
  {
    expression_struct expr;
    Code::init_expr(&expr);
    expr.expr = mputprintf(expr.expr, "%s(", ref_pard->get_refd_assignment()
	->get_genname_from_scope(my_sb, "activate_").c_str());
    ref_pard->get_parlist()->generate_code_noalias(&expr, ref_pard->get_refd_assignment()->get_FormalParList());
    expr.expr = mputc(expr.expr, ')');
    return Code::merge_free_expr(str, &expr);
  }

  char *Statement::generate_code_activate_refd(char *str)
  {
    expression_struct expr;
    Code::init_expr(&expr);
    Value *last_v = fau_refd.value->get_value_refd_last();
    if (last_v->get_valuetype() == Value::V_ALTSTEP) {
      expr.expr = mputprintf(expr.expr, "%s(", last_v->get_refd_fat()
	  ->get_genname_from_scope(my_sb, "activate_").c_str());
    } else {
      fau_refd.value->generate_code_expr_mandatory(&expr);
      expr.expr = mputstr(expr.expr, ".activate(");
    }
    fau_refd.ap_list2->generate_code_noalias(&expr, NULL);
    expr.expr = mputc(expr.expr, ')');
    return Code::merge_free_expr(str, &expr);
  }

  char *Statement::generate_code_deactivate(char *str)
  {
    if(!deactivate) str=mputstr(str, "TTCN_Default::deactivate_all();\n");
    else {
      expression_struct expr;
      Code::init_expr(&expr);
      expr.expr=mputstr(expr.expr, "TTCN_Default::deactivate(");
      deactivate->generate_code_expr(&expr);
      expr.expr=mputstr(expr.expr, ");\n");
      str=Code::merge_free_expr(str, &expr);
    }
    return str;
  }

  char *Statement::generate_code_send(char *str)
  {
    expression_struct expr;
    Code::init_expr(&expr);
    port_op.portref->generate_code(&expr);
    expr.expr = mputstr(expr.expr, ".send(");
    generate_code_expr_sendpar(&expr);
    if (port_op.s.toclause) {
      expr.expr = mputstr(expr.expr, ", ");
      port_op.s.toclause->generate_code_expr(&expr);
    }
    expr.expr = mputc(expr.expr, ')');
    return Code::merge_free_expr(str, &expr);
  }

  char *Statement::generate_code_call(char *str)
  {
    expression_struct expr;
    Code::init_expr(&expr);
    port_op.portref->generate_code(&expr);
    expr.expr = mputstr(expr.expr, ".call(");
    port_op.s.sendpar->generate_code(&expr);
    if(port_op.s.toclause) {
      expr.expr = mputstr(expr.expr, ", ");
      port_op.s.toclause->generate_code_expr(&expr);
    }
    expr.expr = mputc(expr.expr, ')');
    str = Code::merge_free_expr(str, &expr);
    if (port_op.s.call.body) {
      str = mputstr(str, "{\n");
      if (port_op.s.call.timer) {
	str = port_op.s.call.timer->update_location_object(str);
	str = mputstr(str, "TIMER call_timer;\n");
	Code::init_expr(&expr);
	expr.expr = mputstr(expr.expr, "call_timer.start(");
	port_op.s.call.timer->generate_code_expr(&expr);
	expr.expr = mputc(expr.expr, ')');
	str = Code::merge_free_expr(str, &expr);
      }
      // the label name is used for prefixing local variables
      if(!my_sb) FATAL_ERROR("Statement::generate_code_call()");
      str = port_op.s.call.body->generate_code_call_body(str, *this,
	my_sb->get_scope_mod_gen()->get_temporary_id(), false);
      str=mputstr(str, "}\n");
    }
    return str;
  }

  char *Statement::generate_code_reply(char *str)
  {
    expression_struct expr;
    Code::init_expr(&expr);
    port_op.portref->generate_code(&expr);
    expr.expr=mputstr(expr.expr, ".reply(");
    port_op.s.sendpar->generate_code(&expr);
    if(port_op.s.replyval) {
      expr.expr=mputstr(expr.expr, ".set_value_template(");
      port_op.s.replyval->generate_code_expr(&expr);
      expr.expr=mputc(expr.expr, ')');
    }
    if(port_op.s.toclause) {
      expr.expr=mputstr(expr.expr, ", ");
      port_op.s.toclause->generate_code_expr(&expr);
    }
    expr.expr=mputc(expr.expr, ')');
    return Code::merge_free_expr(str, &expr);
  }

  char *Statement::generate_code_raise(char *str)
  {
    expression_struct expr;
    Code::init_expr(&expr);
    port_op.portref->generate_code(&expr);
    expr.expr=mputstr(expr.expr, ".raise(");
    port_op.s.raise.signature_ref->generate_code(&expr);
    expr.expr=mputstr(expr.expr, "_exception(");
    generate_code_expr_sendpar(&expr);
    expr.expr=mputc(expr.expr, ')');
    if(port_op.s.toclause) {
      expr.expr=mputstr(expr.expr, ", ");
      port_op.s.toclause->generate_code_expr(&expr);
    }
    expr.expr=mputc(expr.expr, ')');
    return Code::merge_free_expr(str, &expr);
  }

  char *Statement::generate_code_portop(char *str, const char *opname)
  {
    if (port_op.portref) {
      expression_struct expr;
      Code::init_expr(&expr);
      port_op.portref->generate_code(&expr);
      expr.expr=mputprintf(expr.expr, ".%s()", opname);
      str=Code::merge_free_expr(str, &expr);
    } else {
      str = mputprintf(str, "PORT::all_%s();\n", opname);
    }
    return str;
  }

  char *Statement::generate_code_startcomp(char *str)
  {
    expression_struct expr;
    Code::init_expr(&expr);
    Common::Assignment *func = comp_op.funcinstref->get_refd_assignment();
    expr.expr = mputprintf(expr.expr, "%s(",
      func->get_genname_from_scope(my_sb, "start_").c_str());
    comp_op.compref->generate_code_expr(&expr);
    FormalParList *fplist = func->get_FormalParList();
    if (fplist->get_nof_fps() > 0) {
      expr.expr = mputstr(expr.expr, ", ");
      comp_op.funcinstref->get_parlist()->generate_code_noalias(&expr, fplist);
    }
    expr.expr = mputc(expr.expr, ')');
    return Code::merge_free_expr(str, &expr);
  }

  char *Statement::generate_code_startcomp_refd(char *str)
  {
    expression_struct expr;
    Code::init_expr(&expr);
    Value *last_v = comp_op.derefered.value->get_value_refd_last();
    if (last_v->get_valuetype() == Value::V_FUNCTION) {
      expr.expr = mputprintf(expr.expr, "%s(", last_v->get_refd_fat()
	  ->get_genname_from_scope(my_sb, "start_").c_str());
    } else {
      comp_op.derefered.value->generate_code_expr_mandatory(&expr);
      expr.expr = mputstr(expr.expr, ".start(");
    }
    comp_op.compref->generate_code_expr(&expr);
    if (comp_op.derefered.ap_list2->get_nof_pars() > 0) {
      expr.expr = mputstr(expr.expr, ", ");
      comp_op.derefered.ap_list2->generate_code_noalias(&expr, NULL);
    }
    expr.expr = mputc(expr.expr, ')');
    return Code::merge_free_expr(str, &expr);
  }

  char *Statement::generate_code_compop(char *str, const char *opname)
  {
    expression_struct expr;
    Code::init_expr(&expr);
    if (comp_op.compref) {
      Value *v_last = comp_op.compref->get_value_refd_last();
      if (v_last->get_valuetype() == Value::V_REFD) {
	// the argument is a simple component reference
	v_last->generate_code_expr_mandatory(&expr);
	expr.expr = mputprintf(expr.expr, ".%s()", opname);
      } else {
	bool refers_to_self = false;
	if (v_last->get_valuetype() == Value::V_EXPR) {
	  // the argument is a special component reference (mtc, self, etc.)
	  switch (v_last->get_optype()) {
	  case Value::OPTYPE_COMP_MTC: {
	    Definition *my_def = my_sb->get_my_def();
	    if (my_def && my_def->get_asstype() == Definition::A_TESTCASE)
	      refers_to_self = true;
	    break; }
	  case Value::OPTYPE_COMP_SELF:
	    refers_to_self = true;
	  default:
	    break;
	  }
	}
	if (refers_to_self) {
	  expr.expr = mputprintf(expr.expr, "TTCN_Runtime::%s_execution()",
	    opname);
	} else {
	  expr.expr = mputprintf(expr.expr, "TTCN_Runtime::%s_component(",
	    opname);
	  v_last->generate_code_expr(&expr);
	  expr.expr = mputc(expr.expr, ')');
	}
      }
    } else {
      // the operation refers to all component
      expr.expr = mputprintf(expr.expr,
	"TTCN_Runtime::%s_component(ALL_COMPREF)", opname);
    }
    return Code::merge_free_expr(str, &expr);
  }

  char *Statement::generate_code_configop(char *str, const char *opname)
  {
    expression_struct expr;
    Code::init_expr(&expr);
    expr.expr = mputprintf(expr.expr, "TTCN_Runtime::%s_port(", opname);
    config_op.compref1->generate_code_expr(&expr);
    expr.expr = mputstr(expr.expr, ", ");
    if (config_op.compref1->get_expr_governor(Type::EXPECTED_DYNAMIC_VALUE)) {
      // the component type is known
      // the name of the referred port can be used
      config_op.portref1->generate_code_portref(&expr, my_sb);
      expr.expr = mputstr(expr.expr, ".get_name()");
    } else {
      // the component type is unknown
      // a simple string shall be formed from the port name and array indices
      generate_code_portref(&expr, config_op.portref1);
    }
    expr.expr = mputstr(expr.expr, ", ");
    config_op.compref2->generate_code_expr(&expr);
    expr.expr = mputstr(expr.expr, ", ");
    if (config_op.compref2->get_expr_governor(Type::EXPECTED_DYNAMIC_VALUE)) {
      // the component type is known
      // the name of the referred port can be used
      config_op.portref2->generate_code_portref(&expr, my_sb);
      expr.expr = mputstr(expr.expr, ".get_name()");
    } else {
      // the component type is unknown
      // a simple string shall be formed from the port name and array indices
      generate_code_portref(&expr, config_op.portref2);
    }
    expr.expr = mputc(expr.expr, ')');
    return Code::merge_free_expr(str, &expr);
  }

  char *Statement::generate_code_starttimer(char *str)
  {
    expression_struct expr;
    Code::init_expr(&expr);
    timer_op.timerref->generate_code(&expr);
    expr.expr=mputstr(expr.expr, ".start(");
    if(timer_op.value) timer_op.value->generate_code_expr(&expr);
    expr.expr=mputc(expr.expr, ')');
    str=Code::merge_free_expr(str, &expr);
    return str;
  }

  char *Statement::generate_code_stoptimer(char *str)
  {
    if(!timer_op.timerref) str=mputstr(str, "TIMER::all_stop();\n");
    else {
      expression_struct expr;
      Code::init_expr(&expr);
      timer_op.timerref->generate_code(&expr);
      expr.expr=mputstr(expr.expr, ".stop()");
      str=Code::merge_free_expr(str, &expr);
    }
    return str;
  }

  char *Statement::generate_code_setverdict(char *str)
  {
    expression_struct expr;
    Code::init_expr(&expr);
    expr.expr=mputstr(expr.expr, "TTCN_Runtime::setverdict(");
    setverdict.verdictval->generate_code_expr(&expr);
    if (setverdict.logargs) {
      expr.expr=mputc(expr.expr, ',');
      expression_struct expr_reason;
      Code::init_expr(&expr_reason);
      setverdict.logargs->generate_code_expr(&expr_reason);
      if (expr_reason.preamble)
        expr.preamble = mputprintf(expr.preamble, "%s;\n",
                                   expr_reason.preamble);
      if (expr_reason.postamble)
        expr.postamble = mputprintf(expr.postamble, "%s;\n",
                                    expr_reason.postamble);
      expr.expr = mputprintf(expr.expr, "%s", expr_reason.expr);
      Code::free_expr(&expr_reason);
    }
    expr.expr=mputc(expr.expr, ')');
    str=Code::merge_free_expr(str, &expr);
    return str;
  }

  char *Statement::generate_code_action(char *str)
  {
    str=mputstr(str, "TTCN_Runtime::begin_action();\n");
    if(!logargs) str=mputstr(str, "TTCN_Logger::log_event_str"
                             "(\"<empty action statement>\");\n");
    else str=logargs->generate_code(str);
    str=mputstr(str, "TTCN_Runtime::end_action();\n");
    return str;
  }

  char *Statement::generate_code_testcaseinst(char *str)
  {
    expression_struct expr;
    Code::init_expr(&expr);
    Common::Assignment *testcase = testcase_inst.tcref->get_refd_assignment();
    expr.expr = mputprintf(expr.expr, "%s(",
      testcase->get_genname_from_scope(my_sb, "testcase_").c_str());
    ActualParList *t_aplist = testcase_inst.tcref->get_parlist();
    if (t_aplist->get_nof_pars() > 0) {
      t_aplist->generate_code_alias(&expr, testcase->get_FormalParList(),
        0, false);
      expr.expr = mputstr(expr.expr, ", ");
    }
    if (testcase_inst.timerval) {
      expr.expr = mputstr(expr.expr, "TRUE, ");
      testcase_inst.timerval->generate_code_expr(&expr);
      expr.expr = mputc(expr.expr, ')');
    } else expr.expr = mputstr(expr.expr, "FALSE, 0.0)");
    return Code::merge_free_expr(str, &expr);
  }

  char *Statement::generate_code_execute_refd(char *str)
  {
    expression_struct expr;
    Code::init_expr(&expr);
    Value *last_v = fau_refd.value->get_value_refd_last();
    if (last_v->get_valuetype() == Value::V_TESTCASE) {
      Common::Assignment *testcase = last_v->get_refd_fat();
      expr.expr = mputprintf(expr.expr, "%s(",
	testcase->get_genname_from_scope(my_sb, "testcase_").c_str());
      execute_refd.ap_list2->generate_code_alias(&expr,
	testcase->get_FormalParList(), 0, false);
    } else {
      execute_refd.value->generate_code_expr_mandatory(&expr);
      expr.expr = mputstr(expr.expr, ".execute(");
      execute_refd.ap_list2->generate_code_alias(&expr, 0, 0, false);
    }
    if (execute_refd.ap_list2->get_nof_pars() > 0)
      expr.expr = mputstr(expr.expr, ", ");
    if (execute_refd.timerval) {
      expr.expr = mputstr(expr.expr, "TRUE, ");
      execute_refd.timerval->generate_code_expr(&expr);
      expr.expr = mputc(expr.expr, ')');
    } else expr.expr = mputstr(expr.expr, "FALSE, 0.0)");
    return Code::merge_free_expr(str,&expr);
  }

  void Statement::generate_code_expr_receive(expression_struct *expr,
    const char *opname)
  {
    if (port_op.portref) {
      // The operation refers to a specific port.
      port_op.portref->generate_code(expr);
      expr->expr = mputprintf(expr->expr, ".%s(", opname);
      if (port_op.r.rcvpar) {
        // The receive parameter is present.
        if (use_runtime_2 && TypeConv::needs_conv_redir(port_op.r.rcvpar,
            port_op.r.redirect.value)) {
          // Don't change the first parameter.  Otherwise it won't receive
          // anything.  The only thing we need is a temporary to save the
          // result and a conversion at the end.
          TypeConv::gen_conv_code_redir(expr, port_op.r.rcvpar,
                                        port_op.r.redirect.value);
        } else {
          port_op.r.rcvpar->generate_code(expr);
          expr->expr = mputstr(expr->expr, ", ");
          if (port_op.r.redirect.value) {
            // Value redirect is also present.
            expr->expr = mputstr(expr->expr, "&(");
            port_op.r.redirect.value->generate_code(expr);
            expr->expr = mputc(expr->expr, ')');
          } else expr->expr = mputstr(expr->expr, "NULL");
        }
        expr->expr = mputstr(expr->expr, ", ");
      }
    } else {
      // the operation refers to any port
      expr->expr = mputprintf(expr->expr, "PORT::any_%s(", opname);
    }
    generate_code_expr_fromclause(expr);
    expr->expr = mputstr(expr->expr, ", ");
    generate_code_expr_senderredirect(expr);
    expr->expr = mputc(expr->expr, ')');
  }

  void Statement::generate_code_expr_getcall(expression_struct *expr,
    const char *opname)
  {
    if (port_op.portref) {
      // the operation refers to a specific port
      port_op.portref->generate_code(expr);
      expr->expr = mputprintf(expr->expr, ".%s(", opname);
      if (port_op.r.rcvpar) {
	// the signature template is present
        port_op.r.rcvpar->generate_code(expr);
	expr->expr = mputstr(expr->expr, ", ");
	generate_code_expr_fromclause(expr);
	// a temporary object is needed for parameter redirect
	Type *signature = port_op.r.rcvpar->get_Template()->get_my_governor();
	expr->expr = mputprintf(expr->expr, ", %s_call_redirect(",
	  signature->get_genname_value(my_sb).c_str());
	if (port_op.r.redirect.param)
	  port_op.r.redirect.param->generate_code(expr);
	expr->expr = mputstr(expr->expr, "), ");
	generate_code_expr_senderredirect(expr);
      } else {
	// the signature parameter is not present
	generate_code_expr_fromclause(expr);
	expr->expr = mputstr(expr->expr, ", ");
	generate_code_expr_senderredirect(expr);
      }
    } else {
      // the operation refers to any port
      expr->expr = mputprintf(expr->expr, "PORT::any_%s(", opname);
      generate_code_expr_fromclause(expr);
      expr->expr = mputstr(expr->expr, ", ");
      generate_code_expr_senderredirect(expr);
    }
    expr->expr=mputc(expr->expr, ')');
  }

  void Statement::generate_code_expr_getreply(expression_struct *expr,
    const char *opname)
  {
    if (port_op.portref) {
      // the operation refers to a specific port
      port_op.portref->generate_code(expr);
      expr->expr = mputprintf(expr->expr, ".%s(", opname);
      if (port_op.r.rcvpar) {
	// the signature template is present
        port_op.r.rcvpar->generate_code(expr);
	Type *signature = port_op.r.rcvpar->get_Template()->get_my_governor();
	Type *return_type =
	  signature->get_type_refd_last()->get_signature_return_type();
	if (return_type) {
	  expr->expr = mputstr(expr->expr, ".set_value_template(");
	  if (port_op.r.getreply_valuematch) {
	    // the value match is also present
	    port_op.r.getreply_valuematch->generate_code(expr);
	  } else {
	    // the value match is not present
	    // we must substitute it with ? in the signature template
	    expr->expr = mputprintf(expr->expr, "%s(ANY_VALUE)",
	      return_type->get_genname_template(my_sb).c_str());
	  }
	  expr->expr = mputc(expr->expr, ')');
	}
	expr->expr = mputstr(expr->expr, ", ");
	generate_code_expr_fromclause(expr);
	// a temporary object is needed for value and parameter redirect
	expr->expr = mputprintf(expr->expr, ", %s_reply_redirect(",
	  signature->get_genname_value(my_sb).c_str());
	if (return_type) {
	  // the first argument of the constructor must contain
	  // the value redirect
	  if (port_op.r.redirect.value) {
	    expr->expr = mputstr(expr->expr, "&(");
	    port_op.r.redirect.value->generate_code(expr);
	    expr->expr = mputc(expr->expr, ')');
	  } else expr->expr = mputstr(expr->expr, "NULL");
	  if (port_op.r.redirect.param) expr->expr = mputstr(expr->expr, ", ");
	}
	if (port_op.r.redirect.param)
	  port_op.r.redirect.param->generate_code(expr);
	expr->expr = mputstr(expr->expr, "), ");
	generate_code_expr_senderredirect(expr);
      } else {
	// the signature template is not present
	generate_code_expr_fromclause(expr);
	expr->expr = mputstr(expr->expr, ", ");
	generate_code_expr_senderredirect(expr);
      }
    } else {
      // the operation refers to any port
      expr->expr = mputprintf(expr->expr, "PORT::any_%s(", opname);
      generate_code_expr_fromclause(expr);
      expr->expr = mputstr(expr->expr, ", ");
      generate_code_expr_senderredirect(expr);
    }
    expr->expr = mputc(expr->expr, ')');
  }

  void Statement::generate_code_expr_catch(expression_struct *expr)
  {
    if (port_op.portref) {
      // the operation refers to a specific port
      if (port_op.r.ctch.timeout) {
	// the operation catches the timeout exception
	expr->expr = mputstr(expr->expr, "call_timer.timeout()");
	return;
      }
      port_op.portref->generate_code(expr);
      expr->expr = mputprintf(expr->expr, ".%s(",
	statementtype == S_CHECK_CATCH ? "check_catch" : "get_exception");
      if (port_op.r.ctch.signature_ref) {
	// the signature reference and the exception template is present
	expr->expr = mputprintf(expr->expr, "%s_exception_template(",
	  port_op.r.ctch.signature->get_genname_value(my_sb).c_str());
	port_op.r.rcvpar->generate_code(expr);
	expr->expr = mputstr(expr->expr, ", ");
	if (port_op.r.redirect.value) {
	  // value redirect is also present
	  expr->expr = mputstr(expr->expr, "&(");
	  port_op.r.redirect.value->generate_code(expr);
	  expr->expr = mputc(expr->expr, ')');
	} else expr->expr = mputstr(expr->expr, "NULL");
	expr->expr = mputstr(expr->expr, "), ");
      }
    } else {
      // the operation refers to any port
      expr->expr = mputprintf(expr->expr, "PORT::%s(",
	statementtype == S_CHECK_CATCH ? "any_check_catch" : "any_catch");
    }
    generate_code_expr_fromclause(expr);
    expr->expr = mputstr(expr->expr, ", ");
    generate_code_expr_senderredirect(expr);
    expr->expr = mputc(expr->expr, ')');
  }

  void Statement::generate_code_expr_check(expression_struct *expr)
  {
    if (port_op.portref) {
      // the operation refers to a specific port
      port_op.portref->generate_code(expr);
      expr->expr = mputstr(expr->expr, ".check");
    } else {
      // the operation refers to any port
      expr->expr = mputstr(expr->expr, "PORT::any_check");
    }
    expr->expr = mputc(expr->expr, '(');
    generate_code_expr_fromclause(expr);
    expr->expr = mputstr(expr->expr, ", ");
    generate_code_expr_senderredirect(expr);
    expr->expr = mputc(expr->expr, ')');
  }

  void Statement::generate_code_expr_done(expression_struct *expr)
  {
    if (comp_op.compref) {
      if (comp_op.donereturn.donematch) {
	// value returning done
  // figure out what type the done() function belongs to
  Type *t = comp_op.donereturn.donematch
    ->get_expr_governor(Type::EXPECTED_TEMPLATE);
  if (!t) FATAL_ERROR("Statement::generate_code_expr_done()");
  while (t->is_ref() && !t->has_done_attribute())
    t = t->get_type_refd();
  if (!t->has_done_attribute())
    FATAL_ERROR("Statement::generate_code_expr_done()");
  // determine whether the done() function is in the same module
  Common::Module *t_mod = t->get_my_scope()->get_scope_mod_gen();
  if (t_mod != my_sb->get_scope_mod_gen()) {
    expr->expr = mputprintf(expr->expr, "%s::",
      t_mod->get_modid().get_name().c_str());
  }
	expr->expr = mputstr(expr->expr, "done(");
	comp_op.compref->generate_code_expr(expr);
	expr->expr = mputstr(expr->expr, ", ");
	comp_op.donereturn.donematch->generate_code(expr);
	expr->expr = mputstr(expr->expr, ", ");
	if (comp_op.donereturn.redirect) {
	  // value redirect is present
	  expr->expr = mputstr(expr->expr, "&(");
	  comp_op.donereturn.redirect->generate_code(expr);
	  expr->expr = mputc(expr->expr, ')');
	} else {
	  // value redirect is omitted
	  expr->expr = mputstr(expr->expr, "NULL");
	}
	expr->expr = mputc(expr->expr, ')');
      } else {
	// simple done
	comp_op.compref->generate_code_expr_mandatory(expr);
	expr->expr = mputstr(expr->expr, ".done()");
      }
    } else if (comp_op.any_or_all == C_ANY) {
      // any component.done
      expr->expr = mputstr(expr->expr,
	"TTCN_Runtime::component_done(ANY_COMPREF)");
    } else {
      // all component.done
      expr->expr = mputstr(expr->expr,
	"TTCN_Runtime::component_done(ALL_COMPREF)");
    }
  }

  void Statement::generate_code_expr_killed(expression_struct *expr)
  {
    if (comp_op.compref) {
      // compref.killed
      comp_op.compref->generate_code_expr_mandatory(expr);
      expr->expr = mputstr(expr->expr, ".killed()");
    } else if (comp_op.any_or_all == C_ANY) {
      // any component.killed
      expr->expr = mputstr(expr->expr,
	"TTCN_Runtime::component_killed(ANY_COMPREF)");
    } else {
      // all component.killed
      expr->expr = mputstr(expr->expr,
	"TTCN_Runtime::component_killed(ALL_COMPREF)");
    }
  }

  void Statement::generate_code_expr_timeout(expression_struct *expr)
  {
    if (timer_op.timerref) {
      timer_op.timerref->generate_code(expr);
      expr->expr=mputstr(expr->expr, ".timeout()");
    } else expr->expr = mputstr(expr->expr, "TIMER::any_timeout()");
  }

  void Statement::generate_code_expr_sendpar(expression_struct *expr)
  {
    Template *templ_body = port_op.s.sendpar->get_Template();
    if (!port_op.s.sendpar->get_DerivedRef() &&
	templ_body->get_templatetype() == Template::SPECIFIC_VALUE) {
      // the send parameter is a value: optimization is possible
      Value *t_val = templ_body->get_specific_value();
      bool cast_needed = t_val->explicit_cast_needed();
      if (cast_needed) {
	// the ambiguous C++ expression is converted to the value class
	expr->expr = mputprintf(expr->expr, "%s(",
	  t_val->get_my_governor()->get_genname_value(my_sb).c_str());
      }
      t_val->generate_code_expr_mandatory(expr);
      if (cast_needed) expr->expr = mputc(expr->expr, ')');
    } else {
      // the send parameter is a real template: optimization is not possible
      port_op.s.sendpar->generate_code(expr);
    }
  }

  void Statement::generate_code_expr_fromclause(expression_struct *expr)
  {
    if (port_op.r.fromclause) {
      // the from clause is present: trivial case
      port_op.r.fromclause->generate_code(expr);
    } else if (port_op.r.redirect.sender) {
      // from clause is omitted, but sender redirect is present
      Type *t_var_type = port_op.r.redirect.sender->chk_variable_ref();
      if (!t_var_type)
	FATAL_ERROR("Statement::generate_code_expr_fromclause()");
      if (t_var_type->get_type_refd_last()->get_typetype() ==
	  Type::T_COMPONENT) {
	// the variable can store a component reference
	expr->expr = mputstr(expr->expr, "any_compref");
      } else {
	// the variable can store an address value
	expr->expr = mputprintf(expr->expr, "%s(ANY_VALUE)",
	  t_var_type->get_genname_template(my_sb).c_str());
      }
    } else {
      // neither from clause nor sender redirect is present
      // the operation cannot refer to address type
      expr->expr = mputstr(expr->expr, "any_compref");
    }
  }

  void Statement::generate_code_expr_senderredirect(expression_struct *expr)
  {
    if (port_op.r.redirect.sender) {
      expr->expr = mputstr(expr->expr, "&(");
      port_op.r.redirect.sender->generate_code(expr);
      expr->expr = mputc(expr->expr, ')');
    } else expr->expr = mputstr(expr->expr, "NULL");
  }

  void Statement::generate_code_portref(expression_struct *expr,
    Reference *p_ref)
  {
    // make a backup of the current expression
    char *expr_backup = expr->expr;
    // build the equivalent of p_ref in expr->expr
    expr->expr = mprintf("\"%s\"", p_ref->get_id()->get_dispname().c_str());
    FieldOrArrayRefs *t_subrefs = p_ref->get_subrefs();
    if (t_subrefs) {
      // array indices are present
      for (size_t i = 0; i < t_subrefs->get_nof_refs(); i++) {
	FieldOrArrayRef *t_ref = t_subrefs->get_ref(i);
	if (t_ref->get_type() != FieldOrArrayRef::ARRAY_REF)
	  FATAL_ERROR("Statement::generate_code_portref()");
	// transform expr->expr: XXXX -> get_port_name(XXXX, index)
	char *tmp = expr->expr;
	expr->expr = mcopystr("get_port_name(");
	expr->expr = mputstr(expr->expr, tmp);
	Free(tmp);
	expr->expr = mputstr(expr->expr, ", ");
	t_ref->get_val()->generate_code_expr(expr);
	expr->expr = mputc(expr->expr, ')');
      }
    }
    // now expr->expr contains the equivalent of p_ref
    // append it to the original expression and restore the result
    expr_backup = mputstr(expr_backup, expr->expr);
    Free(expr->expr);
    expr->expr = expr_backup;
  }

  void Statement::set_parent_path(WithAttribPath* p_path) {
    switch (statementtype) {
        case S_DEF:
          def->set_parent_path(p_path);
          break;
        case S_BLOCK:
          block->set_parent_path(p_path);
          break;
        case S_IF:
          if_stmt.ics->set_parent_path(p_path);
          if (if_stmt.elseblock)
            if_stmt.elseblock->set_parent_path(p_path);
          break;
        case S_SELECT:
          select.scs->set_parent_path(p_path);
          break;
        case S_FOR:
          loop.block->set_parent_path(p_path);
          break;
        case S_WHILE:
        case S_DOWHILE:
          loop.block->set_parent_path(p_path);
          break;
        case S_ASSIGNMENT:
        case S_LOG:
        case S_ACTION:
        case S_LABEL:
        case S_GOTO:
        case S_ERROR:
        case S_BREAK:
        case S_CONTINUE:
        case S_STOP_EXEC:
        case S_STOP_TESTCASE:
        case S_REPEAT:
        case S_START_UNDEF:
        case S_STOP_UNDEF:
        case S_UNKNOWN_INSTANCE:
        case S_FUNCTION_INSTANCE:
        case S_ALTSTEP_INSTANCE:
        case S_ACTIVATE:
        case S_ALT:
        case S_INTERLEAVE:
        case S_RETURN:
        case S_DEACTIVATE:
        case S_SEND:
        case S_CALL:
        case S_REPLY:
        case S_RAISE:
        case S_RECEIVE:
        case S_CHECK_RECEIVE:
        case S_TRIGGER:
        case S_GETCALL:
        case S_CHECK_GETCALL:
        case S_GETREPLY:
        case S_CHECK_GETREPLY:
        case S_CATCH:
        case S_CHECK_CATCH:
        case S_CHECK:
        case S_CLEAR:
        case S_START_PORT:
        case S_STOP_PORT:
        case S_HALT:
        case S_START_COMP:
        case S_START_COMP_REFD:
        case S_STOP_COMP:
        case S_KILL:
        case S_KILLED:
        case S_DONE:
        case S_CONNECT:
        case S_MAP:
        case S_DISCONNECT:
        case S_UNMAP:
        case S_START_TIMER:
        case S_STOP_TIMER:
        case S_TIMEOUT:
        case S_SETVERDICT:
        case S_TESTCASE_INSTANCE:
        case S_TESTCASE_INSTANCE_REFD:
        case S_ACTIVATE_REFD:
        case S_UNKNOWN_INVOKED:
        case S_FUNCTION_INVOKED:
        case S_ALTSTEP_INVOKED:
        case S_STRING2TTCN:
          break;
        default:
          FATAL_ERROR("Statement::set_parent_path()");
        }
  }

  // =================================
  // ===== Assignment
  // =================================

  Assignment::Assignment(Reference *p_ref, Template *p_templ)
    : asstype(ASS_UNKNOWN), ref(p_ref), templ(p_templ), self_ref(false),
      template_restriction(TR_NONE), gen_restriction_check(false)
  {
    if(!ref || !templ) FATAL_ERROR("Ttcn::Assignment::Assignment");
  }

  Assignment::Assignment(Reference *p_ref, Value *p_val)
    : asstype(ASS_VAR), ref(p_ref), val(p_val), self_ref(false),
      template_restriction(TR_NONE), gen_restriction_check(false)
  {
    if(!ref || !val) FATAL_ERROR("Ttcn::Assignment::Assignment");
  }

  Assignment::~Assignment()
  {
    switch(asstype) {
    case ASS_UNKNOWN:
    case ASS_TEMPLATE:
      delete ref;
      delete templ;
      break;
    case ASS_VAR:
      delete ref;
      delete val;
      break;
    case ASS_ERROR:
      break;
    default:
      FATAL_ERROR("Ttcn::Assignment::~Assignment()");
    } // switch
  }

  Assignment *Assignment::clone() const
  {
    FATAL_ERROR("Assignment::clone");
  }

  void Assignment::set_my_scope(Scope *p_scope)
  {
    switch(asstype) {
    case ASS_UNKNOWN:
    case ASS_TEMPLATE:
      ref->set_my_scope(p_scope);
      templ->set_my_scope(p_scope);
      break;
    case ASS_VAR:
      ref->set_my_scope(p_scope);
      val->set_my_scope(p_scope);
      break;
    case ASS_ERROR:
      break;
    default:
      FATAL_ERROR("Ttcn::Assignment::set_my_scope()");
    } // switch
  }

  void Assignment::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    switch(asstype) {
    case ASS_UNKNOWN:
    case ASS_TEMPLATE:
      ref->set_fullname(p_fullname);
      templ->set_fullname(p_fullname);
      break;
    case ASS_VAR:
      ref->set_fullname(p_fullname);
      val->set_fullname(p_fullname);
      break;
    case ASS_ERROR:
      break;
    default:
      FATAL_ERROR("Ttcn::Assignment::set_fullname()");
    } // switch
  }

  void Assignment::dump(unsigned int level) const
  {
    // warning, ref is not always set (e.g. ASS_ERROR)
    switch (asstype) {
    case ASS_VAR:
      ref->dump(level+1);
      val->dump(level+1);
      break;

    case ASS_ERROR:
      DEBUG(level, "*** ERROR ***");
      break;

    case ASS_UNKNOWN:
      DEBUG(level, "*** UNKNOWN ***");
      break;

    case ASS_TEMPLATE:
      ref->dump(level+1);
      templ->dump(level+1);
      break;
    }
  }

  void Assignment::chk_unknown_ass()
  {
    Common::Assignment *t_ass = ref->get_refd_assignment();
    if (!t_ass) goto error;
    switch (t_ass->get_asstype()) {
    case Common::Assignment::A_ERROR:
      goto error;
    case Common::Assignment::A_PAR_VAL_IN:
      t_ass->use_as_lvalue(*ref);
      // no break
    case Common::Assignment::A_VAR:
    case Common::Assignment::A_PAR_VAL_OUT:
    case Common::Assignment::A_PAR_VAL_INOUT:
      if (templ->is_Value()) {
	Value *t_val = templ->get_Value();
	delete templ;
	val = t_val;
	asstype = ASS_VAR;
	chk_var_ass();
      } else {
	templ->error("A template body with matching symbols cannot be"
                     " assigned to a variable");
	goto error;
      }
      break;
    case Common::Assignment::A_PAR_TEMPL_IN:
      t_ass->use_as_lvalue(*ref);
      // no break
    case Common::Assignment::A_VAR_TEMPLATE: {
      Type::typetype_t tt = t_ass->get_Type()->get_typetype();
      switch (tt) {
        case Type::T_BSTR:
        case Type::T_BSTR_A:
        case Type::T_HSTR:
        case Type::T_OSTR:
        case Type::T_CSTR:
        case Type::T_USTR:
        case Type::T_UTF8STRING:
        case Type::T_NUMERICSTRING:
        case Type::T_PRINTABLESTRING:
        case Type::T_TELETEXSTRING:
        case Type::T_VIDEOTEXSTRING:
        case Type::T_IA5STRING:
        case Type::T_GRAPHICSTRING:
        case Type::T_VISIBLESTRING:
        case Type::T_GENERALSTRING:
        case Type::T_UNIVERSALSTRING:
        case Type::T_BMPSTRING:
        case Type::T_UTCTIME:
        case Type::T_GENERALIZEDTIME:
        case Type::T_OBJECTDESCRIPTOR: {
          Ttcn::FieldOrArrayRefs *subrefs = ref->get_subrefs();
          if (!subrefs) break;
          size_t nof_subrefs = subrefs->get_nof_refs();
          if (nof_subrefs > 0) {
            Ttcn::FieldOrArrayRef *last_ref = subrefs
              ->get_ref(nof_subrefs - 1);
            if (last_ref->get_type() == Ttcn::FieldOrArrayRef::ARRAY_REF) {
              if (!templ->is_Value()) {
                templ->error("A template body with matching symbols cannot be "
                             "assigned to an element of a template variable");
                goto error;
              }
            }
          }
          break; }
        default:
          break;
      }
    }
    case Common::Assignment::A_PAR_TEMPL_OUT:
    case Common::Assignment::A_PAR_TEMPL_INOUT:
      asstype = ASS_TEMPLATE;
      chk_template_ass();
      break;
    default:
      ref->error("Reference to a variable or template variable was expected "
                 "instead of %s", t_ass->get_description().c_str());
      goto error;
    }
    return;
  error:
    delete ref;
    delete templ;
    asstype = ASS_ERROR;
    return;
  }

  void Assignment::chk_var_ass()
  {
    Common::Assignment *lhs = ref->get_refd_assignment();
    Type *var_type = lhs->get_Type();
    FieldOrArrayRefs *subrefs = ref->get_subrefs();
    Type *type =
      var_type->get_field_type(subrefs, Type::EXPECTED_DYNAMIC_VALUE);
    if (!type) goto error;
    val->set_my_governor(type);
    type->chk_this_value_ref(val);
    if (val->get_value_refd_last()->get_valuetype() == Value::V_OMIT) {
      Identifier *field_id = 0;
      if (subrefs) field_id = subrefs->remove_last_field();
      if (!field_id) {
        val->error("Omit value can be assigned to an optional field of "
                   "a record or set value only");
        goto error;
      }
      Type *base_type = var_type
        ->get_field_type(subrefs, Type::EXPECTED_DYNAMIC_VALUE);
      // Putting field_id back to subrefs.
      subrefs->add(new FieldOrArrayRef(field_id));
      base_type = base_type->get_type_refd_last();
      switch (base_type->get_typetype()) {
      case Type::T_ERROR:
        goto error;
      case Type::T_SEQ_A:
      case Type::T_SEQ_T:
      case Type::T_SET_A:
      case Type::T_SET_T:
        break;
      default:
        val->error("Omit value can be assigned to an optional field of "
                   "a record or set value only");
        goto error;
      }
      if (!base_type->get_comp_byName(*field_id)->get_is_optional()) {
        val->error("Assignment of `omit' to mandatory field `%s' of type "
                   "`%s'", field_id->get_dispname().c_str(),
                   base_type->get_typename().c_str());
        goto error;
      }
    } else {
      bool is_string_element = subrefs && subrefs->refers_to_string_element();
      self_ref |= type->chk_this_value(val, lhs, Type::EXPECTED_DYNAMIC_VALUE,
        INCOMPLETE_ALLOWED, OMIT_NOT_ALLOWED,
        (is_string_element ? NO_SUB_CHK : SUB_CHK), NOT_IMPLICIT_OMIT,
        (is_string_element ? IS_STR_ELEM : NOT_STR_ELEM));
      if (is_string_element) {
        // The length of RHS value shall be 1.
        Value *v_last = val->get_value_refd_last();
        switch (type->get_type_refd_last()->get_typetype()) {
        case Type::T_BSTR:
        case Type::T_BSTR_A:
          if (v_last->get_valuetype() != Value::V_BSTR) v_last = 0;
          break;
        case Type::T_HSTR:
          if (v_last->get_valuetype() != Value::V_HSTR) v_last = 0;
          break;
        case Type::T_OSTR:
          if (v_last->get_valuetype() != Value::V_OSTR) v_last = 0;
          break;
        case Type::T_CSTR:
        case Type::T_NUMERICSTRING:
        case Type::T_PRINTABLESTRING:
        case Type::T_IA5STRING:
        case Type::T_VISIBLESTRING:
        case Type::T_UTCTIME:
        case Type::T_GENERALIZEDTIME:
          if (v_last->get_valuetype() != Value::V_CSTR) v_last = 0;
          break;
        case Type::T_USTR:
        case Type::T_UTF8STRING:
        case Type::T_TELETEXSTRING:
        case Type::T_VIDEOTEXSTRING:
        case Type::T_GRAPHICSTRING:
        case Type::T_GENERALSTRING:
        case Type::T_UNIVERSALSTRING:
        case Type::T_BMPSTRING:
        case Type::T_OBJECTDESCRIPTOR:
          if (v_last->get_valuetype() != Value::V_USTR) v_last = 0;
          break;
        default:
          v_last = 0;
        }
        if (v_last) {
          size_t string_len = v_last->get_val_strlen();
          if (string_len != 1) {
            val->error("The length of the string to be assigned to a string "
                       "element of type `%s' should be 1 instead of %lu",
                       type->get_typename().c_str(),
                       (unsigned long)string_len);
            goto error;
          }
        }
      }
    }
    return;
  error:
    delete ref;
    delete val;
    asstype = ASS_ERROR;
    return;
  }

  void Assignment::chk_template_ass()
  {
    FieldOrArrayRefs *subrefs = ref->get_subrefs();
    Common::Assignment *lhs = ref->get_refd_assignment();
    if (!lhs) FATAL_ERROR("Ttcn::Assignment::chk_template_ass()");
    Type *type = lhs->get_Type()->
      get_field_type(subrefs, Type::EXPECTED_DYNAMIC_VALUE);
    if (!type) goto error;
    if (lhs->get_asstype() != Common::Assignment::A_VAR_TEMPLATE &&
        subrefs && subrefs->refers_to_string_element()) {
      ref->error("It is not allowed to index template strings");
      goto error;
    }
    templ->set_my_governor(type);

    templ->flatten(false);

    type->chk_this_template_ref(templ);
    self_ref |= type->chk_this_template_generic(templ, INCOMPLETE_ALLOWED,
      OMIT_ALLOWED, ANY_OR_OMIT_ALLOWED, SUB_CHK, NOT_IMPLICIT_OMIT, lhs);
    chk_template_restriction();
    return;
  error:
    delete ref;
    delete templ;
    asstype = ASS_ERROR;
    return;
  }

  void Assignment::chk()
  {
    switch(asstype) {
    case ASS_UNKNOWN:
      chk_unknown_ass();
      break;
    case ASS_VAR:
      chk_var_ass();
      break;
    case ASS_TEMPLATE:
      chk_template_ass();
      break;
    case ASS_ERROR:
      break;
    default:
      FATAL_ERROR("Ttcn::Assignment::chk()");
    } // switch
  }

  void Assignment::set_code_section(
    GovernedSimple::code_section_t p_code_section)
  {
    switch(asstype) {
    case ASS_VAR:
      ref->set_code_section(p_code_section);
      val->set_code_section(p_code_section);
      break;
    case ASS_TEMPLATE:
      ref->set_code_section(p_code_section);
      templ->set_code_section(p_code_section);
      break;
    case ASS_UNKNOWN:
    case ASS_ERROR:
      break;
    default:
      FATAL_ERROR("Ttcn::Assignment::set_code_section()");
    } // switch
  }

  void Assignment::chk_template_restriction()
  {
    if (asstype!=ASS_TEMPLATE)
      FATAL_ERROR("Ttcn::Assignment::chk_template_restriction()");
    Common::Assignment *t_ass = ref->get_refd_assignment();
    if (!t_ass) FATAL_ERROR("Ttcn::Assignment::chk_template_restriction()");
    switch (t_ass->get_asstype()) {
    case Common::Assignment::A_VAR_TEMPLATE: {
      Def_Var_Template* dvt = dynamic_cast<Def_Var_Template*>(t_ass);
      if (!dvt) FATAL_ERROR("Ttcn::Assignment::chk_template_restriction()");
      template_restriction = dvt->get_template_restriction();
    } break;
    case Common::Assignment::A_PAR_TEMPL_IN:
    case Common::Assignment::A_PAR_TEMPL_OUT:
    case Common::Assignment::A_PAR_TEMPL_INOUT: {
      FormalPar* fp = dynamic_cast<FormalPar*>(t_ass);
      if (!fp) FATAL_ERROR("Ttcn::Assignment::chk_template_restriction()");
      template_restriction = fp->get_template_restriction();
    } break;
    default:
      template_restriction = TR_NONE;
    }
    // transform the restriction if this is a subfield
    template_restriction = Template::get_sub_restriction(template_restriction, ref);
    // check the template restriction
    gen_restriction_check =
      templ->chk_restriction("template", template_restriction);
  }

  char *Assignment::generate_code(char *str)
  {
    FieldOrArrayRefs *t_subrefs = ref->get_subrefs();
    const bool rhs_copied = self_ref;
    switch (asstype) {
    case ASS_VAR: {
      const string& rhs_copy = val->get_temporary_id();
      string rhs_ref = rhs_copy;
      if (rhs_copied /*&& val->get_valuetype() == Value::V_CHOICE*/) {
        if (val->get_my_governor()->is_optional_field()) {
          str = mputprintf(str, "{\nOPTIONAL<%s> %s;\n",
            val->get_my_governor()->get_genname_value(val->get_my_scope()).c_str(), rhs_copy.c_str());
          rhs_ref += "()";
        } else {
          str = mputprintf(str, "{\n%s %s;\n",
            val->get_my_governor()->get_genname_value(val->get_my_scope()).c_str(), rhs_copy.c_str());
        }
      }
      bool needs_conv = use_runtime_2 && TypeConv::needs_conv_refd(val);
      if (needs_conv) {
        case3:
        // Most complicated case.  The LHS is saved in a temporary reference,
        // in case we need to access it more than once, e.g:
        // x2[1] := { f1 := ..., f2 := ... } in TTCN-3  becomes
        // rectype& tmp = x2[1]; tmp.f1() = ...; tmp.f2() = ...;
        // This saves having to index x2 more than once.
        const string& tmp_id = val->get_temporary_id();  // For "ref".
        const char *tmp_id_str = tmp_id.c_str();
        const string& type_genname =
          val->get_my_governor()->get_genname_value(val->get_my_scope());
        const char *type_genname_str = type_genname.c_str();
        expression_struct expr;
        Code::init_expr(&expr);
        ref->generate_code(&expr);

        if (rhs_copied) {
          if (needs_conv)
            str = TypeConv::gen_conv_code_refd(str, rhs_ref.c_str(), val);
          else   str = val->generate_code_init(str, rhs_ref.c_str());
        }

        str = mputstr(str, "{\n");
        str = mputstr(str, expr.preamble);
        if (t_subrefs && t_subrefs->refers_to_string_element()) {
          // The LHS is a string element.
          str = mputprintf(str, "%s_ELEMENT %s(%s);\n", type_genname_str,
                           tmp_id_str, expr.expr);
        } else {
          // The LHS is a normal value.
          str = mputprintf(str, "%s& %s = %s; /* 7388 */\n", type_genname_str,
            tmp_id_str, expr.expr);
        }
        str = mputstr(str, expr.postamble);
        // We now have a reference to the LHS. Generate the actual assignment
        if (rhs_copied) {
          str = mputprintf(str, "%s = %s;\n", tmp_id_str, rhs_copy.c_str());
        }
        else {
          if (needs_conv)
            str = TypeConv::gen_conv_code_refd(str, tmp_id_str, val);
          else   str = val->generate_code_init(str, tmp_id_str);
        }
        Code::free_expr(&expr);
        str = mputstr(str, "}\n");
      }
      else {
        if (t_subrefs) {
          if (!val->has_single_expr()) goto case3;
          // C++ equivalent of RHS is a single expression.
          expression_struct expr;
          Code::init_expr(&expr);
          ref->generate_code(&expr);// vu.s()
          if (rhs_copied) {
            str = mputprintf(str, "%s = %s;\n",
              rhs_copy.c_str(), val->get_single_expr().c_str());

            expr.expr = mputprintf(expr.expr, " = %s", rhs_copy.c_str());
          }
          else {
            expr.expr = mputprintf(expr.expr,
              " = %s", val->get_single_expr().c_str());
          }
          str = Code::merge_free_expr(str, &expr);
        }
        else {
          // The LHS is a single identifier.
          const string& rhs_name = ref->get_refd_assignment()
            ->get_genname_from_scope(ref->get_my_scope());
          if (val->can_use_increment(ref)) {
            switch (val->get_optype()) {
            case Value::OPTYPE_ADD:
              str = mputprintf(str, "++%s;\n", rhs_name.c_str());
              break;
            case Value::OPTYPE_SUBTRACT:
              str = mputprintf(str, "--%s;\n", rhs_name.c_str());
              break;
            default:
              FATAL_ERROR("Ttcn::Assignment::generate_code()");
            }
          } else {
            str = val->generate_code_init(str,
              (rhs_copied ? rhs_copy : rhs_name).c_str());

            if (rhs_copied) {
              str = mputprintf(str, "%s = %s;\n", rhs_name.c_str(), rhs_copy.c_str());
            }
          }
        }
      }
      if (rhs_copied) {
        str = mputstr(str, "}\n");
      }
      break; }
    case ASS_TEMPLATE: {
      const string& rhs_copy = templ->get_temporary_id();
      if (rhs_copied /*&& val->get_valuetype() == Value::V_CHOICE*/) {
        str = mputprintf(str, "{\n%s %s;\n",
          templ->get_my_governor()->get_genname_template(templ->get_my_scope()).c_str(), rhs_copy.c_str()//, rhs_copy.c_str()
          );
      }
      bool needs_conv = use_runtime_2 && TypeConv::needs_conv_refd(templ);
      if (needs_conv) { // case 3
        case3t:
        // Most complicated case.  The LHS is saved in a temporary reference.
        const string& tmp_id = templ->get_temporary_id();
        const char *tmp_id_str = tmp_id.c_str();
        expression_struct expr;
        Code::init_expr(&expr);
        ref->generate_code(&expr);

        if (rhs_copied) {
          if (needs_conv)
            str = TypeConv::gen_conv_code_refd(str, rhs_copy.c_str(), templ);
          else str = templ->generate_code_init(str, rhs_copy.c_str());

        }
        str = mputstr(str, "{\n");
        str = mputstr(str, expr.preamble);
        str = mputprintf(str, "%s& %s = %s;\n",
          templ->get_my_governor()->get_genname_template(
            templ->get_my_scope()
            ).c_str(), tmp_id_str, expr.expr);
        str = mputstr(str, expr.postamble);
        if (rhs_copied) {
          str = mputprintf(str, "%s = %s;\n", tmp_id_str, rhs_copy.c_str());
        }
        else {
          if (needs_conv)
            str = TypeConv::gen_conv_code_refd(str, tmp_id_str, templ);
          else {
            if (Common::Type::T_SEQOF == templ->get_my_governor()->get_typetype() ||
                Common::Type::T_ARRAY == templ->get_my_governor()->get_typetype()) {
              str = mputprintf(str, "%s.remove_all_permutations();\n", tmp_id_str);
            }
            str = templ->generate_code_init(str, tmp_id_str);
          }
        }
        Code::free_expr(&expr);

        if (template_restriction != TR_NONE && gen_restriction_check)
          str = Template::generate_restriction_check_code(str,
            tmp_id_str, template_restriction);
        str = mputstr(str, "}\n");
      }
      else { // !needs_conv
        if (t_subrefs) {
          if ((template_restriction == TR_NONE || !gen_restriction_check)
            && templ->has_single_expr()) {
            // C++ equivalent of RHS is a single expression and no restriction
            // check.  Skipped if conversion needed.
            expression_struct expr;
            Code::init_expr(&expr);
            ref->generate_code(&expr);
            if (rhs_copied) {
              str = mputprintf(str, "%s = %s;\n",
                rhs_copy.c_str(), templ->get_single_expr(false).c_str());

              expr.expr = mputprintf(expr.expr, " = %s", rhs_copy.c_str());
            }
            else {
              expr.expr = mputprintf(expr.expr,
                " = %s", templ->get_single_expr(false).c_str());
            }
            str = Code::merge_free_expr(str, &expr); // will add a semicolon
          }
          else goto case3t;
        }
        else {
          // LHS is a single identifier
          const string& rhs_name = ref->get_refd_assignment()
            ->get_genname_from_scope(ref->get_my_scope());
          if (Common::Type::T_SEQOF == templ->get_my_governor()->get_typetype() ||
              Common::Type::T_ARRAY == templ->get_my_governor()->get_typetype()) {
            str = mputprintf(str, "%s.remove_all_permutations();\n", (rhs_copied ? rhs_copy : rhs_name).c_str());
          }
          str = templ->generate_code_init(str,
            (rhs_copied ? rhs_copy : rhs_name).c_str());
          if (rhs_copied) {
            str = mputprintf(str, "%s = %s;\n", rhs_name.c_str(), rhs_copy.c_str());
          }
          if (template_restriction != TR_NONE && gen_restriction_check)
            str = Template::generate_restriction_check_code(str,
              ref->get_refd_assignment()->get_genname_from_scope(
                ref->get_my_scope()
                ).c_str(), template_restriction);
        }
      }
      if (rhs_copied) {
        str = mputstr(str, "}\n");
      }
      break; }
    default:
      FATAL_ERROR("Ttcn::Assignment::generate_code()");
    } // switch
    return str;
  }

  // =================================
  // ===== ParamAssignment
  // =================================

  ParamAssignment::ParamAssignment(Identifier *p_id, Reference *p_ref)
    : Node(), Location(), id(p_id), ref(p_ref)
  {
    if(!id || !ref) FATAL_ERROR("Ttcn::ParamAssignment::ParamAssignment()");
  }

  ParamAssignment::~ParamAssignment()
  {
    delete id;
    delete ref;
  }

  ParamAssignment *ParamAssignment::clone() const
  {
    FATAL_ERROR("ParamAssignment::clone");
  }

  void ParamAssignment::set_my_scope(Scope *p_scope)
  {
    if (!ref) FATAL_ERROR("Ttcn::ParamAssignment::set_my_scope()");
    ref->set_my_scope(p_scope);
  }

  void ParamAssignment::set_fullname(const string& p_fullname)
  {
    if (!ref) FATAL_ERROR("Ttcn::ParamAssignment::set_fullname()");
    ref->set_fullname(p_fullname);
  }

  Reference *ParamAssignment::get_ref() const
  {
    if (!ref) FATAL_ERROR("Ttcn::ParamAssignment::get_ref()");
    return ref;
  }

  Reference *ParamAssignment::steal_ref()
  {
    if (!ref) FATAL_ERROR("Ttcn::ParamAssignment::steal_ref()");
    Reference *ret_val = ref;
    ref = 0;
    return ret_val;
  }

  // =================================
  // ===== ParamAssignments
  // =================================

  ParamAssignments::~ParamAssignments()
  {
    for(size_t i=0; i<parasss.size(); i++)
      delete parasss[i];
    parasss.clear();
  }

  ParamAssignments *ParamAssignments::clone() const
  {
    FATAL_ERROR("ParamAssignments::clone");
  }

  void ParamAssignments::set_my_scope(Scope *p_scope)
  {
    for(size_t i=0; i<parasss.size(); i++)
      parasss[i]->set_my_scope(p_scope);
  }

  void ParamAssignments::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    for(size_t i=0; i<parasss.size(); i++)
      parasss[i]->set_fullname(p_fullname+".parass_"+Int2string(i+1));
  }

  void ParamAssignments::add_parass(ParamAssignment *p_parass)
  {
    if(!p_parass)
      FATAL_ERROR("ParamAssignments::add_parass()");
    parasss.add(p_parass);
  }

  // =================================
  // ===== VariableEntry
  // =================================

  VariableEntry::VariableEntry(Reference *p_ref)
    : Node(), Location(), ref(p_ref)
  {
    if(!ref) FATAL_ERROR("VariableEntry::VariableEntry()");
  }

  VariableEntry::~VariableEntry()
  {
    delete ref;
  }

  VariableEntry *VariableEntry::clone() const
  {
    FATAL_ERROR("VariableEntry::clone");
  }

  void VariableEntry::set_my_scope(Scope *p_scope)
  {
    if(ref) ref->set_my_scope(p_scope);
  }

  void VariableEntry::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    if(ref) ref->set_fullname(p_fullname+".ref");
  }

  // =================================
  // ===== VariableEntries
  // =================================

  VariableEntries::~VariableEntries()
  {
    for(size_t i=0; i<ves.size(); i++)
      delete ves[i];
    ves.clear();
  }

  VariableEntries *VariableEntries::clone() const
  {
    FATAL_ERROR("VariableEntries::clone");
  }

  void VariableEntries::set_my_scope(Scope *p_scope)
  {
    for(size_t i=0; i<ves.size(); i++)
      ves[i]->set_my_scope(p_scope);
  }

  void VariableEntries::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    for(size_t i=0; i<ves.size(); i++)
      ves[i]->set_fullname(p_fullname+".ve_"+Int2string(i+1));
  }

  void VariableEntries::add_ve(VariableEntry *p_ve)
  {
    if(!p_ve)
      FATAL_ERROR("VariableEntries::add_ve()");
    ves.add(p_ve);
  }

  // =================================
  // ===== ParamRedirect
  // =================================

  ParamRedirect::ParamRedirect(ParamAssignments *p_parasss)
    : Node(), Location(), parredirtype(P_ASS), parasss(p_parasss)
  {
    if(!parasss) FATAL_ERROR("ParamRedirect::ParamRedirect()");
  }

  ParamRedirect::ParamRedirect(VariableEntries *p_ves)
    : Node(), Location(), parredirtype(P_VAR), ves(p_ves)
  {
    if(!ves) FATAL_ERROR("ParamRedirect::ParamRedirect()");
  }

  ParamRedirect::~ParamRedirect()
  {
    switch(parredirtype) {
    case P_ASS:
      delete parasss;
      break;
    case P_VAR:
      delete ves;
      break;
    default:
      FATAL_ERROR("ParamRedirect::~ParamRedirect()");
    } // switch
  }

  ParamRedirect *ParamRedirect::clone() const
  {
    FATAL_ERROR("ParamRedirect::clone");
  }

  void ParamRedirect::set_my_scope(Scope *p_scope)
  {
    switch(parredirtype) {
    case P_ASS:
      parasss->set_my_scope(p_scope);
      break;
    case P_VAR:
      ves->set_my_scope(p_scope);
      break;
    default:
      FATAL_ERROR("ParamRedirect::set_my_scope()");
    } // switch
  }

  void ParamRedirect::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    switch(parredirtype) {
    case P_ASS:
      parasss->set_fullname(p_fullname+".parasss");
      break;
    case P_VAR:
      ves->set_fullname(p_fullname+".parvars");
      break;
    default:
      FATAL_ERROR("ParamRedirect::set_fullname()");
    } // switch
  }

  void ParamRedirect::chk_erroneous()
  {
    Error_Context cntxt(this, "In parameter redirect");
    switch(parredirtype) {
    case P_ASS: {
      map<string, ParamAssignment> parass_m;
      for (size_t i = 0; i < parasss->get_nof_parasss(); i++) {
	ParamAssignment *t_parass = parasss->get_parass_byIndex(i);
	const Identifier &t_id = t_parass->get_id();
	const string& name = t_id.get_name();
	const char *dispname_str = t_id.get_dispname().c_str();
	if (parass_m.has_key(name)) {
	  t_parass->error("Duplicate redirect for parameter `%s'",
	    dispname_str);
	  parass_m[name]->note("A variable entry for parameter `%s' is "
	    "already given here", dispname_str);
	} else parass_m.add(name, t_parass);
	Error_Context cntxt2(t_parass, "In redirect for parameter `%s'",
	  dispname_str);
	chk_variable_ref(t_parass->get_ref(), 0);
      }
      parass_m.clear();
      break; }
    case P_VAR:
      for (size_t i = 0; i < ves->get_nof_ves(); i++) {
        VariableEntry *t_ve = ves->get_ve_byIndex(i);
	Error_Context cntxt2(t_ve, "In variable entry #%lu",
         (unsigned long) (i + 1));
	chk_variable_ref(t_ve->get_ref(), 0);
      }
      break;
    default:
      FATAL_ERROR("ParamRedirect::chk_erroneous()");
    } // switch
  }

  void ParamRedirect::chk(Type *p_sig, bool is_out)
  {
    SignatureParamList *t_parlist = p_sig->get_signature_parameters();
    if (t_parlist) {
      Error_Context cntxt(this, "In parameter redirect");
      switch (parredirtype) {
      case P_ASS:
	chk_parasss(p_sig, t_parlist, is_out);
	break;
      case P_VAR:
	chk_ves(p_sig, t_parlist, is_out);
	break;
      default:
	FATAL_ERROR("ParamRedirect::chk()");
      }
    } else {
      error("Parameter redirect cannot be used because signature `%s' "
	"does not have parameters", p_sig->get_typename().c_str());
      chk_erroneous();
    }
  }

  void ParamRedirect::chk_parasss(Type *p_sig, SignatureParamList *p_parlist,
    bool is_out)
  {
    map<string, ParamAssignment> parass_m;
    bool error_flag = false;
    for (size_t i = 0; i < parasss->get_nof_parasss(); i++) {
      ParamAssignment *t_parass = parasss->get_parass_byIndex(i);
      const Identifier &t_id = t_parass->get_id();
      const string& name = t_id.get_name();
      const char *dispname_str = t_id.get_dispname().c_str();
      if (parass_m.has_key(name)) {
	t_parass->error("Duplicate redirect for parameter `%s'",
	  dispname_str);
	parass_m[name]->note("A variable entry for parameter `%s' is "
	  "already given here", dispname_str);
	error_flag = true;
      } else parass_m.add(name, t_parass);
      Error_Context cntxt2(t_parass, "In redirect for parameter `%s'",
	dispname_str);
      if (p_parlist->has_param_withName(t_id)) {
	const SignatureParam *t_par = p_parlist->get_param_byName(t_id);
	SignatureParam::param_direction_t t_dir = t_par->get_direction();
	if (is_out) {
	  if (t_dir == SignatureParam::PARAM_IN) {
	    t_parass->error("Parameter `%s' of signature `%s' has `in' "
	      "direction", dispname_str, p_sig->get_typename().c_str());
	    error_flag = true;
	  }
	} else {
	  if (t_dir == SignatureParam::PARAM_OUT) {
	    t_parass->error("Parameter `%s' of signature `%s' has `out' "
	      "direction", dispname_str, p_sig->get_typename().c_str());
	    error_flag = true;
	  }
	}
	chk_variable_ref(t_parass->get_ref(), t_par->get_type());
      } else {
	t_parass->error("Signature `%s' does not have parameter named `%s'",
	  p_sig->get_typename().c_str(), dispname_str);
	error_flag = true;
	chk_variable_ref(t_parass->get_ref(), 0);
      }
    }
    if (!error_flag) {
      // converting the AssignmentList to VariableList
      VariableEntries *t_ves = new VariableEntries;
      size_t upper_limit = is_out ?
        p_parlist->get_nof_out_params() : p_parlist->get_nof_in_params();
      for (size_t i = 0; i < upper_limit; i++) {
	SignatureParam *t_par = is_out ? p_parlist->get_out_param_byIndex(i)
	  : p_parlist->get_in_param_byIndex(i);
	const string& name = t_par->get_id().get_name();
	if (parass_m.has_key(name))
	  t_ves->add_ve(new VariableEntry(parass_m[name]->steal_ref()));
	else t_ves->add_ve(new VariableEntry);
      }
      delete parasss;
      ves = t_ves;
      parredirtype = P_VAR;
    }
    parass_m.clear();
  }

  void ParamRedirect::chk_ves(Type *p_sig, SignatureParamList *p_parlist,
    bool is_out)
  {
    size_t nof_ves = ves->get_nof_ves();
    size_t nof_pars = is_out ?
      p_parlist->get_nof_out_params() : p_parlist->get_nof_in_params();
    if (nof_ves != nof_pars) {
      error("Too %s variable entries compared to the number of %s/inout "
	"parameters in signature `%s': %lu was expected instead of %lu",
	nof_ves > nof_pars ? "many" : "few", is_out ? "out" : "in",
	p_sig->get_typename().c_str(), (unsigned long) nof_pars,
        (unsigned long) nof_ves);
    }
    for (size_t i = 0; i < nof_ves; i++) {
      VariableEntry *t_ve = ves->get_ve_byIndex(i);
      if (i < nof_pars) {
	SignatureParam *t_par = is_out ? p_parlist->get_out_param_byIndex(i)
	  : p_parlist->get_in_param_byIndex(i);
	Error_Context cntxt(t_ve, "In variable entry #%lu (for parameter `%s')",
	  (unsigned long) (i + 1), t_par->get_id().get_dispname().c_str());
        chk_variable_ref(t_ve->get_ref(), t_par->get_type());
      } else {
	Error_Context cntxt(t_ve, "In variable entry #%lu",
          (unsigned long) (i + 1));
        chk_variable_ref(t_ve->get_ref(), 0);
      }
    }
  }

  void ParamRedirect::chk_variable_ref(Reference *p_ref, Type *p_type)
  {
    if (!p_ref) return;
    Type *t_var_type = p_ref->chk_variable_ref();
    if (p_type && t_var_type && !p_type->is_identical(t_var_type)) {
      p_ref->error("Type mismatch in parameter redirect: "
	"A variable of type `%s' was expected instead of `%s'",
	p_type->get_typename().c_str(),
	t_var_type->get_typename().c_str());
    }
  }

  void ParamRedirect::set_code_section(
    GovernedSimple::code_section_t p_code_section)
  {
    // code can be generated from VariableList only
    switch (parredirtype) {
    case P_ASS:
      break;
    case P_VAR:
      for (size_t i = 0; i < ves->get_nof_ves(); i++) {
	Reference *t_ref = ves->get_ve_byIndex(i)->get_ref();
	if (t_ref) t_ref->set_code_section(p_code_section);
      }
      break;
    default:
      FATAL_ERROR("ParamRedirect::set_code_section()");
    }
  }

  void ParamRedirect::generate_code(expression_struct_t *expr)
  {
    // AssignmentList is converted to VariableList during checking
    if (parredirtype != P_VAR) FATAL_ERROR("ParamRedirect::generate_code()");
    for (size_t i = 0; i < ves->get_nof_ves(); i++) {
      if (i > 0) expr->expr = mputstr(expr->expr, ", ");
      Reference *ref = ves->get_ve_byIndex(i)->get_ref();
      if (ref) {
	// the variable reference is present
	expr->expr = mputstr(expr->expr, "&(");
	ref->generate_code(expr);
	expr->expr = mputc(expr->expr, ')');
      } else expr->expr = mputstr(expr->expr, "NULL");
    }
  }

  // =================================
  // ===== LogArgument
  // =================================

  LogArgument::LogArgument(TemplateInstance *p_ti)
    : logargtype(L_UNDEF)
  {
    if (!p_ti) FATAL_ERROR("LogArgument::LogArgument()");
    ti = p_ti;
  }

  LogArgument::~LogArgument()
  {
    switch (logargtype) {
    case L_ERROR:
      break;
    case L_UNDEF:
    case L_TI:
      delete ti;
      break;
    case L_VAL:
    case L_MATCH:
    case L_MACRO:
      delete val;
      break;
    case L_REF:
      delete ref;
      break;
    case L_STR:
      delete cstr;
      break;
    default:
      FATAL_ERROR("LogArgument::~LogArgument()");
    } // switch
  }

  LogArgument *LogArgument::clone() const
  {
    FATAL_ERROR("LogArgument::clone");
  }

  void LogArgument::set_my_scope(Scope *p_scope)
  {
    switch (logargtype) {
    case L_ERROR:
      break;
    case L_UNDEF:
    case L_TI:
      ti->set_my_scope(p_scope);
      break;
    case L_VAL:
    case L_MATCH:
    case L_MACRO:
      val->set_my_scope(p_scope);
      break;
    case L_REF:
      ref->set_my_scope(p_scope);
      break;
    case L_STR:
      break;
    default:
      FATAL_ERROR("LogArgument::set_my_scope()");
    } // switch
  }

  void LogArgument::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    switch (logargtype) {
    case L_ERROR:
      break;
    case L_UNDEF:
    case L_TI:
      ti->set_fullname(p_fullname);
      break;
    case L_VAL:
    case L_MATCH:
    case L_MACRO:
      val->set_fullname(p_fullname);
      break;
    case L_REF:
      ref->set_fullname(p_fullname);
      break;
    case L_STR:
      break;
    default:
      FATAL_ERROR("LogArgument::set_fullname()");
    } // switch
  }

  const string& LogArgument::get_str() const
  {
    if (logargtype != L_STR) FATAL_ERROR("LogArgument::get_str()");
    return *cstr;
  }

  Value *LogArgument::get_val() const
  {
    switch (logargtype) {
    case L_VAL:
    case L_MATCH:
    case L_MACRO:
      break;
    default:
      FATAL_ERROR("LogArgument::get_val()");
    }
    return val;
  }

  Ref_base *LogArgument::get_ref() const
  {
    if (logargtype != L_REF) FATAL_ERROR("LogArgument::get_ref()");
    return ref;
  }

  TemplateInstance *LogArgument::get_ti() const
  {
    if (logargtype != L_TI) FATAL_ERROR("LogArgument::get_ref()");
    return ti;
  }

  void LogArgument::append_str(const string& p_str)
  {
    if (logargtype != L_STR) FATAL_ERROR("LogArgument::append_str()");
    *cstr += p_str;
  }

  void LogArgument::chk() // determine the proper type of the log argument
  {
    if (logargtype != L_UNDEF) return;
    Template *t_templ = ti->get_Template();
    t_templ = t_templ->get_template_refd_last();
    t_templ->set_lowerid_to_ref();
    if (!ti->get_Type() && !ti->get_DerivedRef() && t_templ->is_Value()) {
      // drop the template instance and keep only the embedded value
      Value *t_val = t_templ->get_Value();
      delete ti;
      logargtype = L_VAL;
      val = t_val;
      chk_val();
    } else {
      // try to obtain the governor of the template instance
      Type *governor = ti->get_expr_governor(Type::EXPECTED_TEMPLATE);
      if (!governor) {
	// the governor is still unknown: an error occurred
	// try to interpret possible enum values as references
	t_templ->set_lowerid_to_ref();
	governor = t_templ->get_expr_governor(Type::EXPECTED_TEMPLATE);
      }
      if (governor) {
	logargtype = L_TI;
	ti->chk(governor);
      } else {
	t_templ->error("Cannot determine the type of the argument");
	delete ti;
	logargtype = L_ERROR;
      }
    }
  }

  void LogArgument::chk_ref()
  {
    Common::Assignment *t_ass = ref->get_refd_assignment();
    if (!t_ass) return;
    Common::Assignment::asstype_t asstype = t_ass->get_asstype();
    switch (asstype) {
    case Common::Assignment::A_FUNCTION_RVAL:
    case Common::Assignment::A_FUNCTION_RTEMP:
    case Common::Assignment::A_EXT_FUNCTION_RVAL:
    case Common::Assignment::A_EXT_FUNCTION_RTEMP:
    	ref->get_my_scope()->chk_runs_on_clause(t_ass, *this, "call");
    case Common::Assignment::A_CONST:
    case Common::Assignment::A_EXT_CONST:
    case Common::Assignment::A_MODULEPAR:
    case Common::Assignment::A_MODULEPAR_TEMP:
    case Common::Assignment::A_TEMPLATE:
    case Common::Assignment::A_VAR:
    case Common::Assignment::A_VAR_TEMPLATE:
    case Common::Assignment::A_PAR_VAL_IN:
    case Common::Assignment::A_PAR_VAL_OUT:
    case Common::Assignment::A_PAR_VAL_INOUT:
    case Common::Assignment::A_PAR_TEMPL_IN:
    case Common::Assignment::A_PAR_TEMPL_OUT:
    case Common::Assignment::A_PAR_TEMPL_INOUT: {
      // the reference points to a value or template-like entity
      // checking sub-references
      FieldOrArrayRefs *subrefs = ref->get_subrefs();
      if (subrefs && t_ass->get_Type()->get_field_type(subrefs,
	Type::EXPECTED_DYNAMIC_VALUE)) {
	// subrefs seems to be correct
	// also checking the presence of referred fields if possible
	if (asstype == Common::Assignment::A_CONST) {
	  ReferenceChain refch(ref, "While searching referenced value");
	  t_ass->get_Value()->get_refd_sub_value(subrefs, 0, false, &refch);
	} else if (asstype == Common::Assignment::A_TEMPLATE) {
	  ReferenceChain refch(ref, "While searching referenced template");
	  t_ass->get_Template()
            ->get_refd_sub_template(subrefs, false, &refch);
	}
      }
      break; }
    case Common::Assignment::A_TIMER:
    case Common::Assignment::A_PORT: {
      ArrayDimensions *t_dims = t_ass->get_Dimensions();
      if (t_dims) t_dims->chk_indices(ref, t_ass->get_assname(), true,
	Type::EXPECTED_DYNAMIC_VALUE);
      else if (ref->get_subrefs()) ref->error("Reference to single %s "
	"cannot have field or array sub-references",
	t_ass->get_description().c_str());
      break; }
    case Common::Assignment::A_PAR_TIMER:
    case Common::Assignment::A_PAR_PORT:
      if (ref->get_subrefs()) ref->error("Reference to %s cannot have "
	"field or array sub-references", t_ass->get_description().c_str());
      break;
    case Common::Assignment::A_FUNCTION:
    case Common::Assignment::A_EXT_FUNCTION:
      ref->error("Reference to a value, template, timer or port was expected "
	"instead of a call of %s, which does not have return type",
	t_ass->get_description().c_str());
      break;
    default:
      ref->error("Reference to a value, template, timer or port was expected "
	"instead of %s", t_ass->get_description().c_str());
    }
  }

  void LogArgument::chk_val()
  {
    // literal enumerated values cannot appear in this context
    val->set_lowerid_to_ref();
    switch (val->get_valuetype()) {
    case Value::V_CSTR: {
      string *t_cstr = new string(val->get_val_str());
      delete val;
      cstr = t_cstr;
      logargtype = L_STR;
      return; }
    case Value::V_REFD: {
      Ref_base *t_ref = val->steal_ttcn_ref_base();
      delete val;
      ref = t_ref;
      logargtype = L_REF;
      chk_ref();
      return; }
    case Value::V_EXPR:
      if (val->get_optype() == Value::OPTYPE_MATCH) logargtype = L_MATCH;
      else logargtype = L_VAL;
      break;
    case Value::V_MACRO:
      logargtype = L_MACRO;
      break;
    default:
      logargtype = L_VAL;
    } // switch
    Type *governor = val->get_expr_governor(Type::EXPECTED_DYNAMIC_VALUE);
    if (governor) {
      val->set_my_governor(governor);
      (void)governor->chk_this_value(val, 0, Type::EXPECTED_DYNAMIC_VALUE,
        INCOMPLETE_NOT_ALLOWED, OMIT_NOT_ALLOWED, SUB_CHK);
      if (logargtype == L_VAL && val->get_valuetype() == Value::V_CSTR
	&& !val->is_unfoldable()) { // string value known at compile time
	string *t_cstr = new string(val->get_val_str());
	delete val;
	cstr = t_cstr;
	logargtype = L_STR;
	return;
      }
      if (logargtype == L_MACRO) {
	switch (val->get_valuetype()) {
	case Value::V_CSTR: {
	  // the macro was evaluated to a charstring value
	  string *t_cstr = new string(val->get_val_str());
	  delete val;
	  cstr = t_cstr;
	  logargtype = L_STR;
	  break; }
	case Value::V_MACRO:
	  // the macro could not be evaluated at compile time
	  // leave logargtype as is
	  break;
	default:
	  // the macro was evaluated to other value (e.g. integer)
	  logargtype = L_VAL;
	}
      }
    } else {
      val->error("Cannot determine the type of the argument");
      delete val;
      logargtype = L_ERROR;
    }
  }

  void LogArgument::dump(unsigned int level) const
  {
    Node::dump(level++);
    switch (logargtype) {
    case L_ERROR:
      DEBUG(level, "*error*");
      break;
    case L_UNDEF:
      DEBUG(level, "*undef*");
      break;
    case L_TI:
      DEBUG(level, "TemplateInstance");
      break;
    case L_VAL:
      val->dump(level);
      break;
    case L_MATCH:
      DEBUG(level, "Match");
      break;
    case L_MACRO:
      DEBUG(level, "Macro");
      break;
    case L_REF:
      DEBUG(level, "Reference");
      break;
    case L_STR:
      DEBUG(level, "String=`%s'", cstr->c_str());
      break;
    default:
      FATAL_ERROR("LogArgument::~LogArgument()");
    } // switch
  }

  void LogArgument::set_code_section(
    GovernedSimple::code_section_t p_code_section)
  {
    switch (logargtype) {
    case L_UNDEF:
    case L_TI:
      ti->set_code_section(p_code_section);
      break;
    case L_VAL:
    case L_MATCH:
    case L_MACRO:
      val->set_code_section(p_code_section);
      break;
    case L_REF:
      ref->set_code_section(p_code_section);
      break;
    case L_STR:
    case L_ERROR:
      break;
    default:
      FATAL_ERROR("LogArgument::set_code_section()");
    } // switch
  }

  char *LogArgument::generate_code_log(char *str)
  {
    expression_struct expr;
    Code::init_expr(&expr);
    generate_code_expr(&expr);
    str = Code::merge_free_expr(str, &expr);
    return str;
  }

  void LogArgument::chk_recursions(ReferenceChain& refch)
  {
    switch (logargtype) {
    case L_UNDEF:
    case L_TI:
      ti->chk_recursions(refch);
      break;
    case L_VAL:
    case L_MATCH:
    case L_MACRO:
      val->chk_recursions(refch);
      break;
    case L_REF: {
      Common::Assignment *ass = ref->get_refd_assignment();
      if (!ass) break;
      refch.add(ass->get_fullname());
    } break;
    case L_STR:
    case L_ERROR:
      break;
    default:
      FATAL_ERROR("LogArgument::chk_recursions()");
    } // switch
  }

  bool LogArgument::has_single_expr()
  {
    switch (logargtype) {
    case L_UNDEF:
    case L_TI:
      return ti->has_single_expr();
    case L_VAL:
    case L_MATCH:
    case L_MACRO:
      return ( val->has_single_expr() && !val->explicit_cast_needed() );
    case L_REF: {
      Common::Assignment *ass = ref->get_refd_assignment();
      switch (ass->get_asstype()) {
      case Common::Assignment::A_CONST:
      case Common::Assignment::A_EXT_CONST:
        return ref->has_single_expr();
      default:
        return false;
      }
      break; }
    case L_STR:
    case L_ERROR:
      return true;
    default:
      FATAL_ERROR("LogArgument::has_single_expr()");
    } // switch
    return true;
  }

  void LogArgument::generate_code_expr(expression_struct *expr)
  {
    switch(logargtype) {
    case L_TI: {
      if (ti->is_only_specific_value()) {
        // use the embedded specific value for code generation
        ti->get_Template()->get_specific_value()->generate_code_log(expr);
      } else {
	ti->generate_code(expr);
	expr->expr = mputstr(expr->expr, ".log()");
      }
      break; }
    case L_VAL:
      val->generate_code_log(expr);
      break;
    case L_MATCH:
      val->generate_code_log_match(expr);
      break;
    case L_MACRO:
      if (val->has_single_expr()) {
	expr->expr = mputprintf(expr->expr, "TTCN_Logger::log_event_str(%s)",
	  val->get_single_expr().c_str());
      } else val->generate_code_log(expr);
      break;
    case L_REF: {
      ref->generate_code_const_ref(expr);
      expr->expr=mputstr(expr->expr, ".log()");
      break;}
    case L_STR: {
      size_t str_len = cstr->size();
      const char *str_ptr = cstr->c_str();
      switch (str_len) {
      case 0:
	// the string is empty: do not generate any code
      case 1:
	// the string has one character: use log_char member
	expr->expr = mputstr(expr->expr, "TTCN_Logger::log_char('");
	expr->expr = Code::translate_character(expr->expr, *str_ptr, false);
	expr->expr = mputstr(expr->expr, "')");
	break;
      default:
	// the string has more characters: use log_event_str member
	expr->expr = mputstr(expr->expr, "TTCN_Logger::log_event_str(\"");
	expr->expr = Code::translate_string(expr->expr, str_ptr);
	expr->expr = mputstr(expr->expr, "\")");
      }
      break; }
    default:
      FATAL_ERROR("LogArgument::generate_code_expr()");
    } // switch
  }

  // =================================
  // ===== LogArguments
  // =================================

  LogArguments::~LogArguments()
  {
    for(size_t i=0; i<logargs.size(); i++) delete logargs[i];
    logargs.clear();
  }

  LogArguments *LogArguments::clone() const
  {
    FATAL_ERROR("LogArguments::clone");
  }

  void LogArguments::add_logarg(LogArgument *p_logarg)
  {
    if(!p_logarg)
      FATAL_ERROR("LogArguments::add_logarg()");
    logargs.add(p_logarg);
  }

  void LogArguments::set_my_scope(Scope *p_scope)
  {
    for(size_t i=0; i<logargs.size(); i++)
      logargs[i]->set_my_scope(p_scope);
  }

  void LogArguments::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    for(size_t i=0; i<logargs.size(); i++)
      logargs[i]->set_fullname(p_fullname+".logargs_"+Int2string(i+1));
  }

  void LogArguments::chk()
  {
    for(size_t i=0; i<logargs.size(); i++)
      logargs[i]->chk();
  }

  void LogArguments::join_strings()
  {
    // points to the previous string argument otherwise it is NULL
    LogArgument *prev_arg = 0;
    for (size_t i = 0; i < logargs.size(); ) {
      LogArgument *arg = logargs[i];
      if (arg->get_type() == LogArgument::L_STR) {
	const string& str = arg->get_str();
	if (str.size() > 0) {
	  // the current argument is a non-empty string
	  if (prev_arg) {
	    // append str to prev_arg and drop arg
	    prev_arg->append_str(str);
	    delete arg;
	    logargs.replace(i, 1);
	    // don't increment i
	  } else {
	    // keep it for the next iteration
	    prev_arg = arg;
	    i++;
	  }
	} else {
	  // the current argument is an empty string
	  // simply drop it unless it is the only argument
	  // note: we must distinguish between log() and log("")
	  if (i > 0 || logargs.size() > 1) {
	    delete arg;
	    logargs.replace(i, 1);
	    // don't increment i
	  } else break;
	}
      } else {
	// the current argument is not a string
	// forget the previous arg
	prev_arg = 0;
	i++;
      }
    }
  }

  void LogArguments::set_code_section(
    GovernedSimple::code_section_t p_code_section)
  {
    for (size_t i = 0; i < logargs.size(); i++)
      logargs[i]->set_code_section(p_code_section);
  }

  char *LogArguments::generate_code(char *str)
  {
    for(size_t i=0; i<logargs.size(); i++)
      str=logargs[i]->generate_code_log(str);
    return str;
  }

  void LogArguments::chk_recursions(ReferenceChain& refch)
  {
    for (size_t i=0; i<logargs.size(); i++) {
      refch.mark_state();
      logargs[i]->chk_recursions(refch);
      refch.prev_state();
    }
  }

  bool LogArguments::has_single_expr()
  {
    bool i_have_single_expr = true;
    for (size_t i=0; i<logargs.size(); i++)
      i_have_single_expr = i_have_single_expr && logargs[i]->has_single_expr();
    return i_have_single_expr;
  }

  void LogArguments::generate_code_expr(expression_struct *expr)
  {
    expr->expr = mputstr(expr->expr, "(TTCN_Logger::begin_event_log2str(),");
    for(size_t i=0; i<logargs.size(); i++) {
      logargs[i]->generate_code_expr(expr);
      expr->expr = mputc(expr->expr, ','); // comma operator
    }
    expr->expr = mputstr(expr->expr, "TTCN_Logger::end_event_log2str())");
  }

  // =================================
  // ===== IfClause
  // =================================

  IfClause::IfClause(Value *p_expr, StatementBlock *p_block)
    : expr(p_expr), block(p_block)
  {
    if(!expr || !block)
      FATAL_ERROR("IfClause::IfClause()");
  }

  IfClause::~IfClause()
  {
    delete expr;
    delete block;
  }

  IfClause *IfClause::clone() const
  {
    FATAL_ERROR("IfClause::clone");
  }

  void IfClause::set_my_scope(Scope *p_scope)
  {
    expr->set_my_scope(p_scope);
    block->set_my_scope(p_scope);
  }

  void IfClause::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    expr->set_fullname(p_fullname+".expr");
    block->set_fullname(p_fullname+".block");
  }

  bool IfClause::has_receiving_stmt() const
  {
    return block->has_receiving_stmt();
  }

  void IfClause::chk(bool& unreach)
  {
    Error_Context cntxt(this, "In if statement");
    if(unreach) warning("Control never reaches this code because of"
                        " previous effective condition(s)");
    expr->chk_expr_bool(Type::EXPECTED_DYNAMIC_VALUE);
    if(!expr->is_unfoldable()) {
      if(expr->get_val_bool()) unreach=true;
      else block->warning("Control never reaches this code because the"
                         " conditional expression evaluates to false");
    }
    block->chk();
  }

  void IfClause::set_code_section(
    GovernedSimple::code_section_t p_code_section)
  {
    expr->set_code_section(p_code_section);
    block->set_code_section(p_code_section);
  }

  char* IfClause::generate_code(char *str, size_t& blockcount,
                                bool& unreach, bool& eachfalse)
  {
    if(unreach) return str;
    if(!expr->is_unfoldable()) {
      if(expr->get_val_bool()) unreach=true;
      else return str;
    }
    if (!eachfalse) str = mputstr(str, "else ");
    if (!unreach) {
      if (!eachfalse) {
	str = mputstr(str, "{\n");
	blockcount++;
      }
      str = expr->update_location_object(str);
      str = expr->generate_code_tmp(str, "if (", blockcount);
      str = mputstr(str, ") ");
    }
    eachfalse = false;
    str=mputstr(str, "{\n");
    str=block->generate_code(str);
    str=mputstr(str, "}\n");
    return str;
  }

  void IfClause::ilt_generate_code(ILT *ilt, const char *end_label,
                                   bool& unreach)
  {
    if(unreach) return;
    if(!expr->is_unfoldable()) {
      if(expr->get_val_bool()) unreach=true;
      else return;
    }
    char*& str=ilt->get_out_branches();
    char *label=0;
    if(!unreach) {
      size_t blockcount=0;
      label=mprintf("%s_l%lu",
                    ilt->get_my_tmpid().c_str(),
                    (unsigned long) ilt->get_new_label_num());
      str=expr->update_location_object(str);
      str=expr->generate_code_tmp(str, "if(!", blockcount);
      str=mputprintf(str, ") goto %s;\n", label);
      while(blockcount-->0) str=mputstr(str, "}\n");
    }
    block->ilt_generate_code(ilt);
    if(!unreach) {
      str=mputprintf(str, "goto %s;\n%s:\n",
                     end_label, label);
      Free(label);
    }
  }

  void IfClause::set_parent_path(WithAttribPath* p_path) {
    block->set_parent_path(p_path);
  }

  void IfClause::dump(unsigned int level) const {
    DEBUG(level, "If clause!");
    expr->dump(level + 1);
    block->dump(level + 1);
  }

  // =================================
  // ===== IfClauses
  // =================================

  IfClauses::~IfClauses()
  {
    for(size_t i=0; i<ics.size(); i++) delete ics[i];
    ics.clear();
  }

  IfClauses *IfClauses::clone() const
  {
    FATAL_ERROR("IfClauses::clone");
  }

  void IfClauses::add_ic(IfClause *p_ic)
  {
    if(!p_ic)
      FATAL_ERROR("IfClauses::add_ic()");
    ics.add(p_ic);
  }

  void IfClauses::add_front_ic(IfClause *p_ic)
  {
    if(!p_ic)
      FATAL_ERROR("IfClauses::add_front_ic()");
    ics.add_front(p_ic);
  }

  void IfClauses::set_my_scope(Scope *p_scope)
  {
    for(size_t i=0; i<ics.size(); i++)
      ics[i]->set_my_scope(p_scope);
  }

  void IfClauses::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    for(size_t i=0; i<ics.size(); i++)
      ics[i]->set_fullname(p_fullname+".ic_"+Int2string(i+1));
  }

  void IfClauses::set_my_sb(StatementBlock *p_sb, size_t p_index)
  {
    for(size_t i=0; i<ics.size(); i++)
      ics[i]->get_block()->set_my_sb(p_sb, p_index);
  }

  void IfClauses::set_my_def(Definition *p_def)
  {
    for(size_t i=0; i<ics.size(); i++)
      ics[i]->get_block()->set_my_def(p_def);
  }

  void IfClauses::set_my_ags(AltGuards *p_ags)
  {
    for(size_t i=0; i<ics.size(); i++)
      ics[i]->get_block()->set_my_ags(p_ags);
  }

  void IfClauses::set_my_laic_stmt(AltGuards *p_ags, Statement *p_loop_stmt)
  {
    for(size_t i=0; i<ics.size(); i++)
      ics[i]->get_block()->set_my_laic_stmt(p_ags, p_loop_stmt);
  }

  StatementBlock::returnstatus_t IfClauses::has_return
    (StatementBlock *elseblock) const
  {
    StatementBlock::returnstatus_t ret_val = StatementBlock::RS_MAYBE;
    for (size_t i = 0; i < ics.size(); i++) {
      switch (ics[i]->get_block()->has_return()) {
      case StatementBlock::RS_NO:
	if (ret_val == StatementBlock::RS_YES) return StatementBlock::RS_MAYBE;
	else ret_val = StatementBlock::RS_NO;
	break;
      case StatementBlock::RS_YES:
	if (ret_val == StatementBlock::RS_NO) return StatementBlock::RS_MAYBE;
	else ret_val = StatementBlock::RS_YES;
	break;
      default:
	return StatementBlock::RS_MAYBE;
      }
    }
    StatementBlock::returnstatus_t else_status;
    if (elseblock) else_status = elseblock->has_return();
    else else_status = StatementBlock::RS_NO;
    switch (else_status) {
    case StatementBlock::RS_NO:
      if (ret_val == StatementBlock::RS_YES) return StatementBlock::RS_MAYBE;
      else ret_val = StatementBlock::RS_NO;
      break;
    case StatementBlock::RS_YES:
      if (ret_val == StatementBlock::RS_NO) return StatementBlock::RS_MAYBE;
      else ret_val = StatementBlock::RS_YES;
      break;
    default:
      return StatementBlock::RS_MAYBE;
    }
    return ret_val;
  }

  bool IfClauses::has_receiving_stmt() const
  {
    for(size_t i=0; i<ics.size(); i++)
      if(ics[i]->has_receiving_stmt()) return true;
    return false;
  }

  void IfClauses::chk(bool& unreach)
  {
    for(size_t i=0; i<ics.size(); i++)
      ics[i]->chk(unreach);
  }

  void IfClauses::chk_allowed_interleave()
  {
    for (size_t i = 0; i < ics.size(); i++)
      ics[i]->get_block()->chk_allowed_interleave();
  }

  void IfClauses::set_code_section(
    GovernedSimple::code_section_t p_code_section)
  {
    for (size_t i = 0; i < ics.size(); i++)
      ics[i]->set_code_section(p_code_section);
  }

  char* IfClauses::generate_code(char *str, size_t& blockcount,
                                 bool& unreach, bool& eachfalse)
  {
    for(size_t i=0; i<ics.size(); i++) {
      if(unreach) return str;
      str=ics[i]->generate_code(str, blockcount, unreach, eachfalse);
    }
    return str;
  }

  void IfClauses::ilt_generate_code(ILT *ilt, const char *end_label,
                                    bool& unreach)
  {
    for(size_t i=0; i<ics.size(); i++) {
      if(unreach) return;
      ics[i]->ilt_generate_code(ilt, end_label, unreach);
    }
  }

  void IfClauses::set_parent_path(WithAttribPath* p_path) {
    for (size_t i = 0; i < ics.size(); i++)
      ics[i]->set_parent_path(p_path);
  }

  void IfClauses::dump(unsigned int level) const {
    DEBUG(level, "%lu if clauses", (unsigned long)ics.size());
    for (size_t i = 0; i < ics.size(); i++)
      ics[i]->dump(level + 1);
  }

  // =================================
  // ===== SelectCase
  // =================================

  SelectCase::SelectCase(TemplateInstances *p_tis, StatementBlock *p_block)
    : tis(p_tis), block(p_block)
  {
    if(!block)
      FATAL_ERROR("SelectCase::SelectCase()");
  }

  SelectCase::~SelectCase()
  {
    delete tis;
    delete block;
  }

  SelectCase *SelectCase::clone() const
  {
    FATAL_ERROR("SelectCase::clone");
  }

  void SelectCase::set_my_scope(Scope *p_scope)
  {
    if(tis) tis->set_my_scope(p_scope);
    block->set_my_scope(p_scope);
  }

  void SelectCase::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    if(tis) tis->set_fullname(p_fullname+".tis");
    block->set_fullname(p_fullname+".block");
  }

  /** \todo review */
  void SelectCase::chk(Type *p_gov, bool& unreach)
  {
    Error_Context cntxt(this, "In select case statement");
    if(unreach) warning("Control never reaches this code because of"
                        " previous effective case(s)");
    if(tis)
      for(size_t i=0; i<tis->get_nof_tis(); i++)
        tis->get_ti_byIndex(i)->chk(p_gov);
    else unreach=true; // else statement
    block->chk();
  }

  void SelectCase::set_code_section
  (GovernedSimple::code_section_t p_code_section)
  {
    if(tis) tis->set_code_section(p_code_section);
    block->set_code_section(p_code_section);
  }

  /** \todo review */
  char* SelectCase::generate_code_if(char *str, const char *tmp_prefix,
                                     const char *expr_name, size_t idx,
                                     bool& unreach)
  {
    if(unreach) return str;
    if(tis) {
      for(size_t i=0; i<tis->get_nof_tis(); i++) {
        TemplateInstance *ti=tis->get_ti_byIndex(i);
        Template *tb=ti->get_Template();
        bool specval = tb->is_Value();
        expression_struct exprs;
        Code::init_expr(&exprs);
        if (!specval) ti->generate_code(&exprs);
        else if (tb->get_templatetype() == Template::SPECIFIC_VALUE) {
          tb->get_specific_value()->generate_code_expr_mandatory(&exprs);
        }
        else {
          Value* val = tb->get_Value();
          val->generate_code_expr_mandatory(&exprs);
          delete val;
        }
        str=tb->update_location_object(str);
        if(!exprs.preamble && !exprs.postamble) {
          str=mputstr(str, "if(");
          if(!specval)
            str=mputprintf(str, "%s.match(%s)", exprs.expr, expr_name);
          else str=mputprintf(str, "%s == %s", expr_name, exprs.expr);
          str=mputprintf(str, ") goto %s_%lu;\n", tmp_prefix,
            (unsigned long) idx);
          Code::free_expr(&exprs);
        }
        else {
          str=mputprintf(str, "{\nboolean %s_%lub;\n", tmp_prefix,
            (unsigned long) idx);
          char *s=exprs.expr;
          exprs.expr=mprintf("%s_%lub = ", tmp_prefix, (unsigned long) idx);
          if(!specval)
            exprs.expr=mputprintf(exprs.expr, "%s.match(%s)", s, expr_name);
          else exprs.expr=mputprintf(exprs.expr, "(%s == %s)", expr_name, s);
          Free(s);
          str=Code::merge_free_expr(str, &exprs);
          str=mputprintf(str, "if(%s_%lub) goto %s_%lu;\n}\n",
            tmp_prefix, (unsigned long) idx, tmp_prefix, (unsigned long) idx);
        }
      } // for i
    } // if tis
    else {
      unreach=true; // else statement
      str=mputprintf(str, "goto %s_%lu;\n", tmp_prefix, (unsigned long) idx);
    }
    return str;
  }

  /** \todo review */
  char* SelectCase::generate_code_stmt(char *str, const char *tmp_prefix,
                                       size_t idx, bool& unreach)
  {
    if(unreach) return str;
    if(!tis) unreach=true;
    str=mputprintf(str, "%s_%lu:\n{\n", tmp_prefix, (unsigned long) idx);
    str=block->generate_code(str);
    str=mputprintf(str, "goto %s_end;\n}\n", tmp_prefix);
    return str;
  }

  void SelectCase::ilt_generate_code_stmt(ILT *ilt, const char *tmp_prefix,
                                          size_t idx, bool& unreach)
  {
    if(unreach) return;
    if(!tis) unreach=true;
    char*& str=ilt->get_out_branches();
    str=mputprintf(str, "%s_%lu:\n", tmp_prefix, (unsigned long) idx);
    bool has_recv=block->has_receiving_stmt();
    if(!has_recv) {
      str=mputstr(str, "{\n");
      str=block->generate_code(str);
    }
    else block->ilt_generate_code(ilt);
    str=mputprintf(str, "goto %s_end;\n", tmp_prefix);
    if(!has_recv)
      str=mputstr(str, "}\n");
  }

  void SelectCase::set_parent_path(WithAttribPath* p_path) {
    block->set_parent_path(p_path);
  }

  // =================================
  // ===== SelectCases
  // =================================

  SelectCases::~SelectCases()
  {
    for(size_t i=0; i<scs.size(); i++) delete scs[i];
    scs.clear();
  }

  SelectCases *SelectCases::clone() const
  {
    FATAL_ERROR("SelectCases::clone");
  }

  void SelectCases::add_sc(SelectCase *p_sc)
  {
    if(!p_sc)
      FATAL_ERROR("SelectCases::add_sc()");
    scs.add(p_sc);
  }

  void SelectCases::set_my_scope(Scope *p_scope)
  {
    for(size_t i=0; i<scs.size(); i++)
      scs[i]->set_my_scope(p_scope);
  }

  void SelectCases::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    for(size_t i=0; i<scs.size(); i++)
      scs[i]->set_fullname(p_fullname+".sc_"+Int2string(i+1));
  }

  void SelectCases::set_my_sb(StatementBlock *p_sb, size_t p_index)
  {
    for(size_t i=0; i<scs.size(); i++)
      scs[i]->get_block()->set_my_sb(p_sb, p_index);
  }

  void SelectCases::set_my_def(Definition *p_def)
  {
    for(size_t i=0; i<scs.size(); i++)
      scs[i]->get_block()->set_my_def(p_def);
  }

  void SelectCases::set_my_ags(AltGuards *p_ags)
  {
    for(size_t i=0; i<scs.size(); i++)
      scs[i]->get_block()->set_my_ags(p_ags);
  }

  void SelectCases::set_my_laic_stmt(AltGuards *p_ags, Statement *p_loop_stmt)
  {
    for(size_t i=0; i<scs.size(); i++)
      scs[i]->get_block()->set_my_laic_stmt(p_ags, p_loop_stmt);
  }

  StatementBlock::returnstatus_t SelectCases::has_return() const
  {
    StatementBlock::returnstatus_t ret_val = StatementBlock::RS_MAYBE;
    bool has_else = false;
    for (size_t i = 0; i < scs.size(); i++) {
      SelectCase *sc = scs[i];
      switch (sc->get_block()->has_return()) {
      case StatementBlock::RS_NO:
	if (ret_val == StatementBlock::RS_YES) return StatementBlock::RS_MAYBE;
	else ret_val = StatementBlock::RS_NO;
	break;
      case StatementBlock::RS_YES:
	if (ret_val == StatementBlock::RS_NO) return StatementBlock::RS_MAYBE;
	else ret_val = StatementBlock::RS_YES;
	break;
      default:
	return StatementBlock::RS_MAYBE;
      }
      if (!sc->get_tis()) {
	has_else = true;
	break;
      }
    }
    if (!has_else && ret_val == StatementBlock::RS_YES)
      return StatementBlock::RS_MAYBE;
    else return ret_val;
  }

  bool SelectCases::has_receiving_stmt() const
  {
    for(size_t i=0; i<scs.size(); i++)
      if(scs[i]->get_block()->has_receiving_stmt()) return true;
    return false;
  }

  /** \todo review */
  void SelectCases::chk(Type *p_gov)
  {
    bool unreach=false;
    for(size_t i=0; i<scs.size(); i++)
      scs[i]->chk(p_gov, unreach);
  }

  void SelectCases::chk_allowed_interleave()
  {
    for (size_t i = 0; i < scs.size(); i++)
      scs[i]->get_block()->chk_allowed_interleave();
  }

  void SelectCases::set_code_section
  (GovernedSimple::code_section_t p_code_section)
  {
    for(size_t i=0; i<scs.size(); i++)
      scs[i]->set_code_section(p_code_section);
  }

  char* SelectCases::generate_code(char *str, const char *tmp_prefix,
                                   const char *expr_name)
  {
    bool unreach=false;
    for(size_t i=0; i<scs.size(); i++) {
      str=scs[i]->generate_code_if(str, tmp_prefix, expr_name, i, unreach);
      if(unreach) break;
    }
    if(!unreach) str=mputprintf(str, "goto %s_end;\n", tmp_prefix);
    unreach=false;
    for(size_t i=0; i<scs.size(); i++) {
      str=scs[i]->generate_code_stmt(str, tmp_prefix, i, unreach);
      if(unreach) break;
    }
    str=mputprintf(str, "%s_end: /* empty */;\n", tmp_prefix);
    return str;
  }

  void SelectCases::ilt_generate_code(ILT *ilt, const char *tmp_prefix,
                                      const char *expr_init,
                                      const char *expr_name)
  {
    char*& str=ilt->get_out_branches();
    if(strlen(expr_init)) {
      str=mputstr(str, "{\n"); // (1)
      str=mputstr(str, expr_init);
    }
    bool unreach=false;
    for(size_t i=0; i<scs.size(); i++) {
      if(unreach) break;
      str=scs[i]->generate_code_if(str, tmp_prefix, expr_name, i, unreach);
    }
    if(!unreach) str=mputprintf(str, "goto %s_end;\n", tmp_prefix);
    if(strlen(expr_init)) str=mputstr(str, "}\n"); // (1)
    unreach=false;
    for(size_t i=0; i<scs.size(); i++) {
      if(unreach) break;
      scs[i]->ilt_generate_code_stmt(ilt, tmp_prefix, i, unreach);
    }
    str=mputprintf(str, "%s_end:\n", tmp_prefix);
  }

  void SelectCases::set_parent_path(WithAttribPath* p_path) {
    for (size_t i = 0; i < scs.size(); i++)
      scs[i]->set_parent_path(p_path);
  }

  // =================================
  // ===== AltGuard
  // =================================

  AltGuard::AltGuard(Value *p_expr, Statement *p_stmt, StatementBlock *p_block)
    : altguardtype(AG_OP), expr(p_expr), stmt(p_stmt), block(p_block)
  {
    if (!p_stmt || !p_block) FATAL_ERROR("AltGuard::AltGuard()");
  }

  AltGuard::AltGuard(Value *p_expr, Ref_pard *p_ref, StatementBlock *p_block)
    : altguardtype(AG_REF), expr(p_expr), ref(p_ref), block(p_block)
  {
    if (!p_ref) FATAL_ERROR("AltGuard::AltGuard()");
  }

  AltGuard::AltGuard(Value *p_expr, Value *p_v,
    Ttcn::TemplateInstances *p_t_list, StatementBlock *p_block)
    : altguardtype(AG_INVOKE), expr(p_expr)
    , block(p_block)
  {
    if(!p_v || !p_t_list) FATAL_ERROR("AltGuard::AltGuard()");
    invoke.v = p_v;
    invoke.t_list = p_t_list;
    invoke.ap_list = 0;
  }

  AltGuard::AltGuard(StatementBlock *p_block)
    : altguardtype(AG_ELSE), expr(0), dummy(0), block(p_block)
  {
    if (!p_block) FATAL_ERROR("AltGuard::AltGuard()");
  }

  AltGuard::~AltGuard()
  {
    switch(altguardtype) {
    case AG_OP:
      delete expr;
      delete stmt;
      delete block;
      break;
    case AG_REF:
      delete expr;
      delete ref;
      delete block;
      break;
    case AG_INVOKE:
      delete expr;
      delete invoke.v;
      delete invoke.t_list;
      delete invoke.ap_list;
      delete block;
      break;
    case AG_ELSE:
      delete block;
      break;
    default:
      FATAL_ERROR("AltGuard::~AltGuard()");
    } // switch
  }

  AltGuard *AltGuard::clone() const
  {
    FATAL_ERROR("AltGuard::clone");
  }

  void AltGuard::set_my_scope(Scope *p_scope)
  {
    switch(altguardtype) {
    case AG_OP:
      if(expr) expr->set_my_scope(p_scope);
      stmt->set_my_scope(p_scope);
      block->set_my_scope(p_scope);
      break;
    case AG_REF:
      if(expr) expr->set_my_scope(p_scope);
      ref->set_my_scope(p_scope);
      if(block) block->set_my_scope(p_scope);
      break;
    case AG_INVOKE:
      if(expr) expr->set_my_scope(p_scope);
      invoke.v->set_my_scope(p_scope);
      if(invoke.t_list) invoke.t_list->set_my_scope(p_scope);
      if(invoke.ap_list) invoke.ap_list->set_my_scope(p_scope);
      if(block) block->set_my_scope(p_scope);
      break;
    case AG_ELSE:
      block->set_my_scope(p_scope);
      break;
    default:
      FATAL_ERROR("AltGuard::set_my_scope()");
    } // switch
  }

  void AltGuard::set_my_sb(StatementBlock *p_sb, size_t p_index)
  {
    switch(altguardtype) {
    case AG_OP:
      stmt->set_my_sb(p_sb, p_index);
      block->set_my_sb(p_sb, p_index);
      break;
    case AG_REF:
      if(block) block->set_my_sb(p_sb, p_index);
      break;
    case AG_INVOKE:
      if(block) block->set_my_sb(p_sb, p_index);
      break;
    case AG_ELSE:
      block->set_my_sb(p_sb, p_index);
      break;
    default:
      FATAL_ERROR("AltGuard::set_my_sb()");
    } // switch
  }

  void AltGuard::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    switch(altguardtype) {
    case AG_OP:
      if(expr) expr->set_fullname(p_fullname+".expr");
      stmt->set_fullname(p_fullname+".stmt");
      block->set_fullname(p_fullname+".block");
      break;
    case AG_REF:
      if(expr) expr->set_fullname(p_fullname+".expr");
      ref->set_fullname(p_fullname+".ref");
      if(block) block->set_fullname(p_fullname+".block");
      break;
    case AG_INVOKE:
      if(expr) expr->set_fullname(p_fullname+".expr");
      invoke.v->set_fullname(p_fullname+".function");
      if(invoke.t_list) invoke.t_list->set_fullname(p_fullname+".argument");
      if(invoke.ap_list) invoke.ap_list->set_fullname(p_fullname+".argument");
      if(block) block->set_fullname(p_fullname+".block");
      break;
    case AG_ELSE:
      block->set_fullname(p_fullname+".elseblock");
      break;
    default:
      FATAL_ERROR("AltGuard::set_fullname()");
    } // switch
  }

  Value *AltGuard::get_guard_expr() const
  {
    if (altguardtype == AG_ELSE) FATAL_ERROR("AltGuard::get_guard_expr()");
    return expr;
  }

  Ref_pard *AltGuard::get_guard_ref() const
  {
    if (altguardtype != AG_REF) FATAL_ERROR("AltGuard::get_guard_ref()");
    return ref;
  }

  Statement *AltGuard::get_guard_stmt() const
  {
    if (altguardtype != AG_OP) FATAL_ERROR("AltGuard::get_guard_stmt()");
    return stmt;
  }

  void AltGuard::set_my_def(Definition *p_def)
  {
    switch(altguardtype) {
    case AG_OP:
      stmt->set_my_def(p_def);
      block->set_my_def(p_def);
      break;
    case AG_REF:
      if(block) block->set_my_def(p_def);
      break;
    case AG_INVOKE:
      if(block) block->set_my_def(p_def);
      break;
    case AG_ELSE:
      block->set_my_def(p_def);
      break;
    default:
      FATAL_ERROR("AltGuard::set_my_def()");
    } // switch
  }

  void AltGuard::set_my_ags(AltGuards *p_ags)
  {
    switch (altguardtype) {
    case AG_OP:
      block->set_my_ags(p_ags);
      break;
    case AG_REF:
      if (block) block->set_my_ags(p_ags);
      break;
    case AG_INVOKE:
      if (block) block->set_my_ags(p_ags);
      break;
    case AG_ELSE:
      block->set_my_ags(p_ags);
      break;
    default:
      FATAL_ERROR("AltGuard::set_my_ags()");
    } // switch
  }

  void AltGuard::set_my_laic_stmt(AltGuards *p_ags, Statement *p_loop_stmt)
  {
    switch (altguardtype) {
    case AG_OP:
      block->set_my_laic_stmt(p_ags, p_loop_stmt);
      break;
    case AG_REF:
      if (block) block->set_my_laic_stmt(p_ags, p_loop_stmt);
      break;
    case AG_INVOKE:
      if (block) block->set_my_laic_stmt(p_ags, p_loop_stmt);
      break;
    case AG_ELSE:
      block->set_my_laic_stmt(p_ags, p_loop_stmt);
      break;
    default:
      FATAL_ERROR("AltGuard::set_my_laic_stmt()");
    } // switch
  }

  void AltGuard::chk()
  {
    switch(altguardtype) {
    case AG_OP:
      if (expr) {
	Error_Context cntxt(expr, "In guard expression");
        expr->chk_expr_bool(Type::EXPECTED_DYNAMIC_VALUE);
      }
      {
	Error_Context cntxt(stmt, "In guard operation");
	stmt->chk();
      }
      block->chk();
      break;
    case AG_REF:
      if (expr) {
	Error_Context cntxt(expr, "In guard expression");
        expr->chk_expr_bool(Type::EXPECTED_DYNAMIC_VALUE);
      }
      {
	Error_Context cntxt(ref, "In guard statement");
	Common::Assignment *t_ass = ref->get_refd_assignment();
	if (t_ass) {
	  if (t_ass->get_asstype() == Common::Assignment::A_ALTSTEP) {
	    ref->get_my_scope()->chk_runs_on_clause(t_ass, *ref, "call");
	  } else {
	    ref->error("Reference to an altstep was expected instead of %s",
	      t_ass->get_description().c_str());
	  }
	}
      }
      if (block) block->chk();
      break;
    case AG_INVOKE:
      if (expr) {
	Error_Context cntxt(expr, "In guard expression");
        expr->chk_expr_bool(Type::EXPECTED_DYNAMIC_VALUE);
      }
      {
        if (!invoke.t_list) return; //already_checked
	Error_Context cntxt(ref, "In guard statement");
        switch(invoke.v->get_valuetype()){
        case Value::V_REFER:
          invoke.v->error(
            "A value of an altstep type was expected "
            "in the argument instead of a `refers' statement,"
            " which does not specify any function type");
          return;
        case Value::V_TTCN3_NULL:
          invoke.v->error(
            "A value of an altstep type was expected "
            "in the argument instead of a `null' value,"
            " which does not specify any function type");
          return;
        default:
          break;
        }
	Type *t = invoke.v->get_expr_governor_last();
	if (!t) return;
	switch (t->get_typetype()) {
	case Type::T_ERROR:
	  return;
	case Type::T_ALTSTEP:
	  break;
	default:
	  invoke.v->error("A value of type altstep was expected instead of "
	    "`%s'", t->get_typename().c_str());
	  return;
	}
        invoke.v->get_my_scope()->chk_runs_on_clause(t, *this, "call");
        Ttcn::FormalParList *fp_list = t->get_fat_parameters();
        invoke.ap_list = new Ttcn::ActualParList;
        bool is_erroneous = fp_list->chk_actual_parlist(invoke.t_list,
          invoke.ap_list);
        delete invoke.t_list;
        invoke.t_list = 0;
        if(is_erroneous) {
          delete invoke.ap_list;
          invoke.ap_list = 0;
        } else {
          invoke.ap_list->set_fullname(get_fullname());
	  invoke.ap_list->set_my_scope(invoke.v->get_my_scope());
        }
      }
      if (block) block->chk();
      break;
    case AG_ELSE:
      {
	Error_Context cntxt(this, "In else branch");
	block->chk();
	Statement *first_stmt = block->get_first_stmt();
	if (first_stmt && first_stmt->get_statementtype() ==
	    Statement::S_REPEAT)
	  first_stmt->warning("The first statement of the [else] branch is a "
	    "repeat statement. This will result in busy waiting");
      }
      break;
    default:
      FATAL_ERROR("AltGuard::chk()");
    } // switch
  }

  void AltGuard::set_code_section(
    GovernedSimple::code_section_t p_code_section)
  {
    switch(altguardtype) {
    case AG_OP:
      if (expr) expr->set_code_section(p_code_section);
      stmt->set_code_section(p_code_section);
      block->set_code_section(p_code_section);
      break;
    case AG_REF:
      if (expr) expr->set_code_section(p_code_section);
      ref->set_code_section(p_code_section);
      if (block) block->set_code_section(p_code_section);
      break;
    case AG_INVOKE:
      if (expr) expr->set_code_section(p_code_section);
      invoke.v->set_code_section(p_code_section);
      if(invoke.t_list) invoke.t_list->set_code_section(p_code_section);
      if(invoke.ap_list)
        for(size_t i = 0; i < invoke.ap_list->get_nof_pars(); i++)
            invoke.ap_list->get_par(i)->set_code_section(p_code_section);
      if (block) block->set_code_section(p_code_section);
      break;
    case AG_ELSE:
      block->set_code_section(p_code_section);
      break;
    default:
      FATAL_ERROR("AltGuard::set_fullname()");
    } // switch
  }

  void AltGuard::generate_code_invoke_instance(expression_struct *p_expr)
  {
    if (altguardtype != AG_INVOKE)
      FATAL_ERROR("AltGuard::generate_code_invoke_instance");
    Value *last_v = invoke.v->get_value_refd_last();
    if (last_v->get_valuetype() == Value::V_ALTSTEP) {
      Common::Assignment *altstep = last_v->get_refd_fat();
      p_expr->expr = mputprintf(p_expr->expr, "%s_instance(",
	altstep->get_genname_from_scope(invoke.v->get_my_scope()).c_str());
      invoke.ap_list->generate_code_alias(p_expr,
	altstep->get_FormalParList(), altstep->get_RunsOnType(), false);
    } else {
      invoke.v->generate_code_expr_mandatory(p_expr);
      p_expr->expr = mputstr(p_expr->expr, ".invoke(");
      Type* gov_last = invoke.v->get_expr_governor_last();
      invoke.ap_list->generate_code_alias(p_expr, 0,
	gov_last->get_fat_runs_on_type(), gov_last->get_fat_runs_on_self());
    }
    p_expr->expr = mputc(p_expr->expr, ')');
  }

  // =================================
  // ===== AltGuards
  // =================================

  AltGuards::~AltGuards()
  {
    for(size_t i=0; i<ags.size(); i++) delete ags[i];
    ags.clear();
    delete label;
    delete il_label_end;
  }

  AltGuards *AltGuards::clone() const
  {
    FATAL_ERROR("AltGuards::clone");
  }

  void AltGuards::add_ag(AltGuard *p_ag)
  {
    if(!p_ag)
      FATAL_ERROR("AltGuards::add_ag()");
    ags.add(p_ag);
  }

  void AltGuards::set_my_scope(Scope *p_scope)
  {
    my_scope = p_scope;
    for(size_t i=0; i<ags.size(); i++)
      ags[i]->set_my_scope(p_scope);
  }

  void AltGuards::set_fullname(const string& p_fullname)
  {
    Node::set_fullname(p_fullname);
    for(size_t i=0; i<ags.size(); i++)
      ags[i]->set_fullname(p_fullname+".ag_"+Int2string(i+1));
  }

  void AltGuards::set_my_sb(StatementBlock *p_sb, size_t p_index)
  {
    for(size_t i=0; i<ags.size(); i++)
      ags[i]->set_my_sb(p_sb, p_index);
  }

  void AltGuards::set_my_def(Definition *p_def)
  {
    for(size_t i=0; i<ags.size(); i++)
      ags[i]->set_my_def(p_def);
  }

  void AltGuards::set_my_ags(AltGuards *p_ags)
  {
    for(size_t i=0; i<ags.size(); i++)
      ags[i]->set_my_ags(p_ags);
  }

  void AltGuards::set_my_laic_stmt(AltGuards *p_ags, Statement *p_loop_stmt)
  {
    for(size_t i=0; i<ags.size(); i++)
      ags[i]->set_my_laic_stmt(p_ags, p_loop_stmt);
  }

  bool AltGuards::has_else() const
  {
    for (size_t i = 0; i < ags.size(); i++)
      if (ags[i]->get_type() == AltGuard::AG_ELSE) return true;
    return false;
  }

  StatementBlock::returnstatus_t AltGuards::has_return() const
  {
    StatementBlock::returnstatus_t ret_val = StatementBlock::RS_MAYBE;
    for (size_t i = 0; i < ags.size(); i++) {
      AltGuard *ag = ags[i];
      StatementBlock *block = ag->get_block();
      StatementBlock::returnstatus_t block_status;
      if (block) block_status = block->has_return();
      else block_status = StatementBlock::RS_NO;
      switch (block_status) {
      case StatementBlock::RS_NO:
	if (ret_val == StatementBlock::RS_YES) return StatementBlock::RS_MAYBE;
	else ret_val = StatementBlock::RS_NO;
	break;
      case StatementBlock::RS_YES:
	if (ret_val == StatementBlock::RS_NO) return StatementBlock::RS_MAYBE;
	else ret_val = StatementBlock::RS_YES;
	break;
      default:
	return StatementBlock::RS_MAYBE;
      }
      if (ag->get_type() == AltGuard::AG_ELSE) break;
    }
    return ret_val;
  }

  bool AltGuards::has_receiving_stmt() const
  {
    for(size_t i=0; i<ags.size(); i++)
      if(ags[i]->get_block()->has_receiving_stmt()) return true;
    return false;
  }

  void AltGuards::chk()
  {
    bool unreach_found = false;
    size_t nof_ags = ags.size();
    AltGuard *prev_ag = 0;
    for (size_t i = 0; i < nof_ags; i++) {
      AltGuard *ag = ags[i];
      ag->chk();
      if (!unreach_found && prev_ag &&
	  prev_ag->get_type() == AltGuard::AG_ELSE) {
	  ag->warning("Control never reaches this branch of alternative "
	    "because of the previous [else] branch");
	  unreach_found = true;
      }
      prev_ag = ag;
    }
  }

  void AltGuards::chk_allowed_interleave()
  {
    for (size_t i = 0; i < ags.size(); i++) {
      AltGuard *ag = ags[i];
      switch (ag->get_type()) {
      case AltGuard::AG_OP:
	break;
      case AltGuard::AG_REF:
      case AltGuard::AG_INVOKE:
        ag->error("Invocation of an altstep is not allowed within an "
	  "interleave statement");
	break;
      case AltGuard::AG_ELSE:
        ag->error("Else branch of an alternative is not allowed within an "
	  "interleave statement");
	break;
      default:
        FATAL_ERROR("AltGuards::chk_allowed_interleave()");
      }
      ag->get_block()->chk_allowed_interleave();
    }
  }

  void AltGuards::set_code_section(
    GovernedSimple::code_section_t p_code_section)
  {
    for (size_t i = 0; i < ags.size(); i++)
      ags[i]->set_code_section(p_code_section);
  }

  char *AltGuards::generate_code_alt(char *str, const Location& loc)
  {
    bool label_needed = has_repeat, has_else_branch = false;
    for (size_t i = 0; i < ags.size(); i++) {
      AltGuard *ag = ags[i];
      switch (ag->get_type()) {
      case AltGuard::AG_OP:
	// trigger may return ALT_REPEAT
        if (ag->get_guard_stmt()->can_repeat()) label_needed = true;
        break;
      case AltGuard::AG_REF:
      case AltGuard::AG_INVOKE:
	// an altstep may return ALT_REPEAT
	label_needed = true;
	break;
      case AltGuard::AG_ELSE:
	has_else_branch = true;
	break;
      default:
        FATAL_ERROR("AltGuards::generate_code_alt()");
      }
      if (has_else_branch) break;
    }
    // if there is no [else] branch the defaults may return ALT_REPEAT
    if (!has_else_branch) label_needed = true;
    // opening bracket of the statement block
    str = mputstr(str, "{\n");
    // the label name is also used for prefixing local variables
    if (!my_scope || label) FATAL_ERROR("AltGuards::generate_code_alt()");
    label = new string(my_scope->get_scope_mod_gen()->get_temporary_id());
    const char *label_str = label->c_str();
    if (label_needed) str = mputprintf(str, "%s:\n", label_str);
    // temporary variables used for caching of status codes
    for (size_t i = 0; i < ags.size(); i++) {
      AltGuard *ag = ags[i];
      if (ag->get_type() == AltGuard::AG_ELSE) break;
      str = mputprintf(str, "alt_status %s_alt_flag_%lu = %s;\n",
	label_str, (unsigned long) i,
        ag->get_guard_expr() ? "ALT_UNCHECKED" : "ALT_MAYBE");
    }
    if (!has_else_branch) {
      str = mputprintf(str, "alt_status %s_default_flag = ALT_MAYBE;\n",
	label_str);
    }
    // the first snapshot is taken in non-blocking mode
    // and opening infinite for() loop
    str = mputstr(str, "TTCN_Snapshot::take_new(FALSE);\n"
      "for ( ; ; ) {\n");
    for (size_t i = 0; i < ags.size(); i++) {
      AltGuard *ag = ags[i];
      AltGuard::altguardtype_t agtype = ag->get_type();
      if (agtype == AltGuard::AG_ELSE) {
	// an else branch was found
	str = mputstr(str, "TTCN_Snapshot::else_branch_reached();\n");
	StatementBlock *block = ag->get_block();
	if (block->get_nof_stmts() > 0) {
	  str = mputstr(str, "{\n");
	  str = block->generate_code(str);
	  str = mputstr(str, "}\n");
	}
	// jump out of the infinite for() loop
	if (block->has_return() != StatementBlock::RS_YES)
	  str = mputstr(str, "break;\n");
	// do not generate code for further branches
	break;
      } else {
	Value *guard_expr = ag->get_guard_expr();
	if (guard_expr) {
	  // the branch has a boolean guard expression
	  str = mputprintf(str, "if (%s_alt_flag_%lu == ALT_UNCHECKED) {\n",
	    label_str, (unsigned long) i);
	  str = guard_expr->update_location_object(str);
	  expression_struct expr;
	  Code::init_expr(&expr);
	  guard_expr->generate_code_expr(&expr);
	  str = mputstr(str, expr.preamble);
	  str = mputprintf(str, "if (%s) %s_alt_flag_%lu = ALT_MAYBE;\n"
	    "else %s_alt_flag_%lu = ALT_NO;\n", expr.expr, label_str,
	    (unsigned long) i, label_str, (unsigned long) i);
	  str = mputstr(str, expr.postamble);
	  Code::free_expr(&expr);
	  str = mputstr(str, "}\n");
	}
	// evaluation of guard operation or altstep
	str = mputprintf(str, "if (%s_alt_flag_%lu == ALT_MAYBE) {\n",
	  label_str, (unsigned long) i);
	// indicates whether the guard operation might return ALT_REPEAT
	bool can_repeat;
	expression_struct expr;
	Code::init_expr(&expr);
	expr.expr = mputprintf(expr.expr, "%s_alt_flag_%lu = ", label_str,
          (unsigned long) i);
	switch (agtype) {
	case AltGuard::AG_OP: {
	  // the guard operation is a receiving statement
	  Statement *stmt = ag->get_guard_stmt();
	  str = stmt->update_location_object(str);
	  stmt->generate_code_expr(&expr);
	  can_repeat = stmt->can_repeat();
	  break; }
	case AltGuard::AG_REF: {
	  // the guard operation is an altstep instance
	  Ref_pard *ref = ag->get_guard_ref();
	  str = ref->update_location_object(str);
	  Common::Assignment *altstep = ref->get_refd_assignment();
	  expr.expr = mputprintf(expr.expr, "%s_instance(",
	    altstep->get_genname_from_scope(my_scope).c_str());
	  ref->get_parlist()->generate_code_alias(&expr,
	    altstep->get_FormalParList(), altstep->get_RunsOnType(), false);
	  expr.expr = mputc(expr.expr, ')');
	  can_repeat = true;
	  break; }
	case AltGuard::AG_INVOKE: {
          // the guard operation is an altstep invocation
          str = ag->update_location_object(str);
          ag->generate_code_invoke_instance(&expr);
          can_repeat = true;
          break; }
	default:
	  FATAL_ERROR("AltGuards::generate_code_alt()");
	}
	str = Code::merge_free_expr(str, &expr);
	if (can_repeat) {
	  str = mputprintf(str, "if (%s_alt_flag_%lu == ALT_REPEAT) goto %s;\n",
	    label_str, (unsigned long) i, label_str);
	}
        if (agtype == AltGuard::AG_REF || agtype == AltGuard::AG_INVOKE) {
          str = mputprintf(str, "if (%s_alt_flag_%lu == ALT_BREAK) break;\n",
             label_str, (unsigned long) i);
        }
	// execution of statement block if the guard was successful
	str = mputprintf(str, "if (%s_alt_flag_%lu == ALT_YES) ", label_str,
          (unsigned long) i);
	StatementBlock *block = ag->get_block();
	if (block && block->get_nof_stmts() > 0) {
	  str = mputstr(str, "{\n");
	  str = block->generate_code(str);
	  if (block->has_return() != StatementBlock::RS_YES)
	    str = mputstr(str, "break;\n");
	  str = mputstr(str, "}\n");
	} else str = mputstr(str, "break;\n");
	// closing of if() block
	str = mputstr(str, "}\n");
      }
    }
    if (!has_else_branch) {
      // calling of defaults
      str = mputprintf(str, "if (%s_default_flag == ALT_MAYBE) {\n"
	"%s_default_flag = TTCN_Default::try_altsteps();\n"
	"if (%s_default_flag == ALT_YES || %s_default_flag == ALT_BREAK)"
          " break;\n"
	"else if (%s_default_flag == ALT_REPEAT) goto %s;\n"
	"}\n",
        label_str, label_str, label_str, label_str, label_str, label_str);
      str = loc.update_location_object(str);
      // error handling and taking the next snapshot in blocking mode
      str = mputstr(str, "if (");
      for (size_t i = 0; i < ags.size(); i++)
	str = mputprintf(str, "%s_alt_flag_%lu == ALT_NO && ", label_str,
          (unsigned long) i);
      str = mputprintf(str,"%s_default_flag == ALT_NO) "
	  "TTCN_error(\"None of the branches can be chosen in the alt "
	  "statement in file ", label_str);
      str = Code::translate_string(str, loc.get_filename());
      int first_line = loc.get_first_line(), last_line = loc.get_last_line();
      if (first_line < last_line) str = mputprintf(str,
	" between lines %d and %d", first_line, last_line);
      else str = mputprintf(str, ", line %d", first_line);
      str = mputstr(str, ".\");\n"
	"TTCN_Snapshot::take_new(TRUE);\n");
    }
    // end of for() statement and the statement block
    str = mputstr(str, "}\n"
      "}\n");
    return str;
  }

  char *AltGuards::generate_code_altstep(char *str)
  {
    if (!my_scope) FATAL_ERROR("AltGuards::generate_code_altstep()");
    Common::Module *my_mod = my_scope->get_scope_mod_gen();
    bool has_else_branch = has_else();
    if (!has_else_branch) {
      str = mputstr(str, "alt_status ret_val = ALT_NO;\n");
    }
    for (size_t i = 0; i < ags.size(); i++) {
      AltGuard *ag = ags[i];
      AltGuard::altguardtype_t agtype = ag->get_type();
      if (agtype == AltGuard::AG_ELSE) {
	// an else branch was found
	str = mputstr(str, "TTCN_Snapshot::else_branch_reached();\n");
	StatementBlock *block = ag->get_block();
	if (block->get_nof_stmts() > 0) {
	  str = mputstr(str, "{\n");
	  str = block->generate_code(str);
	  str = mputstr(str, "}\n");
	}
	if (block->has_return() != StatementBlock::RS_YES)
	  str = mputstr(str, "return ALT_YES;\n");
	// do not generate code for further branches
	break;
      } else {
        size_t blockcount = 0;
	Value *guard_expr = ag->get_guard_expr();
	if (guard_expr) {
	  // the branch has a boolean guard expression
	  str = guard_expr->update_location_object(str);
	  str = guard_expr->generate_code_tmp(str, "if (", blockcount);
	  str = mputstr(str, ") {\n");
	  blockcount++;
	}
	// indicates whether the guard operation might return ALT_REPEAT
	bool can_repeat;
	expression_struct expr;
	Code::init_expr(&expr);
	switch (agtype) {
	case AltGuard::AG_OP: {
	  // the guard operation is a receiving statement
	  Statement *stmt = ag->get_guard_stmt();
	  str = stmt->update_location_object(str);
	  stmt->generate_code_expr(&expr);
	  can_repeat = stmt->can_repeat();
	  break; }
	case AltGuard::AG_REF: {
	  // the guard operation is an altstep instance
	  Ref_pard *ref = ag->get_guard_ref();
	  str = ref->update_location_object(str);
	  Common::Assignment *altstep = ref->get_refd_assignment();
	  expr.expr = mputprintf(expr.expr, "%s_instance(",
	    altstep->get_genname_from_scope(my_scope).c_str());
	  ref->get_parlist()->generate_code_alias(&expr,
	    altstep->get_FormalParList(), altstep->get_RunsOnType(), false);
	  expr.expr = mputc(expr.expr, ')');
	  can_repeat = true;
	  break; }
	case AltGuard::AG_INVOKE: {
          str = ag->update_location_object(str);
          ag->generate_code_invoke_instance(&expr);
          can_repeat = true;
	  break; }
	default:
	  FATAL_ERROR("AltGuards::generate_code_altstep()");
        }
	if (expr.preamble || expr.postamble) {
	  if (blockcount == 0) {
	    // open a statement block if it is not done so far
	    str = mputstr(str, "{\n");
	    blockcount++;
	  }
	  const string& tmp_id = my_mod->get_temporary_id();
	  const char *tmp_id_str = tmp_id.c_str();
	  str = mputprintf(str, "alt_status %s;\n"
	    "{\n", tmp_id_str);
	  str = mputstr(str, expr.preamble);
	  str = mputprintf(str, "%s = %s;\n", tmp_id_str, expr.expr);
	  str = mputstr(str, expr.postamble);
	  str = mputprintf(str, "}\n"
	    "switch (%s) {\n", tmp_id_str);
	} else {
	  str = mputprintf(str, "switch (%s) {\n", expr.expr);
	}
	Code::free_expr(&expr);
	str = mputstr(str, "case ALT_YES:\n");
	StatementBlock *block = ag->get_block();
	if (block && block->get_nof_stmts() > 0) {
	  str = mputstr(str, "{\n");
	  str = block->generate_code(str);
	  str = mputstr(str, "}\n");
	}
	if (!block || block->has_return() != StatementBlock::RS_YES)
	  str = mputstr(str, "return ALT_YES;\n");
	if (can_repeat)
	  str = mputstr(str, "case ALT_REPEAT:\n"
	    "return ALT_REPEAT;\n");
        if (agtype == AltGuard::AG_REF || agtype == AltGuard::AG_INVOKE) {
          str = mputprintf(str, "case ALT_BREAK:\n"
	    "return ALT_BREAK;\n");
        }
	if (!has_else_branch)
	  str = mputstr(str, "case ALT_MAYBE:\n"
	    "ret_val = ALT_MAYBE;\n");
	str = mputstr(str, "default:\n"
	  "break;\n"
	  "}\n");
	// closing statement blocks
	for ( ; blockcount > 0; blockcount--) str = mputstr(str, "}\n");
      }
    }
    if (!has_else_branch) str = mputstr(str, "return ret_val;\n");
    return str;
  }

  char* AltGuards::generate_code_call_body(char *str, const Location& loc,
    const string& temp_id, bool in_interleave)
  {
    if (label) FATAL_ERROR("AltGuards::generate_code_call_body()");
    label = new string(temp_id);
    const char *label_str = temp_id.c_str();
    // label is needed only if there is a repeat statement in the branches
    if (has_repeat) str = mputprintf(str, "%s:\n", label_str);
    // temporary variables used for caching of status codes
    for (size_t i = 0; i < ags.size(); i++)
      str = mputprintf(str, "alt_status %s_alt_flag_%lu = %s;\n",
                       label_str, (unsigned long) i,
                       ags[i]->get_guard_expr()?"ALT_UNCHECKED":"ALT_MAYBE");
    str = loc.update_location_object(str);
    // the first snapshot is taken in non-blocking mode
    // and opening infinite for() loop
    str = mputstr(str, "TTCN_Snapshot::take_new(FALSE);\n"
                  "for ( ; ; ) {\n"); // (1)
    for (size_t i = 0; i < ags.size(); i++) {
      AltGuard *ag = ags[i];
      if (ag->get_type() != AltGuard::AG_OP)
	FATAL_ERROR("AltGuards::generate_code_call_body()");
      Value *guard_expr = ag->get_guard_expr();
      if (guard_expr) {
	// the branch has a boolean guard expression
	str = mputprintf(str,
                         "if (%s_alt_flag_%lu == ALT_UNCHECKED) {\n", // (2)
                         label_str, (unsigned long) i);
	str = guard_expr->update_location_object(str);
	expression_struct expr;
	Code::init_expr(&expr);
	guard_expr->generate_code_expr(&expr);
	str = mputstr(str, expr.preamble);
	str = mputprintf(str, "if (%s) %s_alt_flag_%lu = ALT_MAYBE;\n"
                         "else %s_alt_flag_%lu = ALT_NO;\n",
                         expr.expr, label_str, (unsigned long) i,
                         label_str, (unsigned long) i);
	str = mputstr(str, expr.postamble);
	Code::free_expr(&expr);
	str = mputstr(str, "}\n"); // (2)
      }
      // evaluation of guard operation
      str = mputprintf(str, "if (%s_alt_flag_%lu == ALT_MAYBE) {\n", // (2)
                       label_str, (unsigned long) i);
      expression_struct expr;
      Code::init_expr(&expr);
      expr.expr = mputprintf(expr.expr, "%s_alt_flag_%lu = ", label_str,
        (unsigned long) i);
      Statement *stmt = ag->get_guard_stmt();
      str = stmt->update_location_object(str);
      stmt->generate_code_expr(&expr);
      str = Code::merge_free_expr(str, &expr);
      // execution of statement block if the guard was successful
      str = mputprintf(str, "if (%s_alt_flag_%lu == ALT_YES) ", label_str,
        (unsigned long) i);
      StatementBlock *block = ag->get_block();
      if(in_interleave) {
        if(block && block->get_nof_stmts() > 0) {
          if(block->has_receiving_stmt()) {
            str = mputprintf(str, "goto %s_branch%lu;\n",
                             label_str, (unsigned long) i);
          }
          else {
            str = mputstr(str, "{\n"); // (3)
            str = block->generate_code(str);
            str = mputprintf(str, "goto %s_end;\n"
                             "}\n", // (3)
                             label_str);
          }
        }
        else str = mputprintf(str, "goto %s_end;\n", label_str);
      }
      else {
        if (block && block->get_nof_stmts() > 0) {
          str = mputstr(str, "{\n"); // (3)
          str = block->generate_code(str);
	  if (block->has_return() != StatementBlock::RS_YES)
	    str = mputstr(str, "break;\n");
          str = mputstr(str, "}\n"); // (3)
        }
        else str = mputstr(str, "break;\n");
      }
      // closing of if() block
      str = mputstr(str, "}\n"); // (2)
    }
    str = loc.update_location_object(str);
    // error handling and taking the next snapshot in blocking mode
    str = mputstr(str, "if (");
    for (size_t i = 0; i < ags.size(); i++) {
      if (i > 0) str = mputstr(str, " && ");
      str = mputprintf(str, "%s_alt_flag_%lu == ALT_NO", label_str,
        (unsigned long) i);
    }
    str = mputstr(str, ") TTCN_error(\"None of the branches can be chosen in "
      "the response and exception handling part of call statement in file ");
    str = Code::translate_string(str, loc.get_filename());
    int first_line = loc.get_first_line(), last_line = loc.get_last_line();
    if (first_line < last_line) str = mputprintf(str,
      " between lines %d and %d", first_line, last_line);
    else str = mputprintf(str, ", line %d", first_line);
    str = mputstr(str, ".\");\n"
      "TTCN_Snapshot::take_new(TRUE);\n"
      "}\n"); // (1) for
    return str;
  }

  void AltGuards::ilt_generate_code_call_body(ILT *ilt, const char *label_str)
  {
    char*& str=ilt->get_out_branches();
    for(size_t i=0; i<ags.size(); i++) {
      StatementBlock *block = ags[i]->get_block();
      if (block && block->has_receiving_stmt()) {
        str = mputprintf(str, "%s_branch%lu:\n", label_str, (unsigned long) i);
        block->ilt_generate_code(ilt);
        str = mputprintf(str, "goto %s_end;\n", label_str);
      }
    } // for i
  }

} // namespace Ttcn
