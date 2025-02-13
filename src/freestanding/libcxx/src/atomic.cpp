//===------------------------- atomic.cpp ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifdef _LIBCPP_SIMT
#include <details/__config>
#else
#include "__config"
#endif

#ifndef _LIBCPP_HAS_NO_THREADS
#ifdef _LIBCPP_SIMT
#include <simt/atomic>
#else
#include "atomic"
#endif

_LIBCPP_BEGIN_NAMESPACE_STD

#if !defined(_LIBCPP_HAS_NO_THREAD_CONTENTION_TABLE)

__libcpp_contention_t __libcpp_contention_state_[ 256 /* < there's no magic in this number */ ];

_LIBCPP_FUNC_VIS
__libcpp_contention_t * __libcpp_contention_state(void const volatile * p) _NOEXCEPT {
    return __libcpp_contention_state_ + ((std::uintptr_t)p & 255);
}

#endif //_LIBCPP_HAS_NO_THREAD_CONTENTION_TABLE

_LIBCPP_END_NAMESPACE_STD

#endif //_LIBCPP_HAS_NO_THREADS
