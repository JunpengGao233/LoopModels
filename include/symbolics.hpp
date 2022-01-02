#include "math.hpp"
#include <algorithm>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <tuple>
#include <utility>
#include <vector>

enum DivRemainder { Indeterminate, NoRemainder, HasRemainder };

std::pair<intptr_t, intptr_t> divgcd(intptr_t x, intptr_t y) {
    intptr_t g = std::gcd(x, y);
    return std::make_pair(x / g, y / g);
}

struct Rational {
    intptr_t numerator;
    intptr_t denominator;

    Rational() = default;
    Rational(intptr_t coef) : numerator(coef), denominator(1){};
    Rational(intptr_t n, intptr_t d) : numerator(n), denominator(d){};

    Rational operator+(Rational y) {
        auto [xd, yd] = divgcd(denominator, y.denominator);
        return Rational{numerator * yd + y.numerator * xd, denominator * yd};
    }
    Rational &operator+=(Rational y) {
        auto [xd, yd] = divgcd(denominator, y.denominator);
        numerator = numerator * yd + y.numerator * xd;
        denominator = denominator * yd;
        return *this;
    }
    Rational operator-(Rational y) {
        auto [xd, yd] = divgcd(denominator, y.denominator);
        return Rational{numerator * yd - y.numerator * xd, denominator * yd};
    }
    Rational &operator-=(Rational y) {
        auto [xd, yd] = divgcd(denominator, y.denominator);
        numerator = numerator * yd - y.numerator * xd;
        denominator = denominator * yd;
        return *this;
    }
    Rational operator*(Rational y) {
        auto [xn, yd] = divgcd(numerator, y.denominator);
        auto [xd, yn] = divgcd(denominator, y.numerator);
        return Rational{xn * yn, xd * yd};
    }
    Rational &operator*=(Rational y) {
        auto [xn, yd] = divgcd(numerator, y.denominator);
        auto [xd, yn] = divgcd(denominator, y.numerator);
        numerator = xn * yn;
        denominator = xd * yd;
        return *this;
    }
    Rational inv() {
        bool positive = numerator > 0;
        return Rational{positive ? denominator : -denominator,
                        positive ? numerator : -numerator};
    }
    Rational operator/(Rational y) { return (*this) * y.inv(); }
    Rational operator/=(Rational y) { return (*this) *= y.inv(); }
};

// The basic symbol type represents a symbol as a product of some number of
// known IDs as well as with a constant term.
// `5` would thus be an empty `prodIDs` vector and `coef = 5`.
// While if `M, N, K = size(A)`, the strides would be `prodIDs = [], coef = 1`,
// `prodIDs = [M], coef = 1`, and `prodIDs = [M,N], coef = 1`, respectively.
// This lets us tell that each is a multiple of the others.
// Can we assume `prodIDs` are greater than 0?
struct Polynomial {
    struct Monomial {
        std::vector<size_t> prodIDs; // sorted symbolic terms being multiplied
        Rational coef;               // constant coef

        // constructors
        Monomial() : prodIDs(std::vector<size_t>()), coef(0){};
        Monomial(intptr_t coef) : prodIDs(std::vector<size_t>()), coef(coef){};

        inline auto begin() { return prodIDs.begin(); }
        inline auto end() { return prodIDs.end(); }
        Monomial operator*(Monomial &x) {
            Monomial r;
            r.coef = coef * x.coef;
            // prodIDs are sorted, so we can create sorted product in O(N)
            size_t i = 0;
            size_t j = 0;
            size_t n0 = prodIDs.size();
            size_t n1 = x.prodIDs.size();
            r.prodIDs.reserve(n0 + n1);
            for (size_t k = 0; k < (n0 + n1); ++k) {
                size_t a =
                    (i < n0) ? prodIDs[i] : std::numeric_limits<size_t>::max();
                size_t b = (j < n1) ? x.prodIDs[j]
                                    : std::numeric_limits<size_t>::max();
                bool aSmaller = a < b;
                aSmaller ? ++i : ++j;
                r.prodIDs.push_back(aSmaller ? a : b);
            }
            return r;
        }
        Monomial &operator*=(Monomial &x) {
            coef *= x.coef;
            // optimize the length 0 and 1 cases.
            if (x.prodIDs.size() == 0) {
                return *this;
            } else if (x.prodIDs.size() == 1) {
                size_t y = x.prodIDs[0];
                for (auto it = begin(); it != end(); ++it) {
                    if (y < *it) {
                        prodIDs.insert(it, y);
                        return *this;
                    }
                }
                prodIDs.push_back(y);
                return *this;
            }
            size_t n0 = prodIDs.size();
            size_t n1 = x.prodIDs.size();
            prodIDs.reserve(n0 +
                            n1); // reserve capacity, to prevent invalidation
            auto ix = x.begin();
            auto ixe = x.end();
            if (n0) {
                auto it = begin();
                while (ix != ixe) {
                    if (*ix < *it) {
                        prodIDs.insert(it, *ix); // increments `end()`
                        ++it;
                        ++ix;
                    } else {
                        ++it;
                        if (it == end()) {
                            break;
                        }
                    }
                }
            }
            for (; ix != ixe; ++ix) {
                prodIDs.push_back(*ix);
            }
            return *this;
        }
        Monomial operator*(Monomial &&x) {
            x *= (*this);
            return x;
        }
        bool operator==(Monomial x) {
            if (coef != x.coef) {
                return false;
            }
            return prodIDs == x.prodIDs;
        }
        bool operator!=(Monomial x) {
            if (coef == x.coef) {
                return false;
            }
            return prodIDs != x.prodIDs;
        }

        struct Affine;
        template <typename S> Affine operator+(S &&x);
        template <typename S> Affine operator-(S &&x);
        // numerator, denominator rational
        std::pair<Monomial, Monomial> rational(Monomial &x) {
            intptr_t g = std::gcd(coef, x.coef);
            Monomial n(coef / g);
            Monomial d(x.coef / g);
            if (d.coef < 0) {
                // guarantee the denominator coef is positive
                // assumption used in affine divRem, to avoid
                // checking for -1.
                n.coef *= -1;
                d.coef *= -1;
            }
            size_t i = 0;
            size_t j = 0;
            size_t n0 = prodIDs.size();
            size_t n1 = x.prodIDs.size();
            while ((i + j) < (n0 + n1)) {
                size_t a =
                    (i < n0) ? prodIDs[i] : std::numeric_limits<size_t>::max();
                size_t b = (j < n1) ? x.prodIDs[j]
                                    : std::numeric_limits<size_t>::max();
                if (a < b) {
                    n.prodIDs.push_back(a);
                    ++i;
                } else if (a == b) {
                    ++i;
                    ++j;
                } else {
                    d.prodIDs.push_back(b);
                    ++j;
                }
            }
            return std::make_pair(n, d);
        }
        // returns a 3-tuple, containing:
        // 0: Monomial(coef / gcd(coef, x.coef), setDiff(prodIDs, x.prodIDs))
        // 1: x.coef / gcd(coef, x.coef)
        // 2: whether the division failed (i.e., true if prodIDs was not a
        // superset of x.prodIDs)
        std::tuple<Monomial, intptr_t, bool> operator/(Monomial &x) {
            intptr_t g = std::gcd(coef, x.coef);
            Monomial n(coef / g);
            intptr_t d = coef / x.coef;
            if (d < 0) {
                d *= -1;
                n.coef *= -1;
            }
            size_t i = 0;
            size_t j = 0;
            size_t n0 = prodIDs.size();
            size_t n1 = x.prodIDs.size();
            while ((i + j) < (n0 + n1)) {
                size_t a =
                    (i < n0) ? prodIDs[i] : std::numeric_limits<size_t>::max();
                size_t b = (j < n1) ? x.prodIDs[j]
                                    : std::numeric_limits<size_t>::max();
                if (a < b) {
                    n.prodIDs.push_back(a);
                    ++i;
                } else if (a == b) {
                    ++i;
                    ++j;
                } else {
                    return std::make_tuple(n, d, true);
                }
            }
            return std::make_tuple(n, d, false);
        }
        bool isZero() { return coef == 0; }
        bool isOne() { return (coef == 0) & (prodIDs.size() == 0); }
        Monomial &negate() {
            coef *= -1;
            return *this;
        }
        bool isCompileTimeConstant() { return prodIDs.size() == 0; }
    };

    std::vector<Monomial> terms;
    Polynomial() = default;
    Polynomial(Monomial &x) : terms({x}){};
    Polynomial(Monomial &&x) : terms({std::move(x)}){};
    // Polynomial(Monomial &t0, Monomial &t1) : terms({t0, t1}) {};
    // Polynomial(Monomial &&t0, Monomial &&t1) : terms({std::move(t0),
    // std::move(t1)})
    // {};
    Polynomial(std::vector<Monomial> &t) : terms(t){};
    Polynomial(std::vector<Monomial> &&t) : terms(std::move(t)){};

    inline auto begin() { return terms.begin(); }
    inline auto end() { return terms.end(); }
    // Polynomial(std::tuple<Monomial, Monomial> &&x) : terms({std::get<0>(x),
    // std::get<1>(x)}) {}

    template <typename S> void add_term(S &&x) {
        if (x.coef) {
            for (auto it = terms.begin(); it != terms.end(); ++it) {
                if ((it->prodIDs) == x.prodIDs) {
                    (it->coef) += x.coef;
                    if ((it->coef) == 0) {
                        terms.erase(it);
                    }
                    return;
                } else if (lexicographicalLess(x, *it)) {
                    terms.insert(it, std::forward<S>(x));
                    return;
                }
            }
            terms.push_back(std::forward<S>(x));
        }
        return;
    }
    template <typename S> void sub_term(S &&x) {
        if (x.coef) {
            for (auto it = terms.begin(); it != terms.end(); ++it) {
                if ((it->prodIDs) == x.prodIDs) {
                    (it->coef) -= x.coef;
                    if ((it->coef) == 0) {
                        terms.erase(it);
                    }
                    return;
                } else if (lexicographicalLess(x, *it)) {
                    Monomial y(x);
                    y.coef *= -1;
                    terms.insert(it, std::move(y));
                    return;
                }
            }
            Monomial y(x);
            y.coef *= -1;
            terms.push_back(std::move(y));
        }
        return;
    }
    Polynomial &operator+=(Monomial &x) {
        add_term(x);
        return *this;
    }
    Polynomial &operator+=(Monomial &&x) {
        add_term(std::move(x));
        return *this;
    }
    Polynomial &operator-=(Monomial &x) {
        sub_term(x);
        return *this;
    }
    Polynomial &operator-=(Monomial &&x) {
        sub_term(std::move(x));
        return *this;
    }

    bool operator==(Polynomial &x) { return (terms == x.terms); }
    bool operator!=(Polynomial &x) { return (terms != x.terms); }
    Polynomial operator*(Monomial &x) {
        Polynomial p(terms);
        for (auto it = p.begin(); it != p.end(); ++it) {
            (*it) *= x;
        }
        return p;
    }
    Polynomial operator*(Polynomial &x) {
        Polynomial p;
        p.terms.reserve(terms.size() * x.terms.size());
        for (auto it = begin(); it != end(); ++it) {
            for (auto itx = x.begin(); itx != x.end(); ++itx) {
                p.add_term((*it) * (*itx));
            }
        }
        return p;
    }
    Polynomial operator+(Monomial &x) {
        Polynomial y(terms);
        y.add_term(x);
        return y;
    }
    Polynomial operator-(Monomial x) {
        Polynomial y(terms);
        y.sub_term(x);
        return y;
    }
    Polynomial largerCapacityCopy(size_t i) {
        Polynomial s;
        s.terms.reserve(i + terms.size()); // reserve full size
        for (auto it = begin(); it != end(); ++it) {
            s.terms.push_back(*it); // copy initial batch
        }
        return s;
    }
    Polynomial operator+(Polynomial x) {
        Polynomial s = largerCapacityCopy(x.terms.size());
        for (auto it = x.begin(); it != x.end(); ++it) {
            s.add_term(*it); // add term for remainder
        }
        return s;
    }
    Polynomial operator-(Polynomial x) {
        Polynomial s = largerCapacityCopy(x.terms.size());
        for (auto it = x.begin(); it != x.end(); ++it) {
            s.sub_term(*it);
        }
        return s;
    }
    Polynomial &operator+=(Polynomial x) {
        terms.reserve(terms.size() + x.terms.size());
        for (auto it = x.begin(); it != x.end(); ++it) {
            add_term(*it); // add term for remainder
        }
        return *this;
    }
    Polynomial &operator-=(Polynomial x) {
        terms.reserve(terms.size() + x.terms.size());
        for (auto it = x.begin(); it != x.end(); ++it) {
            sub_term(*it); // add term for remainder
        }
        return *this;
    }
    Polynomial &operator*=(Monomial x) {
        if (x.coef == 0) {
            terms.clear();
            return *this;
        } else if (x.isOne()) {
            return *this;
        }
        for (auto it = begin(); it != end(); ++it) {
            (*it) *= x; // add term for remainder
        }
        return *this;
    }
    // bool operator>=(Polynomial x){

    // 	return false;
    // }
    bool isZero() { return (terms.size() == 0); }
    bool isOne() { return (terms.size() == 1) & terms[0].isOne(); }

    bool isCompileTimeConstant() {
        return (terms.size() == 1) && (terms.begin()->isCompileTimeConstant());
    }

    Polynomial &negate() {
        for (auto it = begin(); it != end(); ++it) {
            (*it).negate();
        }
        return *this;
    }
    void divRem(Polynomial &d, Polynomial &r, Monomial &x) {
        for (auto it = begin(); it != end(); ++it) {
            auto [nx, dx, fail] = *it / x;
            if (fail) {
                r.add_term(*it);
            } else {
                if (dx == 1) {
                    // perfectly divided
                    d.add_term(nx);
                } else {
                    intptr_t div = nx.coef / dx;
                    intptr_t rem = nx.coef - dx * div;
                    if (div) {
                        nx.coef = div;
                        d.add_term(nx);
                    }
                    if (rem) {
                        nx.coef = rem;
                        r.add_term(nx);
                    }
                }
            }
        }
    }
    // returns a <div, rem> pair
    std::pair<Polynomial, Polynomial> divRem(Monomial &x) {
        Polynomial d;
        Polynomial r;
        divRem(d, r, x);
        return std::make_pair(d, r);
    }
    // returns a <div, rem> pair
    std::pair<Polynomial, Polynomial> divRem(Polynomial &x) {
        // TODO: algorithm needs to be fixed
        Polynomial d;
        Polynomial r;
        Polynomial y(terms); // copy
        auto it = x.begin();
        auto ite = x.end();
        if (it != ite) {
            while (true) {
                y.divRem(d, r, *it);
                ++it;
                if (it == ite) {
                    break;
                }
                std::swap(y, r);
                r.terms.clear();
            }
        }
        return std::make_pair(std::move(d), std::move(r));
    }
    Polynomial operator/(Polynomial &x) { return divRem(x).first; }
    Polynomial operator%(Polynomial &x) { return divRem(x).second; }
};

bool lexicographicalLess(Polynomial::Monomial &x, Polynomial::Monomial &y) {
    return std::lexicographical_compare(x.begin(), x.end(), y.begin(), y.end());
}
std::strong_ordering lexicographicalCmp(Polynomial::Monomial &x,
                                        Polynomial::Monomial &y) {
    return std::lexicographical_compare_three_way(x.begin(), x.end(), y.begin(),
                                                  y.end());
}

std::tuple<Polynomial::Monomial, Polynomial::Monomial, Polynomial::Monomial>
gcd(Polynomial::Monomial &x, Polynomial::Monomial &y) {
    Polynomial::Monomial g, a, b;
    intptr_t c = std::gcd(x.coef, y.coef);
    g.coef = c;
    a.coef = x.coef / c;
    b.coef = y.coef / c;
    size_t i = 0;
    size_t j = 0;
    size_t n0 = x.prodIDs.size();
    size_t n1 = y.prodIDs.size();
    for (size_t k = 0; k < (n0 + n1); ++k) {
        size_t xk =
            (i < n0) ? x.prodIDs[i] : std::numeric_limits<size_t>::max();
        size_t yk =
            (j < n1) ? y.prodIDs[j] : std::numeric_limits<size_t>::max();
        if (xk < yk) {
            a.prodIDs.push_back(xk);
            ++i;
        } else if (xk > yk) {
            b.prodIDs.push_back(yk);
            ++j;
        } else { // xk == yk
            g.prodIDs.push_back(xk);
            ++i;
            ++j;
            ++k;
        }
    }
    return std::make_tuple(g, a, b);
}

std::pair<Polynomial::Monomial, std::vector<Polynomial::Monomial>>
gcd(std::vector<Polynomial::Monomial> &x) {
    switch (x.size()) {
    case 0:
        return std::make_pair(Polynomial::Monomial(0), x);
    case 1:
        return std::make_pair(
            x[0], std::vector<Polynomial::Monomial>{Polynomial::Monomial(1)});
    default:
        auto [g, a, b] = gcd(x[0], x[1]);
        std::vector<Polynomial::Monomial> f;
        f.reserve(x.size());
        f.push_back(std::move(a));
        f.push_back(std::move(b));
        for (size_t i = 2; i < x.size(); ++i) {
            auto [gt, a, b] = gcd(g, x[i]);
            std::swap(g, gt);
            if (!a.isOne()) {
                for (auto it = f.begin(); it != f.end(); ++it) {
                    (*it) *= a;
                }
            }
            f.push_back(std::move(b));
        }
        return std::make_pair(g, x);
    }
}

std::pair<Symbol, Polynomial> gcd(Polynomial &x) {
    std::pair<Symbol, std::vector<Symbol>> st = gcd(x.terms);
    return std::make_pair(std::move(st.first),
                          Polynomial(std::move(st.second)));
}
std::tuple<Polynomial, Polynomial, Polynomial> gcd(Polynomial &a,
                                                   Polynomial &b) {
    Polynomial x(a);
    Polynomial y(b);
    while (!y.isZero()) { // TODO: add tests and/or proof to make sure this
                          // terminates with symbolics
        x = x % y;
        std::swap(x, y);
    }
    return std::make_tuple(x, a / x, b / x);
}
std::tuple<Polynomial, Polynomial, Polynomial> extended_gcd(Polynomial &a,
                                                            Polynomial &b) {
    Polynomial x(a);
    Polynomial y(b);
    Polynomial oldS(1);
    Polynomial s(0);
    Polynomial oldT(0);
    Polynomial t(1);
    while (!y.isZero()) {
        auto [q, r] = x.divRem(y);
        oldS -= q * s;
        oldT -= q * t;
        x = y;
        y = r;
        std::swap(oldS, s);
        std::swap(oldT, t);
    }
    return std::make_tuple(x, a / x, b / x);
}

template <typename T> T negate(T &&x) { return std::forward<T>(x.negate()); }

template <typename S> Polynomial Symbol::operator+(S &&y) {
    if (prodIDs == y.prodIDs) {
        if (coef == -y.coef) {
            return Polynomial(Symbol(0));
        } else {
            Symbol s(std::forward<S>(y));
            s.coef += coef;
            return Polynomial(std::move(s));
        }
    }
    Polynomial s;
    s.terms.reserve(2);
    if (lexicographicalLess(*this, y)) {
        s.terms.push_back(*this);
    } else {
        s.terms.push_back(std::forward<S>(y));
    }
    return s;
}
template <typename S> Polynomial Symbol::operator-(S &&y) {
    if (prodIDs == y.prodIDs) {
        if (coef == -y.coef) {
            return Polynomial(Symbol(0));
        } else {
            Symbol s(std::forward<S>(y));
            s.coef = coef - s.coef;
            return Polynomial(std::move(s));
        }
    }
    Polynomial s;
    s.terms.reserve(2);
    if (lexicographicalLess(*this, y)) {
        s.terms.push_back(*this);
    } else {
        s.terms.push_back(std::forward<S>(y));
    }
    return s;
}

static std::string programVarName(size_t i) { return "M_" + std::to_string(i); }
std::string toString(Symbol x) {
    std::string poly = "";
    size_t numIndex = x.prodIDs.size();
    Int coef = x.coef;
    if (numIndex) {
        if (numIndex != 1) { // not 0 by prev `if`
            if (coef != 1) {
                poly += std::to_string(coef) + " (";
            }
            for (size_t k = 0; k < numIndex; ++k) {
                poly += programVarName(x.prodIDs[k]);
                if (k + 1 != numIndex)
                    poly += " ";
            }
            if (coef != 1) {
                poly += ")";
            }
        } else { // numIndex == 1
            if (coef != 1) {
                poly += std::to_string(coef) + " ";
            }
            poly += programVarName(x.prodIDs[0]);
        }
    } else {
        poly += std::to_string(coef);
    }
    return poly;
}

std::string toString(Polynomial x) {
    std::string poly = " ( ";
    for (size_t j = 0; j < length(x.terms); ++j) {
        if (j) {
            poly += " + ";
        }
        poly += toString(x.terms[j]);
    }
    return poly + " ) ";
}
void show(Symbol x) { printf("%s", toString(x).c_str()); }
void show(Polynomial x) { printf("%s", toString(x).c_str()); }
Polynomial loopToAffineUpperBound(Vector<Int, MAX_PROGRAM_VARIABLES> loopvars) {
    Symbol firstSym = Symbol(0); // split to avoid vexing parse
    Polynomial aff(std::move(firstSym));
    for (size_t i = 0; i < MAX_PROGRAM_VARIABLES; ++i) {
        if (loopvars[i]) {
            Symbol sym = Symbol(loopvars[i]);
            if (i) {
                sym.prodIDs.push_back(i);
            } // i == 0 => constant
            aff += sym;
        }
    }
    return aff;
}

// A(m, n + k + 1)
// A( 1*m1 + M*(n1 + k1 + 1) )
// A( 1*m2 + M*n2 )
// if m1 != m2 => noalias // find strides
// if n1 == n2 => noalias
// if n1 < n2 => alias
//
// 1. partition into strides
// 2. check if a dependency may exist
//   a) solve Diophantine equations / Banerjee equations
// 3. calc difference vector
//
// [ 0 ], k1 + 1;
//

/*
struct Strides {
    Affine affine;
    std::vector<size_t> loopInductVars;
};
*/

std::intptr_t addWithOverflow(intptr_t x, intptr_t y) {
    intptr_t z;
    if (__builtin_add_overflow(x, y, &z)) {
        z = std::numeric_limits<intptr_t>::max();
    }
    return z;
}
std::intptr_t subWithOverflow(intptr_t x, intptr_t y) {
    intptr_t z;
    if (__builtin_sub_overflow(x, y, &z)) {
        z = std::numeric_limits<intptr_t>::min();
    }
    return z;
}
std::intptr_t mulWithOverflow(intptr_t x, intptr_t y) {
    intptr_t z;
    if (__builtin_mul_overflow(x, y, &z)) {
        if ((x > 0) ^ (y > 0)) { // wouldn't overflow if `x` or `y` were `0`
            z = std::numeric_limits<intptr_t>::min(); // opposite sign
        } else {
            z = std::numeric_limits<intptr_t>::max(); // same sign
        }
    }
    return z;
}

enum Order {
    InvalidOrder,
    EqualTo,
    LessThan,
    LessOrEqual,
    GreaterThan,
    GreaterOrEqual,
    NotEqual,
    UnknownOrder
};
auto maybeEqual(Order o) { return o & 1; }
auto maybeLess(Order o) { return o & 2; }
auto maybeGreater(Order o) { return o & 4; }
struct ValueRange {
    intptr_t lowerBound;
    intptr_t upperBound;
    ValueRange(intptr_t x) : lowerBound(x), upperBound(x) {}
    ValueRange(intptr_t l, intptr_t u) : lowerBound(l), upperBound(u) {}
    ValueRange(const ValueRange &x)
        : lowerBound(x.lowerBound), upperBound(x.upperBound) {}
    ValueRange &operator=(const ValueRange &x) = default;
    bool isKnown() { return lowerBound == upperBound; }
    bool operator<(ValueRange x) { return upperBound < x.lowerBound; }
    Order compare(ValueRange x) {
        // return upperBound < x.lowerBound;
        if (isKnown() & x.isKnown()) {
            return upperBound == x.upperBound ? EqualTo : NotEqual;
        }
        if (upperBound < x.lowerBound) {
            return LessThan;
        } else if (upperBound == x.lowerBound) {
            return LessOrEqual;
        } else if (lowerBound > x.upperBound) {
            return GreaterThan;
        } else if (lowerBound == x.upperBound) {
            return GreaterOrEqual;
        } else {
            return UnknownOrder;
        }
    }
    Order compare(intptr_t x) { return compare(ValueRange{x, x}); }
    ValueRange &operator+=(ValueRange x) {
        lowerBound = addWithOverflow(lowerBound, x.lowerBound);
        upperBound = addWithOverflow(upperBound, x.upperBound);
        return *this;
    }
    ValueRange &operator-=(ValueRange x) {
        lowerBound = subWithOverflow(lowerBound, x.upperBound);
        upperBound = subWithOverflow(upperBound, x.lowerBound);
        return *this;
    }
    ValueRange &operator*=(ValueRange x) {
        intptr_t a = mulWithOverflow(lowerBound, x.lowerBound);
        intptr_t b = mulWithOverflow(lowerBound, x.upperBound);
        intptr_t c = mulWithOverflow(upperBound, x.lowerBound);
        intptr_t d = mulWithOverflow(upperBound, x.upperBound);
        lowerBound = std::min(std::min(a, b), std::min(c, d));
        upperBound = std::max(std::max(a, b), std::max(c, d));
        return *this;
    }
    ValueRange operator+(ValueRange x) {
        ValueRange y(*this);
        return y += x;
    }
    ValueRange operator-(ValueRange x) {
        ValueRange y(*this);
        return y -= x;
    }
    ValueRange operator*(ValueRange x) {
        ValueRange y(*this);
        return y *= x;
    }
};
