#ifndef CONSTANTS_HPP
#define CONSTANTS_HPP

namespace constants {
	template <typename T>
	inline constexpr T sqrt2_v = T(1.414213562373095048801688724209698079l);
	inline constexpr float sqrt2 = sqrt2_v<float>;

	template <typename T>
	inline constexpr T pi_v = T(3.141592653589793238462643383279502884l);
	inline constexpr float pi = pi_v<float>;
}

#endif
