#pragma once

#include "ddraw_include.h"
#include "ddraw_options.h"
#include "ddraw_caps.h"

#include "d3d_common_viewport.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace dxvk {

  struct ProcessVerticesData {
    bool doLighting;
    bool doClipping;
    bool doExtents;
    bool doNotCopyData;
    bool isLegacy;
    uint16_t vertexCount;
    size_t inStride;
    size_t outStride;
    DWORD inFVF;
    DWORD outFVF;
    uint8_t* inData;
    uint8_t* outData;
    const D3DMATRIX* correction;
    D3DSTATUS* dsStatus;
    std::vector<d3d9::D3DLIGHT9>* lights;
  };

  struct PVLIGHT {
    D3DLIGHTTYPE Type;
    D3DCOLORVALUE Diffuse;
    D3DCOLORVALUE Specular;
    D3DCOLORVALUE Ambient;
    float Range;
    float Falloff;
    float Attenuation0;
    float Attenuation1;
    float Attenuation2;
    D3DVECTOR LightDirection;
    D3DVECTOR LightPosition;
    float cosHalfPhi;
    float cosHalfTheta;
  };

  struct D3DVECTOR4 {
    float x,y,z,w;
  };

  using PositionArray = std::array<FLOAT, 8>;
  using TexCoordArray = std::array<std::array<FLOAT, 4>, ddrawCaps::MaxSimultaneousTextures>;

  struct alignas(16) SIMDRow {
    __m128 lo; // Elements 0, 1, 2, 3
    __m128 hi; // Elements 4, 5, 6, 7
  };

  inline void swap_rows_sse(SIMDRow& a, SIMDRow& b) {
    __m128 temp_lo = a.lo;
    __m128 temp_hi = a.hi;
    a.lo = b.lo;
    a.hi = b.hi;
    b.lo = temp_lo;
    b.hi = temp_hi;
  }

  inline void InvertMatrix_SSE2(D3DMATRIX *out, const D3DMATRIX *m) {
    SIMDRow r[4];

    auto get_val = [](const SIMDRow& row, int col) -> float {
      if (col < 4) {
        float f[4];
        _mm_storeu_ps(f, row.lo);
        return f[col];
      } else {
        float f[4];
        _mm_storeu_ps(f, row.hi);
        return f[col - 4];
      }
    };

    r[0].lo = _mm_setr_ps(m->_11, m->_12, m->_13, m->_14);
    r[0].hi = _mm_setr_ps(1.0f, 0.0f, 0.0f, 0.0f);
    r[1].lo = _mm_setr_ps(m->_21, m->_22, m->_23, m->_24);
    r[1].hi = _mm_setr_ps(0.0f, 1.0f, 0.0f, 0.0f);
    r[2].lo = _mm_setr_ps(m->_31, m->_32, m->_33, m->_34);
    r[2].hi = _mm_setr_ps(0.0f, 0.0f, 1.0f, 0.0f);
    r[3].lo = _mm_setr_ps(m->_41, m->_42, m->_43, m->_44);
    r[3].hi = _mm_setr_ps(0.0f, 0.0f, 0.0f, 1.0f);

    if (std::abs(get_val(r[3], 0)) > std::abs(get_val(r[2], 0)))
      swap_rows_sse(r[3], r[2]);
    if (std::abs(get_val(r[2], 0)) > std::abs(get_val(r[1], 0)))
      swap_rows_sse(r[2], r[1]);
    if (std::abs(get_val(r[1], 0)) > std::abs(get_val(r[0], 0)))
      swap_rows_sse(r[1], r[0]);

    float pivot0 = get_val(r[0], 0);
    if (pivot0 == 0.0f)
      return;

    auto eliminate_row = [&](SIMDRow& target, const SIMDRow& source, float factor) {
      __m128 v_factor = _mm_set1_ps(factor);
      target.lo = _mm_sub_ps(target.lo, _mm_mul_ps(v_factor, source.lo));
      target.hi = _mm_sub_ps(target.hi, _mm_mul_ps(v_factor, source.hi));
    };

    eliminate_row(r[1], r[0], get_val(r[1], 0) / pivot0);
    eliminate_row(r[2], r[0], get_val(r[2], 0) / pivot0);
    eliminate_row(r[3], r[0], get_val(r[3], 0) / pivot0);

    if (std::abs(get_val(r[3], 1)) > std::abs(get_val(r[2], 1)))
      swap_rows_sse(r[3], r[2]);
    if (std::abs(get_val(r[2], 1)) > std::abs(get_val(r[1], 1)))
      swap_rows_sse(r[2], r[1]);

    float pivot1 = get_val(r[1], 1);
    if (pivot1 == 0.0f)
      return;

    eliminate_row(r[2], r[1], get_val(r[2], 1) / pivot1);
    eliminate_row(r[3], r[1], get_val(r[3], 1) / pivot1);

    if (std::abs(get_val(r[3], 2)) > std::abs(get_val(r[2], 2)))
      swap_rows_sse(r[3], r[2]);
    float pivot2 = get_val(r[2], 2);
    if (pivot2 == 0.0f)
      return;

    eliminate_row(r[3], r[2], get_val(r[3], 2) / pivot2);

    float pivot3 = get_val(r[3], 3);
    if (pivot3 == 0.0f)
      return;

    __m128 v_inv_pivot3 = _mm_set1_ps(1.0f / pivot3);
    r[3].lo = _mm_mul_ps(r[3].lo, v_inv_pivot3);
    r[3].hi = _mm_mul_ps(r[3].hi, v_inv_pivot3);

    float m2_3 = get_val(r[2], 3);
    r[2].lo = _mm_sub_ps(r[2].lo, _mm_mul_ps(_mm_set1_ps(m2_3), r[3].lo));
    r[2].hi = _mm_sub_ps(r[2].hi, _mm_mul_ps(_mm_set1_ps(m2_3), r[3].hi));

    float s2 = get_val(r[2], 2);
    r[2].lo = _mm_mul_ps(r[2].lo, _mm_set1_ps(1.0f / s2));
    r[2].hi = _mm_mul_ps(r[2].hi, _mm_set1_ps(1.0f / s2));

    float m1_3 = get_val(r[1], 3);
    float m1_2 = get_val(r[1], 2);
    r[1].lo = _mm_sub_ps(r[1].lo, _mm_mul_ps(_mm_set1_ps(m1_3), r[3].lo));
    r[1].hi = _mm_sub_ps(r[1].hi, _mm_mul_ps(_mm_set1_ps(m1_3), r[3].hi));
    r[1].lo = _mm_sub_ps(r[1].lo, _mm_mul_ps(_mm_set1_ps(m1_2), r[2].lo));
    r[1].hi = _mm_sub_ps(r[1].hi, _mm_mul_ps(_mm_set1_ps(m1_2), r[2].hi));

    float s1 = get_val(r[1], 1);
    r[1].lo = _mm_mul_ps(r[1].lo, _mm_set1_ps(1.0f / s1));
    r[1].hi = _mm_mul_ps(r[1].hi, _mm_set1_ps(1.0f / s1));

    float m0_3 = get_val(r[0], 3);
    float m0_2 = get_val(r[0], 2);
    float m0_1 = get_val(r[0], 1);
    r[0].lo = _mm_sub_ps(r[0].lo, _mm_mul_ps(_mm_set1_ps(m0_3), r[3].lo));
    r[0].hi = _mm_sub_ps(r[0].hi, _mm_mul_ps(_mm_set1_ps(m0_3), r[3].hi));
    r[0].lo = _mm_sub_ps(r[0].lo, _mm_mul_ps(_mm_set1_ps(m0_2), r[2].lo));
    r[0].hi = _mm_sub_ps(r[0].hi, _mm_mul_ps(_mm_set1_ps(m0_2), r[2].hi));
    r[0].lo = _mm_sub_ps(r[0].lo, _mm_mul_ps(_mm_set1_ps(m0_1), r[1].lo));
    r[0].hi = _mm_sub_ps(r[0].hi, _mm_mul_ps(_mm_set1_ps(m0_1), r[1].hi));

    float s0 = get_val(r[0], 0);
    r[0].lo = _mm_mul_ps(r[0].lo, _mm_set1_ps(1.0f / s0));
    r[0].hi = _mm_mul_ps(r[0].hi, _mm_set1_ps(1.0f / s0));

    _mm_storeu_ps(&out->_11, r[0].hi); // This assumes _11 is the start of a float sequence
    out->_11 = get_val(r[0], 4); out->_12 = get_val(r[0], 5); out->_13 = get_val(r[0], 6); out->_14 = get_val(r[0], 7);
    out->_21 = get_val(r[1], 4); out->_22 = get_val(r[1], 5); out->_23 = get_val(r[1], 6); out->_24 = get_val(r[1], 7);
    out->_31 = get_val(r[2], 4); out->_32 = get_val(r[2], 5); out->_33 = get_val(r[2], 6); out->_34 = get_val(r[2], 7);
    out->_41 = get_val(r[3], 4); out->_42 = get_val(r[3], 5); out->_43 = get_val(r[3], 6); out->_44 = get_val(r[3], 7);
  }

  inline __m128 cross_product_sse2(__m128 a, __m128 b) {
    __m128 a_yzx = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 0, 2, 1));
    __m128 b_zxy = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 a_zxy = _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 1, 0, 2));
    __m128 b_yzx = _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 0, 2, 1));

    return _mm_sub_ps(_mm_mul_ps(a_yzx, b_zxy), _mm_mul_ps(a_zxy, b_yzx));
  }

  inline void InvertMatrixLegacy_SSE2(D3DMATRIX *out, const D3DMATRIX *in) {
    __m128 r1 = _mm_loadu_ps(&in->_11);
    __m128 r2 = _mm_loadu_ps(&in->_21);
    __m128 r3 = _mm_loadu_ps(&in->_31);

    __m128 v_a = cross_product_sse2(r2, r3);
    __m128 v_b = cross_product_sse2(r3, r1);
    __m128 v_c = cross_product_sse2(r1, r2);

    __m128 dot = _mm_mul_ps(r1, v_a);

    float det;
    float temp_det[4];
    _mm_storeu_ps(temp_det, dot);
    det = temp_det[0] + temp_det[1] + temp_det[2];

    if (std::fabs(det) <= std::numeric_limits<float>::min() * 10)
      return;

    float inv_det = 1.0f / det;

    v_a = _mm_mul_ps(v_a, _mm_set1_ps(inv_det));
    v_b = _mm_mul_ps(v_b, _mm_set1_ps(inv_det));
    v_c = _mm_mul_ps(v_c, _mm_set1_ps(inv_det));

    float va[4], vb[4], vc[4];
    _mm_storeu_ps(va, v_a);
    _mm_storeu_ps(vb, v_b);
    _mm_storeu_ps(vc, v_c);

    out->_11 = va[0]; out->_12 = vb[0]; out->_13 = vc[0];
    out->_21 = va[1]; out->_22 = vb[1]; out->_23 = vc[1];
    out->_31 = va[2]; out->_32 = vb[2]; out->_33 = vc[2];
  }

  inline void ComputeNormalMatrix(D3DMATRIX &normal_matrix, bool isLegacy, const D3DMATRIX &wv) {
    D3DMATRIX mv = wv;
    if (isLegacy)
      InvertMatrixLegacy_SSE2(&mv, &mv);
    else
      InvertMatrix_SSE2(&mv, &mv);

    normal_matrix._11 = mv._11;
    normal_matrix._12 = mv._21;
    normal_matrix._13 = mv._31;
    normal_matrix._21 = mv._12;
    normal_matrix._22 = mv._22;
    normal_matrix._23 = mv._32;
    normal_matrix._31 = mv._13;
    normal_matrix._32 = mv._23;
    normal_matrix._33 = mv._33;
  }

  inline D3DMATRIX D3DMatrixMultiply4x4_SSE2(const D3DMATRIX& a, const D3DMATRIX& b) {
    D3DMATRIX result;

    const __m128 b0 = _mm_loadu_ps(&b._11);
    const __m128 b1 = _mm_loadu_ps(&b._21);
    const __m128 b2 = _mm_loadu_ps(&b._31);
    const __m128 b3 = _mm_loadu_ps(&b._41);

    #define MULROW(dst, src)                                            \
    do {                                                                \
        __m128 r = _mm_mul_ps(_mm_set1_ps((src)[0]), b0);               \
        r = _mm_add_ps(r, _mm_mul_ps(_mm_set1_ps((src)[1]), b1));       \
        r = _mm_add_ps(r, _mm_mul_ps(_mm_set1_ps((src)[2]), b2));       \
        r = _mm_add_ps(r, _mm_mul_ps(_mm_set1_ps((src)[3]), b3));       \
        _mm_storeu_ps((dst), r);                                        \
    } while (0)

    MULROW(&result._11, &a._11);
    MULROW(&result._21, &a._21);
    MULROW(&result._31, &a._31);
    MULROW(&result._41, &a._41);
    #undef MULROW

    return result;
  }

  inline D3DVECTOR4 D3DVec4Transform_SSE2(const D3DMATRIX& m, const D3DVECTOR4& v)
  {
    D3DVECTOR4 result;

    const __m128 r0 = _mm_loadu_ps(&m._11);
    const __m128 r1 = _mm_loadu_ps(&m._21);
    const __m128 r2 = _mm_loadu_ps(&m._31);
    const __m128 r3 = _mm_loadu_ps(&m._41);

    __m128 r = _mm_mul_ps(_mm_set1_ps(v.x), r0);
    r = _mm_add_ps(r, _mm_mul_ps(_mm_set1_ps(v.y), r1));
    r = _mm_add_ps(r, _mm_mul_ps(_mm_set1_ps(v.z), r2));
    r = _mm_add_ps(r, _mm_mul_ps(_mm_set1_ps(v.w), r3));

    _mm_storeu_ps(&result.x, r);

    return result;
  }

  inline D3DVECTOR D3DVec3Transform_SSE2(const D3DMATRIX& m, const D3DVECTOR& v) {
    const __m128 vx = _mm_set1_ps(v.x);
    const __m128 vy = _mm_set1_ps(v.y);
    const __m128 vz = _mm_set1_ps(v.z);

    const __m128 rowX = _mm_setr_ps(m._11, m._12, m._13, 0.0f);
    const __m128 rowY = _mm_setr_ps(m._21, m._22, m._23, 0.0f);
    const __m128 rowZ = _mm_setr_ps(m._31, m._32, m._33, 0.0f);

    __m128 r = _mm_mul_ps(vx, rowX);
    r = _mm_add_ps(r, _mm_mul_ps(vy, rowY));
    r = _mm_add_ps(r, _mm_mul_ps(vz, rowZ));

    D3DVECTOR4 tmp;
    _mm_storeu_ps(&tmp.x, r);

    return D3DVECTOR{tmp.x, tmp.y, tmp.z};
  }

  inline D3DVECTOR D3DVec4to3Transform_SSE2(const D3DMATRIX& m, const D3DVECTOR4& v) {
    const __m128 r0 = _mm_loadu_ps(&m._11);
    const __m128 r1 = _mm_loadu_ps(&m._21);
    const __m128 r2 = _mm_loadu_ps(&m._31);
    const __m128 r3 = _mm_loadu_ps(&m._41);

    __m128 r = _mm_mul_ps(_mm_set1_ps(v.x), r0);
    r = _mm_add_ps(r, _mm_mul_ps(_mm_set1_ps(v.y), r1));
    r = _mm_add_ps(r, _mm_mul_ps(_mm_set1_ps(v.z), r2));
    r = _mm_add_ps(r, _mm_mul_ps(_mm_set1_ps(v.w), r3));

    D3DVECTOR4 tmp;
    _mm_storeu_ps(&tmp.x, r);

    return D3DVECTOR{tmp.x, tmp.y, tmp.z};
  }

  inline float D3DVec3Dot_SSE2(const D3DVECTOR* a, const D3DVECTOR* b) {
    if (a == nullptr || b == nullptr)
      return 0.0f;

    __m128 x = _mm_setr_ps(a->x, a->y, a->z, 0.0f);
    __m128 y = _mm_setr_ps(b->x, b->y, b->z, 0.0f);
    __m128 m = _mm_mul_ps(x, y);
    __m128 t = _mm_add_ps(m, _mm_shuffle_ps(m, m, _MM_SHUFFLE(1,0,3,2)));

    t = _mm_add_ss(t, _mm_shuffle_ps(t, t, _MM_SHUFFLE(2,3,0,1)));

    return _mm_cvtss_f32(t);
  }

  inline __m128 D3DVec3Normalize_SSECore(__m128 v) {
    __m128 mul = _mm_mul_ps(v, v);
    __m128 sum = _mm_add_ps(mul, _mm_shuffle_ps(mul, mul, _MM_SHUFFLE(1,0,3,2)));
    sum = _mm_add_ss(sum, _mm_shuffle_ps(sum, sum, _MM_SHUFFLE(2,3,0,1)));

    __m128 inv = _mm_rsqrt_ss(sum);
    __m128 half  = _mm_set_ss(0.5f);
    __m128 three = _mm_set_ss(3.0f);

    inv = _mm_mul_ss(inv, _mm_sub_ss(three, _mm_mul_ss(_mm_mul_ss(sum, inv), inv)));
    inv = _mm_mul_ss(inv, half);

    return _mm_shuffle_ps(inv, inv, 0x00);
  }

  inline D3DVECTOR D3DVec3Normalize(const D3DVECTOR& v) {
    __m128 vec = _mm_setr_ps(v.x, v.y, v.z, 0.0f);
    __m128 inv = D3DVec3Normalize_SSECore(vec);

    vec = _mm_mul_ps(vec, inv);

    D3DVECTOR out;
    _mm_store_ss(&out.x, vec);
    _mm_store_ss(&out.y, _mm_shuffle_ps(vec, vec, 1));
    _mm_store_ss(&out.z, _mm_shuffle_ps(vec, vec, 2));

    return out;
  }

  inline void D3DColorClamp_SSE2(D3DCOLORVALUE& color) {
    __m128 c = _mm_loadu_ps(&color.r);

    c = _mm_min_ps(c, _mm_set1_ps(1.0f));
    c = _mm_max_ps(c, _mm_setzero_ps());

    _mm_storeu_ps(&color.r, c);
  }

  inline void ColorVMultiplyAdd_SSE2(D3DCOLORVALUE& out, const D3DCOLORVALUE& color, float value) {
    __m128 va = _mm_loadu_ps(&color.r);
    __m128 vb = _mm_loadu_ps(&out.r);

    __m128 vs = _mm_set1_ps(value);

    __m128 r = _mm_add_ps(_mm_mul_ps(va, vs), vb);

    float alpha = out.a;
    _mm_storeu_ps(&out.r, r);
    out.a = alpha;
  }

  inline float D3DSSE_Mad2_Add_Scalar_SSE2(float a1, float a2, float b1, float b2, float c) {
    __m128 r = _mm_mul_ss(_mm_set_ss(a1), _mm_set_ss(a2));
    r = _mm_add_ss(r, _mm_mul_ss(_mm_set_ss(b1), _mm_set_ss(b2)));
    r = _mm_add_ss(r, _mm_set_ss(c));

    return _mm_cvtss_f32(r);
  }

  inline void D3DColorMulRGB_SetAlpha_SSE2(D3DCOLORVALUE& a, const D3DCOLORVALUE& b, bool legacy) {
    __m128 va = _mm_loadu_ps(&a.r);
    __m128 vb = _mm_loadu_ps(&b.r);

    __m128 r = _mm_mul_ps(va, vb);

    _mm_storeu_ps(&a.r, r);

    if (legacy)
      a.a = 0.0f;
    else
      a.a = b.a;
  }

  // D3DCOLOR is DWORD packed ARGB, uint8_t per component
  // D3DCOLORVALUE is struct with float per component normalized to 0.0f - 1.0f
  inline D3DCOLOR ColorVToColor(const D3DCOLORVALUE& c) {
    return D3DCOLOR_ARGB(
      static_cast<int>(roundf(c.a * 255.0f)),
      static_cast<int>(roundf(c.r * 255.0f)),
      static_cast<int>(roundf(c.g * 255.0f)),
      static_cast<int>(roundf(c.b * 255.0f))
    );
  }

  inline D3DCOLORVALUE ColorToColorV(const D3DCOLOR& c) {
    D3DCOLORVALUE result;

    result.a = ((c >> 24) & 0xFF) / 255.0f;
    result.r = ((c >> 16) & 0xFF) / 255.0f;
    result.g = ((c >> 8)  & 0xFF) / 255.0f;
    result.b = ((c >> 0)  & 0xFF) / 255.0f;

    return result;
  }

  inline D3DCOLORVALUE ColorVClamp(const D3DCOLORVALUE& color, float min, float max) {
    D3DCOLORVALUE result;

    result.r = std::clamp(color.r, min, max);
    result.g = std::clamp(color.g, min, max);
    result.b = std::clamp(color.b, min, max);
    result.a = std::clamp(color.a, min, max);

    return result;
  }

  inline void ColorVMultiplyAdd(D3DCOLORVALUE& out, const D3DCOLORVALUE& color, float value) {
    out.r += color.r * value;
    out.g += color.g * value;
    out.b += color.b * value;
  }

  inline void MaterialColorSource(
        d3d9::IDirect3DDevice9* d3d9Device, DWORD inFVF, DWORD* diffuse,
        DWORD* specular, DWORD* ambient, DWORD* emissive, bool isEnabledLighting) {
    if (!isEnabledLighting) {
      *diffuse = d3d9::D3DMCS_COLOR1;
      *specular = d3d9::D3DMCS_COLOR2;
      *ambient = *emissive = d3d9::D3DMCS_MATERIAL;
      return;
    }

    DWORD state;
    d3d9Device->GetRenderState(d3d9::D3DRS_COLORVERTEX, &state);
    if (!state) {
      *ambient = *diffuse = *emissive = *specular = d3d9::D3DMCS_MATERIAL;
      return;
    }

    d3d9Device->GetRenderState(d3d9::D3DRS_DIFFUSEMATERIALSOURCE, diffuse);
    d3d9Device->GetRenderState(d3d9::D3DRS_SPECULARMATERIALSOURCE, specular);
    *emissive = *specular = d3d9::D3DMCS_MATERIAL;
    if (*diffuse != d3d9::D3DMCS_COLOR1 || !(inFVF & D3DFVF_DIFFUSE))
      *diffuse = d3d9::D3DMCS_MATERIAL;
    if (*specular != d3d9::D3DMCS_COLOR2 || !(inFVF & D3DFVF_SPECULAR))
      *specular = d3d9::D3DMCS_MATERIAL;
  }

  inline D3DCOLORVALUE ColorFromMaterialSource(
        D3DCOLOR* diffuse, D3DCOLOR* specular, DWORD colorSource, D3DCOLORVALUE materialColor) {
    switch (colorSource) {
      case d3d9::D3DMCS_COLOR1:
        if (diffuse != nullptr)
          return ColorToColorV(*diffuse);
        else
          return D3DCOLORVALUE{1.0f, 1.0f, 1.0f, 1.0f};
      case d3d9::D3DMCS_COLOR2:
        if (specular != nullptr)
          return ColorToColorV(*specular);
        else
          return D3DCOLORVALUE{0.0f, 0.0f, 0.0f, 0.0f};
      case d3d9::D3DMCS_MATERIAL:
        return materialColor;
      default:
        return D3DCOLORVALUE{0.0f, 0.0f, 0.0f, 0.0f};
    }

    return materialColor;
  }

  inline Vector4 NormalizeVec3(const Vector4& v) {
    Vector4 result = normalize(Vector4(v.x, v.y, v.z, 0.0f));
    result.w = 0.0f;

    return result;
  }

  inline void ApplyLight(
        const PVLIGHT* light9, bool localViewer, D3DCOLORVALUE& diffuse, D3DCOLORVALUE& specular,
        const D3DVECTOR* normals, const D3DVECTOR* hitDirection, const D3DVECTOR* position,
        float attenuation, float materialPower, bool isLegacy) {
    const float direction_dot = std::clamp(D3DVec3Dot_SSE2(hitDirection, normals), 0.0f, 1.0f);
    ColorVMultiplyAdd_SSE2(diffuse, light9->Diffuse, direction_dot * attenuation);

    D3DVECTOR mid = *hitDirection;
    if (localViewer) {
      mid.x -= position->x;
      mid.y -= position->y;
      mid.z -= position->z;
    } else {
      mid.z -= 1.0f;
    }

    mid = D3DVec3Normalize(mid);
    const float direction_transformed_dot = D3DVec3Dot_SSE2(normals, &mid);
    if (direction_transformed_dot > 0.0f && (!isLegacy || materialPower > 0.0f) && direction_dot > 0.0f)
      ColorVMultiplyAdd_SSE2(specular, light9->Specular, powf(direction_transformed_dot, materialPower) * attenuation);
  }

  inline void ApplyFog(
        float* specularAlpha, D3DFOGMODE vertexMode, float density,
        float start, float end, bool useRange, const D3DVECTOR& position) {
    const float coord = useRange ? sqrtf(D3DVec3Dot_SSE2(&position, &position)) : fabsf(position.z);

    switch (vertexMode) {
      // (end - coord) / (end - start)
      case D3DFOG_LINEAR: {
        const float fogFactor = end - coord;
        *specularAlpha = fogFactor / (end - start);
        break;
      }
      // 1 / (e^[coord * density])
      case D3DFOG_EXP: {
        const float fogFactor = coord * density;
        *specularAlpha = expf(-1.0f * fogFactor);
        break;
      }
      // 1 / (e^[coord * density])^2
      case D3DFOG_EXP2: {
        const float fogFactor = coord * density;
        *specularAlpha = expf(-1.0f * fogFactor * fogFactor);
        break;
      }
      default:
        break;
    }
  }

  inline void ProcessVerticesInput(
        bool doNotCopyData, DWORD dwFVF, uint8_t *ptr, PositionArray& position, D3DVECTOR** normals,
        TexCoordArray& texCoords, D3DCOLOR** diffuse, D3DCOLOR** specular) {
    if (uint8_t type = (dwFVF & D3DFVF_POSITION_MASK)) {
      switch (type) {
        case D3DFVF_XYZ:
          memcpy(position.data(), ptr, sizeof(FLOAT) * 3);
          ptr += sizeof(FLOAT) * 3;
          break;
        case D3DFVF_XYZRHW:
          memcpy(position.data(), ptr, sizeof(FLOAT) * 4);
          ptr += sizeof(FLOAT) * 4;
          break;
        case D3DFVF_XYZB1:
          memcpy(position.data(), ptr, sizeof(FLOAT) * 4);
          ptr += sizeof(FLOAT) * 4;
          break;
        case D3DFVF_XYZB2:
          memcpy(position.data(), ptr, sizeof(FLOAT) * 5);
          ptr += sizeof(FLOAT) * 5;
          break;
        case D3DFVF_XYZB3:
          memcpy(position.data(), ptr, sizeof(FLOAT) * 6);
          ptr += sizeof(FLOAT) * 6;
          break;
        case D3DFVF_XYZB4:
          memcpy(position.data(), ptr, sizeof(FLOAT) * 7);
          ptr += sizeof(FLOAT) * 7;
          break;
        case D3DFVF_XYZB5:
          memcpy(position.data(), ptr, sizeof(FLOAT) * 8);
          ptr += sizeof(FLOAT) * 8;
          break;
      }
    }

    if (dwFVF & D3DFVF_NORMAL) {
      *normals = reinterpret_cast<D3DVECTOR*>(ptr);
      ptr += sizeof(D3DVECTOR);
    }

    if (dwFVF & D3DFVF_RESERVED1) {
      ptr += sizeof(DWORD);
    }

    if (dwFVF & D3DFVF_DIFFUSE) {
      *diffuse = reinterpret_cast<D3DCOLOR*>(ptr);
      ptr += sizeof(D3DCOLOR);
    }

    if (dwFVF & D3DFVF_SPECULAR) {
      *specular = reinterpret_cast<D3DCOLOR*>(ptr);
      ptr += sizeof(D3DCOLOR);
    }

    if (unlikely(doNotCopyData))
      return;

    const DWORD dwNumTextures = (dwFVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
    for (DWORD tex = 0; tex < dwNumTextures; tex++) {
      const DWORD texCoordSize = (dwFVF >> (tex * 2 + 16)) & 0x3;
      switch (texCoordSize) {
        case D3DFVF_TEXTUREFORMAT1:
          memcpy(texCoords[tex].data(), ptr, sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          break;
        case D3DFVF_TEXTUREFORMAT2:
          memcpy(texCoords[tex].data(), ptr, sizeof(FLOAT) * 2);
          ptr += sizeof(FLOAT) * 2;
          break;
        case D3DFVF_TEXTUREFORMAT3:
          memcpy(texCoords[tex].data(), ptr, sizeof(FLOAT) * 3);
          ptr += sizeof(FLOAT) * 3;
          break;
        case D3DFVF_TEXTUREFORMAT4:
          memcpy(texCoords[tex].data(), ptr, sizeof(FLOAT) * 4);
          ptr += sizeof(FLOAT) * 4;
          break;
      }
    }
  }

  inline void ProcessVerticesOutput(
        DWORD dwFVF, uint8_t* ptr, const PositionArray& position, const D3DVECTOR* normals,
        TexCoordArray* texCoords, const D3DCOLOR* diffuse, const D3DCOLOR* specular) {
    if (uint8_t type = (dwFVF & D3DFVF_POSITION_MASK)) {
      switch (type) {
        case D3DFVF_XYZ:
          memcpy(ptr, position.data(), sizeof(FLOAT) * 3);
          ptr += sizeof(FLOAT) * 3;
          break;
        case D3DFVF_XYZRHW:
          memcpy(ptr, position.data(), sizeof(FLOAT) * 4);
          ptr += sizeof(FLOAT) * 4;
          break;
        case D3DFVF_XYZB1:
          memcpy(ptr, position.data(), sizeof(FLOAT) * 4);
          ptr += sizeof(FLOAT) * 4;
          break;
        case D3DFVF_XYZB2:
          memcpy(ptr, position.data(), sizeof(FLOAT) * 5);
          ptr += sizeof(FLOAT) * 5;
          break;
        case D3DFVF_XYZB3:
          memcpy(ptr, position.data(), sizeof(FLOAT) * 6);
          ptr += sizeof(FLOAT) * 6;
          break;
        case D3DFVF_XYZB4:
          memcpy(ptr, position.data(), sizeof(FLOAT) * 7);
          ptr += sizeof(FLOAT) * 7;
          break;
        case D3DFVF_XYZB5:
          memcpy(ptr, position.data(), sizeof(FLOAT) * 8);
          ptr += sizeof(FLOAT) * 8;
          break;
      }
    }

    if (dwFVF & D3DFVF_NORMAL) {
      if (likely(normals != nullptr))
        memcpy(ptr, normals, sizeof(FLOAT) * 3);
      ptr += sizeof(FLOAT) * 3;
    }

    if (dwFVF & D3DFVF_RESERVED1) {
      ptr += sizeof(DWORD);
    }

    if (dwFVF & D3DFVF_DIFFUSE) {
      if (likely(diffuse != nullptr))
        memcpy(ptr, diffuse, sizeof(D3DCOLOR));
      ptr += sizeof(D3DCOLOR);
    }

    if (dwFVF & D3DFVF_SPECULAR) {
      if (likely(specular != nullptr))
        memcpy(ptr, specular, sizeof(D3DCOLOR));
      ptr += sizeof(D3DCOLOR);
    }

    if (unlikely(texCoords == nullptr))
      return;

    const DWORD dwNumTextures = (dwFVF & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
    for (DWORD tex = 0; tex < dwNumTextures; tex++) {
      const DWORD texCoordSize = (dwFVF >> (tex * 2 + 16)) & 0x3;
      switch (texCoordSize) {
        case D3DFVF_TEXTUREFORMAT1:
          memcpy(ptr, texCoords[tex].data(), sizeof(FLOAT));
          ptr += sizeof(FLOAT);
          break;
        case D3DFVF_TEXTUREFORMAT2:
          memcpy(ptr, texCoords[tex].data(), sizeof(FLOAT) * 2);
          ptr += sizeof(FLOAT) * 2;
          break;
        case D3DFVF_TEXTUREFORMAT3:
          memcpy(ptr, texCoords[tex].data(), sizeof(FLOAT) * 3);
          ptr += sizeof(FLOAT) * 3;
          break;
        case D3DFVF_TEXTUREFORMAT4:
          memcpy(ptr, texCoords[tex].data(), sizeof(FLOAT) * 4);
          ptr += sizeof(FLOAT) * 4;
          break;
      }
    }
  }

  inline void ProcessVerticesSW(
        d3d9::IDirect3DDevice9* d3d9Device, const D3DOptions* options, ProcessVerticesData* pvData) {
    if (unlikely(pvData == nullptr)) {
      Logger::err("ProcessVerticesSW: Missing processing data");
      return;
    }

    d3d9::D3DVIEWPORT9 viewport9;
    D3DMATRIX world9, view9, projection9;

    HRESULT hr = d3d9Device->GetViewport(&viewport9);
    if (unlikely(FAILED(hr))) {
      Logger::err("ProcessVerticesSW: Failed to get D3D9 viewport");
      return;
    }
    hr = d3d9Device->GetTransform(ConvertTransformState(D3DTRANSFORMSTATE_WORLD), &world9);
    if (unlikely(FAILED(hr))) {
      Logger::err("ProcessVerticesSW: Failed to get D3D9 world transform");
      return;
    }
    hr = d3d9Device->GetTransform(ConvertTransformState(D3DTRANSFORMSTATE_VIEW), &view9);
    if (unlikely(FAILED(hr))) {
      Logger::err("ProcessVerticesSW: Failed to get D3D9 view transform");
      return;
    }
    hr = d3d9Device->GetTransform(ConvertTransformState(D3DTRANSFORMSTATE_PROJECTION), &projection9);
    if (unlikely(FAILED(hr))) {
      Logger::err("ProcessVerticesSW: Failed to get D3D9 projection transform");
      return;
    }

    // Precalculate a few static viewport factors, to save on per-vertex cycles
    const float viewport9HalfWidth  = static_cast<float>(viewport9.Width)  * 0.5f;
    const float viewport9HalfHeight = static_cast<float>(viewport9.Height) * 0.5f;
    const float viewport9ZDelta     = viewport9.MaxZ - viewport9.MinZ;
    const DWORD viewport9Right      = viewport9.X + viewport9.Width;
    const DWORD viewport9Bottom     = viewport9.Y + viewport9.Height;

    if (pvData->doClipping) {
      if (pvData->dsStatus != nullptr && (pvData->dsStatus->dwFlags & D3DSETSTATUS_STATUS)) {
        pvData->dsStatus->dwFlags = 0;
        pvData->dsStatus->dwStatus = 0;
        if (pvData->doExtents) {
          pvData->dsStatus->dwFlags |= D3DSETSTATUS_EXTENTS;
          pvData->dsStatus->drExtent.x1 = viewport9.X;
          pvData->dsStatus->drExtent.y1 = viewport9.Y;
          pvData->dsStatus->drExtent.x2 = viewport9Right;
          pvData->dsStatus->drExtent.y2 = viewport9Bottom;
        }
      }
    }

    const bool needsCorrection = pvData->isLegacy && pvData->correction != nullptr;
    const D3DMATRIX wv         = D3DMatrixMultiply4x4_SSE2(world9, view9);
    const D3DMATRIX wvp        = !needsCorrection ? D3DMatrixMultiply4x4_SSE2(wv, projection9)
                                                  : D3DMatrixMultiply4x4_SSE2(D3DMatrixMultiply4x4_SSE2(wv, projection9), *pvData->correction);

    D3DFOGMODE fogVertexMode;
    float fogStart, fogEnd, fogDensity;
    BOOL isEnabledFogRange, isEnabledLighting, isEnabledSpecular, isEnabledNormalizeNormals, isEnabledLocalViewer;
    D3DCOLOR ambientStateColor;
    // In D3D7 it's quite possible no material is set, even though lighting is enabled
    d3d9::D3DMATERIAL9 material9 = { };

    d3d9Device->GetRenderState(d3d9::D3DRS_FOGVERTEXMODE, reinterpret_cast<DWORD*>(&fogVertexMode));
    const bool isEnabledFog = fogVertexMode != D3DFOG_NONE;
    if (isEnabledFog) {
      d3d9Device->GetRenderState(d3d9::D3DRS_FOGSTART, reinterpret_cast<DWORD*>(&fogStart));
      d3d9Device->GetRenderState(d3d9::D3DRS_FOGEND, reinterpret_cast<DWORD*>(&fogEnd));
      d3d9Device->GetRenderState(d3d9::D3DRS_FOGDENSITY, reinterpret_cast<DWORD*>(&fogDensity));
      d3d9Device->GetRenderState(d3d9::D3DRS_RANGEFOGENABLE, reinterpret_cast<DWORD*>(&isEnabledFogRange));
    }
    if (!pvData->isLegacy)
      d3d9Device->GetRenderState(d3d9::D3DRS_LIGHTING, reinterpret_cast<DWORD*>(&isEnabledLighting));
    // We factor in global lighting state into pvData->doLighting for D3D6 and
    // earlier, since D3DRENDERSTATE_/D3DRS_LIGHTING doesn't exist before D3D7
    const bool useLighting = pvData->isLegacy ? pvData->doLighting : pvData->doLighting && isEnabledLighting;
    if (useLighting) {
      d3d9Device->GetRenderState(d3d9::D3DRS_AMBIENT, reinterpret_cast<DWORD*>(&ambientStateColor));
      d3d9Device->GetRenderState(d3d9::D3DRS_SPECULARENABLE, reinterpret_cast<DWORD*>(&isEnabledSpecular));
      d3d9Device->GetRenderState(d3d9::D3DRS_NORMALIZENORMALS, reinterpret_cast<DWORD*>(&isEnabledNormalizeNormals));
      d3d9Device->GetRenderState(d3d9::D3DRS_LOCALVIEWER, reinterpret_cast<DWORD*>(&isEnabledLocalViewer));
    }
    d3d9Device->GetMaterial(&material9);

    const float materialPower = useLighting && isEnabledSpecular ? material9.Power : 0.0f;

    static constexpr D3DCOLORVALUE defaultAmbientColorValue = {0.0f, 0.0f, 0.0f, 0.0f};
    const D3DCOLORVALUE ambientStateColorValue = useLighting ? ColorToColorV(ambientStateColor) : defaultAmbientColorValue;

    DWORD sourceDiffuse, sourceSpecular, sourceAmbient, sourceEmissive;
    MaterialColorSource(d3d9Device, pvData->inFVF, &sourceDiffuse, &sourceSpecular, &sourceAmbient, &sourceEmissive, useLighting);

    std::vector<PVLIGHT> lights;
    if (useLighting && (pvData->outFVF & (D3DFVF_DIFFUSE | D3DFVF_SPECULAR))) {
      for (const d3d9::D3DLIGHT9& light : *pvData->lights) {
        PVLIGHT* l = new PVLIGHT;

        l->Type     = D3DLIGHTTYPE(light.Type);
        l->Ambient  = light.Ambient;
        l->Diffuse  = light.Diffuse;
        l->Specular = light.Specular;

        switch (l->Type) {
            case D3DLIGHT_DIRECTIONAL:
              l->LightDirection = D3DVec3Normalize(D3DVec4to3Transform_SSE2(view9, {-light.Direction.x, -light.Direction.y, -light.Direction.z, 0.0f}));
              break;
            case D3DLIGHT_POINT:
              l->LightPosition = D3DVec4to3Transform_SSE2(view9, {light.Position.x, light.Position.y, light.Position.z, 1.0f});
              l->Attenuation0 = light.Attenuation0;
              l->Attenuation1 = light.Attenuation1;
              l->Attenuation2 = light.Attenuation2;
              l->Range = light.Range;
              break;
            case D3DLIGHT_SPOT:
              l->LightDirection = D3DVec3Normalize(D3DVec4to3Transform_SSE2(view9, {light.Direction.x, light.Direction.y, light.Direction.z, 0.0f}));
              l->LightPosition = D3DVec4to3Transform_SSE2(view9, {light.Position.x, light.Position.y, light.Position.z, 1.0f});
              l->cosHalfPhi = cosf(light.Phi / 2.0f);
              l->cosHalfTheta = cosf(light.Theta / 2.0f);
              l->Attenuation0 = light.Attenuation0;
              l->Attenuation1 = light.Attenuation1;
              l->Attenuation2 = light.Attenuation2;
              l->Range = light.Range;
              l->Falloff = light.Falloff;
              break;
            default:
              continue;
        }
        lights.push_back(*l);
      }
    }

    for (uint16_t t = 0; t < pvData->vertexCount; t++) {
      uint8_t* inPtr = pvData->inData + t * pvData->inStride;
      uint8_t* outPtr = pvData->outData + t * pvData->outStride;

      PositionArray inPosition = { };
      D3DVECTOR* inNormals = nullptr;
      D3DCOLOR* inDiffuse  = nullptr;
      D3DCOLOR* inSpecular = nullptr;
      TexCoordArray inTexCoords = { };

      ProcessVerticesInput(pvData->doNotCopyData, pvData->inFVF, inPtr, inPosition,
                           &inNormals, inTexCoords, &inDiffuse, &inSpecular);

      // Transform vertices
      PositionArray outPosition;
      memcpy(outPosition.data(), inPosition.data(), sizeof(PositionArray));
      if (likely(!(pvData->inFVF & D3DFVF_XYZRHW))) {
        D3DVECTOR4 h = D3DVec4Transform_SSE2(wvp, D3DVECTOR4{inPosition[0], inPosition[1], inPosition[2], 1.0f});

        // Hidden & Dangerous (D3D6) relies on division by zero and NAN/INF output
        const float rhw = 1.0f / h.w;
        outPosition[0] = viewport9.X + viewport9HalfWidth  * (h.x * rhw + 1.0f);
        outPosition[1] = viewport9.Y + viewport9HalfHeight * (1.0f - h.y * rhw);
        outPosition[2] = viewport9.MinZ + h.z * rhw * viewport9ZDelta;

        // Native supposedly always write rhw without checking output, be more sane until that become a real problem
        if (likely(pvData->outFVF & D3DFVF_XYZRHW))
          outPosition[3] = rhw;

        // Alternate pixel center workaround for the Resident Evil quad alignment problem
        if (unlikely(options->alternatePixelCenter == AlternatePixelCenter::Legacy)) {
          outPosition[0] -= 0.5f;
          outPosition[1] -= 0.5f;
        }
      }

      D3DCOLORVALUE diffuse  = {0.0f, 0.0f, 0.0f, 0.0f};
      D3DCOLORVALUE specular = {0.0f, 0.0f, 0.0f, 0.0f};

      D3DVECTOR4 WVPosition = D3DVec4Transform_SSE2(wv, D3DVECTOR4{inPosition[0], inPosition[1], inPosition[2], 1.0f});
      const D3DVECTOR WVPosition3 = {WVPosition.x, WVPosition.y, WVPosition.z};
      const float positionScale = 1.0f / WVPosition.w;
      WVPosition.x *= positionScale;
      WVPosition.y *= positionScale;
      WVPosition.z *= positionScale;

      D3DCOLORVALUE materialDiffuse  = ColorFromMaterialSource(inDiffuse, inSpecular, sourceDiffuse,  material9.Diffuse);
      D3DCOLORVALUE materialSpecular = ColorFromMaterialSource(inDiffuse, inSpecular, sourceSpecular, material9.Specular);

      if (useLighting && (pvData->outFVF & (D3DFVF_DIFFUSE | D3DFVF_SPECULAR))) {
        const D3DVECTOR NVWPosition = D3DVec3Normalize(WVPosition3);
        D3DVECTOR normals;
        if (inNormals != nullptr) {
          D3DMATRIX normalMatrix{};
          ComputeNormalMatrix(normalMatrix, pvData->isLegacy, wv);
          normals = D3DVec3Transform_SSE2(normalMatrix, *inNormals);
          if (isEnabledNormalizeNormals)
            normals = D3DVec3Normalize(normals);
        }

        D3DCOLORVALUE ambient = ambientStateColorValue;

        for (const PVLIGHT& light : lights) {
          D3DVECTOR hitDirection;
          float attenuation;
          const D3DLIGHTTYPE lightType = D3DLIGHTTYPE(light.Type);
          switch (lightType) {
            case D3DLIGHT_DIRECTIONAL: {
              hitDirection = light.LightDirection;
              attenuation = 1.0f;
              break;
            }
            case D3DLIGHT_POINT:
            case D3DLIGHT_SPOT: {
              hitDirection = D3DVECTOR{light.LightPosition.x - WVPosition.x, light.LightPosition.y - WVPosition.y,
                                       light.LightPosition.z - WVPosition.z};
              Vector4 destination(1.0f, 0.0f, D3DVec3Dot_SSE2(&hitDirection, &hitDirection), 0.0f);
              destination.y = sqrtf(destination.z);
              if (pvData->isLegacy) {
                destination.y = (light.Range - destination.y) / light.Range;
                if (destination.y <= 0.0f)
                  continue;
                destination.z = destination.y * destination.y;
              } else {
                if (destination.y > light.Range)
                  continue;
              }

              hitDirection = D3DVec3Normalize(hitDirection);
              attenuation = destination.x * light.Attenuation0 + destination.y *
                            light.Attenuation1 + destination.z * light.Attenuation2;

              if (!pvData->isLegacy)
                attenuation = 1.0f / attenuation;

              if (lightType == D3DLIGHT_SPOT) {
                const D3DVECTOR revHit = {-hitDirection.x, -hitDirection.y, -hitDirection.z};
                const float rho = D3DVec3Dot_SSE2(&revHit, &light.LightDirection);
                if (rho <= light.cosHalfPhi)
                  attenuation = 0.0f;
                else if (rho <= light.cosHalfTheta)
                  attenuation *= powf((rho - light.cosHalfPhi) / (light.cosHalfTheta - light.cosHalfPhi), light.Falloff);
              }
              break;
            }
            default:
              Logger::warn(str::format("ProcessVerticesSW: Invalid light type: ", lightType));
              continue;
          }

          ColorVMultiplyAdd_SSE2(ambient, light.Ambient, attenuation);
          if (inNormals != nullptr)
            ApplyLight(&light, isEnabledLocalViewer, diffuse, specular, &normals,
                     &hitDirection, &NVWPosition, attenuation, materialPower, pvData->isLegacy);
        }

        D3DCOLORVALUE materialAmbient  = ColorFromMaterialSource(inDiffuse, inSpecular, sourceAmbient, material9.Ambient);
        D3DCOLORVALUE materialEmissive = ColorFromMaterialSource(inDiffuse, inSpecular, sourceEmissive, material9.Emissive);

        diffuse.r = D3DSSE_Mad2_Add_Scalar_SSE2(ambient.r, materialAmbient.r, diffuse.r, materialDiffuse.r, materialEmissive.r);
        diffuse.g = D3DSSE_Mad2_Add_Scalar_SSE2(ambient.g, materialAmbient.g, diffuse.g, materialDiffuse.g, materialEmissive.g);
        diffuse.b = D3DSSE_Mad2_Add_Scalar_SSE2(ambient.b, materialAmbient.b, diffuse.b, materialDiffuse.b, materialEmissive.b);
        diffuse.a = materialDiffuse.a;

        D3DColorMulRGB_SetAlpha_SSE2(specular, materialSpecular, pvData->isLegacy);
      } else {
        diffuse  = materialDiffuse;
        specular = materialSpecular;
      }

      if (isEnabledFog)
        ApplyFog(&specular.a, fogVertexMode, fogDensity, fogStart, fogEnd, isEnabledFogRange, WVPosition3);

      D3DColorClamp_SSE2(diffuse);
      D3DColorClamp_SSE2(specular);

      const D3DCOLOR outDiffuse  = ColorVToColor(diffuse);
      const D3DCOLOR outSpecular = ColorVToColor(specular);

      ProcessVerticesOutput(pvData->outFVF, outPtr, outPosition,
                            (!pvData->doNotCopyData ? inNormals : nullptr),
                            (!pvData->doNotCopyData ? &inTexCoords : nullptr),
                            (!pvData->doNotCopyData ? &outDiffuse : nullptr),
                            (!pvData->doNotCopyData ? &outSpecular : nullptr));
    }
  }

}
