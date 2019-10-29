#include "pbrt/util/crack_atof.h"

// Copied from:
// https://github.com/shaovoon/floatbench/blob/master/BenchmarkFloatConv/BenchmarkFloatConv/BenchmarkFloatConv.cpp
// https://github.com/shaovoon/floatbench

// Original crack_atof version is at http://crackprogramming.blogspot.sg/2012/10/implement-atof.html
// But it cannot convert floating point with high +/- exponent.
// The version below by Tian Bo fixes that problem and improves performance by 10%
// http://coliru.stacked-crooked.com/a/2e28f0d71f47ca5e
double pow10(int n)
{
    double ret = 1.0;
    double r = 10.0;
    if (n < 0) {
        n = -n;
        r = 0.1;
    }

    while (n) {
        if (n & 1) {
            ret *= r;
        }
        r *= r;
        n >>= 1;
    }
    return ret;
}

double crackAtof(std::string_view str)
{
    const char* num = str.data();
    if (!num || !*num) {
        return 0;
    }

    int sign = 1;
    double integerPart = 0.0;
    double fractionPart = 0.0;
    bool hasFraction = false;
    bool hasExpo = false;

    // Take care of +/- sign
    if (*num == '-') {
        ++num;
        sign = -1;
    } else if (*num == '+') {
        ++num;
    }

    while (*num != '\0') {
        if (*num >= '0' && *num <= '9') {
            integerPart = integerPart * 10 + (*num - '0');
        } else if (*num == '.') {
            hasFraction = true;
            ++num;
            break;
        } else if (*num == 'e') {
            hasExpo = true;
            ++num;
            break;
        } else {
            return sign * integerPart;
        }
        ++num;
    }

    if (hasFraction) {
        double fractionExpo = 0.1;

        while (*num != '\0') {
            if (*num >= '0' && *num <= '9') {
                fractionPart += fractionExpo * (*num - '0');
                fractionExpo *= 0.1;
            } else if (*num == 'e') {
                hasExpo = true;
                ++num;
                break;
            } else {
                return sign * (integerPart + fractionPart);
            }
            ++num;
        }
    }

    // parsing exponet part
    double expPart = 1.0;
    if (*num != '\0' && hasExpo) {
        int expSign = 1;
        if (*num == '-') {
            expSign = -1;
            ++num;
        } else if (*num == '+') {
            ++num;
        }

        int e = 0;
        while (*num != '\0' && *num >= '0' && *num <= '9') {
            e = e * 10 + *num - '0';
            ++num;
        }

        expPart = pow10(expSign * e);
    }

    return sign * (integerPart + fractionPart) * expPart;
}
