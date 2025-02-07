#pragma once

#include "./ArrayReference.hpp"
#include "./Graphs.hpp"
#include "./Math.hpp"
#include "llvm/IR/User.h"
#include <cstddef>
#include <cstdint>
#include <llvm/ADT/ArrayRef.h>
#include <llvm/ADT/SmallVector.h>

// We represent a schedule as
// Phi_s'*i + omega_s <_{lex} Phi_t'*s + Omega_t
// means that schedule `s` executes before schedule `t`.
//
// S_0 = {Phi_0, omega_0}
// S_1 = {Phi_1, omega_1}
// given i_0 and i_1, if
// Phi_0 * i_0 + omega_0 << Phi_1 * i_1 + omega_1
// then "i_0" for schedule "S_0" happens before
// "i_1" for schedule "S_1"
//
struct Schedule {
    // given `N` loops, `P` is `N+1 x 2*N+1`
    // even rows give offsets indicating fusion (0-indexed)
    // However, all odd columns of `Phi` are structually zero,
    // so we represent it with an `N x N` matrix instead.
    static constexpr unsigned maxStackLoops = 3;
    static constexpr unsigned maxStackStorage =
        maxStackLoops * (maxStackLoops + 2) + 1;
    // 3*3+ 2*3+1 = 16
    llvm::SmallVector<int64_t, maxStackStorage> data;
    const uint8_t numLoops;
    // -1 indicates not vectorized
    int8_t vectorized = -1;
    // -1 indicates not unrolled
    // inner unroll means either the only unrolled loop, or if outer unrolled,
    // then the inner unroll is nested inside of the outer unroll.
    // if unrolledInner=3, unrolledOuter=2
    // x_0_0; x_1_0; x_2_0
    // x_0_1; x_1_1; x_2_1
    int8_t unrolledInner = -1;
    // -1 indicates not unrolled
    int8_t unrolledOuter = -1;
    Schedule(size_t nLoops)
        : data(llvm::SmallVector<int64_t, maxStackStorage>(
              nLoops * (nLoops + 2) + 1)),
          numLoops(nLoops) {
        MutSquarePtrMatrix<int64_t> Phi(getPhi());
        for (size_t i = 0; i < nLoops; ++i) {
            Phi(i, i) = 1;
        }
    };
    MutSquarePtrMatrix<int64_t> getPhi() {
        // return MutSquarePtrMatrix<int64_t>(data.data(), numLoops);
        return MutSquarePtrMatrix<int64_t>{{}, data.data(), numLoops};
    }
    MutPtrVector<int64_t> getOmega() {
        return {data.data() + numLoops * numLoops, 2 * size_t(numLoops) + 1};
    }
    SquarePtrMatrix<int64_t> getPhi() const {
        return SquarePtrMatrix<int64_t>{{}, data.data(), numLoops};
    }
    PtrVector<int64_t> getOmega() const {
        return {.mem = data.data() + numLoops * numLoops,
                .N = 2 * size_t(numLoops) + 1};
    }
    bool fusedThrough(const Schedule &y, const size_t numLoopsCommon) const {
        llvm::ArrayRef<int64_t> o0 = getOmega();
        llvm::ArrayRef<int64_t> o1 = y.getOmega();
        bool allEqual = true;
        for (size_t n = 0; n < numLoopsCommon; ++n)
            allEqual &= (o0[2 * n] == o1[2 * n]);
        return allEqual;
    }
    bool fusedThrough(const Schedule &y) const {
        return fusedThrough(y, std::min(numLoops, y.numLoops));
    }
    size_t getNumLoops() const { return numLoops; }
};

// TODO:
// refactor to use GraphTraits.h
// https://github.com/llvm/llvm-project/blob/main/llvm/include/llvm/ADT/GraphTraits.h
struct MemoryAccess {
    ArrayReference ref;
    // unsigned ref; // index to ArrayReference
    llvm::User *user;
    // unsigned (instead of ptr) as we build up edges
    // and I don't want to relocate pointers when resizing vector
    Schedule schedule;
    llvm::SmallVector<unsigned> edgesIn;
    llvm::SmallVector<unsigned> edgesOut;
    const bool isLoad;
    MemoryAccess(ArrayReference ref, llvm::User *user, Schedule schedule,
                 bool isLoad)
        : ref(std::move(ref)), user(user), schedule(schedule),
          edgesIn(llvm::SmallVector<unsigned>()),
          edgesOut(llvm::SmallVector<unsigned>()), isLoad(isLoad){};

    void addEdgeIn(unsigned i) { edgesIn.push_back(i); }
    void addEdgeOut(unsigned i) { edgesOut.push_back(i); }
    // size_t getNumLoops() const { return ref->getNumLoops(); }
    // size_t getNumAxes() const { return ref->axes.size(); }
    // std::shared_ptr<AffineLoopNest> loop() { return ref->loop; }
    bool fusedThrough(MemoryAccess &x) {
        // originally separate loops could be fused
        // if (loop() != x.loop()){ return false; }
        return schedule.fusedThrough(x.schedule);
    }
    size_t getNumLoops() const { return schedule.getNumLoops(); }
};
