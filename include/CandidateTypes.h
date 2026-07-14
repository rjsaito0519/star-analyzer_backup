#ifndef CANDIDATE_TYPES_H
#define CANDIDATE_TYPES_H

#include <TVector3.h>

// Helper structure for track information
struct TrackCandidate {
  Int_t eventIndex;
  Float_t pT;
  Float_t eta;
  Float_t phi;
  Short_t charge;
  Short_t nHitsFit;
  Short_t nHitsMax;
  Short_t nHitsDedx;
  Float_t DCA;
  Float_t chi2;
  Float_t nSigmaPion;
  Float_t nSigmaKaon;
  Float_t nSigmaProton;
  Float_t beta;
  Float_t mass2;
  Bool_t tofMatch;
  
  // Helper function to get momentum vector
  TVector3 GetMomentum() const {
    TVector3 p;
    p.SetPtEtaPhi(pT, eta, phi);
    return p;
  }
};

// Helper structure for event information
struct EventCandidate {
  Float_t Vz;
  Float_t Vx;
  Float_t Vy;
  Float_t Vr;
  Int_t refMult;
  Int_t runId;
  Int_t eventId;
  Float_t centrality;
  Float_t Qx;
  Float_t Qy;
  Float_t psi2;
  Int_t nTracks;
  Float_t vzVpd;
};

#endif

