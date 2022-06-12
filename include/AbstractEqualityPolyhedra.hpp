#pragma once

#include "./Polyhedra.hpp"

template <class P, typename T>
struct AbstractEqualityPolyhedra : public AbstractPolyhedra<P, T> {
    Matrix<int64_t, 0, 0, 0> E;
    llvm::SmallVector<T, 8> q;

    using AbstractPolyhedra<P, T>::A;
    using AbstractPolyhedra<P, T>::b;
    using AbstractPolyhedra<P, T>::removeVariable;
    using AbstractPolyhedra<P, T>::getNumVar;
    using AbstractPolyhedra<P, T>::pruneBounds;
    AbstractEqualityPolyhedra(Matrix<int64_t, 0, 0, 0> A,
                              llvm::SmallVector<T, 8> b,
                              Matrix<int64_t, 0, 0, 0> E,
                              llvm::SmallVector<T, 8> q)
        : AbstractPolyhedra<P, T>(std::move(A), std::move(b)), E(std::move(E)),
          q(std::move(q)) {}

    bool isEmpty() const { return (b.size() | q.size()) == 0; }

    bool pruneBounds() {
        return AbstractPolyhedra<P, T>::pruneBounds(A, b, E, q);
    }
    void removeVariable(const size_t i) {
        AbstractPolyhedra<P, T>::removeVariable(A, b, E, q, i);
    }

    friend std::ostream &operator<<(std::ostream &os,
                                    const AbstractEqualityPolyhedra<P, T> &p) {
        return printConstraints(printConstraints(os, p.A, p.b, true), p.E, p.q,
                                false);
    }
};

struct IntegerEqPolyhedra
    : public AbstractEqualityPolyhedra<IntegerEqPolyhedra, int64_t> {

    IntegerEqPolyhedra(Matrix<int64_t, 0, 0, 0> A,
                       llvm::SmallVector<int64_t, 8> b,
                       Matrix<int64_t, 0, 0, 0> E,
                       llvm::SmallVector<int64_t, 8> q)
        : AbstractEqualityPolyhedra(std::move(A), std::move(b), std::move(E),
                                    std::move(q)){};
    bool knownLessEqualZeroImpl(int64_t x) const { return x <= 0; }
    bool knownGreaterEqualZeroImpl(int64_t x) const { return x >= 0; }
};
struct SymbolicEqPolyhedra
    : public AbstractEqualityPolyhedra<SymbolicEqPolyhedra, MPoly> {
    PartiallyOrderedSet poset;

    SymbolicEqPolyhedra(Matrix<int64_t, 0, 0, 0> A,
                        llvm::SmallVector<MPoly, 8> b,
                        Matrix<int64_t, 0, 0, 0> E,
                        llvm::SmallVector<MPoly, 8> q,
                        PartiallyOrderedSet poset)
        : AbstractEqualityPolyhedra(std::move(A), std::move(b), std::move(E),
                                    std::move(q)),
          poset(std::move(poset)){};
    bool knownLessEqualZeroImpl(MPoly x) const {
        return poset.knownLessEqualZero(std::move(x));
    }
    bool knownGreaterEqualZeroImpl(const MPoly &x) const {
        return poset.knownGreaterEqualZero(x);
    }
};
