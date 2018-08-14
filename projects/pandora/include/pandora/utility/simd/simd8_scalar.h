template <typename T>
_vec8<T, 1> min(const _vec8<T, 1>& a, const _vec8<T, 1>& b);

template <typename T>
_vec8<T, 1> max(const _vec8<T, 1>& a, const _vec8<T, 1>& b);

template <typename T>
_vec8<T, 1> blend(const _vec8<T, 1>& a, const _vec8<T, 1>& b, const _mask8<1>& mask);

template <>
class _mask8<1> {
public:
    _mask8() = default;
	inline explicit _mask8(bool v)
	{
		std::fill(std::begin(m_values), std::end(m_values), v);
	}
	inline explicit  _mask8(bool v0, bool v1, bool v2, bool v3, bool v4, bool v5, bool v6, bool v7)
    {
        m_values[0] = v0;
        m_values[1] = v1;
        m_values[2] = v2;
        m_values[3] = v3;
        m_values[4] = v4;
        m_values[5] = v5;
        m_values[6] = v6;
        m_values[7] = v7;
    }

	inline _mask8 operator&&(const _mask8& other) const
	{
		_mask8 result;
		std::transform(
			std::begin(m_values),
			std::end(m_values),
			std::begin(other.m_values),
			std::begin(result.m_values),
			std::logical_and<bool>());
		return result;
	}

	inline _mask8 operator||(const _mask8& other) const
	{
		_mask8 result;
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

	inline bool none() const {
		return count() == 0;
	}

	inline bool any() const {
		return count() > 0;
	}

	inline bool all() const {
		return count() == 8;
	}

	inline std::array<uint32_t, 8> computeCompressPermutation() const
	{
		std::array<uint32_t, 8> result;
		int j = 0;
		for (int i = 0; i < 8; i++) {
			if (m_values[i])
				result[j++] = i;
		}
		for (; j < 8; j++)
			result[j] = 0;
		return result;
	}

private:
    std::array<bool, 8> m_values;

	template <typename T>
	friend _vec8<T, 1> blend<T>(const _vec8<T, 1>& a, const _vec8<T, 1>& b, const _mask8<1>& mask);

    template <typename T, int S>
    friend class _vec8_base; // Not possible to friend only _vec8_base<T, 1>
};

// Partial template specialization does not inherit members/functions from less precise specializations.
// Use a templated base class to inherit functions/members that are the same between different template specializations (i.e. floats & ints)
template <typename T>
class _vec8_base<T, 1> {
public:
    _vec8_base() = default;

	inline explicit _vec8_base(gsl::span<const T, 8> v)
    {
        std::copy(std::begin(v), std::end(v), std::begin(m_values));
    }

	inline explicit _vec8_base(T value)
    {
        std::fill(std::begin(m_values), std::end(m_values), value);
    }

	inline explicit _vec8_base(T v0, T v1, T v2, T v3, T v4, T v5, T v6, T v7)
    {
        m_values[0] = v0;
        m_values[1] = v1;
        m_values[2] = v2;
        m_values[3] = v3;
        m_values[4] = v4;
        m_values[5] = v5;
        m_values[6] = v6;
        m_values[7] = v7;
    }

	inline T operator[](int i) const {
		return m_values[i];
	}

	inline void load(gsl::span<const T, 8> v)
	{
		std::copy(std::begin(v), std::end(v), std::begin(m_values));
	}

	inline void store(gsl::span<T, 8> v) const
	{
		std::copy(std::begin(m_values), std::end(m_values), std::begin(v));
	}

    inline void loadAligned(gsl::span<const T, 8> v)
    {
        std::copy(std::begin(v), std::end(v), std::begin(m_values));
    }

    inline void storeAligned(gsl::span<T, 8> v) const
    {
        std::copy(std::begin(m_values), std::end(m_values), std::begin(v));
    }

    inline void broadcast(const T& v)
    {
        std::fill(std::begin(m_values), std::end(m_values), v);
    }

    inline _vec8<T, 1> operator+(const _vec8<T, 1>& other) const
    {
        _vec8<T, 1> result;
        for (int i = 0; i < 8; i++)
            result.m_values[i] = m_values[i] + other.m_values[i];
        return result;
    }

    inline _vec8<T, 1> operator-(const _vec8<T, 1>& other) const
    {
        _vec8<T, 1> result;
        for (int i = 0; i < 8; i++)
            result.m_values[i] = m_values[i] - other.m_values[i];
        return result;
    }

    inline _vec8<T, 1> operator*(const _vec8<T, 1>& other) const
    {
        _vec8<T, 1> result;
        for (int i = 0; i < 8; i++)
            result.m_values[i] = m_values[i] * other.m_values[i];
        return result;
    }

    inline _mask8<1> operator<(const _vec8<T, 1>& other) const
    {
        _mask8<1> mask;
        for (int i = 0; i < 8; i++)
            mask.m_values[i] = (m_values[i] < other.m_values[i]);
        return mask;
    }

	inline _mask8<1> operator<=(const _vec8<T, 1>& other) const
	{
		_mask8<1> mask;
		for (int i = 0; i < 8; i++)
			mask.m_values[i] = (m_values[i] <= other.m_values[i]);
		return mask;
	}

	inline _mask8<1> operator>(const _vec8<T, 1>& other) const
	{
		_mask8<1> mask;
		for (int i = 0; i < 8; i++)
			mask.m_values[i] = (m_values[i] > other.m_values[i]);
		return mask;
	}

	inline _mask8<1> operator>=(const _vec8<T, 1>& other) const
	{
		_mask8<1> mask;
		for (int i = 0; i < 8; i++)
			mask.m_values[i] = (m_values[i] >= other.m_values[i]);
		return mask;
	}

    inline _vec8<T, 1> compress(_mask8<1> mask) const
    {
        _vec8<T, 1> result;
		int j = 0;
        for (int i = 0; i < 8; i++) {
            if (mask.m_values[i])
                result.m_values[j++] = m_values[i];
        }
		for (; j < 8; j++)
			result.m_values[j] = 0;
        return result;
    }

	inline uint64_t horizontalMinPos() const
	{
		std::array<T, 8> values;
		store(values);
		return std::distance(std::begin(values), std::min_element(std::begin(values), std::end(values)));
	}

	inline T horizontalMin() const
	{
		std::array<T, 8> values;
		store(values);
		return *std::min_element(std::begin(values), std::end(values));
	}

	inline uint64_t horizontalMaxPos() const
	{
		std::array<T, 8> values;
		store(values);
		return std::distance(std::begin(values), std::max_element(std::begin(values), std::end(values)));
	}

	inline T horizontalMax() const
	{
		std::array<T, 8> values;
		store(values);
		return *std::max_element(std::begin(values), std::end(values));
	}

    friend _vec8<T, 1> min<T>(const _vec8<T, 1>& a, const _vec8<T, 1>& b);
    friend _vec8<T, 1> max<T>(const _vec8<T, 1>& a, const _vec8<T, 1>& b);

	friend _vec8<T, 1> blend<T>(const _vec8<T, 1>& a, const _vec8<T, 1>& b, const _mask8<1>& mask);

protected:
    std::array<T, 8> m_values;
};

template <>
class _vec8<uint32_t, 1> : public _vec8_base<uint32_t, 1> {
public:
    // Inherit constructors
    using _vec8_base<uint32_t, 1>::_vec8_base;
    friend class _vec8<float, 1>; // Make friend so it can access us in permute & compress operations

    inline _vec8<uint32_t, 1> operator<<(const _vec8<uint32_t, 1>& amount) const
    {
        _vec8<uint32_t, 1> result;
        for (int i = 0; i < 8; i++) {
            result.m_values[i] = m_values[i] << amount.m_values[i];
        }
        return result;
    }

	inline _vec8<uint32_t, 1> operator<<(uint32_t amount) const
	{
		_vec8<uint32_t, 1> result;
		for (int i = 0; i < 8; i++) {
			result.m_values[i] = m_values[i] << amount;
		}
		return result;
	}

	inline _vec8<uint32_t, 1> operator>>(const _vec8<uint32_t, 1>& amount) const
	{
		_vec8<uint32_t, 1> result;
		for (int i = 0; i < 8; i++) {
			result.m_values[i] = m_values[i] >> amount.m_values[i];
		}
		return result;
	}

	inline _vec8<uint32_t, 1> operator>>(uint32_t amount) const
	{
		_vec8<uint32_t, 1> result;
		for (int i = 0; i < 8; i++) {
			result.m_values[i] = m_values[i] >> amount;
		}
		return result;
	}

    inline _vec8<uint32_t, 1> operator&(const _vec8<uint32_t, 1>& other) const
    {
        _vec8<uint32_t, 1> result;
        for (int i = 0; i < 8; i++) {
            result.m_values[i] = m_values[i] & other.m_values[i];
        }
        return result;
    }

    inline _vec8<uint32_t, 1> permute(const _vec8<uint32_t, 1>& index) const
    {
        _vec8<uint32_t, 1> result;
        for (int i = 0; i < 8; i++) {
            result.m_values[i] = m_values[index.m_values[i]];
        }
        return result;
    }
};

template <>
class _vec8<float, 1> : public _vec8_base<float, 1> {
public:
    // Inherit constructors
    using _vec8_base<float, 1>::_vec8_base;

    // Not in base class because we will not support it on integers
    inline _vec8<float, 1> operator/(const _vec8<float, 1>& other) const
    {
        _vec8<float, 1> result;
        for (int i = 0; i < 8; i++)
            result.m_values[i] = m_values[i] / other.m_values[i];
        return result;
    }

    inline _vec8<float, 1> permute(const _vec8<uint32_t, 1>& index) const
    {
        _vec8<float, 1> result;
        for (int i = 0; i < 8; i++) {
            result.m_values[i] = m_values[index.m_values[i]];
        }
        return result;
    }
};

template <typename T>
inline _vec8<T, 1> min(const _vec8<T, 1>& a, const _vec8<T, 1>& b)
{
	_vec8<T, 1> result;
	for (int i = 0; i < 8; i++)
		result.m_values[i] = std::min(a.m_values[i], b.m_values[i]);
	return result;
}

template <typename T>
inline _vec8<T, 1> max(const _vec8<T, 1>& a, const _vec8<T, 1>& b)
{
	_vec8<T, 1> result;
	for (int i = 0; i < 8; i++)
		result.m_values[i] = std::max(a.m_values[i], b.m_values[i]);
	return result;
}

template <typename T>
inline _vec8<T, 1> blend(const _vec8<T, 1>& a, const _vec8<T, 1>& b, const _mask8<1>& mask)
{
	_vec8<T, 1> result;
	for (int i = 0; i < 8; i++)
		result.m_values[i] = mask.m_values[i] ? b.m_values[i] : a.m_values[i];
	return result;
}

