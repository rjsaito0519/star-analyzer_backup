#ifndef V0_RECONSTRUCTOR_H
#define V0_RECONSTRUCTOR_H

#include <vector>
#include <TMath.h>
#include <TVector3.h>
#include "CandidateTypes.h"
#include "TreeReader.h"
#include "CutConfig.h"

// Lambda mass
const Double_t kLambdaMass = 1.115683;  // GeV/c^2
const Double_t kProtonMass = 0.938272;   // GeV/c^2
const Double_t kPionMass = 0.139570;    // GeV/c^2

// Structure for V0 candidate (Lambda)
struct V0Candidate {
  TrackCandidate proton;   // Daughter track (p for Lambda)
  TrackCandidate pion;     // Daughter track (π⁻ for Lambda)
  
  TVector3 momentum;       // V0 momentum
  Double_t mass;           // Invariant mass
  Double_t pt;             // Transverse momentum
  Double_t eta;            // Pseudorapidity
  Double_t phi;            // Azimuthal angle
  
  // Topology variables
  Double_t daughterDCA;    // DCA between daughters
  Double_t decayLength;     // Decay length
  Double_t pointingAngle;   // Pointing angle (angle between V0 momentum and PV-V0 vector)
  Double_t dcaToPV;        // DCA of V0 to primary vertex
  
  Int_t eventIndex;        // Event index
};

class V0Reconstructor {
public:
  V0Reconstructor();
  ~V0Reconstructor();
  
  // Reconstruct Lambda candidates from p and π tracks
  std::vector<V0Candidate> ReconstructLambda(const std::vector<TrackCandidate>& protons,
                                             const std::vector<TrackCandidate>& pions,
                                             const EventCandidate& event,
                                             Bool_t useTOF = kFALSE) const;
  
  // Apply topology cuts to V0 candidate
  Bool_t PassTopologyCuts(const V0Candidate& v0) const;
  
  // Calculate topology variables
  void CalculateTopology(V0Candidate& v0, const EventCandidate& event) const;
  
  // Get Lambda candidates passing all cuts
  std::vector<V0Candidate> GetLambdaCandidates(const std::vector<TrackCandidate>& protons,
                                                const std::vector<TrackCandidate>& pions,
                                                const EventCandidate& event,
                                                Bool_t useTOF = kFALSE) const;

private:
  // Calculate DCA between two tracks
  Double_t CalculateDCA(const TrackCandidate& trk1, const TrackCandidate& trk2) const;
  
  // Calculate decay length (simplified - assumes straight line)
  Double_t CalculateDecayLength(const V0Candidate& v0, const EventCandidate& event) const;
  
  // Calculate pointing angle
  Double_t CalculatePointingAngle(const V0Candidate& v0, const EventCandidate& event) const;
  
  // Calculate DCA of V0 to primary vertex
  Double_t CalculateDCAToPV(const V0Candidate& v0, const EventCandidate& event) const;
};

#endif

