#ifndef FEMTO_CANDIDATE_H
#define FEMTO_CANDIDATE_H

// Species/channel key naming rules: StMaker/StFemtoMaker/README.md

#include "Rtypes.h"
#include <TLorentzVector.h>
#include <map>
#include <string>
#include <vector>

enum FemtoCandSource {
  kFemtoCandTrack = 0,
  kFemtoCandResonance = 1
};

struct TrackExtra {
  Float_t nSigmaProton;
  Float_t nSigmaKaon;
  Float_t nSigmaHe4;
  Float_t nSigmaDeuteron;
  Float_t nSigmaTriton;
  Float_t nSigmaHe3;
  Float_t mass2;
  Float_t deltaOneOverBeta;
  Bool_t tofMatch;
  Float_t dca;
  Short_t nHitsFit;
  Int_t trackIndex;

  TrackExtra()
      : nSigmaProton(0.0f),
        nSigmaKaon(0.0f),
        nSigmaHe4(0.0f),
        nSigmaDeuteron(0.0f),
        nSigmaTriton(0.0f),
        nSigmaHe3(0.0f),
        mass2(-999.0f),
        deltaOneOverBeta(999.0f),
        tofMatch(kFALSE),
        dca(0.0f),
        nHitsFit(0),
        trackIndex(-1) {}
};

struct ResonanceExtra {
  Float_t invMass;
  Float_t dcaDaughters;
  Float_t betaGamma;
  Int_t dau1EventIndex;
  Int_t dau1Index;
  Int_t dau2EventIndex;
  Int_t dau2Index;

  ResonanceExtra()
      : invMass(0.0f),
        dcaDaughters(0.0f),
        betaGamma(-1.0f),
        dau1EventIndex(-1),
        dau1Index(-1),
        dau2EventIndex(-1),
        dau2Index(-1) {}
};

struct FemtoCandidate {
  Int_t eventIndex;
  FemtoCandSource source;
  std::string speciesKey;
  Short_t charge;

  Float_t px;
  Float_t py;
  Float_t pz;
  Float_t energy;
  Float_t y;
  Float_t pt;
  Float_t eta;
  Float_t phi;

  UInt_t qualityFlags;

  TrackExtra trk;
  ResonanceExtra reso;

  FemtoCandidate()
      : eventIndex(-1),
        source(kFemtoCandTrack),
        charge(0),
        px(0.0f),
        py(0.0f),
        pz(0.0f),
        energy(0.0f),
        y(0.0f),
        pt(0.0f),
        eta(0.0f),
        phi(0.0f),
        qualityFlags(0) {}

  TLorentzVector P4() const { return TLorentzVector(px, py, pz, energy); }

  void SetP4(const TLorentzVector& v) {
    px = (Float_t)v.Px();
    py = (Float_t)v.Py();
    pz = (Float_t)v.Pz();
    energy = (Float_t)v.E();
    pt = (Float_t)v.Pt();
    eta = (Float_t)v.Eta();
    phi = (Float_t)v.Phi();
    y = (Float_t)v.Rapidity();
  }
};

inline Bool_t FemtoTrackRefsMatch(Int_t eventIndexA, Int_t trackIndexA, Int_t eventIndexB, Int_t trackIndexB) {
  return eventIndexA >= 0 && trackIndexA >= 0 && eventIndexA == eventIndexB && trackIndexA == trackIndexB;
}

inline Bool_t FemtoResonanceContainsTrack(const ResonanceExtra& reso, Int_t eventIndex, Int_t trackIndex) {
  return FemtoTrackRefsMatch(reso.dau1EventIndex, reso.dau1Index, eventIndex, trackIndex) ||
         FemtoTrackRefsMatch(reso.dau2EventIndex, reso.dau2Index, eventIndex, trackIndex);
}

inline Bool_t FemtoCandidatesShareTrack(const FemtoCandidate& a, const FemtoCandidate& b) {
  if (a.source == kFemtoCandResonance && b.source == kFemtoCandResonance) {
    return FemtoResonanceContainsTrack(a.reso, b.reso.dau1EventIndex, b.reso.dau1Index) ||
           FemtoResonanceContainsTrack(a.reso, b.reso.dau2EventIndex, b.reso.dau2Index);
  }
  if (a.source == kFemtoCandResonance) {
    return FemtoResonanceContainsTrack(a.reso, b.eventIndex, b.trk.trackIndex);
  }
  if (b.source == kFemtoCandResonance) {
    return FemtoResonanceContainsTrack(b.reso, a.eventIndex, a.trk.trackIndex);
  }
  return FemtoTrackRefsMatch(a.eventIndex, a.trk.trackIndex, b.eventIndex, b.trk.trackIndex);
}

typedef std::map<std::string, std::vector<FemtoCandidate> > FemtoCandidateStore;

#endif
