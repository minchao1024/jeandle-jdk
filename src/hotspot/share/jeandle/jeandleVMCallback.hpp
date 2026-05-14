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

#ifndef SHARE_JEANDLE_VM_CALLBACK_HPP
#define SHARE_JEANDLE_VM_CALLBACK_HPP

#include "jeandle/__llvmHeadersBegin__.hpp"
#include "llvm/IR/Module.h"


// Register VM callbacks (type hierarchy queries) with the LLVM-side
// optimization pipeline. Called once during JeandleCompiler::initialize().
void register_jeandle_vm_callbacks();

// Check if the callee should be inlined based on CompilerOracle directives.
// caller_name is optional (may be nullptr).
bool jeandle_should_inline(const char* caller_name, const char* callee_name);

// Resolve and compile the callee's IR into the given module if not already present.
bool jeandle_resolve_callee(const char* callee_name, llvm::Module& M);

#endif // SHARE_JEANDLE_VM_CALLBACK_HPP
