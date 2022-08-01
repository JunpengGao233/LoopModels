#pragma once
// We'll follow Julia style, so anything that's not a constructor, destructor,
// nor an operator will be outside of the struct/class.
#include "./Macro.hpp"
#include "./TypePromotion.hpp"
#include <bit>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/Optional.h>
#include <llvm/ADT/SmallVector.h>
// #include <mlir/Analysis/Presburger/Matrix.h>
#include <numeric>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
// #ifndef NDEBUG
// #include <memory>
// #include <stacktrace>
// using stacktrace =
//     std::basic_stacktrace<std::allocator<std::stacktrace_entry>>;
// #endif

static int64_t gcd(int64_t x, int64_t y) {
    if (x == 0) {
        return std::abs(y);
    } else if (y == 0) {
        return std::abs(x);
    }
    assert(x != std::numeric_limits<int64_t>::min());
    assert(y != std::numeric_limits<int64_t>::min());
    int64_t a = std::abs(x);
    int64_t b = std::abs(y);
    if ((a == 1) | (b == 1)) {
        return 1;
    }
    int64_t az = std::countr_zero(uint64_t(x));
    int64_t bz = std::countr_zero(uint64_t(y));
    b >>= bz;
    int64_t k = std::min(az, bz);
    while (a) {
        a >>= az;
        int64_t d = a - b;
        az = std::countr_zero(uint64_t(d));
        b = std::min(a, b);
        a = std::abs(d);
    }
    return b << k;
}
static int64_t lcm(int64_t x, int64_t y) {
    if (std::abs(x) == 1) {
        return y;
    } else if (std::abs(y) == 1) {
        return x;
    } else {
        return x * (y / gcd(x, y));
    }
}
// https://en.wikipedia.org/wiki/Extended_Euclidean_algorithm
template <std::integral T> std::tuple<T, T, T> gcdx(T a, T b) {
    T old_r = a;
    T r = b;
    T old_s = 1;
    T s = 0;
    T old_t = 0;
    T t = 1;
    while (r) {
        T quotient = old_r / r;
        old_r -= quotient * r;
        old_s -= quotient * s;
        old_t -= quotient * t;
        std::swap(r, old_r);
        std::swap(s, old_s);
        std::swap(t, old_t);
    }
    // Solving for `t` at the end has 1 extra division, but lets us remove
    // the `t` updates in the loop:
    // T t = (b == 0) ? 0 : ((old_r - old_s * a) / b);
    // For now, I'll favor forgoing the division.
    return std::make_tuple(old_r, old_s, old_t);
}

std::pair<int64_t, int64_t> divgcd(int64_t x, int64_t y) {
    if (x) {
        if (y) {
            int64_t g = gcd(x, y);
            assert(g == std::gcd(x, y));
            return std::make_pair(x / g, y / g);
        } else {
            return std::make_pair(1, 0);
        }
    } else if (y) {
        return std::make_pair(0, 1);
    } else {
        return std::make_pair(0, 0);
    }
}

// template<typename T> T one(const T) { return T(1); }
struct One {
    operator int64_t() { return 1; };
    operator size_t() { return 1; };
};
bool isOne(int64_t x) { return x == 1; }
bool isOne(size_t x) { return x == 1; }

template <typename TRC> auto powBySquare(TRC &&x, size_t i) {
    // typedef typename std::remove_const<TRC>::type TR;
    // typedef typename std::remove_reference<TR>::type T;
    // typedef typename std::remove_reference<TRC>::type TR;
    // typedef typename std::remove_const<TR>::type T;
    typedef typename std::remove_cvref<TRC>::type T;
    switch (i) {
    case 0:
        return T(One());
    case 1:
        return T(std::forward<TRC>(x));
    case 2:
        return T(x * x);
    case 3:
        return T(x * x * x);
    default:
        break;
    }
    if (isOne(x)) {
        return T(One());
    }
    int64_t t = std::countr_zero(i) + 1;
    i >>= t;
    // T z(std::move(x));
    T z(std::forward<TRC>(x));
    T b;
    while (--t) {
        b = z;
        z *= b;
    }
    if (i == 0) {
        return z;
    }
    T y(z);
    while (i) {
        t = std::countr_zero(i) + 1;
        i >>= t;
        while ((--t) >= 0) {
            b = z;
            z *= b;
        }
        y *= z;
    }
    return y;
}

template <typename T>
concept HasMul = requires(T t) {
    t.mul(t, t);
};

// a and b are temporary, z stores the final results.
template <HasMul T> void powBySquare(T &z, T &a, T &b, T const &x, size_t i) {
    switch (i) {
    case 0:
        z = One();
        return;
    case 1:
        z = x;
        return;
    case 2:
        z.mul(x, x);
        return;
    case 3:
        b.mul(x, x);
        z.mul(b, x);
        return;
    default:
        break;
    }
    if (isOne(x)) {
        z = x;
        return;
    }
    int64_t t = std::countr_zero(i) + 1;
    i >>= t;
    z = x;
    while (--t) {
        b.mul(z, z);
        std::swap(b, z);
    }
    if (i == 0) {
        return;
    }
    a = z;
    while (i) {
        t = std::countr_zero(i) + 1;
        i >>= t;
        while ((--t) >= 0) {
            b.mul(a, a);
            std::swap(b, a);
        }
        b.mul(a, z);
        std::swap(b, z);
    }
    return;
}
template <HasMul TRC> auto powBySquare(TRC &&x, size_t i) {
    // typedef typename std::remove_const<TRC>::type TR;
    // typedef typename std::remove_reference<TR>::type T;
    // typedef typename std::remove_reference<TRC>::type TR;
    // typedef typename std::remove_const<TR>::type T;
    typedef typename std::remove_cvref<TRC>::type T;
    switch (i) {
    case 0:
        return T(One());
    case 1:
        return T(std::forward<TRC>(x));
    case 2:
        return T(x * x);
    case 3:
        return T(x * x * x);
    default:
        break;
    }
    if (isOne(x)) {
        return T(One());
    }
    int64_t t = std::countr_zero(i) + 1;
    i >>= t;
    // T z(std::move(x));
    T z(std::forward<TRC>(x));
    T b;
    while (--t) {
        b.mul(z, z);
        std::swap(b, z);
    }
    if (i == 0) {
        return z;
    }
    T y(z);
    while (i) {
        t = std::countr_zero(i) + 1;
        i >>= t;
        while ((--t) >= 0) {
            b.mul(z, z);
            std::swap(b, z);
        }
        b.mul(y, z);
        std::swap(b, y);
    }
    return y;
}

template <typename T, typename S> void divExact(T &x, S const &y) {
    auto d = x / y;
    assert(d * y == x);
    x = d;
}

enum class VarType : uint32_t {
    Constant = 0x0,
    LoopInductionVariable = 0x1,
    Memory = 0x2,
    Term = 0x3
};
std::ostream &operator<<(std::ostream &os, VarType s) {
    switch (s) {
    case VarType::Constant:
        os << "Constant";
        break;
    case VarType::LoopInductionVariable:
        os << "Induction Variable";
        break;
    case VarType::Memory:
        os << "Memory";
        break;
    case VarType::Term:
        os << "Term";
        break;
    }
    return os;
}

typedef uint32_t IDType;
struct VarID {
    IDType id;
    VarID(IDType id) : id(id) {}
    VarID(IDType i, VarType typ) : id((static_cast<IDType>(typ) << 30) | i) {}
    bool operator<(VarID x) const { return id < x.id; }
    bool operator<=(VarID x) const { return id <= x.id; }
    bool operator==(VarID x) const { return id == x.id; }
    bool operator>(VarID x) const { return id > x.id; }
    bool operator>=(VarID x) const { return id >= x.id; }
    std::strong_ordering operator<=>(VarID x) { return id <=> x.id; }
    IDType getID() const { return id & 0x3fffffff; }
    // IDType getID() const { return id & 0x3fff; }
    VarType getType() const { return static_cast<VarType>(id >> 30); }
    std::pair<VarType, IDType> getTypeAndId() const {
        return std::make_pair(getType(), getID());
    }
    bool isIndVar() { return getType() == VarType::LoopInductionVariable; }
    bool isLoopInductionVariable() const {
        return getType() == VarType::LoopInductionVariable;
    }
};
std::ostream &operator<<(std::ostream &os, VarID s) {
    return os << s.getType() << ": " << s.getID();
}

inline bool isZero(auto x) { return x == 0; }

bool allZero(const auto &x) {
    for (auto &a : x)
        if (!isZero(a))
            return false;
    return true;
}

template <typename T>
concept AbstractVector = HasEltype<T> && requires(T t, size_t i) {
    {
        t(i)
        } -> std::convertible_to<typename std::remove_reference_t<T>::eltype>;
    { t.size() } -> std::convertible_to<size_t>;
    {t.view()};
};
// template <typename T>
// concept AbstractMatrix = HasEltype<T> && requires(T t, size_t i) {
//     { t(i, i) } -> std::convertible_to<typename T::eltype>;
//     { t.numRow() } -> std::convertible_to<size_t>;
//     { t.numCol() } -> std::convertible_to<size_t>;
// };
template <typename T>
concept AbstractMatrixCore = HasEltype<T> && requires(T t, size_t i) {
    {
        t(i, i)
        } -> std::convertible_to<typename std::remove_reference_t<T>::eltype>;
    { t.numRow() } -> std::convertible_to<size_t>;
    { t.numCol() } -> std::convertible_to<size_t>;
};
template <typename T>
concept AbstractMatrix = AbstractMatrixCore<T> && requires(T t) {
    { t.view() } -> AbstractMatrixCore;
};

struct Add {
    constexpr auto operator()(auto x, auto y) const { return x + y; }
};
struct Sub {
    constexpr auto operator()(auto x) { return -x; }
    constexpr auto operator()(auto x, auto y) const { return x - y; }
};
struct Mul {
    constexpr auto operator()(auto x, auto y) const { return x * y; }
};
struct Div {
    constexpr auto operator()(auto x, auto y) const { return x / y; }
};

template <typename Op, typename A> struct ElementwiseUnaryOp {
    const Op op;
    const A a;
    auto operator()(size_t i) const { return op(a(i)); }
    auto operator()(size_t i, size_t j) const { return op(a(i, j)); }

    size_t size() { return a.size(); }
    size_t numRow() { return a.numRow(); }
    size_t numCol() { return a.numCol(); }
    inline auto view() const { return *this; };
};
// scalars broadcast
inline auto get(const std::integral auto A, size_t) { return A; }
inline auto get(const std::floating_point auto A, size_t) { return A; }
inline auto get(const std::integral auto A, size_t, size_t) { return A; }
inline auto get(const std::floating_point auto A, size_t, size_t) { return A; }
inline auto get(const AbstractVector auto &A, size_t i) { return A(i); }
inline auto get(const AbstractMatrix auto &A, size_t i, size_t j) {
    return A(i, j);
}

constexpr size_t size(const std::integral auto) { return 1; }
constexpr size_t size(const std::floating_point auto) { return 1; }
inline size_t size(const AbstractVector auto &x) { return x.size(); }

struct Rational;
template <typename T>
concept Scalar =
    std::integral<T> || std::floating_point<T> || std::same_as<T, Rational>;

template <typename T>
concept VectorOrScalar = AbstractVector<T> || Scalar<T>;
template <typename T>
concept MatrixOrScalar = AbstractMatrix<T> || Scalar<T>;

template <typename Op, VectorOrScalar A, VectorOrScalar B>
struct ElementwiseVectorBinaryOp {
    using eltype = typename PromoteEltype<A, B>::eltype;
    Op op;
    A a;
    B b;
    auto operator()(size_t i) const { return op(get(a, i), get(b, i)); }
    size_t size() const {
        if constexpr (AbstractVector<A> && AbstractVector<B>) {
            const size_t N = a.size();
            assert(N == b.size());
            return N;
        } else if constexpr (AbstractVector<A>) {
            return a.size();
        } else { // if constexpr (AbstractVector<B>) {
            return b.size();
        }
    }
    auto &view() const { return *this; };
};

template <typename Op, MatrixOrScalar A, MatrixOrScalar B>
struct ElementwiseMatrixBinaryOp {
    using eltype = typename PromoteEltype<A, B>::eltype;
    Op op;
    A a;
    B b;
    auto operator()(size_t i, size_t j) const {
        return op(get(a, i, j), get(b, i, j));
    }
    size_t numRow() const {
        static_assert(AbstractMatrix<A> || std::integral<A> ||
                          std::floating_point<A>,
                      "Argument A to elementwise binary op is not a matrix.");
        static_assert(AbstractMatrix<B> || std::integral<B> ||
                          std::floating_point<B>,
                      "Argument B to elementwise binary op is not a matrix.");
        if constexpr (AbstractMatrix<A> && AbstractMatrix<B>) {
            const size_t N = a.numRow();
            assert(N == b.numRow());
            return N;
        } else if constexpr (AbstractMatrix<A>) {
            return a.numRow();
        } else if constexpr (AbstractMatrix<B>) {
            return b.numRow();
        }
    }
    size_t numCol() const {
        static_assert(AbstractMatrix<A> || std::integral<A> ||
                          std::floating_point<A>,
                      "Argument A to elementwise binary op is not a matrix.");
        static_assert(AbstractMatrix<B> || std::integral<B> ||
                          std::floating_point<B>,
                      "Argument B to elementwise binary op is not a matrix.");
        if constexpr (AbstractMatrix<A> && AbstractMatrix<B>) {
            const size_t N = a.numCol();
            assert(N == b.numCol());
            return N;
        } else if constexpr (AbstractMatrix<A>) {
            return a.numCol();
        } else if constexpr (AbstractMatrix<B>) {
            return b.numCol();
        }
    }
    auto &view() const { return *this; };
};

template <typename A> struct Transpose {
    using eltype = typename A::eltype;
    A a;
    auto operator()(size_t i, size_t j) const { return a(j, i); }
    size_t numRow() const { return a.numCol(); }
    size_t numCol() const { return a.numRow(); }
    auto &view() const { return *this; };
};
template <AbstractMatrix A, AbstractMatrix B> struct MatMatMul {
    using eltype = typename PromoteEltype<A, B>::eltype;
    A a;
    B b;
    auto operator()(size_t i, size_t j) const {
        static_assert(AbstractMatrix<B>, "B should be an AbstractMatrix");
        auto s = (a(i, 0) * b(0, j)) * 0;
        for (size_t k = 0; k < a.numCol(); ++k)
            s += a(i, k) * b(k, j);
        return s;
    }
    size_t numRow() const { return a.numRow(); }
    size_t numCol() const { return b.numCol(); }
    inline auto view() const { return *this; };
};
template <AbstractMatrix A, AbstractVector B> struct MatVecMul {
    using eltype = typename PromoteEltype<A, B>::eltype;
    A a;
    B b;
    auto operator()(size_t i) const {
        static_assert(AbstractVector<B>, "B should be an AbstractVector");
        auto s = (a(i, 0) * b(0)) * 0;
        for (size_t k = 0; k < a.numCol(); ++k)
            s += a(i, k) * b(k);
        return s;
    }
    size_t size() const { return a.numRow(); }
    inline auto view() const { return *this; };
};

template <typename B, typename E> struct Range {
    B begin;
    E end;
};
struct Begin {
} begin;
struct End {
} end;
struct Colon {
    constexpr Range<size_t, size_t> operator()(std::integral auto i,
                                               std::integral auto j) {
        return Range<size_t, size_t>{size_t(i), size_t(j)};
    }
    template <typename B, typename E>
    constexpr Range<B, E> operator()(B i, E j) {
        return Range<B, E>{i, j};
    }
} _;

inline Range<size_t, size_t> canonicalizeRange(Range<size_t, size_t> r,
                                               size_t) {
    return r;
}
inline Range<size_t, size_t> canonicalizeRange(Range<Begin, size_t> r, size_t) {
    return Range<size_t, size_t>{0, r.end};
}
inline Range<size_t, size_t> canonicalizeRange(Range<size_t, End> r, size_t M) {
    return Range<size_t, size_t>{r.begin, M};
}
inline Range<size_t, size_t> canonicalizeRange(Range<Begin, End>, size_t M) {
    return Range<size_t, size_t>{0, M};
}
inline Range<size_t, size_t> canonicalizeRange(Colon, size_t M) {
    return Range<size_t, size_t>{0, M};
}

template <typename T, typename V> struct BaseVector {
    using eltype = T;
    inline T &ref(size_t i) { return static_cast<V *>(this)(i); }
    inline T &size() { return static_cast<V *>(this)->size(); }
    V &operator=(AbstractVector auto &x) {
        const size_t N = size();
        const size_t M = x.size();
        V &self = *static_cast<V *>(this);
        if constexpr (static_cast<V *>(this)->canResize()) {
            if (M != N)
                self.resizeForOverwrite(M);
        } else {
            assert(M == N);
        }
        for (size_t i = 0; i < M; ++i)
            self(i) = x(i);
        return self;
    }
    bool operator==(AbstractVector auto &x) {
        const size_t N = size();
        if (N != x.size())
            return false;
        for (size_t n = 0; n < N; ++n)
            if (ref(n) != x(n))
                return false;
        return true;
    }
    V &view(Begin, End) { return *static_cast<V *>(this); }
    inline auto view(Begin, size_t i) {
        return static_cast<V *>(this)->view(0, i);
    }
    inline auto view(size_t i, End) {
        return static_cast<V *>(this)->view(i, size());
    }
};

//
// Vectors
//
template <typename T, size_t M = 0>
struct Vector : BaseVector<T, Vector<T, M>> {
    using eltype = T;
    T data[M];
    T &operator()(size_t i) {
        assert(i < M);
        return data[i];
    }
    const T &operator()(size_t i) const {
        assert(i < M);
        return data[i];
    }
    T &operator[](size_t i) {
        assert(i < M);
        return data[i];
    }
    const T &operator[](size_t i) const {
        assert(i < M);
        return data[i];
    }
    T *begin() { return data; }
    T *end() { return begin() + M; }
    const T *begin() const { return data; }
    const T *end() const { return begin() + M; }
};
template <typename T, size_t M = 0>
struct PtrVector : BaseVector<T, PtrVector<T, M>> {
    using eltype = T;
    T *ptr;
    PtrVector(T *ptr) : ptr(ptr){};
    T &operator()(size_t i) const {
        assert(i < M);
        return ptr[i];
    }
    T &operator[](size_t i) { return ptr[i]; }
    const T &operator[](size_t i) const { return ptr[i]; }
    T *begin() { return ptr; }
    T *end() { return ptr + M; }
    const T *begin() const { return ptr; }
    const T *end() const { return ptr + M; }
    constexpr size_t size() const { return M; }
    PtrVector<T, M> view() { return *this; };
    PtrVector<const T, M> view() const {
        return PtrVector<const T, M>{.ptr = ptr};
    };
};
template <typename T> struct PtrVector<T, 0> {
    using eltype = T;
    T *ptr;
    size_t M;
    T &operator[](size_t i) {
        assert(i < M);
        return ptr[i];
    }
    const T &operator[](size_t i) const {
        assert(i < M);
        return ptr[i];
    }
    T &operator()(size_t i) {
        assert(i < M);
        return ptr[i];
    }
    const T &operator()(size_t i) const {
        assert(i < M);
        return ptr[i];
    }
    PtrVector<T, 0> operator()(Range<size_t, size_t> i) {
        assert(i.begin <= i.end);
        assert(i.end <= M);
        return PtrVector{.ptr = ptr + i.begin, .M = i.end - i.begin};
    }
    PtrVector<const T, 0> operator()(Range<size_t, size_t> i) const {
        assert(i.begin <= i.end);
        assert(i.end <= M);
        return PtrVector{.ptr = ptr + i.begin, .M = i.end - i.begin};
    }
    template <typename F, typename L>
    PtrVector<T, 0> operator()(Range<F, L> i) {
        return (*this)(canonicalizeRange(i, M));
    }
    template <typename F, typename L>
    PtrVector<const T, 0> operator()(Range<F, L> i) const {
        return (*this)(canonicalizeRange(i, M));
    }
    T *begin() { return ptr; }
    T *end() { return ptr + M; }
    const T *begin() const { return ptr; }
    const T *end() const { return ptr + M; }
    size_t size() const { return M; }
    operator llvm::ArrayRef<std::remove_const_t<T>>() const {
        return llvm::ArrayRef<std::remove_const_t<T>>{ptr, M};
    }
    // llvm::ArrayRef<T> arrayref() const { return llvm::ArrayRef<T>(ptr, M); }
    bool operator==(const PtrVector<T, 0> x) const {
        return llvm::ArrayRef<std::remove_const_t<T>>(*this) ==
               llvm::ArrayRef<std::remove_const_t<T>>(x);
    }
    bool operator==(const llvm::ArrayRef<std::remove_const_t<T>> x) const {
        return llvm::ArrayRef<std::remove_const_t<T>>(*this) == x;
    }
    PtrVector<T, 0> view() { return *this; };
    PtrVector<const T, 0> view() const {
        return PtrVector<const T, 0>{.ptr = ptr, .M = M};
    };
    PtrVector<T, 0> operator=(const AbstractVector auto &x) {
        const size_t N = x.size();
        assert(M == N);
        for (size_t i = 0; i < M; ++i)
            ptr[i] = x(i);
        return *this;
    }
    PtrVector<T, 0> operator+=(const AbstractVector auto &x) {
        const size_t N = x.size();
        assert(M == N);
        for (size_t i = 0; i < M; ++i)
            ptr[i] += x(i);
        return *this;
    }
    PtrVector<T, 0> operator-=(const AbstractVector auto &x) {
        const size_t N = x.size();
        assert(M == N);
        for (size_t i = 0; i < M; ++i)
            ptr[i] -= x(i);
        return *this;
    }
    PtrVector<T, 0> operator*=(const AbstractVector auto &x) {
        const size_t N = x.size();
        assert(M == N);
        for (size_t i = 0; i < M; ++i)
            ptr[i] *= x(i);
        return *this;
    }
    PtrVector<T, 0> operator/=(const AbstractVector auto &x) {
        const size_t N = x.size();
        assert(M == N);
        for (size_t i = 0; i < M; ++i)
            ptr[i] /= x(i);
        return *this;
    }
    PtrVector<T, 0> operator*=(const std::integral auto x) {
        for (size_t i = 0; i < M; ++i)
            ptr[i] *= x;
        return *this;
    }
    PtrVector<T, 0> operator/=(const std::integral auto x) {
        for (size_t i = 0; i < M; ++i)
            ptr[i] /= x;
        return *this;
    }
    // PtrVector(T *data, size_t len) : ptr(data), M(len) {}
    // PtrVector(llvm::MutableArrayRef<T> x) : ptr(x.data()), M(x.size()) {}
    // PtrVector(llvm::ArrayRef<std::remove_const_t<T>> x)
    //     : ptr(x.data()), M(x.size()) {}

    operator PtrVector<const T>() {
        return PtrVector<const T>{.ptr = ptr, .M = M};
    }
    // PtrVector(const AbstractVector auto &A)
    //     : mem(llvm::SmallVector<T>{}), M(A.numRow()), N(A.numCol()),
    //       X(A.numCol()) {
    //     mem.resize_for_overwrite(M * N);
    //     for (size_t m = 0; m < M; ++m)
    //         for (size_t n = 0; n < N; ++n)
    //             mem[m * X + n] = A(m, n);
    // }
};

template <typename T> inline auto view(llvm::SmallVectorImpl<T> &x) {
    return PtrVector<T>{x.data(), x.size()};
}
template <typename T> inline auto view(const llvm::SmallVectorImpl<T> &x) {
    return PtrVector<const T>{x.data(), x.size()};
}
template <typename T> inline auto view(llvm::MutableArrayRef<T> x) {
    return PtrVector<T>{x.data(), x.size()};
}
template <typename T> inline auto view(const llvm::ArrayRef<T> x) {
    return PtrVector<const T>{x.data(), x.size()};
}

template <typename T> struct Vector<T, 0> {
    using eltype = T;
    llvm::SmallVector<T, 16> data;

    Vector(size_t N = 0) : data(llvm::SmallVector<T>(N)){};

    Vector(const llvm::SmallVector<T> &A) : data(A.begin(), A.end()){};
    Vector(llvm::SmallVector<T> &&A) : data(std::move(A)){};

    T &operator()(size_t i) {
        assert(i < data.size());
        return data[i];
    }
    const T &operator()(size_t i) const {
        assert(i < data.size());
        return data[i];
    }
    T &operator[](size_t i) { return data[i]; }
    const T &operator[](size_t i) const { return data[i]; }
    // bool operator==(Vector<T, 0> x0) const { return allMatch(*this, x0); }
    auto begin() { return data.begin(); }
    auto end() { return data.end(); }
    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }
    size_t size() const { return data.size(); }
    PtrVector<T, 0> view() {
        return PtrVector<T, 0>{.ptr = data.data(), .M = data.size()};
    };
    PtrVector<const T, 0> view() const {
        return PtrVector<const T, 0>{.ptr = data.data(), .M = data.size()};
    };
    template <typename A> void push_back(A &&x) {
        data.push_back(std::forward<A>(x));
    }
    template <typename... A> void emplace_back(A &&...x) {
        data.emplace_back(std::forward<A>(x)...);
    }
    Vector(const AbstractVector auto &x) : data(llvm::SmallVector<T>{}) {
        const size_t N = x.size();
        data.resize_for_overwrite(N);
        for (size_t n = 0; n < N; ++n)
            data[n] = x(n);
    }
    void resize(size_t N) { data.resize(N); }
    void resizeForOverwrite(size_t N) { data.resize_for_overwrite(N); }

    operator PtrVector<T>() {
        return PtrVector<T>{.ptr = data.data(), .M = data.size()};
    }
    operator PtrVector<const T>() const {
        return PtrVector<const T>{.ptr = data.data(), .M = data.size()};
    }
};

template <typename T, size_t M>
bool operator==(Vector<T, M> const &x0, Vector<T, M> const &x1) {
    return allMatch(x0, x1);
}
static_assert(std::copyable<Vector<intptr_t, 4>>);
static_assert(std::copyable<Vector<intptr_t, 0>>);

template <typename T> struct StridedVector {
    T *d;
    size_t N;
    size_t x;
    struct StridedIterator {
        T *d;
        size_t x;
        auto operator++() {
            d += x;
            return *this;
        }
        auto operator--() {
            d -= x;
            return *this;
        }
        T &operator*() { return *d; }
        bool operator==(const StridedIterator y) const { return d == y.d; }
    };
    auto begin() { return StridedIterator{d, x}; }
    auto end() { return StridedIterator{d + N * x, x}; }
    auto begin() const { return StridedIterator{d, x}; }
    auto end() const { return StridedIterator{d + N * x, x}; }
    T &operator[](size_t i) { return d[i * x]; }
    const T &operator[](size_t i) const { return d[i * x]; }
    size_t size() const { return N; }
    bool operator==(StridedVector<T> x) const {
        if (size() != x.size())
            return false;
        for (size_t i = 0; i < size(); ++i) {
            if ((*this)[i] != x[i])
                return false;
        }
        return true;
    }
    inline StridedVector<T> view(size_t start, size_t stop) {
        return StridedVector<T>{*d + start * x, stop - start, x};
    }
    inline StridedVector<const T> view(size_t start, size_t stop) const {
        return StridedVector<const T>{*d + start * x, stop - start, x};
    }
    StridedVector<T> view() { return *this; }
    StridedVector<const T> view() const { return *this; }
};

// template <typename T>
// inline llvm::ArrayRef<T> view(llvm::ArrayRef<T> a, size_t start, size_t stop)
// {
//     return llvm::ArrayRef<T>{a.data() + start, stop - start};
// }
// template <typename T>
// inline llvm::MutableArrayRef<T> view(llvm::MutableArrayRef<T> a, size_t
// start,
//                                      size_t stop) {
//     return llvm::MutableArrayRef<T>{a.data() + start, stop - start};
// }

template <typename T> struct SmallSparseMatrix;
template <typename T> struct PtrMatrix {
    using eltype = T;
    T *mem;
    const size_t M, N, X;

    static constexpr bool fixedNumRow = true;
    static constexpr bool fixedNumCol = true;
    // PtrMatrix(T *mem, size_t M, size_t N, size_t X)
    //     : mem(mem), M(M), N(N), X(X){};

    T *begin() { return mem; }
    T *end() { return mem + (M * N); }
    const T *begin() const { return mem; }
    const T *end() const { return mem + (M * N); }

    inline size_t numRow() const { return M; }
    inline size_t numCol() const { return N; }
    inline size_t rowStride() const { return X; }
    static constexpr size_t colStride() { return 1; }
    static constexpr size_t getConstCol() { return 0; }

    inline std::pair<size_t, size_t> size() const {
        return std::make_pair(M, N);
    }
    inline PtrMatrix<const T> view() const {
        return PtrMatrix<const T>{.mem = mem, .M = M, .N = N, .X = X};
    };

    T *data() { return mem; }
    const T *data() const { return mem; }

    operator PtrMatrix<const T>() const {
        return PtrMatrix<const T>{.mem = mem, .M = M, .N = N, .X = X};
    }
    PtrMatrix<T> operator=(SmallSparseMatrix<T> &A) {
        assert(M == A.numRow());
        assert(N == A.numCol());
        size_t k = 0;
        for (size_t i = 0; i < M; ++i) {
            uint32_t m = A.rows[i] & 0x00ffffff;
            size_t j = 0;
            while (m) {
                uint32_t tz = std::countr_zero(m);
                m >>= tz + 1;
                j += tz;
                mem[i * X + (j++)] = A.nonZeros[k++];
            }
        }
        assert(k == A.nonZeros.size());
        return *this;
    }
    inline T &operator()(size_t row, size_t col) {
        assert(row <= M);
        assert(col <= N);
        return *(mem + col + row * X);
    }
    inline PtrMatrix<T> operator()(Range<size_t, size_t> rows,
                                   Range<size_t, size_t> cols) {
        assert(rows.end >= rows.begin);
        assert(cols.end >= cols.begin);
        assert(rows.end <= M);
        assert(cols.end <= N);
        return PtrMatrix<T>{.mem = mem + cols.begin + rows.begin * X,
                            .M = rows.end - rows.begin,
                            .N = cols.end - cols.begin,
                            .X = X};
    }
    template <typename R0, typename R1, typename C0, typename C1>
    inline PtrMatrix<T> operator()(Range<R0, R1> rows, Range<C0, C1> cols) {
        return (*this)(canonicalizeRange(rows, M), canonicalizeRange(cols, N));
    }
    template <typename C0, typename C1>
    inline PtrMatrix<T> operator()(Colon, Range<C0, C1> cols) {
        return (*this)(std::make_pair(0, M), canonicalizeRange(cols, N));
    }
    template <typename R0, typename R1>
    inline PtrMatrix<T> operator()(Range<R0, R1> rows, Colon) {
        return (*this)(canonicalizeRange(rows, M), std::make_pair(0, N));
    }
    inline PtrMatrix<T> operator()(Colon, Colon) { return *this; }
    template <typename R0, typename R1>
    inline auto operator()(Range<R0, R1> rows, size_t col) {
        return getCol(col)(canonicalizeRange(rows, M));
    }
    template <typename C0, typename C1>
    inline auto operator()(size_t row, Range<C0, C1> cols) {
        return getRow(row)(canonicalizeRange(cols, N));
    }
    inline auto operator()(Colon, size_t col) { return getCol(col); }
    inline auto operator()(size_t row, Colon) { return getRow(row); }

    inline T operator()(size_t row, size_t col) const {
        assert(row <= M);
        assert(col <= N);
        return *(mem + col + row * X);
    }
    inline PtrMatrix<const T> operator()(Range<size_t, size_t> rows,
                                         Range<size_t, size_t> cols) const {
        assert(rows.end >= rows.begin);
        assert(cols.end >= cols.begin);
        assert(rows.end <= M);
        assert(cols.end <= N);
        return PtrMatrix<T>{.mem = mem + cols.begin + rows.begin * X,
                            .M = rows.end - rows.begin,
                            .N = cols.end - cols.begin,
                            .X = X};
    }
    template <typename R0, typename R1, typename C0, typename C1>
    inline PtrMatrix<const T> operator()(Range<R0, R1> rows,
                                         Range<C0, C1> cols) const {
        return view(canonicalizeRange(rows, M), canonicalizeRange(cols, N));
    }
    template <typename C0, typename C1>
    inline PtrMatrix<const T> operator()(Colon, Range<C0, C1> cols) const {
        return view(std::make_pair(0, M), canonicalizeRange(cols, N));
    }
    template <typename R0, typename R1>
    inline PtrMatrix<const T> operator()(Range<R0, R1> rows, Colon) const {
        return view(canonicalizeRange(rows, M), std::make_pair(0, N));
    }
    template <typename R0, typename R1, typename C0, typename C1>
    inline PtrMatrix<const T> operator()(Colon, Colon) const {
        return *this;
    }
    template <typename R0, typename R1>
    inline auto operator()(Range<R0, R1> rows, size_t col) const {
        return getCol(col)(canonicalizeRange(rows, M));
    }
    template <typename C0, typename C1>
    inline auto operator()(size_t row, Range<C0, C1> cols) const {
        return getRow(row)(canonicalizeRange(cols, N));
    }
    inline auto operator()(Colon, size_t col) const { return getCol(col); }
    inline auto operator()(size_t row, Colon) const { return getRow(row); }

    inline PtrVector<T, 0> getRow(size_t i) {
        return PtrVector<T, 0>{.ptr = mem + i * X, .M = N};
    }
    inline PtrVector<const T, 0> getRow(size_t i) const {
        return PtrVector<const T, 0>{.ptr = mem + i * X, .M = N};
    }
    // void copyRow(llvm::ArrayRef<T> x, size_t i) {
    //     for (size_t j = 0; j < numCol(); ++j)
    //         (*this)(i, j) = x[j];
    // }
    inline StridedVector<T> getCol(size_t n) {
        return StridedVector<T>{mem + n, M, X};
    }
    inline StridedVector<const T> getCol(size_t n) const {
        return StridedVector<const T>{mem + n, M, X};
    }
    PtrMatrix<T> operator=(const AbstractMatrix auto &B) {
        assert(M == B.numRow());
        assert(N == B.numCol());
        for (size_t r = 0; r < M; ++r)
            for (size_t c = 0; c < N; ++c)
                (*this)(r, c) = B(r, c);
        return *this;
    }

    PtrMatrix<T> operator+=(const AbstractMatrix auto &B) {
        assert(M == B.numRow());
        assert(N == B.numCol());
        for (size_t r = 0; r < M; ++r)
            for (size_t c = 0; c < N; ++c)
                (*this)(r, c) += B(r, c);
        return *this;
    }
    PtrMatrix<T> operator-=(const AbstractMatrix auto &B) {
        assert(M == B.numRow());
        assert(N == B.numCol());
        for (size_t r = 0; r < M; ++r)
            for (size_t c = 0; c < N; ++c)
                (*this)(r, c) -= B(r, c);
        return *this;
    }
    PtrMatrix<T> operator*=(const std::integral auto b) {
        for (size_t r = 0; r < M; ++r)
            for (size_t c = 0; c < N; ++c)
                (*this)(r, c) *= b;
        return *this;
    }
    PtrMatrix<T> operator/=(const std::integral auto b) {
        for (size_t r = 0; r < M; ++r)
            for (size_t c = 0; c < N; ++c)
                (*this)(r, c) /= b;
        return *this;
    }

    bool operator==(const AbstractMatrix auto &B) const {
        const size_t M = B.numRow();
        const size_t N = B.numCol();
        if ((M != numRow()) || (N != numCol()))
            return false;
        for (size_t r = 0; r < M; ++r)
            for (size_t c = 0; c < N; ++c)
                if ((*this)(r, c) != B(r, c))
                    return false;
        return true;
    }
    bool operator==(PtrMatrix<T> B) const {
        const size_t M = B.numRow();
        const size_t N = B.numCol();
        if ((M != numRow()) || (N != numCol()))
            return false;
        for (size_t r = 0; r < M; ++r)
            for (size_t c = 0; c < N; ++c)
                if ((*this)(r, c) != B(r, c))
                    return false;
        return true;
    }
};
static_assert(std::is_trivially_copyable_v<PtrMatrix<int64_t>>,
              "PtrMatrix<int64_t> is not trivially copyable!");
static_assert(std::is_trivially_copyable_v<PtrVector<int64_t, 0>>,
              "PtrVector<int64_t,0> is not trivially copyable!");

static_assert(!AbstractVector<PtrMatrix<int64_t>>,
              "PtrMatrix<int64_t> isa AbstractVector succeeded");
static_assert(!AbstractVector<PtrMatrix<const int64_t>>,
              "PtrMatrix<const int64_t> isa AbstractVector succeeded");
static_assert(!AbstractVector<const PtrMatrix<const int64_t>>,
              "PtrMatrix<const int64_t> isa AbstractVector succeeded");

static_assert(AbstractMatrix<PtrMatrix<int64_t>>,
              "PtrMatrix<int64_t> isa AbstractMatrix failed");
static_assert(AbstractMatrix<PtrMatrix<const int64_t>>,
              "PtrMatrix<const int64_t> isa AbstractMatrix failed");
static_assert(AbstractMatrix<const PtrMatrix<const int64_t>>,
              "PtrMatrix<const int64_t> isa AbstractMatrix failed");

static_assert(AbstractVector<PtrVector<int64_t>>,
              "PtrVector<int64_t> isa AbstractVector failed");
static_assert(AbstractVector<PtrVector<const int64_t>>,
              "PtrVector<const int64_t> isa AbstractVector failed");
static_assert(AbstractVector<const PtrVector<const int64_t>>,
              "PtrVector<const int64_t> isa AbstractVector failed");

static_assert(!AbstractMatrix<PtrVector<int64_t>>,
              "PtrVector<int64_t> isa AbstractMatrix succeeded");
static_assert(!AbstractMatrix<PtrVector<const int64_t>>,
              "PtrVector<const int64_t> isa AbstractMatrix succeeded");
static_assert(!AbstractMatrix<const PtrVector<const int64_t>>,
              "PtrVector<const int64_t> isa AbstractMatrix succeeded");

static_assert(
    AbstractMatrix<
        ElementwiseMatrixBinaryOp<Mul, PtrMatrix<const long int>, int>>,
    "ElementwiseBinaryOp isa AbstractMatrix failed");

static_assert(
    !AbstractVector<MatMatMul<PtrMatrix<const long>, PtrMatrix<const long>>>,
    "MatMul should not be an AbstractVector!");
static_assert(
    AbstractMatrix<MatMatMul<PtrMatrix<const long>, PtrMatrix<const long>>>,
    "MatMul is not an AbstractMatrix!");

template <typename T>
concept IntVector = requires(T t, int64_t y) {
    { t.size() } -> std::convertible_to<size_t>;
    { t[y] } -> std::convertible_to<int64_t>;
};

template <typename T, typename A> struct BaseMatrix {
    using eltype = T;
    inline T &getLinearElement(size_t i) {
        return static_cast<A *>(this)->getLinearElement(i);
    }
    inline const T &getLinearElement(size_t i) const {
        return static_cast<const A *>(this)->getLinearElement(i);
    }
    inline T &operator[](size_t i) { return getLinearElement(i); }
    inline const T &operator[](size_t i) const { return getLinearElement(i); }
    inline size_t numCol() const {
        return static_cast<const A *>(this)->numCol();
    }
    inline size_t numRow() const {
        return static_cast<const A *>(this)->numRow();
    }
    inline size_t rowStride() const {
        return static_cast<const A *>(this)->rowStride();
    }
    inline size_t colStride() const {
        return static_cast<const A *>(this)->colStride();
    }
    auto begin() { return static_cast<A *>(this)->begin(); }
    auto end() { return static_cast<A *>(this)->end(); }
    auto begin() const { return static_cast<const A *>(this)->begin(); }
    auto end() const { return static_cast<const A *>(this)->end(); }

    T *data() { return static_cast<A *>(this)->data(); }
    const T *data() const { return static_cast<const A *>(this)->data(); }

    void sizeExtend(size_t M, size_t N) {
        if constexpr (static_cast<A *>(this)->fixedNumRow) {
            assert(numRow() == M);
        } else {
            if (M > numRow())
                static_cast<A *>(this)->resizeRows(M);
        }
        if constexpr (static_cast<A *>(this)->fixedNumCol) {
            assert(numCol() == N);
        } else {
            if (N > numCol())
                static_cast<A *>(this)->resizeCols(N);
        }
    }

    A &operator=(const AbstractMatrix auto &B) {
        const size_t M = B.numRow();
        const size_t N = B.numCol();
        sizeExtend(M, N);
        for (size_t r = 0; r < M; ++r)
            for (size_t c = 0; c < N; ++c)
                (*this)(r, c) = B(r, c);
        return *this;
    }
    // bool operator==(const BaseMatrix<T,A> &B) const {
    //     const size_t M = B.numRow();
    //     const size_t N = B.numCol();
    // 	if ((M != numRow()) || (N != numCol()))
    // 	    return false;
    //     for (size_t r = 0; r < M; ++r)
    //         for (size_t c = 0; c < N; ++c)
    //             if ((*this)(r, c) != B(r, c))
    // 		    return false;
    //     return true;
    // }
    bool operator==(const AbstractMatrix auto &B) const {
        const size_t M = B.numRow();
        const size_t N = B.numCol();
        if ((M != numRow()) || (N != numCol()))
            return false;
        for (size_t r = 0; r < M; ++r)
            for (size_t c = 0; c < N; ++c)
                if ((*this)(r, c) != B(r, c))
                    return false;
        return true;
    }

    std::pair<size_t, size_t> size() const {
        return std::make_pair(numRow(), numCol());
    }
    size_t size(size_t i) const {
        if (i) {
            return numCol();
        } else {
            return numRow();
        }
    }
    size_t length() const { return numRow() * numCol(); }

    T &operator()(size_t i, size_t j) {
        assert(i < numRow());
        assert(j < numCol());
        return getLinearElement(i * rowStride() + j * colStride());
    }
    const T &operator()(size_t i, size_t j) const {
        const size_t M = numRow();
        assert(i < M);
        assert(j < numCol());
        return getLinearElement(i * rowStride() + j * colStride());
    }

    template <typename C> bool operator==(const BaseMatrix<T, C> &B) const {
        const size_t M = numRow();
        const size_t N = numCol();
        if ((N != B.numCol()) || M != B.numRow()) {
            return false;
        }
        for (size_t i = 0; i < M * N; ++i) {
            if (getLinearElement(i) != B[i]) {
                return false;
            }
        }
        return true;
    }

    static constexpr size_t getConstCol() { return A::getConstCol(); }
    inline auto getRow(size_t i) {
        constexpr size_t N = getConstCol();
        if constexpr (N) {
            return PtrVector<T, N>{data() + i * N};
            // } else if constexpr (std::is_const_v<T>) {
            //     const size_t _N = numCol();
            //     return PtrVector<T, 0>(data() + i * rowStride(), _N);
        } else {
            const size_t _N = numCol();
            // return llvm::MutableArrayRef<T>(data() + i * rowStride(), _N);
            return PtrVector<T, 0>{data() + i * rowStride(), _N};
        }
    }
    inline auto getRow(size_t i) const {
        constexpr size_t N = getConstCol();
        if constexpr (N) {
            return PtrVector<const T, N>{data() + i * N};
        } else {
            const size_t _N = numCol();
            return PtrVector<const T, 0>{data() + i * rowStride(), _N};
            // return llvm::ArrayRef<T>(data() + i * rowStride(), _N);
            // return PtrVector<const T, 0>{
        }
    }
    void copyRow(llvm::ArrayRef<T> x, size_t i) {
        for (size_t j = 0; j < numCol(); ++j)
            (*this)(i, j) = x[j];
    }
    inline StridedVector<T> getCol(size_t n) {
        return StridedVector<T>{data() + n, numRow(), rowStride()};
    }
    inline StridedVector<const T> getCol(size_t n) const {
        return StridedVector<const T>{data() + n, numRow(), rowStride()};
    }


    A &operator=(SmallSparseMatrix<T> &B) {
        const size_t M = numRow();
        const size_t N = numCol();
        assert(M == B.numRow());
        assert(N == B.numCol());
        const size_t X = rowStride();
        T *mem = data();
        size_t k = 0;
        for (size_t i = 0; i < M; ++i) {
            uint32_t m = B.rows[i] & 0x00ffffff;
            size_t j = 0;
            while (m) {
                uint32_t tz = std::countr_zero(m);
                m >>= tz + 1;
                j += tz;
                mem[i * X + (j++)] = B.nonZeros[k++];
            }
        }
        assert(k == B.nonZeros.size());
        return *this;
    }

    inline PtrMatrix<T> operator()(Range<size_t, size_t> rows,
                                   Range<size_t, size_t> cols) {
        assert(rows.end >= rows.begin);
        assert(cols.end >= cols.begin);
        assert(rows.end <= numRow());
        assert(cols.end <= numCol());
        return PtrMatrix<T>{.mem =
                                data() + cols.begin + rows.begin * rowStride(),
                            .M = rows.end - rows.begin,
                            .N = cols.end - cols.begin,
                            .X = rowStride()};
    }
    template <typename R0, typename R1, typename C0, typename C1>
    inline PtrMatrix<T> operator()(Range<R0, R1> rows, Range<C0, C1> cols) {
        return (*this)(canonicalizeRange(rows, numRow()),
                       canonicalizeRange(cols, numCol()));
    }
    template <typename C0, typename C1>
    inline PtrMatrix<T> operator()(Colon, Range<C0, C1> cols) {
        return (*this)(std::make_pair(0, numRow()),
                       canonicalizeRange(cols, numCol()));
    }
    template <typename R0, typename R1>
    inline PtrMatrix<T> operator()(Range<R0, R1> rows, Colon) {
        return (*this)(canonicalizeRange(rows, numRow()),
                       std::make_pair(0, numCol()));
    }
    inline PtrMatrix<T> operator()(Colon, Colon) { return *this; }
    template <typename R0, typename R1>
    inline auto operator()(Range<R0, R1> rows, size_t col) {
        return getCol(col)(canonicalizeRange(rows, numRow()));
    }
    template <typename C0, typename C1>
    inline auto operator()(size_t row, Range<C0, C1> cols) {
        return getRow(row)(canonicalizeRange(cols, numCol()));
    }
    inline auto operator()(Colon, size_t col) { return getCol(col); }
    inline auto operator()(size_t row, Colon) { return getRow(row); }

    inline PtrMatrix<const T> operator()(Range<size_t, size_t> rows,
                                         Range<size_t, size_t> cols) const {
        assert(rows.end >= rows.begin);
        assert(cols.end >= cols.begin);
        assert(rows.end <= numRow());
        assert(cols.end <= numCol());
        return PtrMatrix<const T>{.mem = data() + cols.begin +
                                         rows.begin * rowStride(),
                                  .M = rows.end - rows.begin,
                                  .N = cols.end - cols.begin,
                                  .X = rowStride()};
    }
    template <typename R0, typename R1, typename C0, typename C1>
    inline PtrMatrix<const T> operator()(Range<R0, R1> rows,
                                         Range<C0, C1> cols) const {
        return (*this)(canonicalizeRange(rows, numRow()),
                       canonicalizeRange(cols, numCol()));
    }
    template <typename C0, typename C1>
    inline PtrMatrix<const T> operator()(Colon, Range<C0, C1> cols) const {
        return (*this)(std::make_pair(0, numRow()),
                       canonicalizeRange(cols, numCol()));
    }
    template <typename R0, typename R1>
    inline PtrMatrix<const T> operator()(Range<R0, R1> rows, Colon) const {
        return (*this)(canonicalizeRange(rows, numRow()),
                       std::make_pair(0, numCol()));
    }
    inline PtrMatrix<const T> operator()(Colon, Colon) const { return *this; }
    template <typename R0, typename R1>
    inline auto operator()(Range<R0, R1> rows, size_t col) const {
        return getCol(col)(canonicalizeRange(rows, numRow()));
    }
    template <typename C0, typename C1>
    inline auto operator()(size_t row, Range<C0, C1> cols) const {
        return getRow(row)(canonicalizeRange(cols, numCol()));
    }
    inline auto operator()(Colon, size_t col) const { return getCol(col); }
    inline auto operator()(size_t row, Colon) const { return getRow(row); }

    inline PtrMatrix<const T> view() const {
        return PtrMatrix<const T>{
            .mem = data(), .M = numRow(), .N = numCol(), .X = rowStride()};
    };

    PtrMatrix<T> operator=(const AbstractMatrix auto &B) {
        const size_t M = numRow();
        const size_t N = numCol();
        assert(M == B.numRow());
        assert(N == B.numCol());
        for (size_t r = 0; r < M; ++r)
            for (size_t c = 0; c < N; ++c)
                (*this)(r, c) = B(r, c);
        return *this;
    }
    operator PtrMatrix<T>() {
        return PtrMatrix<T>{
            .mem = data(), .M = numRow(), .N = numCol(), .X = rowStride()};
    }
    operator PtrMatrix<const T>() const {
        return PtrMatrix<const T>{
            .mem = data(), .M = numRow(), .N = numCol(), .X = rowStride()};
    }
};

//
// Matrix
//
template <typename T, size_t M = 0, size_t N = 0, size_t S = 64>
struct Matrix : BaseMatrix<T, Matrix<T, M, N, S>> {
    // static_assert(M * N == S,
    //               "if specifying non-zero M and N, we should have M*N == S");
    static constexpr bool fixedNumRow = M;
    static constexpr bool fixedNumCol = N;
    T mem[S];
    inline T &getLinearElement(size_t i) { return mem[i]; }
    inline const T &getLinearElement(size_t i) const { return mem[i]; }
    T *begin() { return mem; }
    T *end() { return begin() + M; }
    const T *begin() const { return mem; }
    const T *end() const { return begin() + M; }
    static constexpr size_t numRow() { return M; }
    static constexpr size_t numCol() { return N; }
    static constexpr size_t rowStride() { return N; }
    static constexpr size_t colStride() { return 1; }

    T *data() { return mem; }
    const T *data() const { return mem; }

    static constexpr size_t getConstCol() { return N; }
};

template <typename T, size_t M, size_t S>
struct Matrix<T, M, 0, S> : BaseMatrix<T, Matrix<T, M, 0, S>> {
    llvm::SmallVector<T, S> mem;
    size_t N, X;

    Matrix(size_t n) : mem(llvm::SmallVector<T, S>(M * n)), N(n), X(n){};

    inline T &getLinearElement(size_t i) { return mem[i]; }
    inline const T &getLinearElement(size_t i) const { return mem[i]; }
    auto begin() { return mem.begin(); }
    auto end() { return mem.end(); }
    auto begin() const { return mem.begin(); }
    auto end() const { return mem.end(); }
    size_t numRow() const { return M; }
    size_t numCol() const { return N; }
    inline size_t rowStride() const { return X; }
    static constexpr size_t colStride() { return 1; }

    static constexpr size_t getConstCol() { return 0; }

    T *data() { return mem.data(); }
    const T *data() const { return mem.data(); }
};
template <typename T, size_t N, size_t S>
struct Matrix<T, 0, N, S> : BaseMatrix<T, Matrix<T, 0, N, S>> {
    llvm::SmallVector<T, S> mem;
    size_t M;

    Matrix(size_t m) : mem(llvm::SmallVector<T, S>(m * N)), M(m){};

    inline T &getLinearElement(size_t i) { return mem[i]; }
    inline const T &getLinearElement(size_t i) const { return mem[i]; }
    auto begin() { return mem.begin(); }
    auto end() { return mem.end(); }
    auto begin() const { return mem.begin(); }
    auto end() const { return mem.end(); }

    inline size_t numRow() const { return M; }
    static constexpr size_t numCol() { return N; }
    static constexpr size_t rowStride() { return N; }
    static constexpr size_t colStride() { return 1; }
    static constexpr size_t getConstCol() { return N; }

    T *data() { return mem.data(); }
    const T *data() const { return mem.data(); }
};

template <typename T>
struct SquarePtrMatrix : BaseMatrix<T, SquarePtrMatrix<T>> {
    T *mem;
    const size_t M;
    SquarePtrMatrix(T *data, size_t M) : mem(data), M(M){};
    static constexpr bool fixedNumCol = true;
    static constexpr bool fixedNumRow = true;

    inline T &getLinearElement(size_t i) { return mem[i]; }
    inline const T &getLinearElement(size_t i) const { return mem[i]; }
    T *begin() { return mem; }
    T *end() { return mem + (M * M); }
    const T *begin() const { return mem; }
    const T *end() const { return mem + (M * M); }

    size_t numRow() const { return M; }
    size_t numCol() const { return M; }
    inline size_t rowStride() const { return M; }
    static constexpr size_t colStride() { return 1; }
    static constexpr size_t getConstCol() { return 0; }

    T *data() { return mem; }
    const T *data() const { return mem; }
    explicit operator PtrMatrix<const T>() const {
        return PtrMatrix<const T>(mem, M, M, M);
    }
    operator SquarePtrMatrix<const T>() const {
        return SquarePtrMatrix<const T>(mem, M);
    }
};

template <typename T, unsigned STORAGE = 4>
struct SquareMatrix : BaseMatrix<T, SquareMatrix<T, STORAGE>> {
    typedef T eltype;
    static constexpr unsigned TOTALSTORAGE = STORAGE * STORAGE;
    llvm::SmallVector<T, TOTALSTORAGE> mem;
    size_t M;
    static constexpr bool fixedNumCol = true;
    static constexpr bool fixedNumRow = true;

    SquareMatrix(size_t m)
        : mem(llvm::SmallVector<T, TOTALSTORAGE>(m * m)), M(m){};

    inline T &getLinearElement(size_t i) { return mem[i]; }
    inline const T &getLinearElement(size_t i) const { return mem[i]; }
    auto begin() { return mem.begin(); }
    auto end() { return mem.end(); }
    auto begin() const { return mem.begin(); }
    auto end() const { return mem.end(); }

    size_t numRow() const { return M; }
    size_t numCol() const { return M; }
    inline size_t rowStride() const { return M; }
    static constexpr size_t colStride() { return 1; }
    static constexpr size_t getConstCol() { return 0; }

    T *data() { return mem.data(); }
    const T *data() const { return mem.data(); }
    void copyRow(llvm::ArrayRef<T> a, size_t j) {
        for (size_t m = 0; m < M; ++m) {
            (*this)(j, m) = a[m];
        }
    }
    void copyCol(llvm::ArrayRef<T> a, size_t j) {
        for (size_t m = 0; m < M; ++m) {
            (*this)(m, j) = a[m];
        }
    }
    void copyCol(const SquareMatrix<T> &A, size_t j) {
        copyCol(A.getCol(j), j);
    }
    // returns the inverse, followed by bool where true means failure

    static SquareMatrix<T> identity(size_t N) {
        SquareMatrix<T> A(N);
        for (size_t r = 0; r < N; ++r)
            A(r, r) = 1;
        return A;
    }
    operator PtrMatrix<T>() {
        return PtrMatrix<T>{.mem = mem.data(), .M = M, .N = M, .X = M};
    }
    operator PtrMatrix<const T>() const {
        return PtrMatrix<const T>{.mem = mem.data(), .M = M, .N = M, .X = M};
    }
    operator SquarePtrMatrix<T>() {
        return SquarePtrMatrix(mem.data(), size_t(M));
    }
    PtrMatrix<T> view() {
        return PtrMatrix<T>{.mem = mem.data(), .M = M, .N = M, .X = M};
    }
    PtrMatrix<const T> view() const {
        return PtrMatrix<const T>{.mem = mem.data(), .M = M, .N = M, .X = M};
    }
    Transpose<PtrMatrix<const T>> transpose() const {
        return Transpose<PtrMatrix<const T>>{view()};
    }
};

static_assert(std::is_same_v<SquareMatrix<int64_t>::eltype, int64_t>);

template <typename T, size_t S>
struct Matrix<T, 0, 0, S> : BaseMatrix<T, Matrix<T, 0, 0, S>> {
    llvm::SmallVector<T, S> mem;

    size_t M, N, X;

    Matrix(llvm::SmallVector<T, S> content, size_t m, size_t n)
        : mem(std::move(content)), M(m), N(n), X(n){};

    Matrix(size_t m, size_t n)
        : mem(llvm::SmallVector<T, S>(m * n)), M(m), N(n), X(n){};

    Matrix() : M(0), N(0), X(0){};
    Matrix(SquareMatrix<T> &&A)
        : mem(std::move(A.mem)), M(A.M), N(A.M), X(A.M){};
    Matrix(const SquareMatrix<T> &A)
        : mem(A.mem.begin(), A.mem.end()), M(A.M), N(A.M), X(A.M){};
    Matrix(const AbstractMatrix auto &A)
        : mem(llvm::SmallVector<T>{}), M(A.numRow()), N(A.numCol()),
          X(A.numCol()) {
        mem.resize_for_overwrite(M * N);
        std::cout << "M = " << M << "; N = " << N << std::endl;
        for (size_t m = 0; m < M; ++m)
            for (size_t n = 0; n < N; ++n)
                mem[m * X + n] = A(m, n);
    }

    operator PtrMatrix<const T>() const {
        return PtrMatrix<const T>{.mem = mem.data(), .M = M, .N = N, .X = X};
    }
    operator PtrMatrix<T>() {
        return PtrMatrix<T>{.mem = mem.data(), .M = M, .N = N, .X = X};
    }

    inline T &getLinearElement(size_t i) { return mem[i]; }
    inline const T &getLinearElement(size_t i) const { return mem[i]; }
    auto begin() { return mem.begin(); }
    auto end() { return mem.end(); }
    auto begin() const { return mem.begin(); }
    auto end() const { return mem.end(); }

    size_t numRow() const { return M; }
    size_t numCol() const { return N; }
    inline size_t rowStride() const { return X; }
    static constexpr size_t colStride() { return 1; }
    static constexpr size_t getConstCol() { return 0; }

    T *data() { return mem.data(); }
    const T *data() const { return mem.data(); }

    static Matrix<T, 0, 0, S> uninitialized(size_t MM, size_t NN) {
        Matrix<T, 0, 0, S> A(0, 0);
        A.M = MM;
        A.X = A.N = NN;
        A.mem.resize_for_overwrite(MM * NN);
        return A;
    }
    static Matrix<T, 0, 0, S> identity(size_t MM) {
        Matrix<T, 0, 0, S> A(MM, MM);
        for (size_t i = 0; i < MM; ++i) {
            A(i, i) = 1;
        }
        return A;
    }
    void clear() {
        M = N = X = 0;
        mem.clear();
    }
    void resize(size_t MM, size_t NN, size_t XX) {
        mem.resize(MM * XX);
        if (NN > X) {
            for (size_t m = std::min(M, MM) - 1; m > 0; --m) {
                for (size_t n = N; n > 0;) {
                    --n;
                    mem[m * NN + n] = mem[m * X + n];
                }
            }
            for (size_t m = 1; m < std::min(M, MM); ++m) {
                for (size_t n = N; n < NN; ++n) {
                    mem[m * NN + n] = 0;
                }
            }
            X = NN;
        }
        M = MM;
        N = NN;
    }
    void resize(size_t MM, size_t NN) {
        size_t XX = NN > X ? NN : X;
        resize(MM, NN, XX);
    }
    void reserve(size_t MM, size_t NN) { mem.reserve(MM * std::max(X, NN)); }
    void resizeForOverwrite(size_t MM, size_t NN, size_t XX) {
        assert(XX >= NN);
        M = MM;
        N = NN;
        X = XX;
        if (M * X > mem.size())
            mem.resize_for_overwrite(M * X);
    }
    void resizeForOverwrite(size_t MM, size_t NN) {
        M = MM;
        X = N = NN;
        if (M * X > mem.size())
            mem.resize_for_overwrite(M * X);
    }

    void resizeRows(size_t MM) {
        if (MM > M) {
            mem.resize(MM * X);
        }
        M = MM;
    }
    void resizeRowsForOverwrite(size_t MM) {
        if (MM > M) {
            mem.resize_for_overwrite(M * X);
        }
        M = MM;
    }
    void resizeCols(size_t NN) { resize(M, NN); }
    void resizeColsForOverwrite(size_t NN) {
        if (NN > X) {
            X = NN;
            mem.resize_for_overwrite(M * X);
        }
        N = NN;
    }
    void eraseCol(size_t i) {
        assert(i < N);
        // TODO: optimize this to reduce copying
        for (size_t m = 0; m < M; ++m)
            for (size_t n = 0; n < N; ++n)
                mem.erase(mem.begin() + m * X + n);
        --N;
        --X;
    }
    void eraseRow(size_t i) {
        assert(i < M);
        auto it = mem.begin() + i * X;
        mem.erase(it, it + X);
        --M;
    }
    void truncateCols(size_t NN) {
        assert(NN <= N);
        N = NN;
    }
    void truncateRows(size_t MM) {
        assert(MM <= M);
        M = MM;
    }
    PtrMatrix<T> view() {
        return PtrMatrix<T>{.mem = mem.data(), .M = M, .N = N, .X = X};
    }
    PtrMatrix<const T> view() const {
        return PtrMatrix<const T>{.mem = mem.data(), .M = M, .N = N, .X = X};
    }
    Transpose<PtrMatrix<const T>> transpose() const {
        return Transpose<PtrMatrix<const T>>{view()};
    }
    bool operator==(const AbstractMatrix auto &B) const {
        const size_t M = B.numRow();
        const size_t N = B.numCol();
        if ((M != numRow()) || (N != numCol()))
            return false;
        for (size_t r = 0; r < M; ++r)
            for (size_t c = 0; c < N; ++c)
                if ((*this)(r, c) != B(r, c))
                    return false;
        return true;
    }

    bool operator==(const Matrix<T, 0, 0, S> &B) const {
        const size_t M = B.numRow();
        const size_t N = B.numCol();
        if ((M != numRow()) || (N != numCol()))
            return false;
        for (size_t r = 0; r < M; ++r)
            for (size_t c = 0; c < N; ++c)
                if ((*this)(r, c) != B(r, c))
                    return false;
        return true;
    }

    // Matrix<T, 0, 0, S> A(Matrix<T, 0, 0, S>::Uninitialized(N, M));
    // for (size_t n = 0; n < N; ++n)
    //     for (size_t m = 0; m < M; ++m)
    //         A(n, m) = (*this)(m, n);
    // return A;
    // }

    // inline PtrMatrix<T> view(size_t rowStart, size_t rowEnd, size_t colStart,
    //                          size_t colEnd) {
    //     assert(rowEnd > rowStart);
    //     assert(colEnd > colStart);
    //     return PtrMatrix<T>{.mem = mem.data() + colStart + rowStart * X,
    //                         .M = rowEnd - rowStart,
    //                         .N = colEnd - colStart,
    //                         .X = X};
    // }
    // inline PtrMatrix<T> view(size_t rowEnd, size_t colEnd) {
    //     return view(0, rowEnd, 0, colEnd);
    // }
    // inline PtrMatrix<const T> view(size_t rowStart, size_t rowEnd,
    //                                size_t colStart, size_t colEnd) const {
    //     assert(rowEnd > rowStart);
    //     assert(colEnd > colStart);
    //     return PtrMatrix<const T>{.mem = mem.data() + colStart + rowStart *
    //     X,
    //                               .M = rowEnd - rowStart,
    //                               .N = colEnd - colStart,
    //                               .X = X};
    // }
    // inline PtrMatrix<T> view(size_t rowEnd, size_t colEnd) const {
    //     return view(0, rowEnd, 0, colEnd);
    // }

    // PtrMatrix<T> operator=(PtrMatrix<T> A) {
    //     assert(M == A.numRow());
    //     assert(N == A.numCol());
    //     for (size_t m = 0; m < M; ++m)
    //         for (size_t n = 0; n < N; ++n)
    //             mem[n + m * X] = A(m, n);
    //     return *this;
    // }
};
template <typename T> using DynamicMatrix = Matrix<T, 0, 0, 64>;
static_assert(std::same_as<DynamicMatrix<int64_t>, Matrix<int64_t>>,
              "DynamicMatrix should be identical to Matrix");
typedef DynamicMatrix<int64_t> IntMatrix;
static_assert(std::copyable<Matrix<int64_t, 4, 4>>);
static_assert(std::copyable<Matrix<int64_t, 4, 0>>);
static_assert(std::copyable<Matrix<int64_t, 0, 4>>);
static_assert(std::copyable<Matrix<int64_t, 0, 0>>);
static_assert(std::copyable<SquareMatrix<int64_t>>);

template <typename T, typename P>
std::pair<size_t, size_t> size(BaseMatrix<T, P> const &A) {
    return std::make_pair(A.numRow(), A.numCol());
}

template <typename T>
std::ostream &printVector(std::ostream &os, PtrVector<const T> a) {
    os << "[ ";
    if (size_t M = a.size()) {
        os << a[0];
        for (size_t m = 1; m < M; m++) {
            os << ", " << a[m];
        }
    }
    os << " ]";
    return os;
}
template <typename T>
std::ostream &printVector(std::ostream &os, const llvm::SmallVectorImpl<T> &a) {
    return printVector(os, PtrVector<const T>{a.data(), a.size()});
}

template <typename T>
std::ostream &operator<<(std::ostream &os, PtrVector<const T> const &A) {
    return printVector(os, A);
}
template <typename T>
std::ostream &operator<<(std::ostream &os, Vector<T> const &A) {
    return printVector(os, A.view());
}
// template <typename T>
// std::ostream &operator<<(std::ostream &os, SquareMatrix<T> const &A) {
//     return printMatrix(os, A);
// }

bool allMatch(const AbstractVector auto &x0, const AbstractVector auto &x1) {
    size_t N = x0.size();
    if (N != x1.size())
        return false;
    for (size_t n = 0; n < N; ++n)
        if (x0(n) != x1(n))
            return false;
    return true;
}

MULTIVERSION void matmul(PtrMatrix<int64_t> C, PtrMatrix<const int64_t> A,
                         PtrMatrix<const int64_t> B) {
    unsigned M = A.numRow();
    unsigned K = A.numCol();
    unsigned N = B.numCol();
    assert(K == B.numRow());
    assert(M == C.numRow());
    assert(N == C.numCol());
    for (size_t m = 0; m < M; ++m) {
        for (size_t k = 0; k < K; ++k) {
            VECTORIZE
            for (size_t n = 0; n < N; ++n) {
                C(m, n) += A(m, k) * B(k, n);
            }
        }
    }
}
MULTIVERSION IntMatrix matmul(PtrMatrix<const int64_t> A,
                              PtrMatrix<const int64_t> B) {
    unsigned M = A.numRow();
    unsigned N = B.numCol();
    IntMatrix C(M, N);
    matmul(C, A, B);
    return C;
}
MULTIVERSION void matmulnt(PtrMatrix<int64_t> C, PtrMatrix<const int64_t> A,
                           PtrMatrix<const int64_t> B) {
    unsigned M = A.numRow();
    unsigned K = A.numCol();
    unsigned N = B.numRow();
    assert(K == B.numCol());
    assert(M == C.numRow());
    assert(N == C.numCol());
    for (size_t m = 0; m < M; ++m) {
        for (size_t k = 0; k < K; ++k) {
            VECTORIZE
            for (size_t n = 0; n < N; ++n) {
                C(m, n) += A(m, k) * B(n, k);
            }
        }
    }
}
MULTIVERSION IntMatrix matmulnt(PtrMatrix<const int64_t> A,
                                PtrMatrix<const int64_t> B) {
    unsigned M = A.numRow();
    unsigned N = B.numRow();
    IntMatrix C(M, N);
    matmulnt(C, A, B);
    return C;
}
MULTIVERSION void matmultn(PtrMatrix<int64_t> C, PtrMatrix<const int64_t> A,
                           PtrMatrix<const int64_t> B) {
    unsigned M = A.numCol();
    unsigned K = A.numRow();
    unsigned N = B.numCol();
    assert(K == B.numRow());
    assert(M == C.numRow());
    assert(N == C.numCol());
    for (size_t m = 0; m < M; ++m) {
        for (size_t k = 0; k < K; ++k) {
            VECTORIZE
            for (size_t n = 0; n < N; ++n) {
                C(m, n) += A(k, m) * B(k, n);
            }
        }
    }
}
MULTIVERSION IntMatrix matmultn(PtrMatrix<const int64_t> A,
                                PtrMatrix<const int64_t> B) {
    unsigned M = A.numCol();
    unsigned N = B.numCol();
    IntMatrix C(M, N);
    matmultn(C, A, B);
    return C;
}
MULTIVERSION void matmultt(PtrMatrix<int64_t> C, PtrMatrix<const int64_t> A,
                           PtrMatrix<const int64_t> B) {
    unsigned M = A.numCol();
    unsigned K = A.numRow();
    unsigned N = B.numRow();
    assert(K == B.numCol());
    assert(M == C.numRow());
    assert(N == C.numCol());
    for (size_t m = 0; m < M; ++m) {
        for (size_t k = 0; k < K; ++k) {
            VECTORIZE
            for (size_t n = 0; n < N; ++n) {
                C(m, n) += A(k, m) * B(n, k);
            }
        }
    }
}
MULTIVERSION IntMatrix matmultt(PtrMatrix<const int64_t> A,
                                PtrMatrix<const int64_t> B) {
    unsigned M = A.numCol();
    unsigned N = B.numRow();
    IntMatrix C(M, N);
    matmultt(C, A, B);
    return C;
}

MULTIVERSION inline void swapRows(PtrMatrix<int64_t> A, size_t i, size_t j) {
    if (i == j)
        return;
    const unsigned int M = A.numRow();
    const unsigned int N = A.numCol();
    assert((i < M) & (j < M));
    VECTORIZE
    for (size_t n = 0; n < N; ++n) {
        std::swap(A(i, n), A(j, n));
    }
}
MULTIVERSION inline void swapCols(PtrMatrix<int64_t> A, size_t i, size_t j) {
    if (i == j) {
        return;
    }
    const unsigned int M = A.numRow();
    const unsigned int N = A.numCol();
    assert((i < N) & (j < N));
    VECTORIZE
    for (size_t m = 0; m < M; ++m) {
        std::swap(A(m, i), A(m, j));
    }
}
template <typename T>
void swapCols(llvm::SmallVectorImpl<T> &A, size_t i, size_t j) {
    std::swap(A[i], A[j]);
}
template <typename T>
void swapRows(llvm::SmallVectorImpl<T> &A, size_t i, size_t j) {
    std::swap(A[i], A[j]);
}

template <int Bits, class T>
constexpr bool is_uint_v = sizeof(T) == (Bits / 8) && std::is_integral_v<T> &&
                           !std::is_signed_v<T>;

template <class T> inline T zeroUpper(T x) requires is_uint_v<16, T> {
    return x & 0x00ff;
}
template <class T> inline T zeroLower(T x) requires is_uint_v<16, T> {
    return x & 0xff00;
}
template <class T> inline T upperHalf(T x) requires is_uint_v<16, T> {
    return x >> 8;
}

template <class T> inline T zeroUpper(T x) requires is_uint_v<32, T> {
    return x & 0x0000ffff;
}
template <class T> inline T zeroLower(T x) requires is_uint_v<32, T> {
    return x & 0xffff0000;
}
template <class T> inline T upperHalf(T x) requires is_uint_v<32, T> {
    return x >> 16;
}
template <class T> inline T zeroUpper(T x) requires is_uint_v<64, T> {
    return x & 0x00000000ffffffff;
}
template <class T> inline T zeroLower(T x) requires is_uint_v<64, T> {
    return x & 0xffffffff00000000;
}
template <class T> inline T upperHalf(T x) requires is_uint_v<64, T> {
    return x >> 32;
}

template <typename T> std::pair<size_t, T> findMax(llvm::ArrayRef<T> x) {
    size_t i = 0;
    T max = std::numeric_limits<T>::min();
    for (size_t j = 0; j < x.size(); ++j) {
        T xj = x[j];
        if (max < xj) {
            max = xj;
            i = j;
        }
    }
    return std::make_pair(i, max);
}

template <class T, int Bits>
concept is_int_v = std::signed_integral<T> && sizeof(T) == (Bits / 8);

template <is_int_v<64> T> inline __int128_t widen(T x) { return x; }
template <is_int_v<32> T> inline int64_t splitInt(T x) { return x; }

template <typename T>
concept TriviallyCopyable = std::is_trivially_copyable_v<T>;

template <typename T>
concept TriviallyCopyableVectorOrScalar =
    std::is_trivially_copyable_v<T> && VectorOrScalar<T>;
template <typename T>
concept TriviallyCopyableMatrixOrScalar =
    std::is_trivially_copyable_v<T> && MatrixOrScalar<T>;

static_assert(TriviallyCopyableMatrixOrScalar<PtrMatrix<const long>>);
static_assert(TriviallyCopyableMatrixOrScalar<int>);
static_assert(TriviallyCopyable<Mul>);
static_assert(TriviallyCopyableMatrixOrScalar<
              ElementwiseMatrixBinaryOp<Mul, PtrMatrix<const long>, int>>);
static_assert(TriviallyCopyableMatrixOrScalar<
              MatMatMul<PtrMatrix<const long>, PtrMatrix<const long>>>);

template <TriviallyCopyable OP, TriviallyCopyableVectorOrScalar A,
          TriviallyCopyableVectorOrScalar B>
inline auto _binaryOp(OP op, A a, B b) {
    return ElementwiseVectorBinaryOp<OP, A, B>{.op = op, .a = a, .b = b};
}
template <TriviallyCopyable OP, TriviallyCopyableMatrixOrScalar A,
          TriviallyCopyableMatrixOrScalar B>
inline auto _binaryOp(OP op, A a, B b) {
    return ElementwiseMatrixBinaryOp<OP, A, B>{.op = op, .a = a, .b = b};
}

// template <TriviallyCopyable OP, TriviallyCopyable A, TriviallyCopyable B>
// inline auto binaryOp(const OP op, const A a, const B b) {
//     return _binaryOp(op, a, b);
// }
// template <TriviallyCopyable OP, typename A, TriviallyCopyable B>
// inline auto binaryOp(const OP op, const A &a, const B b) {
//     return _binaryOp(op, a.view(), b);
// }
// template <TriviallyCopyable OP, TriviallyCopyable A, typename B>
// inline auto binaryOp(const OP op, const A a, const B &b) {
//     return _binaryOp(op, a, b.view());
// }
template <TriviallyCopyable OP, typename A, typename B>
inline auto binaryOp(const OP op, const A &a, const B &b) {
    if constexpr (std::is_trivially_copyable_v<A>) {
        if constexpr (std::is_trivially_copyable_v<B>) {
            return _binaryOp(op, a, b);
        } else {
            return _binaryOp(op, a, b.view());
        }
    } else if constexpr (std::is_trivially_copyable_v<B>) {
        return _binaryOp(op, a.view(), b);
    } else {
        return _binaryOp(op, a.view(), b.view());
    }
}

inline auto bin2(std::integral auto x) { return (x * (x - 1)) >> 1; }

struct Rational {
    int64_t numerator;
    int64_t denominator;

    Rational() : numerator(0), denominator(1){};
    Rational(int64_t coef) : numerator(coef), denominator(1){};
    Rational(int coef) : numerator(coef), denominator(1){};
    Rational(int64_t n, int64_t d)
        : numerator(d > 0 ? n : -n), denominator(n ? (d > 0 ? d : -d) : 1) {}
    static Rational create(int64_t n, int64_t d) {
        if (n) {
            int64_t sign = 2 * (d > 0) - 1;
            int64_t g = gcd(n, d);
            n *= sign;
            d *= sign;
            if (g != 1) {
                n /= g;
                d /= g;
            }
            return Rational{n, d};
        } else {
            return Rational{0, 1};
        }
    }
    static Rational createPositiveDenominator(int64_t n, int64_t d) {
        if (n) {
            int64_t g = gcd(n, d);
            if (g != 1) {
                n /= g;
                d /= g;
            }
            return Rational{n, d};
        } else {
            return Rational{0, 1};
        }
    }
    llvm::Optional<Rational> operator+(Rational y) const {
        auto [xd, yd] = divgcd(denominator, y.denominator);
        int64_t a, b, n, d;
        bool o1 = __builtin_mul_overflow(numerator, yd, &a);
        bool o2 = __builtin_mul_overflow(y.numerator, xd, &b);
        bool o3 = __builtin_mul_overflow(denominator, yd, &d);
        bool o4 = __builtin_add_overflow(a, b, &n);
        if ((o1 | o2) | (o3 | o4)) {
            return llvm::Optional<Rational>();
        } else if (n) {
            auto [nn, nd] = divgcd(n, d);
            return Rational{nn, nd};
        } else {
            return Rational{0, 1};
        }
    }
    Rational &operator+=(Rational y) {
        llvm::Optional<Rational> a = *this + y;
        assert(a.hasValue());
        *this = a.getValue();
        return *this;
    }
    llvm::Optional<Rational> operator-(Rational y) const {
        auto [xd, yd] = divgcd(denominator, y.denominator);
        int64_t a, b, n, d;
        bool o1 = __builtin_mul_overflow(numerator, yd, &a);
        bool o2 = __builtin_mul_overflow(y.numerator, xd, &b);
        bool o3 = __builtin_mul_overflow(denominator, yd, &d);
        bool o4 = __builtin_sub_overflow(a, b, &n);
        if ((o1 | o2) | (o3 | o4)) {
            return llvm::Optional<Rational>();
        } else if (n) {
            auto [nn, nd] = divgcd(n, d);
            return Rational{nn, nd};
        } else {
            return Rational{0, 1};
        }
    }
    Rational &operator-=(Rational y) {
        llvm::Optional<Rational> a = *this - y;
        assert(a.hasValue());
        *this = a.getValue();
        return *this;
    }
    llvm::Optional<Rational> operator*(int64_t y) const {
        auto [xd, yn] = divgcd(denominator, y);
        int64_t n;
        if (__builtin_mul_overflow(numerator, yn, &n)) {
            return llvm::Optional<Rational>();
        } else {
            return Rational{n, xd};
        }
    }
    llvm::Optional<Rational> operator*(Rational y) const {
        if ((numerator != 0) & (y.numerator != 0)) {
            auto [xn, yd] = divgcd(numerator, y.denominator);
            auto [xd, yn] = divgcd(denominator, y.numerator);
            int64_t n, d;
            bool o1 = __builtin_mul_overflow(xn, yn, &n);
            bool o2 = __builtin_mul_overflow(xd, yd, &d);
            if (o1 | o2) {
                return llvm::Optional<Rational>();
            } else {
                return Rational{n, d};
            }
        } else {
            return Rational{0, 1};
        }
    }
    Rational &operator*=(Rational y) {
        if ((numerator != 0) & (y.numerator != 0)) {
            auto [xn, yd] = divgcd(numerator, y.denominator);
            auto [xd, yn] = divgcd(denominator, y.numerator);
            numerator = xn * yn;
            denominator = xd * yd;
        } else {
            numerator = 0;
            denominator = 1;
        }
        return *this;
    }
    Rational inv() const {
        if (numerator < 0) {
            // make sure we don't have overflow
            assert(denominator != std::numeric_limits<int64_t>::min());
            return Rational{-denominator, -numerator};
        } else {
            return Rational{denominator, numerator};
        }
        // return Rational{denominator, numerator};
        // bool positive = numerator > 0;
        // return Rational{positive ? denominator : -denominator,
        //                 positive ? numerator : -numerator};
    }
    llvm::Optional<Rational> operator/(Rational y) const {
        return (*this) * y.inv();
    }
    // *this -= a*b
    bool fnmadd(Rational a, Rational b) {
        if (llvm::Optional<Rational> ab = a * b) {
            if (llvm::Optional<Rational> c = *this - ab.getValue()) {
                *this = c.getValue();
                return false;
            }
        }
        return true;
    }
    bool div(Rational a) {
        if (llvm::Optional<Rational> d = *this / a) {
            *this = d.getValue();
            return false;
        }
        return true;
    }
    // Rational operator/=(Rational y) { return (*this) *= y.inv(); }
    operator double() { return numerator / denominator; }

    bool operator==(Rational y) const {
        return (numerator == y.numerator) & (denominator == y.denominator);
    }
    bool operator!=(Rational y) const {
        return (numerator != y.numerator) | (denominator != y.denominator);
    }
    bool isEqual(int64_t y) const {
        if (denominator == 1)
            return (numerator == y);
        else if (denominator == -1)
            return (numerator == -y);
        else
            return false;
    }
    bool operator==(int y) const { return isEqual(y); }
    bool operator==(int64_t y) const { return isEqual(y); }
    bool operator!=(int y) const { return !isEqual(y); }
    bool operator!=(int64_t y) const { return !isEqual(y); }
    bool operator<(Rational y) const {
        return (widen(numerator) * widen(y.denominator)) <
               (widen(y.numerator) * widen(denominator));
    }
    bool operator<=(Rational y) const {
        return (widen(numerator) * widen(y.denominator)) <=
               (widen(y.numerator) * widen(denominator));
    }
    bool operator>(Rational y) const {
        return (widen(numerator) * widen(y.denominator)) >
               (widen(y.numerator) * widen(denominator));
    }
    bool operator>=(Rational y) const {
        return (widen(numerator) * widen(y.denominator)) >=
               (widen(y.numerator) * widen(denominator));
    }
    bool operator>=(int y) const { return *this >= Rational(y); }

    friend bool isZero(Rational x) { return x.numerator == 0; }
    friend bool isOne(Rational x) { return (x.numerator == x.denominator); }
    bool isInteger() const { return denominator == 1; }
    void negate() { numerator = -numerator; }
    operator bool() const { return numerator != 0; }

    friend std::ostream &operator<<(std::ostream &os, const Rational &x) {
        os << x.numerator;
        if (x.denominator != 1) {
            os << " // " << x.denominator;
        }
        return os;
    }
    void dump() const { std::cout << *this << std::endl; }

    template <AbstractMatrix B> inline auto operator+(B &&b) {
        return binaryOp(Add{}, *this, std::forward<B>(b));
    }
    template <AbstractVector B> inline auto operator+(B &&b) {
        return binaryOp(Add{}, *this, std::forward<B>(b));
    }
    template <AbstractMatrix B> inline auto operator-(B &&b) {
        return binaryOp(Sub{}, *this, std::forward<B>(b));
    }
    template <AbstractVector B> inline auto operator-(B &&b) {
        return binaryOp(Sub{}, *this, std::forward<B>(b));
    }
    template <AbstractMatrix B> inline auto operator/(B &&b) {
        return binaryOp(Div{}, *this, std::forward<B>(b));
    }
    template <AbstractVector B> inline auto operator/(B &&b) {
        return binaryOp(Div{}, *this, std::forward<B>(b));
    }

    template <AbstractVector B> inline auto operator*(B &&b) {
        return binaryOp(Mul{}, *this, std::forward<B>(b));
    }
    template <AbstractMatrix B> inline auto operator*(B &&b) {
        return binaryOp(Mul{}, *this, std::forward<B>(b));
    }
};
llvm::Optional<Rational> gcd(Rational x, Rational y) {
    return Rational{gcd(x.numerator, y.numerator),
                    lcm(x.denominator, y.denominator)};
}
template <> struct GetEltype<Rational> { using eltype = Rational; };
template <> struct PromoteType<Rational, Rational> { using eltype = Rational; };
template <std::integral I> struct PromoteType<I, Rational> {
    using eltype = Rational;
};
template <std::integral I> struct PromoteType<Rational, I> {
    using eltype = Rational;
};

static void normalizeByGCD(PtrVector<int64_t> x) {
    if (size_t N = x.size()) {
        if (N == 1) {
            x[0] = 1;
            return;
        }
        int64_t g = gcd(x[0], x[1]);
        for (size_t n = 2; (n < N) & (g != 1); ++n)
            g = gcd(g, x[n]);
        if (g > 1)
            for (auto &&a : x)
                a /= g;
    }
}

template <typename T>
std::ostream &printMatrix(std::ostream &os, PtrMatrix<const T> A) {
    // std::ostream &printMatrix(std::ostream &os, T const &A) {
    auto [m, n] = A.size();
    for (size_t i = 0; i < m; i++) {
        if (i) {
            os << "  ";
        } else {
            os << "[ ";
        }
        for (int64_t j = 0; j < int64_t(n) - 1; j++) {
            auto Aij = A(i, j);
            if (Aij >= 0) {
                os << " ";
            }
            os << Aij << " ";
        }
        if (n) {
            auto Aij = A(i, n - 1);
            if (Aij >= 0) {
                os << " ";
            }
            os << Aij;
        }
        if (i != m - 1) {
            os << std::endl;
        }
    }
    os << " ]";
    return os;
}

template <typename T> struct SmallSparseMatrix {
    // non-zeros
    llvm::SmallVector<T> nonZeros;
    // masks, the upper 8 bits give the number of elements in previous rows
    // the remaining 24 bits are a mask indicating non-zeros within this row
    static constexpr size_t maxElemPerRow = 24;
    llvm::SmallVector<uint32_t> rows;
    size_t col;
    size_t numRow() const { return rows.size(); }
    size_t numCol() const { return col; }
    SmallSparseMatrix(size_t numRows, size_t numCols)
        : nonZeros{}, rows{llvm::SmallVector<uint32_t>(numRows)}, col{numCols} {
        assert(col <= maxElemPerRow);
    }
    T get(size_t i, size_t j) const {
        assert(j < col);
        uint32_t r(rows[i]);
        uint32_t jshift = uint32_t(1) << j;
        if (r & (jshift)) {
            // offset from previous rows
            uint32_t prevRowOffset = r >> maxElemPerRow;
            uint32_t rowOffset = std::popcount(r & (jshift - 1));
            return nonZeros[rowOffset + prevRowOffset];
        } else {
            return 0;
        }
    }
    inline T operator()(size_t i, size_t j) const { return get(i, j); }
    void insert(T x, size_t i, size_t j) {
        assert(j < col);
        uint32_t r{rows[i]};
        uint32_t jshift = uint32_t(1) << j;
        // offset from previous rows
        uint32_t prevRowOffset = r >> maxElemPerRow;
        uint32_t rowOffset = std::popcount(r & (jshift - 1));
        size_t k = rowOffset + prevRowOffset;
        if (r & jshift) {
            nonZeros[k] = std::move(x);
        } else {
            nonZeros.insert(nonZeros.begin() + k, std::move(x));
            rows[i] = r | jshift;
            for (size_t k = i + 1; k < rows.size(); ++k)
                rows[k] += uint32_t(1) << maxElemPerRow;
        }
    }

    struct Reference {
        SmallSparseMatrix<T> *A;
        size_t i, j;
        operator T() const { return A->get(i, j); }
        void operator=(T x) {
            A->insert(std::move(x), i, j);
            return;
        }
    };
    Reference operator()(size_t i, size_t j) { return Reference{this, i, j}; }
    operator DynamicMatrix<T>() {
        DynamicMatrix<T> A(numRow(), numCol());
        size_t k = 0;
        for (size_t i = 0; i < numRow(); ++i) {
            uint32_t m = rows[i] & 0x00ffffff;
            size_t j = 0;
            while (m) {
                uint32_t tz = std::countr_zero(m);
                m >>= tz + 1;
                j += tz;
                A(i, j++) = nonZeros[k++];
            }
        }
        assert(k == nonZeros.size());
        return A;
    }
};

template <typename T>
std::ostream &operator<<(std::ostream &os, SmallSparseMatrix<T> const &A) {
    size_t k = 0;
    os << "[ ";
    for (size_t i = 0; i < A.numRow(); ++i) {
        if (i)
            os << "  ";
        uint32_t m = A.rows[i] & 0x00ffffff;
        size_t j = 0;
        while (m) {
            if (j)
                os << " ";
            uint32_t tz = std::countr_zero(m);
            m >>= (tz + 1);
            j += (tz + 1);
            while (tz--)
                os << " 0 ";
            const T &x = A.nonZeros[k++];
            if (x >= 0)
                os << " ";
            os << x;
        }
        for (; j < A.numCol(); ++j)
            os << "  0";
        os << "\n";
    }
    os << " ]";
    assert(k == A.nonZeros.size());
}

std::ostream &operator<<(std::ostream &os, PtrMatrix<const int64_t> A) {
    // std::ostream &operator<<(std::ostream &os, Matrix<T, M, N> const &A)
    // {
    return printMatrix(os, A);
}
template <typename T, typename A>
std::ostream &operator<<(std::ostream &os, const BaseMatrix<T, A> &B) {
    // std::ostream &operator<<(std::ostream &os, Matrix<T, M, N> const &A)
    // {
    return printMatrix(os, PtrMatrix<T>(B));
}
template <AbstractMatrix T> std::ostream &operator<<(std::ostream &os, const T &A) {
    // std::ostream &operator<<(std::ostream &os, Matrix<T, M, N> const &A)
    // {
    Matrix<std::remove_const_t<typename T::eltype>> B{A};
    return printMatrix(os, PtrMatrix<const typename T::eltype>(B));
}
// template <typename T>
// std::ostream &operator<<(std::ostream &os, PtrMatrix<const T> &A) {
//     // std::ostream &operator<<(std::ostream &os, Matrix<T, M, N> const &A)
//     // {
//     return printMatrix(os, A);
// }

inline auto operator-(const AbstractVector auto &a) {
    auto AA{a.view()};
    return ElementwiseUnaryOp<Mul, decltype(AA)>{.op = Mul{}, .a = AA};
}
inline auto operator-(const AbstractMatrix auto &a) {
    auto AA{a.view()};
    return ElementwiseUnaryOp<Mul, decltype(AA)>{.op = Mul{}, .a = AA};
}

template <AbstractMatrix A, typename B> inline auto operator+(A &&a, B &&b) {
    return binaryOp(Add{}, std::forward<A>(a), std::forward<B>(b));
}
template <AbstractVector A, typename B> inline auto operator+(A &&a, B &&b) {
    return binaryOp(Add{}, std::forward<A>(a), std::forward<B>(b));
}
template <AbstractMatrix B> inline auto operator+(std::integral auto a, B &&b) {
    return binaryOp(Add{}, a, std::forward<B>(b));
}
template <AbstractVector B> inline auto operator+(std::integral auto a, B &&b) {
    return binaryOp(Add{}, a, std::forward<B>(b));
}

template <AbstractMatrix A, typename B> inline auto operator-(A &&a, B &&b) {
    return binaryOp(Sub{}, std::forward<A>(a), std::forward<B>(b));
}
template <AbstractVector A, typename B> inline auto operator-(A &&a, B &&b) {
    return binaryOp(Sub{}, std::forward<A>(a), std::forward<B>(b));
}
template <AbstractMatrix B> inline auto operator-(std::integral auto a, B &&b) {
    return binaryOp(Sub{}, a, std::forward<B>(b));
}
template <AbstractVector B> inline auto operator-(std::integral auto a, B &&b) {
    return binaryOp(Sub{}, a, std::forward<B>(b));
}

template <AbstractMatrix A, typename B> inline auto operator/(A &&a, B &&b) {
    return binaryOp(Div{}, std::forward<A>(a), std::forward<B>(b));
}
template <AbstractVector A, typename B> inline auto operator/(A &&a, B &&b) {
    return binaryOp(Div{}, std::forward<A>(a), std::forward<B>(b));
}
template <AbstractMatrix B> inline auto operator/(std::integral auto a, B &&b) {
    return binaryOp(Div{}, a, std::forward<B>(b));
}
template <AbstractVector B> inline auto operator/(std::integral auto a, B &&b) {
    return binaryOp(Div{}, a, std::forward<B>(b));
}
inline auto operator*(const AbstractMatrix auto &a,
                      const AbstractMatrix auto &b) {
    auto AA{a.view()};
    auto BB{b.view()};
    std::cout << "a.numRow() = " << a.numRow()
              << "; AA.numRow() = " << AA.numRow() << std::endl;
    std::cout << "b.numRow() = " << b.numRow()
              << "; BB.numRow() = " << BB.numRow() << std::endl;
    std::cout << "a.numCol() = " << a.numCol()
              << "; AA.numCol() = " << AA.numCol() << std::endl;
    std::cout << "b.numCol() = " << b.numCol()
              << "; BB.numCol() = " << BB.numCol() << std::endl;
    std::cout << "a = \n"
              << a << "\nAA = \n"
              << AA << "\nb =\n"
              << b << "\nBB =\n"
              << BB << std::endl;
    assert(AA.numCol() == BB.numRow());
    return MatMatMul<decltype(AA), decltype(BB)>{.a = AA, .b = BB};
}
inline auto operator*(const AbstractMatrix auto &a,
                      const AbstractVector auto &b) {
    auto AA{a.view()};
    auto BB{b.view()};
    assert(AA.numCol() == BB.size());
    return MatVecMul<decltype(AA), decltype(BB)>{.a = AA, .b = BB};
}
template <AbstractMatrix A> inline auto operator*(A &&a, std::integral auto b) {
    return binaryOp(Mul{}, std::forward<A>(a), b);
}
// template <AbstractMatrix A> inline auto operator*(A &&a, Rational b) {
//     return binaryOp(Mul{}, std::forward<A>(a), b);
// }
template <AbstractVector A, AbstractVector B>
inline auto operator*(A &&a, B &&b) {
    return binaryOp(Mul{}, std::forward<A>(a), std::forward<B>(b));
}
template <AbstractVector A> inline auto operator*(A &&a, std::integral auto b) {
    return binaryOp(Mul{}, std::forward<A>(a), b);
}
// template <AbstractVector A> inline auto operator*(A &&a, Rational b) {
//     return binaryOp(Mul{}, std::forward<A>(a), b);
// }
template <AbstractMatrix B> inline auto operator*(std::integral auto a, B &&b) {
    return binaryOp(Mul{}, a, std::forward<B>(b));
}
template <AbstractVector B> inline auto operator*(std::integral auto a, B &&b) {
    return binaryOp(Mul{}, a, std::forward<B>(b));
}

// inline auto operator*(AbstractMatrix auto &A, AbstractVector auto &x) {
//     auto AA{A.view()};
//     auto xx{x.view()};
//     return MatMul<decltype(AA), decltype(xx)>{.a = AA, .b = xx};
// }

template <AbstractVector V>
inline auto operator*(const Transpose<V> &a, const AbstractVector auto &b) {
    typename V::eltype s = 0;
    for (size_t i = 0; i < b.size(); ++i)
        s += a.a(i) * b(i);
    return s;
}

static_assert(AbstractVector<Vector<int64_t>>);
static_assert(AbstractVector<const Vector<int64_t>>);
static_assert(AbstractVector<Vector<int64_t> &>);
static_assert(AbstractMatrix<IntMatrix>);
static_assert(AbstractMatrix<IntMatrix &>);
