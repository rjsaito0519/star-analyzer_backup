#ifndef STAR_ANALYZER_ST_PICO_KF_PARTICLE_INTERFACE_H
#define STAR_ANALYZER_ST_PICO_KF_PARTICLE_INTERFACE_H

#include "KfParticleHelper.h"
#include <string>
#include <vector>

class KfParticleCutConfig;
class StPicoDst;

struct KfParticleEventStats {
  KfParticleEventStats();
  UInt_t rawTracks, qualityTracks, covarianceTracks, pidTracks;
  UInt_t pidHypotheses, protonHypotheses, pionHypotheses, unknownHypotheses;
  UInt_t primaryHypotheses, finderParticles, lambdaParticles, validCandidates;
  UInt_t xiParticles, validXiCandidates, invalidXiCandidates;
  UInt_t invalidCandidates, invalidCovariance, invalidPid, invalidTrackIds;
  UInt_t tofTracks;
};

// Event-level port of the STAR Pico Interface, not a separate pair finder.
// PIMPL keeps namespaced KF/SIMD classes out of CINT and Maker dictionaries.
class StPicoKFParticleInterface {
public:
  explicit StPicoKFParticleInterface(const KfParticleCutConfig& cuts);
  ~StPicoKFParticleInterface();
  Bool_t ProcessEvent(StPicoDst* picoDst);
  void Clear();
  const std::vector<KfLambdaCandidate>& Candidates() const;
  const std::vector<KfXiCandidate>& XiCandidates() const;
  const KfParticleEventStats& Stats() const;
  const std::string& LastError() const;
  static const char* BackendDescription();
private:
  class Impl;
  Impl* mImpl;
  StPicoKFParticleInterface(const StPicoKFParticleInterface&);
  StPicoKFParticleInterface& operator=(const StPicoKFParticleInterface&);
};

#endif
