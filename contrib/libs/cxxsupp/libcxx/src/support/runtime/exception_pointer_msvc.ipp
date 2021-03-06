// -*- C++ -*-
//===----------------------------------------------------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is dual licensed under the MIT and the University of Illinois Open
// Source Licenses. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <stdio.h>
#include <stdlib.h>

#if !defined(_CRTIMP2_PURE)
#define _CRTIMP2_PURE
#endif

#if !defined(__CLRCALL_PURE_OR_CDECL)
#define __CLRCALL_PURE_OR_CDECL __cdecl
#endif

_CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL __ExceptionPtrCreate(void*);
_CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL __ExceptionPtrDestroy(void*);
_CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL __ExceptionPtrCopy(void*,
                                                              const void*);
_CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL
__ExceptionPtrAssign(void*, const void*);
_CRTIMP2_PURE bool __CLRCALL_PURE_OR_CDECL
__ExceptionPtrCompare(const void*, const void*);
_CRTIMP2_PURE bool __CLRCALL_PURE_OR_CDECL
__ExceptionPtrToBool(const void*);
_CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL __ExceptionPtrSwap(void*, void*);
_CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL
__ExceptionPtrCurrentException(void*);
[[noreturn]] _CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL
__ExceptionPtrRethrow(const void*);
_CRTIMP2_PURE void __CLRCALL_PURE_OR_CDECL
__ExceptionPtrCopyException(void*, const void*, const void*);

namespace std {

exception_ptr::exception_ptr() _NOEXCEPT { __ExceptionPtrCreate(this); }
exception_ptr::exception_ptr(nullptr_t) _NOEXCEPT { __ExceptionPtrCreate(this); }

exception_ptr::exception_ptr(const exception_ptr& __other) _NOEXCEPT {
  __ExceptionPtrCopy(this, &__other);
}
exception_ptr& exception_ptr::operator=(const exception_ptr& __other) _NOEXCEPT {
  __ExceptionPtrAssign(this, &__other);
  return *this;
}

exception_ptr& exception_ptr::operator=(nullptr_t) _NOEXCEPT {
  exception_ptr dummy;
  __ExceptionPtrAssign(this, &dummy);
  return *this;
}

exception_ptr::~exception_ptr() _NOEXCEPT { __ExceptionPtrDestroy(this); }

exception_ptr::operator bool() const _NOEXCEPT {
  return __ExceptionPtrToBool(this);
}

bool operator==(const exception_ptr& __x, const exception_ptr& __y) _NOEXCEPT {
  return __ExceptionPtrCompare(&__x, &__y);
}


void swap(exception_ptr& lhs, exception_ptr& rhs) _NOEXCEPT {
  __ExceptionPtrSwap(&rhs, &lhs);
}

exception_ptr __copy_exception_ptr(void* __exception, const void* __ptr) {
  exception_ptr __ret = nullptr;
  if (__ptr)
    __ExceptionPtrCopyException(&__ret, __exception, __ptr);
  return __ret;
}

exception_ptr current_exception() _NOEXCEPT {
  exception_ptr __ret;
  __ExceptionPtrCurrentException(&__ret);
  return __ret;
}

_LIBCPP_NORETURN
void rethrow_exception(exception_ptr p) { __ExceptionPtrRethrow(&p); }

nested_exception::nested_exception() _NOEXCEPT : __ptr_(current_exception()) {}

nested_exception::~nested_exception() _NOEXCEPT {}

_LIBCPP_NORETURN
void nested_exception::rethrow_nested() const {
  if (__ptr_ == nullptr)
    terminate();
  rethrow_exception(__ptr_);
}

} // namespace std
