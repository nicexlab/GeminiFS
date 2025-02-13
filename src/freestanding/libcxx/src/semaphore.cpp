//===------------------------ semaphore.cpp -------------------------------===//
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
#include <simt/semaphore>
#else
#include "barrier"
#endif

_LIBCPP_BEGIN_NAMESPACE_STD

#if !defined(_LIBCPP_HAS_NO_SEMAPHORES)

_LIBCPP_EXPORTED_FROM_ABI
__sem_semaphore_basic_base::__sem_semaphore_basic_base(ptrdiff_t __count) : 
    __semaphore()
{ 
    __libcpp_semaphore_init(&__semaphore, __count);
}
_LIBCPP_EXPORTED_FROM_ABI
__sem_semaphore_basic_base::~__sem_semaphore_basic_base() {
#ifdef __APPLE__
    auto __b = __balance.load(memory_order_relaxed);
    for(; __b > 0; --__b) __libcpp_semaphore_wait(&__semaphore);
    for(; __b < 0; ++__b) __libcpp_semaphore_post(&__semaphore);
#endif
    __libcpp_semaphore_destroy(&__semaphore);
}
_LIBCPP_EXPORTED_FROM_ABI
void __sem_semaphore_basic_base::release(ptrdiff_t __update) 
{
#ifdef __APPLE__
    __balance.fetch_add(__update, memory_order_relaxed);
#endif
    for(; __update; --__update)
        __libcpp_semaphore_post(&__semaphore);
}
_LIBCPP_EXPORTED_FROM_ABI
void __sem_semaphore_basic_base::acquire() 
{
    __libcpp_semaphore_wait(&__semaphore);
#ifdef __APPLE__
    __balance.fetch_sub(1, memory_order_relaxed);
#endif
}
_LIBCPP_EXPORTED_FROM_ABI
bool __sem_semaphore_basic_base::try_acquire_for(chrono::nanoseconds __rel_time) 
{
    auto const __success = __libcpp_semaphore_wait_timed(&__semaphore, __rel_time);
#ifdef __APPLE__
    __balance.fetch_sub(1, memory_order_relaxed);
#endif
    return __success;
}

#ifndef _LIBCPP_HAS_NO_SEMAPHORE_BACK_BUFFER

_LIBCPP_INLINE_VISIBILITY
void __sem_semaphore_back_buffered_base::__backfill() 
{
    ptrdiff_t __expect = 2;
    while(__expect != 0) 
    {
        ptrdiff_t const __sub = __expect > 1 ? 2 : 1;
        if(!__backbuffer.compare_exchange_weak(__expect, __expect - __sub, memory_order_acquire, memory_order_relaxed))
            continue;
        if(__sub > 1)
            __semaphore.release(1);
        __semaphore.release(1);
        break;
    }
}    
_LIBCPP_EXPORTED_FROM_ABI
__sem_semaphore_back_buffered_base::__sem_semaphore_back_buffered_base(ptrdiff_t __count) : 
    __semaphore(__count), __backbuffer(0)
{ 
}
_LIBCPP_EXPORTED_FROM_ABI
__sem_semaphore_back_buffered_base::~__sem_semaphore_back_buffered_base()
{ 
}
_LIBCPP_EXPORTED_FROM_ABI
void __sem_semaphore_back_buffered_base::release(ptrdiff_t __update) 
{
    if(__update > 2)
        __backbuffer.fetch_add(__update - 2, memory_order_acq_rel);
    if(__update > 1)
        __semaphore.release(1);
    __semaphore.release(1);
}
_LIBCPP_EXPORTED_FROM_ABI
void __sem_semaphore_back_buffered_base::acquire() 
{
    __semaphore.acquire();
    __backfill();
}
_LIBCPP_EXPORTED_FROM_ABI
bool __sem_semaphore_back_buffered_base::try_acquire_for(chrono::nanoseconds __rel_time) 
{
    if(!__semaphore.try_acquire_for(__rel_time))
        return false;
    __backfill();
    return true;
}

#endif //_LIBCPP_HAS_NO_SEMAPHORE_BACK_BUFFER

#ifndef _LIBCPP_HAS_NO_SEMAPHORE_FRONT_BUFFER

_LIBCPP_INLINE_VISIBILITY
bool __sem_semaphore_front_buffered_base::__try_acquire_fast() 
{
    ptrdiff_t __old;
    __libcpp_thread_poll_with_backoff([&]() {
        __old = __frontbuffer.load(memory_order_relaxed);
        return 0 != (__old >> 32);
    }, chrono::microseconds(5));
    // always steal if you can
    while(__old >> 32)
        if(__frontbuffer.compare_exchange_weak(__old, __old - (1ll << 32), memory_order_acquire))
            return true;
    // record we're waiting
    __old = __frontbuffer.fetch_add(1ll, memory_order_release);
    // ALWAYS steal if you can!
    while(__old >> 32)
        if(__frontbuffer.compare_exchange_weak(__old, __old - (1ll << 32), memory_order_acquire))
            break;
    // not going to wait after all
    if(__old >> 32) {
        __try_done();
        return true;
    }
    // the wait has begun...
    return false;
}
_LIBCPP_INLINE_VISIBILITY
void __sem_semaphore_front_buffered_base::__try_done() 
{
    // record we're NOT waiting
    __frontbuffer.fetch_sub(1ll, memory_order_release);
}
_LIBCPP_EXPORTED_FROM_ABI
__sem_semaphore_front_buffered_base::__sem_semaphore_front_buffered_base(ptrdiff_t __count) : 
    __semaphore(0), __frontbuffer(__count << 32)
{ 
}
_LIBCPP_EXPORTED_FROM_ABI
__sem_semaphore_front_buffered_base::~__sem_semaphore_front_buffered_base() 
{
}
_LIBCPP_EXPORTED_FROM_ABI
void __sem_semaphore_front_buffered_base::release(ptrdiff_t __update) 
{
    // boldly assume the semaphore is taken but uncontended
    ptrdiff_t __old = 0;
    // try to fast-release as long as it's uncontended
    while(0 == (__old & ~0ul))
        if(__frontbuffer.compare_exchange_weak(__old, __old + (__update << 32), memory_order_acq_rel))
            return;
    __semaphore.release(__update);
}
_LIBCPP_EXPORTED_FROM_ABI
void __sem_semaphore_front_buffered_base::acquire() 
{
    if(__try_acquire_fast())
        return;
    __semaphore.acquire();
    __try_done();
}
_LIBCPP_EXPORTED_FROM_ABI
bool __sem_semaphore_front_buffered_base::try_acquire_for(chrono::nanoseconds __rel_time) 
{
    if(__try_acquire_fast())
        return true;
    auto const __success = __semaphore.try_acquire_for(__rel_time);
    __try_done();
    return __success;
}

#endif //_LIBCPP_HAS_NO_SEMAPHORE_FRONT_BUFFER

#endif

_LIBCPP_END_NAMESPACE_STD

#endif //_LIBCPP_HAS_NO_THREADS
