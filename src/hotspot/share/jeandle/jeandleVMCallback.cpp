/*
 * Copyright (c) 2025, the Jeandle-JDK Authors. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include "jeandle/__llvmHeadersBegin__.hpp"
#include "llvm/IR/Jeandle/VMCallback.h"
#include "llvm/IR/Jeandle/VMCallbackLog.h"

#include "jeandle/jeandleAbstractInterpreter.hpp"
#include "jeandle/jeandleCompilation.hpp"
#include "jeandle/jeandleUtils.hpp"
#include "jeandle/jeandleVMCallback.hpp"

#include "jeandle/__hotspotHeadersBegin__.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmClasses.hpp"
#include "oops/fieldInfo.inline.hpp"
#include "oops/fieldStreams.inline.hpp"
#include "oops/instanceKlass.hpp"
#include "oops/klass.inline.hpp"
#include "runtime/handles.inline.hpp"
#include "runtime/javaThread.hpp"

namespace {

bool jeandle_is_subtype(uintptr_t sub_klass, uintptr_t super_klass) {
  return ((Klass*)sub_klass)->is_subtype_of((Klass*)super_klass);
}

uintptr_t jeandle_get_common_super_klass(uintptr_t k1, uintptr_t k2) {
  Klass* lca = ((Klass*)k1)->LCA((Klass*)k2);
  return (uintptr_t)lca;
}

uintptr_t jeandle_get_field_type(uintptr_t klass_ptr, int offset) {
  Klass* klass = (Klass*)klass_ptr;
  if (!klass->is_instance_klass()) return 0;

  InstanceKlass* ik = InstanceKlass::cast(klass);
  for (JavaFieldStream fs(ik); !fs.done(); fs.next()) {
    if (fs.offset() == offset) {
      Symbol* sig = fs.signature();
      if (sig->char_at(0) == JVM_SIGNATURE_CLASS ||
          sig->char_at(0) == JVM_SIGNATURE_ARRAY) {
        Thread* current = Thread::current();
        HandleMark hm(current);
        Klass* field_klass = SystemDictionary::find_instance_or_array_klass(
            current, sig, Handle(current, ik->class_loader()),
            Handle(current, ik->protection_domain()));
        return (uintptr_t)field_klass; // 0 if not loaded
      }
      return 0; // primitive field
    }
  }
  return 0; // field not found at offset
}

bool jeandle_is_interface(uintptr_t klass_ptr) {
  return ((Klass*)klass_ptr)->is_interface();
}

bool jeandle_is_object_klass(uintptr_t klass_ptr) {
  return (Klass*)klass_ptr == vmClasses::Object_klass();
}

bool jeandle_is_effectively_final(uintptr_t klass_ptr) {
  Klass* klass = (Klass*)klass_ptr;
  if (klass->is_instance_klass())
    return InstanceKlass::cast(klass)->is_final();
  if (klass->is_typeArray_klass())
    return true;
  if (klass->is_objArray_klass())
    return jeandle_is_effectively_final(
        (uintptr_t)ObjArrayKlass::cast(klass)->bottom_klass());
  return false;
}

ciMethod* jeandle_callback_method(uintptr_t method) {
  assert(method != 0, "callback method pointer must not be null");
  return (ciMethod*)method;
}

bool jeandle_is_ok_to_inline(int scope_id, int bci, uintptr_t callee_method) {
  JeandleCompilation* comp = JeandleCompilation::current();
  assert(comp != nullptr, "Must be called in compile thread");
  JeandleInlineTree* caller_tree = comp->inline_tree_for_scope(scope_id);
  assert(caller_tree != nullptr, "caller inline tree must exist");
  ciMethod* callee = jeandle_callback_method(callee_method);
  if (caller_tree->callee_at(bci, callee) != nullptr) {
    return true;
  }
  return caller_tree->ok_to_inline(comp, callee, bci);
}

bool jeandle_record_inline_success(int scope_id, int bci, uintptr_t callee_method) {
  JeandleCompilation* comp = JeandleCompilation::current();
  assert(comp != nullptr, "Must be called in compile thread");
  ciMethod* callee = jeandle_callback_method(callee_method);

  // LLVM calls this callback only after the inline transformation succeeds.
  // Keep Jeandle's inline tree in sync at that point so later policy checks
  // account for the bytecodes that were actually inlined.
  return comp->build_inline_tree_for_callee(scope_id, bci, callee) != nullptr;
}

bool jeandle_get_inline_callee_ir(uintptr_t callee_method) {
  JeandleCompilation* comp = JeandleCompilation::current();
  assert(comp != nullptr, "Must be called in compile thread");
  llvm::Module* M = comp->llvm_module();
  ciMethod* callee = jeandle_callback_method(callee_method);
  std::string callee_name = JeandleFuncSig::method_name_with_signature(callee);
  llvm::Function* callee_func = M->getFunction(callee_name);
  if (callee_func != nullptr && !callee_func->isDeclaration()) {
    return true;
  }
  {
    JeandleParseContext parse_context = JeandleParseContext::inlinee(callee);
    JeandleAbstractInterpreter interpret(parse_context, -1, *M, *comp->compiled_code(), comp->trap_hist());
    llvm::Function* resolved_func = M->getFunction(callee_name);
    assert(resolved_func != nullptr, "callee function not found");
    JeandleFuncSig::setup_description(resolved_func, callee->is_accessor());
    JeandleFuncSig::setup_java_method_pointer(resolved_func, callee);
    resolved_func->setLinkage(llvm::GlobalValue::AvailableExternallyLinkage);
  }
  return !comp->error_occurred();
}

bool jeandle_record_inlining_complete() {
  if (JeandleRecordVMCallbacks) {
    JeandleCompilation* comp = JeandleCompilation::current();
    assert(comp != nullptr, "Must be called in compile thread");
    comp->dump_inline_callee_replay_module();
  }
  return true;
}

} // anonymous namespace

void register_jeandle_vm_callbacks() {
  llvm::jeandle::VMCallbacks callbacks;
  callbacks.IsSubtype = &jeandle_is_subtype;
  callbacks.GetCommonSuperKlass = &jeandle_get_common_super_klass;
  callbacks.GetFieldType = &jeandle_get_field_type;
  callbacks.IsInterface = &jeandle_is_interface;
  callbacks.IsObjectKlass = &jeandle_is_object_klass;
  callbacks.IsEffectivelyFinal = &jeandle_is_effectively_final;
  callbacks.GetInlineCalleeIR = &jeandle_get_inline_callee_ir;
  callbacks.IsOkToInline = &jeandle_is_ok_to_inline;
  callbacks.RecordInlineSuccess = &jeandle_record_inline_success;
  callbacks.RecordInliningComplete = &jeandle_record_inlining_complete;
  llvm::jeandle::registerVMCallbacks(callbacks);

  if (JeandleRecordVMCallbacks) {
    llvm::jeandle::enableVMCallbackRecording();
  }
}
