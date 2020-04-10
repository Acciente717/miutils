/* Copyright [2019] Zhiyao Ma */
#ifndef MACROS_HPP_
#define MACROS_HPP_

#define XMM_WIDTH 16

#if defined(__SSE__) && defined(__POPCNT__)
#define ACCEL_AVAIL 1
#endif

#define if_likely(x)      if (__builtin_expect(static_cast<bool>(x), true))
#define if_unlikely(x)    if (__builtin_expect(static_cast<bool>(x), false))

#endif  // MACROS_HPP_
