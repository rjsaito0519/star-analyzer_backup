#ifndef PHI_DAUGHTER_PID_H
#define PHI_DAUGHTER_PID_H

#include <cmath>

namespace phi_daughter_pid {

/** Phi-daughter kaon TOF PID. Cut values come from PID YAML. */
struct Cuts {
  double pMomKaonPID;
  bool tofUseMass2Cut;
  bool tofUseDeltaInvBetaCut;
  bool requireTofForNegative;
  double minMass2Kaon;
  double maxMass2Kaon;
  double maxAbsDeltaOneOverBetaKaon;

  Cuts()
      : pMomKaonPID(0.0),
        tofUseMass2Cut(false),
        tofUseDeltaInvBetaCut(false),
        requireTofForNegative(true),
        minMass2Kaon(0.0),
        maxMass2Kaon(0.0),
        maxAbsDeltaOneOverBetaKaon(0.0) {}
};

inline bool InKaonMass2Window(double mass2, const Cuts& cuts) {
  return (mass2 > cuts.minMass2Kaon && mass2 < cuts.maxMass2Kaon);
}

/**
 * Production TOF PID for a phi daughter.
 * Momentum is full |p|, not pT. p == pMomKaonPID is the low-p side.
 * Unmatched high-p tracks fail even when collection uses tofFallbackMode: tpcOnly.
 *
 * Charge asymmetry (YAML phiDaughterKaonMinusRequireTof, default true):
 * K- always requires a TOF match. Low-p TPC-only is K+ only.
 */
inline bool Pass(double pMag, bool tofMatch, double mass2, double deltaOneOverBeta, int charge,
                 const Cuts& cuts) {
  if (!tofMatch) {
    if (cuts.requireTofForNegative && charge < 0) return false;
    return pMag <= cuts.pMomKaonPID;
  }
  bool pass = true;
  if (cuts.tofUseMass2Cut) {
    pass = pass && InKaonMass2Window(mass2, cuts);
  }
  if (cuts.tofUseDeltaInvBetaCut) {
    pass = pass && (std::fabs(deltaOneOverBeta) <= cuts.maxAbsDeltaOneOverBetaKaon);
  }
  return pass;
}

}  // namespace phi_daughter_pid

#endif
