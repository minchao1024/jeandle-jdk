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
#include "jeandle/jeandleVMCallback.hpp"

#include "jeandle/__hotspotHeadersBegin__.hpp"
#include "classfile/systemDictionary.hpp"
#include "classfile/vmClasses.hpp"
#include "compiler/compilerOracle.hpp"
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

} // anonymous namespace

bool jeandle_should_inline(uintptr_t caller_name, uintptr_t callee_name) {
  JeandleCompilation* comp = JeandleCompilation::current();
  if (comp == nullptr) {
    return false;
  }
  ciMethod* callee = comp->get_inline_candidate((const char*)callee_name);
  if (callee == nullptr) {
    return false;
  }
  return CompilerOracle::should_inline(methodHandle(Thread::current(), callee->get_Method()));
}

bool jeandle_resolve_callee(uintptr_t callee_name) {
  JeandleCompilation* comp = JeandleCompilation::current();
  if (comp == nullptr) {
    return false;
  }
  llvm::Module* M = comp->llvm_module();
  llvm::Function* callee_func = M->getFunction((const char*)callee_name);
  if (callee_func != nullptr && !callee_func->isDeclaration()) {
    return true;
  }
  ciMethod* callee = comp->get_inline_candidate((const char*)callee_name);
  if (callee == nullptr) {
    return false;
  }
  {
    SetInlinee inlinee_guard(callee);
    JeandleAbstractInterpreter interpret(callee, -1, *M, *comp->compiled_code());
    llvm::Function* resolved_func = M->getFunction((const char*)callee_name);
    assert(resolved_func != nullptr, "callee function not found");
    JeandleFuncSig::setup_description(resolved_func);
    resolved_func->setLinkage(llvm::GlobalValue::AvailableExternallyLinkage);
  }
  return !comp->error_occurred();
}

void register_jeandle_vm_callbacks() {
  llvm::jeandle::VMCallbacks callbacks;
  callbacks.IsSubtype = &jeandle_is_subtype;
  callbacks.GetCommonSuperKlass = &jeandle_get_common_super_klass;
  callbacks.GetFieldType = &jeandle_get_field_type;
  callbacks.IsInterface = &jeandle_is_interface;
  callbacks.IsObjectKlass = &jeandle_is_object_klass;
  callbacks.IsEffectivelyFinal = &jeandle_is_effectively_final;
  callbacks.ShouldInline = &jeandle_should_inline;
  callbacks.ResolveCallee = &jeandle_resolve_callee;
  llvm::jeandle::registerVMCallbacks(callbacks);

  if (JeandleRecordVMCallbacks) {
    llvm::jeandle::enableVMCallbackRecording();
  }
}
