//===-- Implementation of the C++20 bit header  -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// This is inspired by LLVM ADT/bit.h header.
// Some functions are missing, we can add them as needed (popcount, byteswap).

#ifndef LLVM_LIBC_SRC___SUPPORT_CPP_BIT_H
#define LLVM_LIBC_SRC___SUPPORT_CPP_BIT_H

#include "src/__support/CPP/limits.h" // numeric_limits
#include "src/__support/CPP/type_traits.h"
#include "src/__support/macros/attributes.h"
#include "src/__support/macros/config.h" // LIBC_HAS_BUILTIN
#include "src/__support/macros/sanitizer.h"

#include <stdint.h>

namespace LIBC_NAMESPACE::cpp {

#if LIBC_HAS_BUILTIN(__builtin_memcpy_inline)
#define LLVM_LIBC_HAS_BUILTIN_MEMCPY_INLINE
#endif

// This implementation of bit_cast requires trivially-constructible To, to avoid
// UB in the implementation.
template <
    typename To, typename From,
    typename = cpp::enable_if_t<sizeof(To) == sizeof(From) &&
                                cpp::is_trivially_constructible<To>::value &&
                                cpp::is_trivially_copyable<To>::value &&
                                cpp::is_trivially_copyable<From>::value>>
LIBC_INLINE constexpr To bit_cast(const From &from) {
  MSAN_UNPOISON(&from, sizeof(From));
#if LIBC_HAS_BUILTIN(__builtin_bit_cast)
  return __builtin_bit_cast(To, from);
#else
  To to;
  char *dst = reinterpret_cast<char *>(&to);
  const char *src = reinterpret_cast<const char *>(&from);
#if LIBC_HAS_BUILTIN(__builtin_memcpy_inline)
  __builtin_memcpy_inline(dst, src, sizeof(To));
#else
  for (unsigned i = 0; i < sizeof(To); ++i)
    dst[i] = src[i];
#endif // LIBC_HAS_BUILTIN(__builtin_memcpy_inline)
  return to;
#endif // LIBC_HAS_BUILTIN(__builtin_bit_cast)
}

template <typename T, typename = cpp::enable_if_t<cpp::is_unsigned_v<T>>>
[[nodiscard]] LIBC_INLINE constexpr bool has_single_bit(T value) {
  return (value != 0) && ((value & (value - 1)) == 0);
}

// A temporary macro to add template function specialization when compiler
// builtin is available.
#define ADD_SPECIALIZATION(NAME, TYPE, BUILTIN)                                \
  template <> [[nodiscard]] LIBC_INLINE constexpr int NAME<TYPE>(TYPE value) { \
    static_assert(cpp::is_unsigned_v<TYPE>);                                   \
    return value == 0 ? cpp::numeric_limits<TYPE>::digits : BUILTIN(value);    \
  }

/// Count number of 0's from the least significant bit to the most
///   stopping at the first 1.
///
/// Only unsigned integral types are allowed.
///
/// Returns cpp::numeric_limits<T>::digits on an input of 0.
template <typename T, typename = cpp::enable_if_t<cpp::is_unsigned_v<T>>>
[[nodiscard]] LIBC_INLINE constexpr int countr_zero(T value) {
  if (!value)
    return cpp::numeric_limits<T>::digits;
  if (value & 0x1)
    return 0;
  // Bisection method.
  unsigned zero_bits = 0;
  unsigned shift = cpp::numeric_limits<T>::digits >> 1;
  T mask = cpp::numeric_limits<T>::max() >> shift;
  while (shift) {
    if ((value & mask) == 0) {
      value >>= shift;
      zero_bits |= shift;
    }
    shift >>= 1;
    mask >>= shift;
  }
  return zero_bits;
}
#if LIBC_HAS_BUILTIN(__builtin_ctzs)
ADD_SPECIALIZATION(countr_zero, unsigned short, __builtin_ctzs)
#endif
ADD_SPECIALIZATION(countr_zero, unsigned int, __builtin_ctz)
ADD_SPECIALIZATION(countr_zero, unsigned long, __builtin_ctzl)
ADD_SPECIALIZATION(countr_zero, unsigned long long, __builtin_ctzll)

/// Count number of 0's from the most significant bit to the least
///   stopping at the first 1.
///
/// Only unsigned integral types are allowed.
///
/// Returns cpp::numeric_limits<T>::digits on an input of 0.
template <typename T, typename = cpp::enable_if_t<cpp::is_unsigned_v<T>>>
[[nodiscard]] LIBC_INLINE constexpr int countl_zero(T value) {
  if (!value)
    return cpp::numeric_limits<T>::digits;
  // Bisection method.
  unsigned zero_bits = 0;
  for (unsigned shift = cpp::numeric_limits<T>::digits >> 1; shift;
       shift >>= 1) {
    T tmp = value >> shift;
    if (tmp)
      value = tmp;
    else
      zero_bits |= shift;
  }
  return zero_bits;
}
#if LIBC_HAS_BUILTIN(__builtin_clzs)
ADD_SPECIALIZATION(countl_zero, unsigned short, __builtin_clzs)
#endif
ADD_SPECIALIZATION(countl_zero, unsigned int, __builtin_clz)
ADD_SPECIALIZATION(countl_zero, unsigned long, __builtin_clzl)
ADD_SPECIALIZATION(countl_zero, unsigned long long, __builtin_clzll)

#undef ADD_SPECIALIZATION

/// Count the number of ones from the most significant bit to the first
/// zero bit.
///
/// Ex. countl_one(0xFF0FFF00) == 8.
/// Only unsigned integral types are allowed.
///
/// Returns cpp::numeric_limits<T>::digits on an input of all ones.
template <typename T, typename = cpp::enable_if_t<cpp::is_unsigned_v<T>>>
[[nodiscard]] LIBC_INLINE constexpr int countl_one(T value) {
  return cpp::countl_zero<T>(~value);
}

/// Count the number of ones from the least significant bit to the first
/// zero bit.
///
/// Ex. countr_one(0x00FF00FF) == 8.
/// Only unsigned integral types are allowed.
///
/// Returns cpp::numeric_limits<T>::digits on an input of all ones.
template <typename T, typename = cpp::enable_if_t<cpp::is_unsigned_v<T>>>
[[nodiscard]] LIBC_INLINE constexpr int countr_one(T value) {
  return cpp::countr_zero<T>(~value);
}

/// Returns the number of bits needed to represent value if value is nonzero.
/// Returns 0 otherwise.
///
/// Ex. bit_width(5) == 3.
template <typename T, typename = cpp::enable_if_t<cpp::is_unsigned_v<T>>>
[[nodiscard]] LIBC_INLINE constexpr int bit_width(T value) {
  return cpp::numeric_limits<T>::digits - cpp::countl_zero(value);
}

/// Returns the largest integral power of two no greater than value if value is
/// nonzero.  Returns 0 otherwise.
///
/// Ex. bit_floor(5) == 4.
template <typename T, typename = cpp::enable_if_t<cpp::is_unsigned_v<T>>>
[[nodiscard]] LIBC_INLINE constexpr T bit_floor(T value) {
  if (!value)
    return 0;
  return T(1) << (cpp::bit_width(value) - 1);
}

/// Returns the smallest integral power of two no smaller than value if value is
/// nonzero.  Returns 1 otherwise.
///
/// Ex. bit_ceil(5) == 8.
///
/// The return value is undefined if the input is larger than the largest power
/// of two representable in T.
template <typename T, typename = cpp::enable_if_t<cpp::is_unsigned_v<T>>>
[[nodiscard]] LIBC_INLINE constexpr T bit_ceil(T value) {
  if (value < 2)
    return 1;
  return T(1) << cpp::bit_width<T>(value - 1u);
}

// Rotate algorithms make use of "Safe, Efficient, and Portable Rotate in C/C++"
// from https://blog.regehr.org/archives/1063.

// Forward-declare rotr so that rotl can use it.
template <typename T, typename = cpp::enable_if_t<cpp::is_unsigned_v<T>>>
[[nodiscard]] LIBC_INLINE constexpr T rotr(T value, int rotate);

template <typename T, typename = cpp::enable_if_t<cpp::is_unsigned_v<T>>>
[[nodiscard]] LIBC_INLINE constexpr T rotl(T value, int rotate) {
  constexpr unsigned N = cpp::numeric_limits<T>::digits;
  rotate = rotate % N;
  if (!rotate)
    return value;
  if (rotate < 0)
    return cpp::rotr(value, -rotate);
  return (value << rotate) | (value >> (N - rotate));
}

template <typename T, typename>
[[nodiscard]] LIBC_INLINE constexpr T rotr(T value, int rotate) {
  constexpr unsigned N = cpp::numeric_limits<T>::digits;
  rotate = rotate % N;
  if (!rotate)
    return value;
  if (rotate < 0)
    return cpp::rotl(value, -rotate);
  return (value >> rotate) | (value << (N - rotate));
}

// TODO: Do we need this function at all? How is it different from
// 'static_cast'?
template <class To, class From>
LIBC_INLINE constexpr To bit_or_static_cast(const From &from) {
  if constexpr (sizeof(To) == sizeof(From)) {
    return bit_cast<To>(from);
  } else {
    return static_cast<To>(from);
  }
}

template <typename T, typename = cpp::enable_if_t<cpp::is_unsigned_v<T>>>
[[nodiscard]] LIBC_INLINE constexpr int first_leading_zero(T value) {
  return value == cpp::numeric_limits<T>::max() ? 0 : countl_one(value) + 1;
}

template <typename T, typename = cpp::enable_if_t<cpp::is_unsigned_v<T>>>
[[nodiscard]] LIBC_INLINE constexpr int first_leading_one(T value) {
  return first_leading_zero(static_cast<T>(~value));
}

} // namespace LIBC_NAMESPACE::cpp

#endif // LLVM_LIBC_SRC___SUPPORT_CPP_BIT_H
