constexpr std::array<uint64_t, 256> genCompressLUT8()
{
    std::array<uint64_t, 256> result = {};
    for (uint64_t mask = 0; mask < 256; mask++) {
        uint64_t indices = 0; // Permutation indices
        uint64_t k = 0;
        for (uint64_t bit = 0; bit < 8; bit++) {
            if ((mask & (1ull << bit)) != 0) { // If bit is set
                indices |= bit << k; // Add to permutation indices
                k += 8; // One byte
            }
        }
        result[mask] = indices;
    }
    return result;
}
constexpr std::array<uint64_t, 256> s_avxIndicesLUT = genCompressLUT8();
const __m256i s_avxMaskOnes = _mm256_set1_epi32(0xFFFFFFFF);

template <typename T, int S>
class vec8;

template <int S>
class mask8;

template <typename T>
vec8<T, 8> blend(const vec8<T, 8>& a, const vec8<T, 8>& b, const mask8<8>& mask);

template <>
class alignas(32) mask8<8> {
public:
    mask8() = default;
    explicit inline mask8(const __m256i& value)
        : m_value(value)
        , m_bitMask(_mm256_movemask_ps(_mm256_castsi256_ps(m_value)))
    {
    }
    explicit inline mask8(const __m256& value)
        : m_value(_mm256_castps_si256(value))
        , m_bitMask(_mm256_movemask_ps(value))
    {
    }
    inline mask8(bool v0, bool v1, bool v2, bool v3, bool v4, bool v5, bool v6, bool v7)
    {
        m_value = _mm256_set_epi32(
            v7 ? 0xFFFFFFFF : 0x0,
            v6 ? 0xFFFFFFFF : 0x0,
            v5 ? 0xFFFFFFFF : 0x0,
            v4 ? 0xFFFFFFFF : 0x0,
            v3 ? 0xFFFFFFFF : 0x0,
            v2 ? 0xFFFFFFFF : 0x0,
            v1 ? 0xFFFFFFFF : 0x0,
            v0 ? 0xFFFFFFFF : 0x0);
        m_bitMask = _mm256_movemask_ps(_mm256_castsi256_ps(m_value));
    }

	inline mask8 operator&&(const mask8& other)
	{
		mask8 result;
		result.m_bitMask = m_bitMask & other.m_bitMask;
		result.m_value = _mm256_and_si256(m_value, other.m_value);
		return result;
	}

	inline mask8 operator||(const mask8& other)
	{
		mask8 result;
		result.m_bitMask = m_bitMask | other.m_bitMask;
		result.m_value = _mm256_or_si256(m_value, other.m_value);
		return result;
	}

    inline int count(unsigned validMask) const
    {
        uint32_t mask = m_bitMask & validMask;
        return _mm_popcnt_u32(mask);
    }

    inline int count() const
    {
        return _mm_popcnt_u32(m_bitMask);
    }

    inline bool none() const
    {
        return count() == 0;
    }

    inline bool any() const
    {
        return count() > 0;
    }

    inline bool all() const
    {
        return count() == 8;
    }

    inline __m256i computeCompressPermutation() const
    {
        uint64_t wantedIndices = s_avxIndicesLUT[m_bitMask];

        /*// https://stackoverflow.com/questions/36932240/avx2-what-is-the-most-efficient-way-to-pack-left-based-on-a-mask/36951611
		// Emulate compress operation since it is not part of AVX2 (only AVX512)
		uint64_t expandedMask = _pdep_u64(mask.m_bitMask, 0x0101010101010101); // Unpack each bit to a byte
		expandedMask *= 0xFF; // mask |= mask<<1 | mask<<2 | ... | mask<<7;
		// ABC... -> AAAAAAAABBBBBBBBCCCCCCCC...: replicate each bit to fill its byte

		const uint64_t identityIndices = 0x0706050403020100; // The identity shuffle for vpermps, packed to one index per byte
		uint64_t wantedIndices = _pext_u64(identityIndices, expandedMask);*/

        __m128i byteVec = _mm_cvtsi64_si128(wantedIndices);
        return _mm256_cvtepu8_epi32(byteVec);
    }

private:
    __m256i m_value;
    unsigned m_bitMask;

    template <typename T, int S>
    friend class vec8;

	friend vec8<uint32_t, 8> blend(const vec8<uint32_t, 8>& a, const vec8<uint32_t, 8>& b, const mask8<8>& mask);
	friend vec8<float, 8> blend(const vec8<float, 8>& a, const vec8<float, 8>& b, const mask8<8>& mask);
};

template <>
class alignas(32) vec8<uint32_t, 8> {
public:
    friend class vec8<float, 8>; // Make friend so it can access us in permute & compress operations

    vec8() = default;
    explicit inline vec8(gsl::span<const uint32_t, 8> v)
    {
        load(v);
    }
    explicit inline vec8(uint32_t value)
    {
        broadcast(value);
    }
    explicit inline vec8(uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3, uint32_t v4, uint32_t v5, uint32_t v6, uint32_t v7)
    {
        m_value = _mm256_set_epi32(v7, v6, v5, v4, v3, v2, v1, v0);
    }
    explicit inline vec8(__m256i value)
        : m_value(value)
    {
    }

    inline void loadAligned(gsl::span<const uint32_t, 8> v)
    {
        m_value = _mm256_load_si256(reinterpret_cast<const __m256i*>(v.data()));
    }

    inline void storeAligned(gsl::span<uint32_t, 8> v) const
    {
        _mm256_store_si256(reinterpret_cast<__m256i*>(v.data()), m_value);
    }

    inline void load(gsl::span<const uint32_t, 8> v)
    {
        m_value = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(v.data()));
    }

    inline void store(gsl::span<uint32_t, 8> v) const
    {
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(v.data()), m_value);
    }

    inline void broadcast(uint32_t value)
    {
        m_value = _mm256_broadcastd_epi32(_mm_set_epi32(value, value, value, value));
    }

    inline vec8<uint32_t, 8> operator+(const vec8<uint32_t, 8>& other) const
    {
        return vec8(_mm256_add_epi32(m_value, other.m_value));
    }

    inline vec8<uint32_t, 8> operator-(const vec8<uint32_t, 8>& other) const
    {
        return vec8(_mm256_sub_epi32(m_value, other.m_value));
    }

    inline vec8<uint32_t, 8> operator*(const vec8<uint32_t, 8>& other) const
    {
        return vec8(_mm256_mullo_epi32(m_value, other.m_value));
    }

    // No integer divide (not part of AVX anyways)

    inline vec8<uint32_t, 8> operator<<(const vec8<uint32_t, 8>& amount) const
    {
        return vec8(_mm256_sllv_epi32(m_value, amount.m_value));
    }

    inline vec8<uint32_t, 8> operator<<(uint32_t amount) const
    {
        return vec8(_mm256_slli_epi32(m_value, amount));
    }

    inline vec8<uint32_t, 8> operator>>(const vec8<uint32_t, 8>& amount) const
    {
        return vec8(_mm256_srlv_epi32(m_value, amount.m_value));
    }

    inline vec8<uint32_t, 8> operator>>(uint32_t amount) const
    {
        return vec8(_mm256_srli_epi32(m_value, amount));
    }

    inline vec8<uint32_t, 8> operator&(const vec8<uint32_t, 8>& other) const
    {
        return vec8(_mm256_and_si256(m_value, other.m_value));
    }

    inline mask8<8> operator<(const vec8<uint32_t, 8>& other) const
    {
        return mask8<8>(_mm256_cmpgt_epi32(other.m_value, m_value));
    }

    inline mask8<8> operator<=(const vec8<uint32_t, 8>& other) const
    {
        __m256i greaterThan = _mm256_cmpgt_epi32(m_value, other.m_value);
        __m256i notGreaterThan = _mm256_xor_si256(greaterThan, s_avxMaskOnes);
        return mask8<8>(notGreaterThan);
    }

    inline mask8<8> operator>(const vec8<uint32_t, 8>& other) const
    {
        return mask8<8>(_mm256_cmpgt_epi32(m_value, other.m_value));
    }

    inline mask8<8> operator>=(const vec8<uint32_t, 8>& other) const
    {
        __m256i lessThan = _mm256_cmpgt_epi32(other.m_value, m_value);
        __m256i notLessThan = _mm256_xor_si256(lessThan, s_avxMaskOnes);
        return mask8<8>(notLessThan);
    }

    inline vec8<uint32_t, 8> permute(const vec8<uint32_t, 8>& index) const
    {
        return vec8(_mm256_permutevar8x32_epi32(m_value, index.m_value));
    }

    inline vec8<uint32_t, 8> compress(const mask8<8>& mask) const
    {
        __m256i shuffleMask = mask.computeCompressPermutation();
        return vec8(_mm256_permutevar8x32_epi32(m_value, shuffleMask));
    }

    inline uint64_t horizontalMinPos() const
    {
        std::array<uint32_t, 8> values;
        store(values);
        return std::distance(std::begin(values), std::min_element(std::begin(values), std::end(values)));
    }

    inline uint32_t horizontalMin() const
    {
        std::array<uint32_t, 8> values;
        store(values);
        return *std::min_element(std::begin(values), std::end(values));
    }

    inline uint64_t horizontalMaxPos() const
    {
        std::array<uint32_t, 8> values;
        store(values);
        return std::distance(std::begin(values), std::max_element(std::begin(values), std::end(values)));
    }

    inline uint32_t horizontalMax() const
    {
        std::array<uint32_t, 8> values;
        store(values);
        return *std::max_element(std::begin(values), std::end(values));
    }

    friend vec8<uint32_t, 8> min(const vec8<uint32_t, 8>& a, const vec8<uint32_t, 8>& b);
    friend vec8<uint32_t, 8> max(const vec8<uint32_t, 8>& a, const vec8<uint32_t, 8>& b);
	friend vec8<uint32_t, 8> blend(const vec8<uint32_t, 8>& a, const vec8<uint32_t, 8>& b, const mask8<8>& mask);

private:
    __m256i m_value;
};

template <>
class alignas(32) vec8<float, 8> {
public:
    vec8() = default;
    explicit inline vec8(gsl::span<const float, 8> v)
    {
        load(v);
    }
    explicit inline vec8(float value)
    {
        broadcast(value);
    }
    explicit inline vec8(float v0, float v1, float v2, float v3, float v4, float v5, float v6, float v7)
    {
        m_value = _mm256_set_ps(v7, v6, v5, v4, v3, v2, v1, v0);
    }
    explicit inline vec8(__m256 value)
        : m_value(value)
    {
    }

    inline void loadAligned(gsl::span<const float, 8> v)
    {
        m_value = _mm256_load_ps(v.data());
    }

    inline void storeAligned(gsl::span<float, 8> v) const
    {
        _mm256_store_ps(v.data(), m_value);
    }

    inline void load(gsl::span<const float, 8> v)
    {
        m_value = _mm256_loadu_ps(v.data());
    }

    inline void store(gsl::span<float, 8> v) const
    {
        _mm256_storeu_ps(v.data(), m_value);
    }

    inline void broadcast(float value)
    {
        m_value = _mm256_broadcast_ss(&value);
    }

    inline vec8<float, 8> operator+(const vec8<float, 8>& other) const
    {
        return vec8(_mm256_add_ps(m_value, other.m_value));
    }

    inline vec8<float, 8> operator-(const vec8<float, 8>& other) const
    {
        return vec8(_mm256_sub_ps(m_value, other.m_value));
    }

    inline vec8<float, 8> operator*(const vec8<float, 8>& other) const
    {
        return vec8(_mm256_mul_ps(m_value, other.m_value));
    }

    inline vec8<float, 8> operator/(const vec8<float, 8>& other) const
    {
        return vec8(_mm256_div_ps(m_value, other.m_value));
    }

    inline mask8<8> operator<(const vec8<float, 8>& other) const
    {
        return mask8<8>(_mm256_cmp_ps(m_value, other.m_value, _CMP_LT_OS));
    }

    inline mask8<8> operator<=(const vec8<float, 8>& other) const
    {
        return mask8<8>(_mm256_cmp_ps(m_value, other.m_value, _CMP_LE_OS));
    }

    inline mask8<8> operator>(const vec8<float, 8>& other) const
    {
        return mask8<8>(_mm256_cmp_ps(m_value, other.m_value, _CMP_GT_OS));
    }

    inline mask8<8> operator>=(const vec8<float, 8>& other) const
    {
        return mask8<8>(_mm256_cmp_ps(m_value, other.m_value, _CMP_GE_OS));
    }

    inline vec8<float, 8> permute(const vec8<uint32_t, 8>& index) const
    {
        return vec8(_mm256_permutevar8x32_ps(m_value, index.m_value));
    }

    inline vec8<float, 8> compress(const mask8<8>& mask) const
    {
        __m256i shuffleMask = mask.computeCompressPermutation();
        return vec8(_mm256_permutevar8x32_ps(m_value, shuffleMask));
    }

    inline uint64_t horizontalMinPos() const
    {
        std::array<float, 8> values;
        store(values);
        return std::distance(std::begin(values), std::min_element(std::begin(values), std::end(values)));
    }

    inline float horizontalMin() const
    {
        std::array<float, 8> values;
        store(values);
        return *std::min_element(std::begin(values), std::end(values));
    }

    inline uint64_t horizontalMaxPos() const
    {
        std::array<float, 8> values;
        store(values);
        return std::distance(std::begin(values), std::max_element(std::begin(values), std::end(values)));
    }

    inline float horizontalMax() const
    {
        std::array<float, 8> values;
        store(values);
        return *std::max_element(std::begin(values), std::end(values));
    }

    friend vec8<float, 8> min(const vec8<float, 8>& a, const vec8<float, 8>& b);
    friend vec8<float, 8> max(const vec8<float, 8>& a, const vec8<float, 8>& b);
	friend vec8<float, 8> blend(const vec8<float, 8>& a, const vec8<float, 8>& b, const mask8<8>& mask);;

private:
    __m256 m_value;
};

inline vec8<uint32_t, 8> min(const vec8<uint32_t, 8>& a, const vec8<uint32_t, 8>& b)
{
    vec8<uint32_t, 8> result;
    result.m_value = _mm256_min_epu32(a.m_value, b.m_value);
    return result;
}

inline vec8<uint32_t, 8> max(const vec8<uint32_t, 8>& a, const vec8<uint32_t, 8>& b)
{
    vec8<uint32_t, 8> result;
    result.m_value = _mm256_max_epu32(a.m_value, b.m_value);
    return result;
}

inline vec8<float, 8> min(const vec8<float, 8>& a, const vec8<float, 8>& b)
{
    vec8<float, 8> result;
    result.m_value = _mm256_min_ps(a.m_value, b.m_value);
    return result;
}

inline vec8<float, 8> max(const vec8<float, 8>& a, const vec8<float, 8>& b)
{
    vec8<float, 8> result;
    result.m_value = _mm256_max_ps(a.m_value, b.m_value);
    return result;
}

inline vec8<uint32_t, 8> blend(const vec8<uint32_t, 8>& a, const vec8<uint32_t, 8>& b, const mask8<8>& mask)
{
	// Cant use _mm_blend_epi32 because it relies on a compile time constant mask
	// Casting to float because _mm_blendv_ps faster than _mm_blendv_epi8
	return vec8<uint32_t, 8>(_mm256_castps_si256(_mm256_blendv_ps(_mm256_castsi256_ps(a.m_value), _mm256_castsi256_ps(b.m_value), _mm256_castsi256_ps(mask.m_value))));
}

inline vec8<float, 8> blend(const vec8<float, 8>& a, const vec8<float, 8>& b, const mask8<8>& mask)
{
	// Cant use _mm_blend_ps because it relies on a compile time constant mask
	return vec8<float, 8>(_mm256_blendv_ps(a.m_value, b.m_value, _mm256_castsi256_ps(mask.m_value)));
}
