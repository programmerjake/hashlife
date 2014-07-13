#ifndef BIGFLOAT_H_INCLUDED
#define BIGFLOAT_H_INCLUDED

#include <cstdint>
#include <cmath>
#include <istream>
#include <ostream>
#include <cassert>

using namespace std;

#define USE_DOUBLE_FOR_BIGFLOAT

#ifndef USE_DOUBLE_FOR_BIGFLOAT
class BigFloat
{
private:
    typedef int32_t Word;
    typedef uint32_t UnsignedWord;
    typedef int64_t DoubleWord;
    typedef uint64_t UnsignedDoubleWord;
    Word mantissa;
    int exponent;
    static constexpr int WordSize = 32;
    void normalize()
    {
        if(mantissa != 0 && (mantissa & 1) == 0)
        {
            do
            {
                mantissa >>= 1;
                exponent++;
            }
            while((mantissa & 1) == 0);
        }
        else if(mantissa == 0)
            exponent = 0;
    }
    void unnormalize()
    {
        if(mantissa == 0)
            return;
        while(mantissa << 1 == (DoubleWord)mantissa << 1)
        {
            mantissa <<= 1;
            exponent--;
        }
    }
    static bool isTooBigForWord(DoubleWord v)
    {
        return (Word)v != v;
    }
public:
    BigFloat(Word mantissa = 0, int exponent = 0)
        : mantissa(mantissa), exponent(exponent)
    {
        normalize();
    }
    BigFloat(double v)
    {
        assert(!std::isnan(v) && !std::isinf(v));
        int e;
        v = frexp(v, &e);
        v *= std::pow(2.0, WordSize);
        e -= WordSize;
        normalize();
    }
    explicit operator double() const
    {
        return ldexp((double)mantissa, exponent);
    }
    explicit operator int() const
    {
        if(exponent <= -WordSize)
            return mantissa < 0 ? -1 : 0;
        if(exponent >= WordSize)
            return 0;
        if(exponent < 0)
            return mantissa >> -exponent;
        return mantissa << exponent;
    }
    friend BigFloat floor(BigFloat x)
    {
        if(x.exponent >= 0)
            return x;
        if(x.exponent <= -WordSize)
            return x.mantissa < 0 ? -1 : 0;
        return BigFloat(x.mantissa >> -x.exponent);
    }
    friend BigFloat ceil(BigFloat x)
    {
        if(x.exponent >= 0)
            return x;
        if(x.exponent <= -WordSize)
            return x.mantissa > 0 ? 1 : 0;
        return BigFloat(-(-x.mantissa >> -x.exponent));
    }
    BigFloat operator -() const
    {
        return BigFloat(-mantissa, exponent);
    }
    const BigFloat & operator +() const
    {
        return *this;
    }
    friend BigFloat operator +(BigFloat a, BigFloat b)
    {
        if(a.mantissa == 0)
            return b;
        if(b.mantissa == 0)
            return a;
        a.unnormalize();
        b.unnormalize();
        int maxExponent = max(a.exponent, b.exponent);
        DoubleWord result = 0;
        if(maxExponent - a.exponent >= WordSize)
        {
            b.normalize();
            return b;
        }
        if(maxExponent - b.exponent >= WordSize)
        {
            a.normalize();
            return a;
        }
        result += a.mantissa >> (maxExponent - a.exponent);
        result += b.mantissa >> (maxExponent - b.exponent);
        if(isTooBigForWord(result))
        {
            maxExponent++;
            result >>= 1;
        }
        return BigFloat((Word)result, maxExponent);
    }
    const BigFloat & operator +=(BigFloat r)
    {
        return *this = *this + r;
    }
    friend BigFloat operator -(BigFloat a, BigFloat b)
    {
        return a + -b;
    }
    const BigFloat & operator -=(BigFloat r)
    {
        return *this = *this - r;
    }
    friend BigFloat operator *(BigFloat a, BigFloat b)
    {
        int resultExponent = a.exponent + b.exponent;
        DoubleWord result = a.mantissa;
        result *= b.mantissa;
        while(isTooBigForWord(result))
        {
            resultExponent++;
            result >>= 1;
        }
        return BigFloat((Word)result, resultExponent);
    }
    const BigFloat & operator *=(BigFloat r)
    {
        return *this = *this * r;
    }
    friend BigFloat operator /(BigFloat a, BigFloat b)
    {
        DoubleWord result = a.mantissa;
        int resultExponent = a.exponent - b.exponent;
        result <<= WordSize;
        resultExponent -= WordSize;
        result /= a.mantissa;
        while(isTooBigForWord(result))
        {
            resultExponent++;
            result >>= 1;
        }
        return BigFloat((Word)result, resultExponent);
    }
    const BigFloat & operator /=(BigFloat r)
    {
        return *this = *this / r;
    }
    friend bool operator ==(BigFloat a, BigFloat b)
    {
        return a.mantissa == b.mantissa && a.exponent == b.exponent;
    }
    friend bool operator !=(BigFloat a, BigFloat b)
    {
        return a.mantissa != b.mantissa || a.exponent != b.exponent;
    }
    friend int sgn(BigFloat v)
    {
        if(v.mantissa < 0)
            return -1;
        if(v.mantissa > 0)
            return 1;
        return 0;
    }
    friend BigFloat abs(BigFloat v)
    {
        return BigFloat(abs(v.mantissa), v.exponent);
    }
    friend bool operator <(BigFloat a, BigFloat b)
    {
        return sgn(a - b) < 0;
    }
    friend bool operator <=(BigFloat a, BigFloat b)
    {
        return sgn(a - b) <= 0;
    }
    friend bool operator >(BigFloat a, BigFloat b)
    {
        return sgn(a - b) > 0;
    }
    friend bool operator >=(BigFloat a, BigFloat b)
    {
        return sgn(a - b) >= 0;
    }
    friend BigFloat ldexp(BigFloat v, int exponent)
    {
        return BigFloat(v.mantissa, v.exponent + exponent);
    }
    friend BigFloat operator <<(BigFloat v, int exponent)
    {
        return ldexp(v, exponent);
    }
    friend BigFloat operator >>(BigFloat v, int exponent)
    {
        return ldexp(v, -exponent);
    }
    const BigFloat & operator <<=(int e)
    {
        if(mantissa != 0)
            exponent += e;
        return *this;
    }
    const BigFloat & operator >>=(int e)
    {
        if(mantissa != 0)
            exponent -= e;
        return *this;
    }
    friend BigFloat pow(BigFloat base, unsigned exponent)
    {
        BigFloat retval = 1;
        if((exponent & 1) != 0)
            retval = base;
        exponent &= ~(unsigned)1;
        for(unsigned mask = 1 << 1; exponent != 0; exponent &= ~mask, mask <<= 1)
        {
            base *= base;
            if((exponent & mask) != 0)
            {
                retval *= base;
            }
        }
        return retval;
    }
    friend BigFloat pow(BigFloat base, int exponent)
    {
        BigFloat retval = pow(base, (unsigned)abs(exponent));
        if(exponent < 0)
            return BigFloat(1) / retval;
        return retval;
    }
    friend double log2(BigFloat v)
    {
        assert(v.mantissa > 0);
        return std::log((double)v.mantissa) * (1.0 / std::log(2.0)) + v.exponent;
    }
    friend double log(BigFloat v)
    {
        assert(v.mantissa > 0);
        return std::log((double)v.mantissa) + v.exponent * std::log(2.0);
    }
    friend double log10(BigFloat v)
    {
        assert(v.mantissa > 0);
        return std::log10((double)v.mantissa) + v.exponent * std::log10(2.0);
    }
    static BigFloat exp2(double v)
    {
        double intPart = std::floor(v);
        double fraction = v - intPart;
        return ldexp(BigFloat(std::pow(2.0, fraction)), (int)intPart);
    }
    static BigFloat exp(double v)
    {
        return exp2(v * (1.0 / std::log(2.0)));
    }
    static BigFloat exp10(double v)
    {
        return exp2(v * (1.0 / std::log10(2.0)));
    }
    friend BigFloat pow(BigFloat base, double exponent)
    {
        if(base.mantissa == 0)
            return 0;
        return BigFloat::exp2(exponent * log2(base));
    }
    friend ostream & operator <<(ostream & os, BigFloat v)
    {
        return os << (double)v;
    }
    friend istream & operator >>(istream & is, BigFloat & v)
    {
        double value;
        is >> value;
        if(is)
            v = BigFloat(value);
        return is;
    }
    const char * c_str() const; // for debuggins
};
#else
typedef double BigFloat;
#endif

inline BigFloat operator "" _bf(long double v)
{
    return BigFloat((double)v);
}

inline BigFloat operator "" _bf(unsigned long long v)
{
    return BigFloat((int)v);
}

#endif // BIGFLOAT_H_INCLUDED
