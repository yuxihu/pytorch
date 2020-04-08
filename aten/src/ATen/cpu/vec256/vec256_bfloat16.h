#pragma once

#include <ATen/cpu/vec256/intrinsics.h>
#include <ATen/cpu/vec256/vec256_base.h>
#if defined(__AVX__) && !defined(_MSC_VER)
#include <sleef.h>
#endif

namespace at {
namespace vec256 {
// See Note [Acceptable use of anonymous namespace in header]
namespace {

#if defined(__AVX2__) && !defined(_MSC_VER)

static inline void cvtbf16_fp32(const __m256i& a, __m256& o1, __m256& o2) {
  __m128i lo = _mm256_extractf128_si256(a, 0);
  __m128i hi = _mm256_extractf128_si256(a, 1);
  o1 = _mm256_castsi256_ps(_mm256_slli_epi32(_mm256_cvtepu16_epi32(lo), 16));
  o2 = _mm256_castsi256_ps(_mm256_slli_epi32(_mm256_cvtepu16_epi32(hi), 16));
}
static inline __m256i cvtfp32_bf16(const __m256& a, const __m256& b) {
  __m256i lo = _mm256_castps_si256(a);
  __m256i hi = _mm256_castps_si256(b);
  __m256i nan = _mm256_set1_epi32(0x7fc0);
  __m256i mask_lo = _mm256_castps_si256(_mm256_cmp_ps(a, a, _CMP_ORD_Q));
  __m256i mask_hi = _mm256_castps_si256(_mm256_cmp_ps(b, b, _CMP_ORD_Q));
  __m256i ones = _mm256_set1_epi32(0x1);
  __m256i vec_bias = _mm256_set1_epi32(0x7fff);
  // uint32_t lsb = (input >> 16) & 1;
  auto t_lo = _mm256_and_si256(_mm256_srli_epi32(lo, 16), ones);
  auto t_hi = _mm256_and_si256(_mm256_srli_epi32(hi, 16), ones);
  // uint32_t rounding_bias = 0x7fff + lsb;
  t_lo = _mm256_add_epi32(t_lo, vec_bias);
  t_hi = _mm256_add_epi32(t_hi, vec_bias);
  // input += rounding_bias;
  t_lo = _mm256_add_epi32(t_lo, lo);
  t_hi = _mm256_add_epi32(t_hi, hi);
  // input = input >> 16;
  t_lo = _mm256_srli_epi32(t_lo, 16);
  t_hi = _mm256_srli_epi32(t_hi, 16);
  // Check NaN before converting back to bf16
  t_lo = _mm256_blendv_epi8(nan, t_lo, mask_lo);
  t_hi = _mm256_blendv_epi8(nan, t_hi, mask_hi);

  t_lo = _mm256_packus_epi32(t_lo, t_hi);      // t_hi[4-7] t_lo[4-7] t_hi[0-4] t_lo[0-4]
  return _mm256_permute4x64_epi64(t_lo, 0xd8); // 11        01        10        00
}

template <> class Vec256<BFloat16> {
private:
  __m256i values;
public:
  using value_type = uint16_t;
  static constexpr int size() {
    return 16;
  }
  Vec256() {}
  Vec256(__m256i v) : values(v) {}
  Vec256(BFloat16 val) {
    value_type uw = val.x;
    values = _mm256_set1_epi16(uw);
  }
  Vec256(BFloat16 val1, BFloat16 val2, BFloat16 val3, BFloat16 val4,
         BFloat16 val5, BFloat16 val6, BFloat16 val7, BFloat16 val8,
         BFloat16 val9, BFloat16 val10, BFloat16 val11, BFloat16 val12,
         BFloat16 val13, BFloat16 val14, BFloat16 val15, BFloat16 val16) {
    values = _mm256_setr_epi16(
        val1.x, val2.x, val3.x, val4.x, val5.x, val6.x, val7.x, val8.x,
        val9.x, val10.x, val11.x, val12.x, val13.x, val14.x, val15.x, val16.x);
  }
  operator __m256i() const {
    return values;
  }
  BFloat16& operator[](int idx) = delete;
  const BFloat16& operator[](int idx) const  = delete;
  static Vec256<BFloat16> loadu(const void* ptr) {
    return _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
  }
  static Vec256<BFloat16> loadu(const void* ptr, int16_t count) {
    __at_align32__ int16_t tmp_values[size()];
    std::memcpy(tmp_values, ptr, count * sizeof(int16_t));
    return loadu(tmp_values);
  }
  void store(void* ptr, int count = size()) const {
    if (count == size()) {
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(ptr), values);
    } else if (count > 0) {
      __at_align32__ int16_t tmp_values[size()];
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(tmp_values), values);
      std::memcpy(ptr, tmp_values, count * sizeof(int16_t));
    }
  }
  template <int64_t mask>
  static Vec256<BFloat16> blend(const Vec256<BFloat16>& a, const Vec256<BFloat16>& b) {
    __at_align32__ int16_t tmp_values[size()];
    a.store(tmp_values);
    if (mask & 0x01)
      tmp_values[0] = _mm256_extract_epi16(b.values, 0);
    if (mask & 0x02)
      tmp_values[1] = _mm256_extract_epi16(b.values, 1);
    if (mask & 0x04)
      tmp_values[2] = _mm256_extract_epi16(b.values, 2);
    if (mask & 0x08)
      tmp_values[3] = _mm256_extract_epi16(b.values, 3);
    if (mask & 0x10)
      tmp_values[4] = _mm256_extract_epi16(b.values, 4);
    if (mask & 0x20)
      tmp_values[5] = _mm256_extract_epi16(b.values, 5);
    if (mask & 0x40)
      tmp_values[6] = _mm256_extract_epi16(b.values, 6);
    if (mask & 0x80)
      tmp_values[7] = _mm256_extract_epi16(b.values, 7);
    if (mask & 0x100)
      tmp_values[8] = _mm256_extract_epi16(b.values, 8);
    if (mask & 0x200)
      tmp_values[9] = _mm256_extract_epi16(b.values, 9);
    if (mask & 0x400)
      tmp_values[10] = _mm256_extract_epi16(b.values, 10);
    if (mask & 0x800)
      tmp_values[11] = _mm256_extract_epi16(b.values, 11);
    if (mask & 0x1000)
      tmp_values[12] = _mm256_extract_epi16(b.values, 12);
    if (mask & 0x2000)
      tmp_values[13] = _mm256_extract_epi16(b.values, 13);
    if (mask & 0x4000)
      tmp_values[14] = _mm256_extract_epi16(b.values, 14);
    if (mask & 0x8000)
      tmp_values[15] = _mm256_extract_epi16(b.values, 15);
    return loadu(tmp_values);
  }
  static Vec256<BFloat16> blendv(const Vec256<BFloat16>& a,
      const Vec256<BFloat16>& b, const Vec256<BFloat16>& mask) {
    return _mm256_blendv_epi8(a.values, b.values, mask.values);
  }
  template<typename step_t>
  static Vec256<BFloat16> arange(BFloat16 base = 0.f, step_t step = static_cast<step_t>(1)) {
    return Vec256<BFloat16>(
      base,             base +      step, base +  2 * step, base +  3 * step,
      base +  4 * step, base +  5 * step, base +  6 * step, base +  7 * step,
      base +  8 * step, base +  9 * step, base + 10 * step, base + 11 * step,
      base + 12 * step, base + 13 * step, base + 14 * step, base + 15 * step);
  }
  static Vec256<BFloat16> set(const Vec256<BFloat16>& a,
      const Vec256<BFloat16>& b, int64_t count = size()) {
    switch (count) {
      case 0:
        return a;
      case 1:
        return blend<1>(a, b);
      case 2:
        return blend<3>(a, b);
      case 3:
        return blend<7>(a, b);
      case 4:
        return blend<15>(a, b);
      case 5:
        return blend<31>(a, b);
      case 6:
        return blend<63>(a, b);
      case 7:
        return blend<127>(a, b);
      case 8:
        return blend<255>(a, b);
      case 9:
        return blend<511>(a, b);
      case 10:
        return blend<1023>(a, b);
      case 11:
        return blend<2047>(a, b);
      case 12:
        return blend<4095>(a, b);
      case 13:
        return blend<8191>(a, b);
      case 14:
        return blend<16383>(a, b);
      case 15:
        return blend<32767>(a, b);
    }
    return b;
  }
  Vec256<BFloat16> map(const __m256 (*vop)(__m256)) const {
    __m256 lo, hi;
    cvtbf16_fp32(values, lo, hi);
    auto o1 = vop(lo);
    auto o2 = vop(hi);
    return cvtfp32_bf16(o1, o2);
  }
  Vec256<BFloat16> abs() const {
    __m256 lo, hi;
    cvtbf16_fp32(values, lo, hi);
    auto mask = _mm256_set1_ps(-0.f);
    auto o1 = _mm256_andnot_ps(mask, lo);
    auto o2 = _mm256_andnot_ps(mask, hi);
    return cvtfp32_bf16(o1, o2);
  }
  Vec256<BFloat16> angle() const {
    return _mm256_set1_epi16(0);
  }
  Vec256<BFloat16> real() const {
    return *this;
  }
  Vec256<BFloat16> imag() const {
    return _mm256_set1_epi16(0);
  }
  Vec256<BFloat16> conj() const {
    return *this;
  }
  Vec256<BFloat16> acos() const {
    return map(Sleef_acosf8_u10);
  }
  Vec256<BFloat16> asin() const {
    return map(Sleef_asinf8_u10);
  }
  Vec256<BFloat16> atan() const {
    return map(Sleef_atanf8_u10);
  }
  Vec256<BFloat16> atan2(const Vec256<BFloat16> &b) const {
    __m256 lo, hi;
    __m256 b1, b2;
    cvtbf16_fp32(values, lo, hi);
    cvtbf16_fp32(b.values, b1, b2);
    auto o1 = Sleef_atan2f8_u10(lo, b1);
    auto o2 = Sleef_atan2f8_u10(hi, b2);
    return cvtfp32_bf16(o1, o2);
  }
  Vec256<BFloat16> erf() const {
    return map(Sleef_erff8_u10);
  }
  Vec256<BFloat16> erfc() const {
    return map(Sleef_erfcf8_u15);
  }
  Vec256<BFloat16> erfinv() const {
    __at_align32__ int16_t tmp[size()];
    store(tmp);
    for (int64_t i = 0; i < size(); i++) {
      tmp[i] = calc_erfinv((float)tmp[i]);
    }
    return loadu(tmp);
  }
  Vec256<BFloat16> exp() const {
    return map(Sleef_expf8_u10);
  }
  Vec256<BFloat16> expm1() const {
    return map(Sleef_expm1f8_u10);
  }
  Vec256<BFloat16> log() const {
    return map(Sleef_logf8_u10);
  }
  Vec256<BFloat16> log2() const {
    return map(Sleef_log2f8_u10);
  }
  Vec256<BFloat16> log10() const {
    return map(Sleef_log10f8_u10);
  }
  Vec256<BFloat16> log1p() const {
    return map(Sleef_log1pf8_u10);
  }
  Vec256<BFloat16> frac() const;
  Vec256<BFloat16> sin() const {
    return map(Sleef_sinf8_u10);
  }
  Vec256<BFloat16> sinh() const {
    return map(Sleef_sinhf8_u10);
  }
  Vec256<BFloat16> cos() const {
    return map(Sleef_cosf8_u10);
  }
  Vec256<BFloat16> cosh() const {
    return map(Sleef_coshf8_u10);
  }
  Vec256<BFloat16> ceil() const {
    __m256 lo, hi;
    cvtbf16_fp32(values, lo, hi);
    auto o1 = _mm256_ceil_ps(lo);
    auto o2 = _mm256_ceil_ps(hi);
    return cvtfp32_bf16(o1, o2);
  }
  Vec256<BFloat16> floor() const {
    __m256 lo, hi;
    cvtbf16_fp32(values, lo, hi);
    auto o1 = _mm256_floor_ps(lo);
    auto o2 = _mm256_floor_ps(hi);
    return cvtfp32_bf16(o1, o2);
  }
  Vec256<BFloat16> neg() const {
    __m256 lo, hi;
    cvtbf16_fp32(values, lo, hi);
    auto mask = _mm256_set1_ps(-0.f);
    auto o1 = _mm256_xor_ps(mask, lo);
    auto o2 = _mm256_xor_ps(mask, hi);
    return cvtfp32_bf16(o1, o2);
  }
  Vec256<BFloat16> round() const {
    __m256 lo, hi;
    cvtbf16_fp32(values, lo, hi);
    auto o1 = _mm256_round_ps(lo, (_MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
    auto o2 = _mm256_round_ps(hi, (_MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC));
    return cvtfp32_bf16(o1, o2);
  }
  Vec256<BFloat16> tan() const {
    return map(Sleef_tanf8_u10);
  }
  Vec256<BFloat16> tanh() const {
    return map(Sleef_tanhf8_u10);
  }
  Vec256<BFloat16> trunc() const {
    __m256 lo, hi;
    cvtbf16_fp32(values, lo, hi);
    auto o1 = _mm256_round_ps(lo, (_MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC));
    auto o2 = _mm256_round_ps(hi, (_MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC));
    return cvtfp32_bf16(o1, o2);
  }
  Vec256<BFloat16> lgamma() const {
    return map(Sleef_lgammaf8_u10);
  }
  Vec256<BFloat16> sqrt() const {
    __m256 lo, hi;
    cvtbf16_fp32(values, lo, hi);
    auto o1 = _mm256_sqrt_ps(lo);
    auto o2 = _mm256_sqrt_ps(hi);
    return cvtfp32_bf16(o1, o2);
  }
  Vec256<BFloat16> reciprocal() const {
    __m256 lo, hi;
    cvtbf16_fp32(values, lo, hi);
    auto ones = _mm256_set1_ps(1);
    auto o1 = _mm256_div_ps(ones, lo);
    auto o2 = _mm256_div_ps(ones, hi);
    return cvtfp32_bf16(o1, o2);
  }
  Vec256<BFloat16> rsqrt() const {
    __m256 lo, hi;
    cvtbf16_fp32(values, lo, hi);
    auto ones = _mm256_set1_ps(1);
    auto o1 = _mm256_div_ps(ones, _mm256_sqrt_ps(lo));
    auto o2 = _mm256_div_ps(ones, _mm256_sqrt_ps(hi));
    return cvtfp32_bf16(o1, o2);
  }
  Vec256<BFloat16> pow(const Vec256<BFloat16> &b) const {
    __m256 lo, hi;
    __m256 b1, b2;
    cvtbf16_fp32(values, lo, hi);
    cvtbf16_fp32(b.values, b1, b2);
    auto o1 = Sleef_powf8_u10(lo, b1);
    auto o2 = Sleef_powf8_u10(hi, b2);
    return cvtfp32_bf16(o1, o2);
  }
  // Comparison using the _CMP_**_OQ predicate.
  //   `O`: get false if an operand is NaN
  //   `Q`: do not raise if an operand is NaN
  template<int imm8>
  Vec256<BFloat16> compare(const Vec256<BFloat16>& other) const {
    __m256 a_lo, a_hi;
    __m256 b_lo, b_hi;
    cvtbf16_fp32(values, a_lo, a_hi);
    cvtbf16_fp32(other.values, b_lo, b_hi);
    auto o1 = _mm256_cmp_ps(a_lo, b_lo, imm8);
    auto o2 = _mm256_cmp_ps(a_hi, b_hi, imm8);
    return cvtfp32_bf16(o1, o2);
  }
  Vec256<BFloat16> operator==(const Vec256<BFloat16>& other) const {
    return compare<_CMP_EQ_OQ>(other);
  }
  Vec256<BFloat16> operator!=(const Vec256<BFloat16>& other) const {
    return compare<_CMP_NEQ_OQ>(other);
  }
  Vec256<BFloat16> operator<(const Vec256<BFloat16>& other) const {
    return compare<_CMP_LT_OQ>(other);
  }
  Vec256<BFloat16> operator<=(const Vec256<BFloat16>& other) const {
    return compare<_CMP_LE_OQ>(other);
  }
  Vec256<BFloat16> operator>(const Vec256<BFloat16>& other) const {
    return compare<_CMP_GT_OQ>(other);
  }
  Vec256<BFloat16> operator>=(const Vec256<BFloat16>& other) const {
    return compare<_CMP_GE_OQ>(other);
  }
};

#define DEFINE_BF16_OP(op, func)                                                            \
template<>                                                                                  \
Vec256<BFloat16> inline operator op(const Vec256<BFloat16>& a, const Vec256<BFloat16>& b) { \
  __m256 a_lo, a_hi;                                                                        \
  __m256 b_lo, b_hi;                                                                        \
  cvtbf16_fp32(__m256i(a), a_lo, a_hi);                                                     \
  cvtbf16_fp32(__m256i(b), b_lo, b_hi);                                                     \
  auto o1 = func(a_lo, b_lo);                                                               \
  auto o2 = func(a_hi, b_hi);                                                               \
  return cvtfp32_bf16(o1, o2);                                                              \
}

DEFINE_BF16_OP(+, _mm256_add_ps)
DEFINE_BF16_OP(-, _mm256_sub_ps)
DEFINE_BF16_OP(*, _mm256_mul_ps)
DEFINE_BF16_OP(/, _mm256_div_ps)
DEFINE_BF16_OP(&, _mm256_and_ps)
DEFINE_BF16_OP(|, _mm256_or_ps)
DEFINE_BF16_OP(^, _mm256_xor_ps)

// frac. Implement this here so we can use subtraction
Vec256<BFloat16> Vec256<BFloat16>::frac() const {
  return *this - this->trunc();
}

// Implements the IEEE 754 201X `maximum` operation, which propagates NaN if
// either input is a NaN.
template <>
Vec256<BFloat16> inline maximum(const Vec256<BFloat16>& a, const Vec256<BFloat16>& b) {
  __m256 a_lo, a_hi;
  __m256 b_lo, b_hi;
  cvtbf16_fp32(__m256i(a), a_lo, a_hi);
  cvtbf16_fp32(__m256i(b), b_lo, b_hi);
  auto max_lo = _mm256_max_ps(a_lo, b_lo);
  auto max_hi = _mm256_max_ps(a_hi, b_hi);
  auto nan_lo = _mm256_cmp_ps(a_lo, b_lo, _CMP_UNORD_Q);
  auto nan_hi = _mm256_cmp_ps(a_hi, b_hi, _CMP_UNORD_Q);
  // Exploit the fact that all-ones is a NaN.
  auto o1 = _mm256_or_ps(max_lo, nan_lo);
  auto o2 = _mm256_or_ps(max_hi, nan_hi);
  return cvtfp32_bf16(o1, o2);
}

// Implements the IEEE 754 201X `minimum` operation, which propagates NaN if
// either input is a NaN.
template <>
Vec256<BFloat16> inline minimum(const Vec256<BFloat16>& a, const Vec256<BFloat16>& b) {
  __m256 a_lo, a_hi;
  __m256 b_lo, b_hi;
  cvtbf16_fp32(__m256i(a), a_lo, a_hi);
  cvtbf16_fp32(__m256i(b), b_lo, b_hi);
  auto min_lo = _mm256_min_ps(a_lo, b_lo);
  auto min_hi = _mm256_min_ps(a_hi, b_hi);
  auto nan_lo = _mm256_cmp_ps(a_lo, b_lo, _CMP_UNORD_Q);
  auto nan_hi = _mm256_cmp_ps(a_hi, b_hi, _CMP_UNORD_Q);
  // Exploit the fact that all-ones is a NaN.
  auto o1 = _mm256_or_ps(min_lo, nan_lo);
  auto o2 = _mm256_or_ps(min_hi, nan_hi);
  return cvtfp32_bf16(o1, o2);
}

template <>
Vec256<BFloat16> inline clamp(const Vec256<BFloat16>& a,
    const Vec256<BFloat16>& min, const Vec256<BFloat16>& max) {
  __m256 a_lo, a_hi;
  __m256 min_lo, min_hi;
  __m256 max_lo, max_hi;
  cvtbf16_fp32(__m256i(a), a_lo, a_hi);
  cvtbf16_fp32(__m256i(min), min_lo, min_hi);
  cvtbf16_fp32(__m256i(max), max_lo, max_hi);
  auto o1 = _mm256_min_ps(max_lo, _mm256_max_ps(min_lo, a_lo));
  auto o2 = _mm256_min_ps(max_hi, _mm256_max_ps(min_hi, a_hi));
  return cvtfp32_bf16(o1, o2);
}

template <>
Vec256<BFloat16> inline clamp_max(const Vec256<BFloat16>& a, const Vec256<BFloat16>& max) {
  __m256 a_lo, a_hi;
  __m256 max_lo, max_hi;
  cvtbf16_fp32(__m256i(a), a_lo, a_hi);
  cvtbf16_fp32(__m256i(max), max_lo, max_hi);
  auto o1 = _mm256_min_ps(max_lo, a_lo);
  auto o2 = _mm256_min_ps(max_hi, a_hi);
  return cvtfp32_bf16(o1, o2);
}

template <>
Vec256<BFloat16> inline clamp_min(const Vec256<BFloat16>& a, const Vec256<BFloat16>& min) {
  __m256 a_lo, a_hi;
  __m256 min_lo, min_hi;
  cvtbf16_fp32(__m256i(a), a_lo, a_hi);
  cvtbf16_fp32(__m256i(min), min_lo, min_hi);
  auto o1 = _mm256_max_ps(min_lo, a_lo);
  auto o2 = _mm256_max_ps(min_hi, a_hi);
  return cvtfp32_bf16(o1, o2);
}

template <>
inline void convert(const BFloat16* src, BFloat16* dst, int64_t n) {
  int64_t i;
#pragma unroll
  for (i = 0; i <= (n - Vec256<BFloat16>::size()); i += Vec256<BFloat16>::size()) {
    auto vsrc = _mm256_loadu_si256(reinterpret_cast<__m256i*>((void*)(src + i)));
    _mm256_storeu_si256(reinterpret_cast<__m256i*>((void*)(dst + i)), vsrc);
  }
#pragma unroll
  for (; i < n; i++) {
    dst[i] = src[i];
  }
}

template <>
Vec256<BFloat16> inline fmadd(const Vec256<BFloat16>& a,
    const Vec256<BFloat16>& b, const Vec256<BFloat16>& c) {
  __m256 a_lo, a_hi;
  __m256 b_lo, b_hi;
  __m256 c_lo, c_hi;
  cvtbf16_fp32(__m256i(a), a_lo, a_hi);
  cvtbf16_fp32(__m256i(b), b_lo, b_hi);
  cvtbf16_fp32(__m256i(c), c_lo, c_hi);
  auto o1 = _mm256_fmadd_ps(a_lo, b_lo, c_lo);
  auto o2 = _mm256_fmadd_ps(a_hi, b_hi, c_hi);
  return cvtfp32_bf16(o1, o2);
}

#endif

}}}
