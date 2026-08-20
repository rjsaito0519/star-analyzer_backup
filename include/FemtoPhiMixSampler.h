#ifndef FEMTO_PHI_MIX_SAMPLER_H
#define FEMTO_PHI_MIX_SAMPLER_H

#include "FemtoMixingSampler.h"

#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace femto_phi_mix {

typedef femto_mixing::PairCount PairCount;

// Deterministic SplitMix64. Independent of ROOT gRandom.
class SplitMix64 {
 public:
  explicit SplitMix64(PairCount seed = 0x9E3779B97F4A7C15ULL) : state_(seed) {}

  void Seed(PairCount seed) { state_ = seed; }
  PairCount State() const { return state_; }

  PairCount NextU64() {
    PairCount z = (state_ += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
  }

  // Uniform integer in [lo, hi], inclusive. Rejection sampling, no modulo bias.
  PairCount UniformInclusive(PairCount lo, PairCount hi) {
    if (hi < lo) throw std::out_of_range("SplitMix64 UniformInclusive: hi < lo");
    if (hi == lo) return lo;
    const PairCount spanMinusOne = hi - lo;
    if (spanMinusOne == std::numeric_limits<PairCount>::max()) return NextU64();
    const PairCount n = spanMinusOne + 1;
    const PairCount maxv = std::numeric_limits<PairCount>::max();
    const PairCount threshold = (maxv / n) * n;
    PairCount r;
    do {
      r = NextU64();
    } while (r >= threshold);
    return lo + (r % n);
  }

 private:
  PairCount state_;
};

// Sparse Fisher-Yates: yields a uniform random permutation of [0, n) one index
// at a time. Memory is O(number of Next() calls), not O(n).
class LazyPermutationSampler {
 public:
  LazyPermutationSampler(PairCount n, SplitMix64& rng) : n_(n), i_(0), rng_(rng) {}

  bool Done() const { return i_ >= n_; }
  PairCount Drawn() const { return i_; }
  PairCount Population() const { return n_; }

  bool Next(PairCount& out) {
    if (i_ >= n_) return false;
    const PairCount j = rng_.UniformInclusive(i_, n_ - 1);
    const PairCount ai = At(i_);
    const PairCount aj = At(j);
    Set(j, ai);
    // Position i is consumed; do not keep it in the sparse map.
    map_.erase(i_);
    out = aj;
    ++i_;
    return true;
  }

 private:
  PairCount At(PairCount k) const {
    std::map<PairCount, PairCount>::const_iterator it = map_.find(k);
    return (it == map_.end()) ? k : it->second;
  }

  void Set(PairCount k, PairCount v) {
    if (v == k)
      map_.erase(k);
    else
      map_[k] = v;
  }

  PairCount n_;
  PairCount i_;
  SplitMix64& rng_;
  std::map<PairCount, PairCount> map_;
};

enum PhiMixQaBin {
  kQaPairPopulation = 0,
  kQaPairPopulationFwd = 1,
  kQaPairPopulationRev = 2,
  kQaCombosPrePid = 3,
  kQaPidPassCurrentPlus = 4,
  kQaPidPassCurrentMinus = 5,
  kQaPidPassBufferPlus = 6,
  kQaPidPassBufferMinus = 7,
  kQaAttempted = 8,
  kQaAttemptedFwd = 9,
  kQaAttemptedRev = 10,
  kQaStored = 11,
  kQaStoredFwd = 12,
  kQaStoredRev = 13,
  kQaPairCutRejected = 14,
  kQaEligibleLowerBound = 15,
  kQaCapHit = 16,
  kQaEligibleExact = 17,
  kQaIndexDuplicate = 18,
  kQaIndexOutOfRange = 19,
  kQaSeed = 20,
  kQaEvents = 21,
  kQaBinCount = 22
};

struct CapSampleStats {
  CapSampleStats()
      : attempted(0),
        attemptedForward(0),
        attemptedReverse(0),
        pairCutRejected(0),
        storedForward(0),
        storedReverse(0),
        eligibleLowerBound(0),
        nEligibleExact(0),
        nEligibleExactValid(false),
        capHit(false),
        exhausted(false),
        duplicateIndexErrors(0),
        outOfRangeErrors(0) {}

  PairCount attempted;
  PairCount attemptedForward;
  PairCount attemptedReverse;
  PairCount pairCutRejected;
  PairCount storedForward;
  PairCount storedReverse;
  PairCount eligibleLowerBound;
  PairCount nEligibleExact;
  bool nEligibleExactValid;
  bool capHit;
  bool exhausted;
  PairCount duplicateIndexErrors;
  PairCount outOfRangeErrors;
};

enum EvalStatus { kEvalAccept = 0, kEvalReject = 1, kEvalResolveFail = 2 };

// Uniform without-replacement sample of eligible pairs from a flat-index
// population. evaluate(flatIndex, ref) must:
//   - resolve via ResolvePairReference (caller may do that inside)
//   - return kEvalAccept if the pair is eligible (PID already applied to the
//     population; remaining cuts are invMass / opening angle / pair rapidity)
//   - return kEvalReject if pair cuts fail
//   - return kEvalResolveFail on index errors
//
// Why this is uniform on the eligible set:
// A uniform random permutation of all pair indices is generated lazily.
// Evaluating that order and keeping the first `cap` indices that pass pair
// cuts is equivalent to ranking a uniform random permutation of the eligible
// subset and taking its prefix of length min(cap, N_eligible). Every eligible
// k-subset is therefore equally likely. Loop order, pool order, and
// forward/reverse block order only change the flat-index numbering, not the
// induced distribution on logical pairs.
//
// maxCandidates <= 0: uncapped sequential scan of 0..N-1 (validation only).
// Cap hit is true iff at least cap+1 eligible pairs exist. When sampling,
// the (cap+1)th eligible pair is sought but not stored.
template <typename Evaluate>
void SampleEligiblePairs(const femto_mixing::SamplingPlan& plan, PairCount maxCandidates, SplitMix64& rng,
                         Evaluate evaluate, std::vector<PairCount>& storedIndices, CapSampleStats& stats) {
  storedIndices.clear();
  stats = CapSampleStats();
  const PairCount n = plan.eligiblePairs;
  if (n == 0) {
    stats.exhausted = true;
    stats.nEligibleExactValid = true;
    return;
  }

  std::set<PairCount> seen;

  auto consider = [&](PairCount flatIndex) -> EvalStatus {
    if (flatIndex >= n) {
      ++stats.outOfRangeErrors;
      return kEvalResolveFail;
    }
    if (!seen.insert(flatIndex).second) {
      ++stats.duplicateIndexErrors;
      return kEvalResolveFail;
    }
    femto_mixing::PairReference ref;
    if (!femto_mixing::ResolvePairReference(plan, flatIndex, ref)) {
      ++stats.outOfRangeErrors;
      return kEvalResolveFail;
    }
    ++stats.attempted;
    if (ref.reverse) {
      ++stats.attemptedReverse;
    } else {
      ++stats.attemptedForward;
    }
    const EvalStatus st = evaluate(flatIndex, ref);
    if (st == kEvalReject) ++stats.pairCutRejected;
    if (st == kEvalResolveFail) ++stats.outOfRangeErrors;
    return st;
  };

  auto noteStored = [&](const PairCount flatIndex) {
    femto_mixing::PairReference ref;
    femto_mixing::ResolvePairReference(plan, flatIndex, ref);
    if (ref.reverse) {
      ++stats.storedReverse;
    } else {
      ++stats.storedForward;
    }
  };

  if (maxCandidates == 0 || maxCandidates > n) {
    // Uncapped, or cap larger than the pair-combination population: scan all.
    PairCount nAccept = 0;
    for (PairCount idx = 0; idx < n; ++idx) {
      const EvalStatus st = consider(idx);
      if (st == kEvalAccept) {
        ++nAccept;
        storedIndices.push_back(idx);
        noteStored(idx);
      }
    }
    stats.exhausted = true;
    stats.nEligibleExact = nAccept;
    stats.nEligibleExactValid = true;
    stats.eligibleLowerBound = nAccept;
    stats.capHit = false;
    return;
  }

  LazyPermutationSampler perm(n, rng);
  while (storedIndices.size() < maxCandidates) {
    PairCount idx = 0;
    if (!perm.Next(idx)) {
      stats.exhausted = true;
      break;
    }
    if (consider(idx) == kEvalAccept) {
      storedIndices.push_back(idx);
      noteStored(idx);
    }
  }

  if (storedIndices.size() == maxCandidates && !perm.Done()) {
    PairCount idx = 0;
    while (perm.Next(idx)) {
      const EvalStatus st = consider(idx);
      if (st == kEvalAccept) {
        stats.capHit = true;
        break;
      }
    }
    stats.exhausted = perm.Done() && !stats.capHit;
  } else {
    stats.exhausted = perm.Done();
    stats.capHit = false;
  }

  const PairCount nStored = static_cast<PairCount>(storedIndices.size());
  stats.eligibleLowerBound = nStored + (stats.capHit ? 1 : 0);
  if (!stats.capHit && stats.exhausted) {
    stats.nEligibleExactValid = true;
    stats.nEligibleExact = nStored;
  }
}

}  // namespace femto_phi_mix

#endif
