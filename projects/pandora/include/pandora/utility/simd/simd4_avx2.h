constexpr std::array<uint32_t, 16> genCompressLUT4()
{
    std::array<uint32_t, 16> result = {};
    for (uint32_t mask = 0; mask < 16; mask++) {
        uint32_t indices = 0; // Permutation indices
        uint32_t k = 0;
        for (uint32_t bit = 0; bit < 8; bit++) {
            if ((mask & (1ull << bit)) != 0) { // If bit is set
                indices |= bit << k; // Add to permutation indices
                k += 8; // One byte
            }
        }
        result[mask] = indices;
    }
    return result;
}
constexpr std::array<uint32_t, 16> s_sseIndicesLUT = genCompressLUT4();
const __m128i s_sseMaskOnes = _mm_set1_epi32(0xFFFFFFFF);

template <typename T, int S>
class _vec4;

template <int S>
class _mask4;

template <typename T>
_vec4<T, 4> min(const _vec4<T, 4>& a, const _vec4<T, 4>& b);

template <typename T>
_vec4<T, 4> max(const _vec4<T, 4>& a, const _vec4<T, 4>& b);

template <typename T>
_vec4<T, 4> blend(const _vec4<T, 4>& a, const _vec4<T, 4>& b, const _mask4<4>& mask);

template <>
class alignas(16) _mask4<4> {
public:
    _mask4() = default;
    explicit inline _mask4(const __m128i& value)
        : m_value(value)
        , m_bitMask(_mm_movemask_ps(_mm_castsi128_ps(m_value)))
    {
    }
    explicit inline _mask4(const __m128& value)
        : m_value(_mm_castps_si128(value))
        , m_bitMask(_mm_movemask_ps(value))
    {
    }
    _mask4(bool v0, bool v1, bool v2, bool v3)
    {
        m_value = _mm_set_epi32(
            v3 ? 0xFFFFFFFF : 0x0,
            v2 ? 0xFFFFFFFF : 0x0,
            v1 ? 0xFFFFFFFF : 0x0,
            v0 ? 0xFFFFFFFF : 0x0);
        m_bitMask = _mm_movemask_ps(_mm_castsi128_ps(m_value));
    }

	inline _mask4 operator&&(const _mask4& other) const
	{
		_mask4 result;
		result.m_bitMask = m_bitMask & other.m_bitMask;
		result.m_value = _mm_and_si128(m_value, other.m_value);
		return result;
	}

	inline _mask4 operator||(const _mask4& other) const
	{
		_mask4 result;
		result.m_bitMask = m_bitMask | other.m_bitMask;
		result.m_value = _mm_or_si128(m_value, other.m_value);
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
        return count() == 4;
    }

    inline __m128i computeCompressPermutation() const
    {
        uint32_t wantedIndices = s_sseIndicesLUT[m_bitMask];
        return _mm_cvtepu8_epi32(_mm_cvtsi32_si128(wantedIndices));
    }

private:
    __m128i m_value;
    int32_t m_bitMask;

    template <typename T, int S>
    friend class _vec4;

	friend _vec4<uint32_t, 4> blend(const _vec4<uint32_t, 4>& a, const _vec4<uint32_t, 4>& b, const _mask4<4>& mask);
	friend _vec4<float, 4> blend(const _vec4<float, 4>& a, const _vec4<float, 4>& b, const _mask4<4>& mask);
};

template <>
class _vec4<uint32_t, 4> {
public:
    friend class _vec4<float, 4>; // Make friend so it can access us in permute & compress operations

    _vec4() = default;
    explicit inline _vec4(gsl::span<const uint32_t, 4> v)
    {
        load(v);
    }
    explicit inline _vec4(uint32_t value)
    {
        broadcast(value);
    }
    explicit inline _vec4(uint32_t v0, uint32_t v1, uint32_t v2, uint32_t v3)
    {
        m_value = _mm_set_epi32(v3, v2, v1, v0);
    }
    explicit inline _vec4(__m128i value)
        : m_value(value)
    {
    }

    inline void loadAligned(gsl::span<const uint32_t, 4> v)
    {
        m_value = _mm_load_si128(reinterpret_cast<const __m128i*>(v.data()));
    }

    inline void storeAligned(gsl::span<uint32_t, 4> v) const
    {
        _mm_store_si128(reinterpret_cast<__m128i*>(v.data()), m_value);
    }

    inline void load(gsl::span<const uint32_t, 4> v)
    {
        m_value = _mm_loadu_si128(reinterpret_cast<const __m128i*>(v.data()));
    }

    inline void store(gsl::span<uint32_t, 4> v) const
    {
        _mm_storeu_si128(reinterpret_cast<__m128i*>(v.data()), m_value);
    }

    inline void broadcast(uint32_t value)
    {
        m_value = _mm_set1_epi32(value);
    }

    inline _vec4<uint32_t, 4> operator+(const _vec4<uint32_t, 4>& other) const
    {
        return _vec4(_mm_add_epi32(m_value, other.m_value));
    }

    inline _vec4<uint32_t, 4> operator-(const _vec4<uint32_t, 4>& other) const
    {
        return _vec4(_mm_sub_epi32(m_value, other.m_value));
    }

    inline _vec4<uint32_t, 4> operator*(const _vec4<uint32_t, 4>& other) const
    {
        return _vec4(_mm_mullo_epi32(m_value, other.m_value));
    }

    // No integer divide (not part of SSE/AVX anyways)

    inline _vec4<uint32_t, 4> operator<<(const _vec4<uint32_t, 4>& amount) const
    {
        // AVX2 functionality
        return _vec4(_mm_sllv_epi32(m_value, amount.m_value));
    }

    inline _vec4<uint32_t, 4> operator<<(uint32_t amount) const
    {
        return _vec4(_mm_slli_epi32(m_value, amount));
    }

    inline _vec4<uint32_t, 4> operator>>(const _vec4<uint32_t, 4>& amount) const
    {
        // AVX2 functionality
        return _vec4(_mm_srlv_epi32(m_value, amount.m_value));
    }

    inline _vec4<uint32_t, 4> operator>>(uint32_t amount) const
    {
        return _vec4(_mm_srli_epi32(m_value, amount));
    }

    inline _vec4<uint32_t, 4> operator&(const _vec4<uint32_t, 4>& other) const
    {
        return _vec4(_mm_and_si128(m_value, other.m_value));
    }

    inline _mask4<4> operator<(const _vec4<uint32_t, 4>& other) const
    {
        return _mask4<4>(_mm_cmplt_epi32(m_value, other.m_value));
    }

    inline _mask4<4> operator<=(const _vec4<uint32_t, 4>& other) const
    {
        __m128i greaterThan = _mm_cmpgt_epi32(m_value, other.m_value);
        __m128i notGreaterThan = _mm_xor_si128(greaterThan, s_sseMaskOnes);
        return _mask4<4>(notGreaterThan);
    }

    inline _mask4<4> operator>(const _vec4<uint32_t, 4>& other) const
    {
        return _mask4<4>(_mm_cmpgt_epi32(m_value, other.m_value));
    }

    inline _mask4<4> operator>=(const _vec4<uint32_t, 4>& other) const
    {
        __m128i lessThan = _mm_cmpgt_epi32(other.m_value, m_value);
        __m128i notLessThan = _mm_xor_si128(lessThan, s_sseMaskOnes);
        return _mask4<4>(notLessThan);
    }

    inline _vec4<uint32_t, 4> permute(const _vec4<uint32_t, 4>& index) const
    {
        // AVX functionality
        return _vec4(_mm_castps_si128(_mm_permutevar_ps(_mm_castsi128_ps(m_value), index.m_value)));
    }

    inline _vec4<uint32_t, 4> compress(const _mask4<4>& mask) const
    {
        __m128i shuffleMask = mask.computeCompressPermutation();
        return permute(_vec4(shuffleMask));
    }

    inline uint64_t horizontalMinPos() const
    {
        std::array<uint32_t, 4> values;
        store(values);
        return std::distance(std::begin(values), std::min_element(std::begin(values), std::end(values)));
    }

    inline uint32_t horizontalMin() const
    {
        std::array<uint32_t, 4> values;
        store(values);
        return *std::min_element(std::begin(values), std::end(values));
    }

    inline uint64_t horizontalMaxPos() const
    {
        std::array<uint32_t, 4> values;
        store(values);
        return std::distance(std::begin(values), std::max_element(std::begin(values), std::end(values)));
    }

    inline uint32_t horizontalMax() const
    {
        std::array<uint32_t, 4> values;
        store(values);
        return *std::max_element(std::begin(values), std::end(values));
    }

    friend _vec4<uint32_t, 4> min(const _vec4<uint32_t, 4>& a, const _vec4<uint32_t, 4>& b);
    friend _vec4<uint32_t, 4> max(const _vec4<uint32_t, 4>& a, const _vec4<uint32_t, 4>& b);

	friend _vec4<uint32_t, 4> blend(const _vec4<uint32_t, 4>& a, const _vec4<uint32_t, 4>& b, const _mask4<4>& mask);
private:
    __m128i m_value;
};

template <>
class alignas(16) _vec4<float, 4> {
public:
    _vec4() = default;
    inline _vec4(gsl::span<const float, 4> v)
    {
        load(v);
    }
    inline _vec4(float value)
    {
        broadcast(value);
    }
    inline _vec4(float v0, float v1, float v2, float v3)
    {
        m_value = _mm_set_ps(v3, v2, v1, v0);
    }
    explicit inline _vec4(__m128 value)
        : m_value(value)
    {
    }

    inline void loadAligned(gsl::span<const float, 4> v)
    {
        m_value = _mm_load_ps(v.data());
    }

    inline void storeAligned(gsl::span<float, 4> v) const
    {
        _mm_store_ps(v.data(), m_value);
    }

    inline void load(gsl::span<const float, 4> v)
    {
        m_value = _mm_loadu_ps(v.data());
    }

    inline void store(gsl::span<float, 4> v) const
    {
        _mm_storeu_ps(v.data(), m_value);
    }

    inline void broadcast(float value)
    {
        m_value = _mm_set_ps1(value);
    }

    inline _vec4<float, 4> operator+(const _vec4<float, 4>& other) const
    {
        return _vec4(_mm_add_ps(m_value, other.m_value));
    }

    inline _vec4<float, 4> operator-(const _vec4<float, 4>& other) const
    {
        return _vec4(_mm_sub_ps(m_value, other.m_value));
    }

    inline _vec4<float, 4> operator*(const _vec4<float, 4>& other) const
    {
        return _vec4(_mm_mul_ps(m_value, other.m_value));
    }

    inline _vec4<float, 4> operator/(const _vec4<float, 4>& other) const
    {
        return _vec4(_mm_div_ps(m_value, other.m_value));
    }

    inline _mask4<4> operator<(const _vec4<float, 4>& other) const
    {
        return _mask4<4>(_mm_cmplt_ps(m_value, other.m_value));
    }

    inline _mask4<4> operator<=(const _vec4<float, 4>& other) const
    {
        return _mask4<4>(_mm_cmple_ps(m_value, other.m_value));
    }

    inline _mask4<4> operator>(const _vec4<float, 4>& other) const
    {
        return _mask4<4>(_mm_cmpgt_ps(m_value, other.m_value));
    }

    inline _mask4<4> operator>=(const _vec4<float, 4>& other) const
    {
        return _mask4<4>(_mm_cmpge_ps(m_value, other.m_value));
    }

    inline _vec4<float, 4> permute(const _vec4<uint32_t, 4>& index) const
    {
        // AVX functionality
        return _vec4(_mm_permutevar_ps(m_value, index.m_value));
    }

    inline _vec4<float, 4> compress(const _mask4<4>& mask) const
    {
        __m128i shuffleMask = mask.computeCompressPermutation();
        return permute(_vec4<uint32_t, 4>(shuffleMask));
    }

    inline uint64_t horizontalMinPos() const
    {
        std::array<float, 4> values;
        store(values);
        return std::distance(std::begin(values), std::min_element(std::begin(values), std::end(values)));
    }

    inline float horizontalMin() const
    {
        std::array<float, 4> values;
        store(values);
        return *std::min_element(std::begin(values), std::end(values));
    }

    inline uint64_t horizontalMaxPos() const
    {
        std::array<float, 4> values;
        store(values);
        return std::distance(std::begin(values), std::max_element(std::begin(values), std::end(values)));
    }

    inline float horizontalMax() const
    {
        std::array<float, 4> values;
        store(values);
        return *std::max_element(std::begin(values), std::end(values));
    }

    friend _vec4<float, 4> min(const _vec4<float, 4>& a, const _vec4<float, 4>& b);
    friend _vec4<float, 4> max(const _vec4<float, 4>& a, const _vec4<float, 4>& b);

	friend _vec4<float, 4> blend(const _vec4<float, 4>& a, const _vec4<float, 4>& b, const _mask4<4>& mask);

private:
    __m128 m_value;
};

inline _vec4<uint32_t, 4> min(const _vec4<uint32_t, 4>& a, const _vec4<uint32_t, 4>& b)
{
    return _vec4<uint32_t, 4>(_mm_min_epu32(a.m_value, b.m_value));
}

inline _vec4<uint32_t, 4> max(const _vec4<uint32_t, 4>& a, const _vec4<uint32_t, 4>& b)
{
    return _vec4<uint32_t, 4>(_mm_max_epu32(a.m_value, b.m_value));
}

inline _vec4<float, 4> min(const _vec4<float, 4>& a, const _vec4<float, 4>& b)
{
    return _vec4<float, 4>(_mm_min_ps(a.m_value, b.m_value));
}

inline _vec4<float, 4> max(const _vec4<float, 4>& a, const _vec4<float, 4>& b)
{
    return _vec4<float, 4>(_mm_max_ps(a.m_value, b.m_value));
}

inline _vec4<uint32_t, 4> blend(const _vec4<uint32_t, 4>& a, const _vec4<uint32_t, 4>& b, const _mask4<4>& mask)
{
	// Cant use _mm_blend_epi32 because it relies on a compile time constant mask
	// Casting to float because _mm_blendv_ps faster than _mm_blendv_epi8
	return _vec4<uint32_t, 4>(_mm_castps_si128(_mm_blendv_ps(_mm_castsi128_ps(a.m_value), _mm_castsi128_ps(b.m_value), _mm_castsi128_ps(mask.m_value))));
}

inline _vec4<float, 4> blend(const _vec4<float, 4>& a, const _vec4<float, 4>& b, const _mask4<4>& mask)
{
	// Cant use _mm_blend_ps because it relies on a compile time constant mask
	return _vec4<float, 4>(_mm_blendv_ps(a.m_value, b.m_value, _mm_castsi128_ps(mask.m_value)));
}

