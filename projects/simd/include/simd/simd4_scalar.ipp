template <typename T, int S>
class _vec4;

template <int S>
class _mask4;

template <typename T>
_vec4<T, 1> min(const _vec4<T, 1>& a, const _vec4<T, 1>& b);

template <typename T>
_vec4<T, 1> max(const _vec4<T, 1>& a, const _vec4<T, 1>& b);

template <typename T>
_vec4<T, 1> blend(const _vec4<T, 1>& a, const _vec4<T, 1>& b, const _mask4<1>& mask);

template <>
class _mask4<1> {
public:
    _mask4() = default;
    inline _mask4(bool v)
    {
        std::fill(std::begin(m_values), std::end(m_values), v);
    }
    inline _mask4(bool v0, bool v1, bool v2, bool v3)
    {
        m_values[0] = v0;
        m_values[1] = v1;
        m_values[2] = v2;
        m_values[3] = v3;
    }

    inline _mask4 operator&&(const _mask4& other) const
    {
        _mask4 result;
        std::transform(
            std::begin(m_values),
            std::end(m_values),
            std::begin(other.m_values),
            std::begin(result.m_values),
            std::logical_and<bool>());
        return result;
    }

    inline _mask4 operator||(const _mask4& other) const
    {
        _mask4 result;
        std::transform(
            std::begin(m_values),
            std::end(m_values),
            std::begin(other.m_values),
            std::begin(result.m_values),
            std::logical_or<bool>());
        return result;
    }

    inline int count(unsigned validMask) const
    {
        int c = 0;
        for (bool v : m_values) {
            if (v && (validMask & 0b1) == 0b1)
                c++;
            validMask >>= 1;
        }
        return c;
    }

    inline int count() const
    {
        int c = 0;
        for (bool v : m_values)
            if (v)
                c++;
        return c;
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

    inline int bitMask() const
    {
        int bitMask = 0;
        for (int i = 0; i < 4; i++)
            if (m_values[i])
                bitMask |= (1 << i);
        return bitMask;
    }

    inline std::array<uint32_t, 4> computeCompressPermutation() const
    {
        std::array<uint32_t, 4> result;
        int j = 0;
        for (int i = 0; i < 4; i++) {
            if (m_values[i])
                result[j++] = i;
        }
        for (; j < 4; j++)
            result[j] = 0;
        return result;
    }

private:
    std::array<bool, 4> m_values;

    template <typename T>
    friend _vec4<T, 1> blend(const _vec4<T, 1>& a, const _vec4<T, 1>& b, const _mask4<1>& mask);

    template <typename T, int S>
    friend class _vec4_base; // Not possible to friend only _vec4_base<T, 1>
};

// Partial template specialization does not inherit members/functions from less precise specializations.
// Use a templated base class to inherit functions/members that are the same between different template specializations (i.e. floats & ints)
template <typename T>
class _vec4_base<T, 1> {
public:
    _vec4_base() = default;

    _vec4_base(std::span<const T, 4> v)
    {
        std::copy(std::begin(v), std::end(v), std::begin(m_values));
    }

    _vec4_base(T value)
    {
        std::fill(std::begin(m_values), std::end(m_values), value);
    }

    _vec4_base(T v0, T v1, T v2, T v3)
    {
        m_values[0] = v0;
        m_values[1] = v1;
        m_values[2] = v2;
        m_values[3] = v3;
    }

    inline T operator[](int i) const
    {
        return m_values[i];
    }

    inline void load(std::span<const T, 4> v)
    {
        std::copy(std::begin(v), std::end(v), std::begin(m_values));
    }

    inline void store(std::span<T, 4> v) const
    {
        std::copy(std::begin(m_values), std::end(m_values), std::begin(v));
    }

    inline void loadAligned(std::span<const T, 4> v)
    {
        std::copy(std::begin(v), std::end(v), std::begin(m_values));
    }

    inline void storeAligned(std::span<T, 4> v) const
    {
        std::copy(std::begin(m_values), std::end(m_values), std::begin(v));
    }

    inline void broadcast(const T& v)
    {
        std::fill(std::begin(m_values), std::end(m_values), v);
    }

    inline _vec4<T, 1> operator+(const _vec4<T, 1>& other) const
    {
        _vec4<T, 1> result;
        for (int i = 0; i < 4; i++)
            result.m_values[i] = m_values[i] + other.m_values[i];
        return result;
    }

    inline _vec4<T, 1> operator-(const _vec4<T, 1>& other) const
    {
        _vec4<T, 1> result;
        for (int i = 0; i < 4; i++)
            result.m_values[i] = m_values[i] - other.m_values[i];
        return result;
    }

    inline _vec4<T, 1> operator*(const _vec4<T, 1>& other) const
    {
        _vec4<T, 1> result;
        for (int i = 0; i < 4; i++)
            result.m_values[i] = m_values[i] * other.m_values[i];
        return result;
    }

    inline _mask4<1> operator<(const _vec4<T, 1>& other) const
    {
        _mask4<1> mask;
        for (int i = 0; i < 4; i++)
            mask.m_values[i] = (m_values[i] < other.m_values[i]);
        return mask;
    }

    inline _mask4<1> operator<=(const _vec4<T, 1>& other) const
    {
        _mask4<1> mask;
        for (int i = 0; i < 4; i++)
            mask.m_values[i] = (m_values[i] <= other.m_values[i]);
        return mask;
    }

    inline _mask4<1> operator>(const _vec4<T, 1>& other) const
    {
        _mask4<1> mask;
        for (int i = 0; i < 4; i++)
            mask.m_values[i] = (m_values[i] > other.m_values[i]);
        return mask;
    }

    inline _mask4<1> operator>=(const _vec4<T, 1>& other) const
    {
        _mask4<1> mask;
        for (int i = 0; i < 4; i++)
            mask.m_values[i] = (m_values[i] >= other.m_values[i]);
        return mask;
    }

    inline _vec4<T, 1> abs() const
    {
        _vec4<T, 1> result;
        for (int i = 0; i < 4; i++) {
            result.m_values[i] = std::abs(m_values[i]);
        }
        return result;
    }

    inline _vec4<T, 1> operator-() const
    {
        _vec4<T, 1> result;
        for (int i = 0; i < 4; i++) {
            result.m_values[i] = -m_values[i];
        }
        return result;
    }

    inline _vec4<T, 1> compress(_mask4<1> mask) const
    {
        _vec4<T, 1> result;
        int j = 0;
        for (int i = 0; i < 4; i++) {
            if (mask.m_values[i])
                result.m_values[j++] = m_values[i];
        }
        for (; j < 4; j++)
            result.m_values[j] = 0;
        return result;
    }

    inline unsigned horizontalMinIndex() const
    {
        std::array<T, 4> values;
        store(values);
        return static_cast<unsigned>(std::distance(std::begin(values), std::min_element(std::begin(values), std::end(values))));
    }

    inline _vec4<T, 1> horizontalMinVec() const
    {
        std::array<T, 4> values;
        store(values);
        return _vec4<T, 1>(*std::min_element(std::begin(values), std::end(values)));
    }

    inline T horizontalMin() const
    {
        std::array<T, 4> values;
        store(values);
        return *std::min_element(std::begin(values), std::end(values));
    }

    inline unsigned horizontalMaxIndex() const
    {
        std::array<T, 4> values;
        store(values);
        return static_cast<unsigned>(std::distance(std::begin(values), std::max_element(std::begin(values), std::end(values))));
    }

    inline _vec4<T, 1> horizontalMaxVec() const
    {
        std::array<T, 4> values;
        store(values);
        return _vec4<T, 1>(*std::max_element(std::begin(values), std::end(values)));
    }

    inline T horizontalMax() const
    {
        std::array<T, 4> values;
        store(values);
        return *std::max_element(std::begin(values), std::end(values));
    }

    friend _vec4<T, 1> min<T>(const _vec4<T, 1>& a, const _vec4<T, 1>& b);
    friend _vec4<T, 1> max<T>(const _vec4<T, 1>& a, const _vec4<T, 1>& b);

    friend _vec4<T, 1> blend<T>(const _vec4<T, 1>& a, const _vec4<T, 1>& b, const _mask4<1>& mask);

    friend _vec4<uint32_t, 1> floatBitsToInt(const _vec4<float, 1>& a);
    friend _vec4<float, 1> intBitsToFloat(const _vec4<uint32_t, 1>& a);

protected:
    std::array<T, 4> m_values;
};

template <>
class _vec4<uint32_t, 1> : public _vec4_base<uint32_t, 1> {
public:
    // Inherit constructors
    using _vec4_base<uint32_t, 1>::_vec4_base;
    friend class _vec4<float, 1>; // Make friend so it can access us in permute & compress operations

    inline _vec4<uint32_t, 1> operator<<(const _vec4<uint32_t, 1>& amount) const
    {
        _vec4<uint32_t, 1> result;
        for (int i = 0; i < 4; i++) {
            result.m_values[i] = m_values[i] << amount.m_values[i];
        }
        return result;
    }

    inline _vec4<uint32_t, 1> operator<<(uint32_t amount) const
    {
        _vec4<uint32_t, 1> result;
        for (int i = 0; i < 4; i++) {
            result.m_values[i] = m_values[i] << amount;
        }
        return result;
    }

    inline _vec4<uint32_t, 1> operator>>(const _vec4<uint32_t, 1>& amount) const
    {
        _vec4<uint32_t, 1> result;
        for (int i = 0; i < 4; i++) {
            result.m_values[i] = m_values[i] >> amount.m_values[i];
        }
        return result;
    }

    inline _vec4<uint32_t, 1> operator>>(uint32_t amount) const
    {
        _vec4<uint32_t, 1> result;
        for (int i = 0; i < 4; i++) {
            result.m_values[i] = m_values[i] >> amount;
        }
        return result;
    }

    inline _vec4<uint32_t, 1> operator&(const _vec4<uint32_t, 1>& other) const
    {
        _vec4<uint32_t, 1> result;
        for (int i = 0; i < 4; i++) {
            result.m_values[i] = m_values[i] & other.m_values[i];
        }
        return result;
    }

    inline _vec4<uint32_t, 1> operator|(const _vec4<uint32_t, 1>& other) const
    {
        _vec4<uint32_t, 1> result;
        for (int i = 0; i < 4; i++) {
            result.m_values[i] = m_values[i] | other.m_values[i];
        }
        return result;
    }

    inline _vec4<uint32_t, 1> operator^(const _vec4<uint32_t, 1>& other) const
    {
        _vec4<uint32_t, 1> result;
        for (int i = 0; i < 4; i++) {
            result.m_values[i] = m_values[i] ^ other.m_values[i];
        }
        return result;
    }

    inline _vec4<uint32_t, 1> permute(const _vec4<uint32_t, 1>& index) const
    {
        _vec4<uint32_t, 1> result;
        for (int i = 0; i < 4; i++) {
            result.m_values[i] = m_values[index.m_values[i]];
        }
        return result;
    }
};

template <>
class _vec4<float, 1> : public _vec4_base<float, 1> {
public:
    // Inherit constructors
    using _vec4_base<float, 1>::_vec4_base;

    // Not in base class because we will not support it on integers
    inline _vec4<float, 1> operator/(const _vec4<float, 1>& other) const
    {
        _vec4<float, 1> result;
        for (int i = 0; i < 4; i++)
            result.m_values[i] = m_values[i] / other.m_values[i];
        return result;
    }

    inline _vec4<float, 1> permute(const _vec4<uint32_t, 1>& index) const
    {
        _vec4<float, 1> result;
        for (int i = 0; i < 4; i++) {
            result.m_values[i] = m_values[index.m_values[i]];
        }
        return result;
    }
};

inline _vec4<uint32_t, 1> floatBitsToInt(const _vec4<float, 1>& a)
{
    _vec4<uint32_t, 1> result;
    std::memcpy(result.m_values.data(), a.m_values.data(), sizeof(float) * 4);
    return result;
}

inline _vec4<float, 1> intBitsToFloat(const _vec4<uint32_t, 1>& a)
{
    _vec4<float, 1> result;
    std::memcpy(result.m_values.data(), a.m_values.data(), sizeof(uint32_t) * 4);
    return result;
}

template <typename T>
inline _vec4<T, 1> min(const _vec4<T, 1>& a, const _vec4<T, 1>& b)
{
    _vec4<T, 1> result;
    for (int i = 0; i < 4; i++)
        result.m_values[i] = std::min(a.m_values[i], b.m_values[i]);
    return result;
}

template <typename T>
inline _vec4<T, 1> max(const _vec4<T, 1>& a, const _vec4<T, 1>& b)
{
    _vec4<T, 1> result;
    for (int i = 0; i < 4; i++)
        result.m_values[i] = std::max(a.m_values[i], b.m_values[i]);
    return result;
}

template <typename T>
inline _vec4<T, 1> blend(const _vec4<T, 1>& a, const _vec4<T, 1>& b, const _mask4<1>& mask)
{
    _vec4<T, 1> result;
    for (int i = 0; i < 4; i++)
        result.m_values[i] = mask.m_values[i] ? b.m_values[i] : a.m_values[i];
    return result;
}
