#include "V0Reconstructor.h"
#include <TMath.h>

V0Reconstructor::V0Reconstructor() {
}

V0Reconstructor::~V0Reconstructor() {
}

std::vector<V0Candidate> V0Reconstructor::ReconstructLambda(
    const std::vector<TrackCandidate>& protons,
    const std::vector<TrackCandidate>& pions,
    const EventCandidate& event,
    Bool_t useTOF) const {
  
  std::vector<V0Candidate> candidates;
  
  // Loop over all p-π⁻ combinations
  for (const auto& p : protons) {
    // Lambda: p + π⁻, so proton should be positive charge
    if (p.charge <= 0) continue;
    
    for (const auto& pi : pions) {
      // Pion should be negative charge for Lambda
      if (pi.charge >= 0) continue;
      
      // Calculate invariant mass assuming p + π⁻ → Λ
      Double_t invMass = TreeReader::CalculateInvariantMass(p, pi, kProtonMass, kPionMass);
      
      // Create V0 candidate
      V0Candidate v0;
      v0.proton = p;
      v0.pion = pi;
      v0.mass = invMass;
      v0.eventIndex = p.eventIndex;
      
      // Calculate momentum
      TVector3 pMom = p.GetMomentum();
      TVector3 piMom = pi.GetMomentum();
      v0.momentum = pMom + piMom;
      v0.pt = v0.momentum.Perp();
      v0.eta = v0.momentum.PseudoRapidity();
      v0.phi = v0.momentum.Phi();
      
      // Calculate topology variables
      CalculateTopology(v0, event);
      
      candidates.push_back(v0);
    }
  }
  
  return candidates;
}

void V0Reconstructor::CalculateTopology(V0Candidate& v0, const EventCandidate& event) const {
  // Calculate DCA between daughters
  v0.daughterDCA = CalculateDCA(v0.proton, v0.pion);
  
  // Calculate decay length (simplified)
  v0.decayLength = CalculateDecayLength(v0, event);
  
  // Calculate pointing angle
  v0.pointingAngle = CalculatePointingAngle(v0, event);
  
  // Calculate DCA to PV
  v0.dcaToPV = CalculateDCAToPV(v0, event);
}

Double_t V0Reconstructor::CalculateDCA(const TrackCandidate& trk1, const TrackCandidate& trk2) const {
  // Simplified DCA calculation
  // In reality, this requires track helix parameters and proper calculation
  // For now, use a simplified approximation based on track positions
  
  TVector3 p1 = trk1.GetMomentum();
  TVector3 p2 = trk2.GetMomentum();
  
  // Approximate DCA as distance between track positions at their DCA points
  // This is a simplified version - full implementation requires helix calculations
  Double_t dca = TMath::Sqrt(trk1.DCA * trk1.DCA + trk2.DCA * trk2.DCA);
  
  return dca;
}

Double_t V0Reconstructor::CalculateDecayLength(const V0Candidate& v0, const EventCandidate& event) const {
  // Simplified decay length calculation
  // In reality, this requires the V0 decay vertex position
  // For now, use an approximation based on track DCAs
  
  // Approximate decay length as average of daughter DCAs
  Double_t decayLength = (v0.proton.DCA + v0.pion.DCA) / 2.0;
  
  // Apply a scaling factor (this is a rough approximation)
  decayLength *= 2.0;
  
  return decayLength;
}

Double_t V0Reconstructor::CalculatePointingAngle(const V0Candidate& v0, const EventCandidate& event) const {
  // Pointing angle: angle between V0 momentum and vector from PV to V0 decay point
  // Simplified calculation
  
  TVector3 pVtx(event.Vx, event.Vy, event.Vz);
  TVector3 v0Pos = pVtx;  // Simplified: assume decay point is at PV + some offset
  
  // Calculate vector from PV to V0 (simplified)
  TVector3 toV0 = v0.momentum.Unit() * v0.decayLength;
  
  // Pointing angle (cosine of angle between momentum and PV-V0 vector)
  Double_t cosAngle = v0.momentum.Unit() * toV0.Unit();
  
  // Return the angle in radians
  return TMath::ACos(TMath::Abs(cosAngle));
}

Double_t V0Reconstructor::CalculateDCAToPV(const V0Candidate& v0, const EventCandidate& event) const {
  // DCA of V0 to primary vertex
  // Simplified: use average of daughter DCAs
  return (v0.proton.DCA + v0.pion.DCA) / 2.0;
}

Bool_t V0Reconstructor::PassTopologyCuts(const V0Candidate& v0) const {
  // Apply topology cuts
  const auto& v0Cuts = CutConfig::V0::Get();
  if (v0.daughterDCA < v0Cuts.minDaughterDCA) return kFALSE;
  if (v0.daughterDCA > v0Cuts.maxDaughterDCA) return kFALSE;
  if (v0.decayLength < v0Cuts.minDecayLength) return kFALSE;
  if (v0.decayLength > v0Cuts.maxDecayLength) return kFALSE;
  if (v0.pointingAngle > v0Cuts.maxPointingAngle) return kFALSE;
  if (v0.dcaToPV > v0Cuts.maxDCAtoPV) return kFALSE;
  
  // Mass window cut
  if (TMath::Abs(v0.mass - v0Cuts.lambdaMass) > v0Cuts.lambdaMassWindow) {
    return kFALSE;
  }
  
  return kTRUE;
}

std::vector<V0Candidate> V0Reconstructor::GetLambdaCandidates(
    const std::vector<TrackCandidate>& protons,
    const std::vector<TrackCandidate>& pions,
    const EventCandidate& event,
    Bool_t useTOF) const {
  
  // Reconstruct all Lambda candidates
  std::vector<V0Candidate> allCandidates = ReconstructLambda(protons, pions, event, useTOF);
  
  // Apply topology cuts
  std::vector<V0Candidate> selectedCandidates;
  for (const auto& v0 : allCandidates) {
    if (PassTopologyCuts(v0)) {
      selectedCandidates.push_back(v0);
    }
  }
  
  return selectedCandidates;
}

