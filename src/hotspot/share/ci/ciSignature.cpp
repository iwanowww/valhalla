/*
 * Copyright (c) 1999, 2020, Oracle and/or its affiliates. All rights reserved.
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
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

#include "precompiled.hpp"
#include "ci/ciMethodType.hpp"
#include "ci/ciSignature.hpp"
#include "ci/ciUtilities.inline.hpp"
#include "memory/allocation.inline.hpp"
#include "memory/resourceArea.hpp"
#include "oops/oop.inline.hpp"
#include "runtime/signature.hpp"

// ciSignature
//
// This class represents the signature of a method.

// ------------------------------------------------------------------
// ciSignature::ciSignature
ciSignature::ciSignature(ciKlass* accessing_klass, const constantPoolHandle& cpool, ciSymbol* symbol) {
  ASSERT_IN_VM;
  EXCEPTION_CONTEXT;
  assert(accessing_klass != NULL, "need origin of access");
  _accessing_klass = accessing_klass;
  _symbol = symbol;

  ciEnv* env = CURRENT_ENV;
  Arena* arena = env->arena();
  _types = new (arena) GrowableArray<ciType*>(arena, 8, 0, NULL);

  int size = 0;
  int count = 0;
  ResourceMark rm(THREAD);
  Symbol* sh = symbol->get_symbol();
  SignatureStream ss(sh);
  for (; ; ss.next()) {
    // Process one element of the signature
    ciType* type;
    if (!ss.is_reference()) {
      type = ciType::make(ss.type());
    } else {
      ciSymbol* klass_name = env->get_symbol(ss.as_symbol());
      type = env->get_klass_by_name_impl(_accessing_klass, cpool, klass_name, false);
    }
    if (type->is_valuetype() && ss.type() == T_VALUETYPE) {
      type = env->make_never_null_wrapper(type);
    }
    _types->append(type);
    if (ss.at_return_type()) {
      // Done processing the return type; do not add it into the count.
      break;
    }
    size += type->size();
    count++;
  }
  _size = size;
  _count = count;
}

// ------------------------------------------------------------------
// ciSignature::ciSignature
ciSignature::ciSignature(ciKlass* accessing_klass, ciSymbol* symbol, ciMethodType* method_type) :
  _symbol(symbol),
  _accessing_klass(accessing_klass),
  _size( method_type->ptype_slot_count()),
  _count(method_type->ptype_count())
{
  ASSERT_IN_VM;
  EXCEPTION_CONTEXT;
  ciEnv* env =  CURRENT_ENV;
  Arena* arena = env->arena();
  _types = new (arena) GrowableArray<ciType*>(arena, _count + 1, 0, NULL);
  ciType* type = NULL;
  bool never_null = false;
  for (int i = 0; i < _count; i++) {
    type = method_type->ptype_at(i, never_null);
    if (type->is_valuetype() && never_null) {
      type = env->make_never_null_wrapper(type);
    }
    _types->append(type);
  }
  type = method_type->rtype(never_null);
  if (type->is_valuetype() && never_null) {
    type = env->make_never_null_wrapper(type);
  }
  _types->append(type);
}

// ------------------------------------------------------------------
// ciSignature::return_type
//
// What is the return type of this signature?
ciType* ciSignature::return_type() const {
  return _types->at(_count)->unwrap();
}

// ------------------------------------------------------------------
// ciSignature::type_at
//
// What is the type of the index'th element of this
// signature?
ciType* ciSignature::type_at(int index) const {
  assert(index < _count, "out of bounds");
  // The first _klasses element holds the return klass.
  return _types->at(index)->unwrap();
}

// ------------------------------------------------------------------
// ciSignature::return_never_null
//
// True if we statically know that the return value is never null.
bool ciSignature::returns_never_null() const {
  return _types->at(_count)->is_never_null();
}

// ------------------------------------------------------------------
// ciSignature::maybe_return_never_null
//
// True if we statically know that the return value is never null, or
// if the return type has a Q signature but is not yet loaded, in which case
// it could be a never-null type.
bool ciSignature::maybe_returns_never_null() const {
  ciType* ret_type = _types->at(_count);
  if (ret_type->is_never_null()) {
    return true;
  } else if (ret_type->is_instance_klass() && !ret_type->as_instance_klass()->is_loaded()) {
    GUARDED_VM_ENTRY(if (get_symbol()->is_Q_method_signature()) { return true; })
  }
  return false;
}

// ------------------------------------------------------------------
// ciSignature::never_null_at
//
// True if we statically know that the argument at 'index' is never null.
bool ciSignature::is_never_null_at(int index) const {
  assert(index < _count, "out of bounds");
  return _types->at(index)->is_never_null();
}

// ------------------------------------------------------------------
// ciSignature::equals
//
// Compare this signature to another one.  Signatures with different
// accessing classes but with signature-types resolved to the same
// types are defined to be equal.
bool ciSignature::equals(ciSignature* that) {
  // Compare signature
  if (!this->as_symbol()->equals(that->as_symbol()))  return false;
  // Compare all types of the arguments
  for (int i = 0; i < _count; i++) {
    if (this->type_at(i) != that->type_at(i))         return false;
  }
  // Compare the return type
  if (this->return_type() != that->return_type())     return false;
  return true;
}

// ------------------------------------------------------------------
// ciSignature::print_signature
void ciSignature::print_signature() {
  _symbol->print_symbol();
}

// ------------------------------------------------------------------
// ciSignature::print
void ciSignature::print() {
  tty->print("<ciSignature symbol=");
  print_signature();
 tty->print(" accessing_klass=");
  _accessing_klass->print();
  tty->print(" address=" INTPTR_FORMAT ">", p2i((address)this));
}
