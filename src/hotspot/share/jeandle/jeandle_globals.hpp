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

#ifndef SHARE_JEANDLE_GLOBALS_HPP
#define SHARE_JEANDLE_GLOBALS_HPP

#include "runtime/globals_shared.hpp"
#include "utilities/macros.hpp"
//
// Declare all global flags used by jeandle.
//
#define JEANDLE_FLAGS(develop,                                              \
                      develop_pd,                                           \
                      product,                                              \
                      product_pd,                                           \
                      notproduct,                                           \
                      range,                                                \
                      constraint)                                           \
                                                                            \
  product(bool, JeandleDumpObjects, false,                                  \
          "Dump object files after compilation")                            \
                                                                            \
  product(bool, JeandleDumpIR, false,                                       \
          "Dump ir before and after optimization")                          \
                                                                            \
  product(ccstr, JeandleDumpDirectory, nullptr,                             \
          "Dump destination for all Jeandle items")                         \
                                                                            \
  develop(bool, JeandleCrashOnError, DEBUG_ONLY(true) NOT_DEBUG(false),     \
          "Crash JVM on Jeandle errors")                                    \
                                                                            \
  product(bool, JeandleDumpRuntimeStubs, false,                             \
          "Dump Jeandle runtime stubs")                                     \
                                                                            \

// end of JEANDLE_FLAGS

DECLARE_FLAGS(JEANDLE_FLAGS)

#endif // SHARE_JEANDLE_GLOBALS_HPP
