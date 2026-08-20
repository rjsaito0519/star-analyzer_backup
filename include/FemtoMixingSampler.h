#ifndef FEMTO_MIXING_SAMPLER_H
#define FEMTO_MIXING_SAMPLER_H

#include <cstddef>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

namespace femto_mixing {

typedef unsigned long long PairCount;

struct EventCandidateCounts {
  EventCandidateCounts(std::size_t a = 0, std::size_t b = 0) : nA(a), nB(b) {}

  std::size_t nA;
  std::size_t nB;
};

struct PairBlock {
  PairBlock(std::size_t event, bool isReverse, std::size_t first, std::size_t second, PairCount begin,
            PairCount count)
      : poolEventIndex(event),
        reverse(isReverse),
        nFirst(first),
        nSecond(second),
        beginIndex(begin),
        pairCount(count) {}

  std::size_t poolEventIndex;
  bool reverse;
  std::size_t nFirst;
  std::size_t nSecond;
  PairCount beginIndex;
  PairCount pairCount;
};

struct SamplingPlan {
  SamplingPlan()
      : eligiblePairs(0),
        eligibleForwardPairs(0),
        eligibleReversePairs(0),
        eligibleBufferDirections(0),
        emptyBufferDirections(0) {}

  std::vector<PairBlock> blocks;
  PairCount eligiblePairs;
  PairCount eligibleForwardPairs;
  PairCount eligibleReversePairs;
  PairCount eligibleBufferDirections;
  PairCount emptyBufferDirections;
};

struct PairReference {
  PairReference() : poolEventIndex(0), reverse(false), firstIndex(0), secondIndex(0) {}

  std::size_t poolEventIndex;
  bool reverse;
  std::size_t firstIndex;
  std::size_t secondIndex;
};

inline PairCount CheckedPairProduct(std::size_t first, std::size_t second) {
  if (first == 0 || second == 0) return 0;
  const PairCount a = static_cast<PairCount>(first);
  const PairCount b = static_cast<PairCount>(second);
  if (a > std::numeric_limits<PairCount>::max() / b) {
    throw std::overflow_error("Femto mixing candidate-pair count overflow");
  }
  return a * b;
}

inline void AddPairBlock(SamplingPlan& plan, std::size_t poolEventIndex, bool reverse, std::size_t nFirst,
                         std::size_t nSecond) {
  const PairCount count = CheckedPairProduct(nFirst, nSecond);
  if (count == 0) return;
  if (plan.eligiblePairs > std::numeric_limits<PairCount>::max() - count) {
    throw std::overflow_error("Femto mixing total candidate-pair count overflow");
  }
  plan.blocks.push_back(PairBlock(poolEventIndex, reverse, nFirst, nSecond, plan.eligiblePairs, count));
  plan.eligiblePairs += count;
  if (reverse) {
    plan.eligibleReversePairs += count;
  } else {
    plan.eligibleForwardPairs += count;
  }
  ++plan.eligibleBufferDirections;
}

// Forward: current A x buffered B. Reverse: buffered A x current B.
// Each direction is enabled solely by the current-side collection it needs.
inline SamplingPlan BuildSamplingPlan(std::size_t currentA, std::size_t currentB,
                                      const std::vector<EventCandidateCounts>& bufferedEvents,
                                      bool mixBothDirections) {
  SamplingPlan plan;
  for (std::size_t ie = 0; ie < bufferedEvents.size(); ++ie) {
    const EventCandidateCounts& evt = bufferedEvents[ie];
    if (currentA > 0) {
      if (evt.nB > 0) {
        AddPairBlock(plan, ie, false, currentA, evt.nB);
      } else {
        ++plan.emptyBufferDirections;
      }
    }
    if (mixBothDirections && currentB > 0) {
      if (evt.nA > 0) {
        AddPairBlock(plan, ie, true, evt.nA, currentB);
      } else {
        ++plan.emptyBufferDirections;
      }
    }
  }
  return plan;
}

inline bool ResolvePairReference(const SamplingPlan& plan, PairCount flatIndex, PairReference& out) {
  if (flatIndex >= plan.eligiblePairs) return false;
  std::size_t low = 0;
  std::size_t high = plan.blocks.size();
  while (low < high) {
    const std::size_t ib = low + (high - low) / 2;
    const PairBlock& block = plan.blocks[ib];
    if (flatIndex < block.beginIndex) {
      high = ib;
      continue;
    }
    if (flatIndex >= block.beginIndex + block.pairCount) {
      low = ib + 1;
      continue;
    }
    const PairCount local = flatIndex - block.beginIndex;
    out.poolEventIndex = block.poolEventIndex;
    out.reverse = block.reverse;
    out.firstIndex = static_cast<std::size_t>(local / static_cast<PairCount>(block.nSecond));
    out.secondIndex = static_cast<std::size_t>(local % static_cast<PairCount>(block.nSecond));
    return true;
  }
  return false;
}

// bufferAll passes applyCap=false and exhausts the eligible population.
// randomSample passes applyCap=true and samples at most maxPairs (0 = unlimited).
inline PairCount PlannedAttemptCount(PairCount eligiblePairs, PairCount maxPairs, bool applyCap) {
  if (!applyCap || maxPairs == 0 || eligiblePairs <= maxPairs) return eligiblePairs;
  return maxPairs;
}

// Floyd's algorithm: a uniform subset without replacement, using O(sampleSize)
// memory. drawInclusive(max) must return a uniform integer in [0, max].
template <typename DrawInclusive>
std::vector<PairCount> SampleWithoutReplacement(PairCount populationSize, PairCount sampleSize,
                                                DrawInclusive&& drawInclusive) {
  if (sampleSize > populationSize) sampleSize = populationSize;
  std::set<PairCount> selected;
  for (PairCount j = populationSize - sampleSize; j < populationSize; ++j) {
    const PairCount candidate = drawInclusive(j);
    if (candidate > j) throw std::out_of_range("Femto mixing sampler RNG returned an out-of-range index");
    if (!selected.insert(candidate).second) selected.insert(j);
  }
  return std::vector<PairCount>(selected.begin(), selected.end());
}

enum SamplerQaBin {
  kQaAttempted = 0,
  kQaEligible = 1,
  kQaFilled = 2,
  kQaSkipped = 3,
  kQaSkippedByCap = 4,
  kQaSkippedByPairCut = 5,
  kQaEligibleBufferDirections = 6,
  kQaEmptyBufferDirections = 7,
  kQaSelectedEmptyBufferDirections = 8,
  kQaFilledForward = 9,
  kQaFilledReverse = 10,
  kQaEligibleForward = 11,
  kQaEligibleReverse = 12,
  kQaSkippedOverlap = 13,
  kQaSkippedSignalWindow = 14,
  kQaBinCount = 15
};

}  // namespace femto_mixing

#endif
