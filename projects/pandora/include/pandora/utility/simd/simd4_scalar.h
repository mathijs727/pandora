#pragma once

template <typename T, int S>
class vec4;

template <int S>
class mask4;

template <typename T>
vec4<T, 1> min(const vec4<T, 1>& a, const vec4<T, 1>& b);

template <typename T>
vec4<T, 1> max(const vec4<T, 1>& a, const vec4<T, 1>& b);

template <>
class mask4<1> {
public:
	mask4() = default;
	mask4(bool v)
	{
		std::fill(std::begin(m_values), std::end(m_values), v);
	}
	mask4(bool v0, bool v1, bool v2, bool v3)
	{
		m_values[0] = v0;
		m_values[1] = v1;
		m_values[2] = v2;
		m_values[3] = v3;
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

	template <typename T, int S>
	friend class vec4_base; // Not possible to friend only vec4_base<T, 1>
};

// Partial template specialization does not inherit members/functions from less precise specializations.
// Use a templated base class to inherit functions/members that are the same between different template specializations (i.e. floats & ints)
template <typename T>
class vec4_base<T, 1> {
public:
	vec4_base() = default;

	vec4_base(gsl::span<const T, 4> v)
	{
		std::copy(std::begin(v), std::end(v), std::begin(m_values));
	}

	vec4_base(T value)
	{
		std::fill(std::begin(m_values), std::end(m_values), value);
	}

	vec4_base(T v0, T v1, T v2, T v3)
	{
		m_values[0] = v0;
		m_values[1] = v1;
		m_values[2] = v2;
		m_values[3] = v3;
	}

	inline T operator[](int i) const {
		return m_values[i];
	}

	inline void load(gsl::span<const T, 4> v)
	{
		std::copy(std::begin(v), std::end(v), std::begin(m_values));
	}

	inline void store(gsl::span<T, 4> v) const
	{
		std::copy(std::begin(m_values), std::end(m_values), std::begin(v));
	}

	inline void loadAligned(gsl::span<const T, 4> v)
	{
		std::copy(std::begin(v), std::end(v), std::begin(m_values));
	}

	inline void storeAligned(gsl::span<T, 4> v) const
	{
		std::copy(std::begin(m_values), std::end(m_values), std::begin(v));
	}

	inline void broadcast(const T& v)
	{
		std::fill(std::begin(m_values), std::end(m_values), v);
	}

	inline vec4<T, 1> operator+(const vec4<T, 1>& other) const
	{
		vec4<T, 1> result;
		for (int i = 0; i < 4; i++)
			result.m_values[i] = m_values[i] + other.m_values[i];
		return result;
	}

	inline vec4<T, 1> operator-(const vec4<T, 1>& other) const
	{
		vec4<T, 1> result;
		for (int i = 0; i < 4; i++)
			result.m_values[i] = m_values[i] - other.m_values[i];
		return result;
	}

	inline vec4<T, 1> operator*(const vec4<T, 1>& other) const
	{
		vec4<T, 1> result;
		for (int i = 0; i < 4; i++)
			result.m_values[i] = m_values[i] * other.m_values[i];
		return result;
	}

	inline mask4<1> operator<(const vec4<T, 1>& other) const
	{
		mask4<1> mask;
		for (int i = 0; i < 4; i++)
			mask.m_values[i] = (m_values[i] < other.m_values[i]);
		return mask;
	}

	inline mask4<1> operator<=(const vec4<T, 1>& other) const
	{
		mask4<1> mask;
		for (int i = 0; i < 4; i++)
			mask.m_values[i] = (m_values[i] <= other.m_values[i]);
		return mask;
	}

	inline mask4<1> operator>(const vec4<T, 1>& other) const
	{
		mask4<1> mask;
		for (int i = 0; i < 4; i++)
			mask.m_values[i] = (m_values[i] > other.m_values[i]);
		return mask;
	}

	inline mask4<1> operator>=(const vec4<T, 1>& other) const
	{
		mask4<1> mask;
		for (int i = 0; i < 4; i++)
			mask.m_values[i] = (m_values[i] >= other.m_values[i]);
		return mask;
	}

	inline vec4<T, 1> compress(mask4<1> mask) const
	{
		vec4<T, 1> result;
		int j = 0;
		for (int i = 0; i < 4; i++) {
			if (mask.m_values[i])
				result.m_values[j++] = m_values[i];
		}
		for (; j < 4; j++)
			result.m_values[j] = 0;
		return result;
	}

	friend vec4<T, 1> min<T>(const vec4<T, 1>& a, const vec4<T, 1>& b);
	friend vec4<T, 1> max<T>(const vec4<T, 1>& a, const vec4<T, 1>& b);

protected:
	std::array<T, 4> m_values;
};

template <>
class vec4<uint32_t, 1> : public vec4_base<uint32_t, 1> {
public:
	// Inherit constructors
	using vec4_base<uint32_t, 1>::vec4_base;
	friend class vec4<float, 1>; // Make friend so it can access us in permute & compress operations

	inline vec4<uint32_t, 1> operator<<(const vec4<uint32_t, 1>& amount) const
	{
		vec4<uint32_t, 1> result;
		for (int i = 0; i < 4; i++) {
			result.m_values[i] = m_values[i] << amount.m_values[i];
		}
		return result;
	}

	inline vec4<uint32_t, 1> operator>>(const vec4<uint32_t, 1>& amount) const
	{
		vec4<uint32_t, 1> result;
		for (int i = 0; i < 4; i++) {
			result.m_values[i] = m_values[i] >> amount.m_values[i];
		}
		return result;
	}

	inline vec4<uint32_t, 1> operator&(const vec4<uint32_t, 1>& other) const
	{
		vec4<uint32_t, 1> result;
		for (int i = 0; i < 4; i++) {
			result.m_values[i] = m_values[i] & other.m_values[i];
		}
		return result;
	}

	inline vec4<uint32_t, 1> permute(const vec4<uint32_t, 1>& index) const
	{
		vec4<uint32_t, 1> result;
		for (int i = 0; i < 4; i++) {
			result.m_values[i] = m_values[index.m_values[i]];
		}
		return result;
	}
};

template <>
class vec4<float, 1> : public vec4_base<float, 1> {
public:
	// Inherit constructors
	using vec4_base<float, 1>::vec4_base;

	// Not in base class because we will not support it on integers
	inline vec4<float, 1> operator/(const vec4<float, 1>& other) const
	{
		vec4<float, 1> result;
		for (int i = 0; i < 4; i++)
			result.m_values[i] = m_values[i] / other.m_values[i];
		return result;
	}

	inline vec4<float, 1> permute(const vec4<uint32_t, 1>& index) const
	{
		vec4<float, 1> result;
		for (int i = 0; i < 4; i++) {
			result.m_values[i] = m_values[index.m_values[i]];
		}
		return result;
	}
};

template <typename T>
inline vec4<T, 1> min(const vec4<T, 1>& a, const vec4<T, 1>& b)
{
	vec4<T, 1> result;
	for (int i = 0; i < 4; i++)
		result.m_values[i] = std::min(a.m_values[i], b.m_values[i]);
	return result;
}

template <typename T>
inline vec4<T, 1> max(const vec4<T, 1>& a, const vec4<T, 1>& b)
{
	vec4<T, 1> result;
	for (int i = 0; i < 4; i++)
		result.m_values[i] = std::max(a.m_values[i], b.m_values[i]);
	return result;
}
