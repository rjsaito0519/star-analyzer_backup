#ifndef TREE_STRUCTURE_H
#define TREE_STRUCTURE_H

// This file documents the TTree structure for resonance reconstruction analysis
// The trees are created in readPicoDst.C and read by analysis macros

/*
 * Event Tree Structure:
 * Tree name: "EventTree"
 * 
 * Branches:
 * - Vz (Float_t): Primary vertex Z position [cm]
 * - Vx (Float_t): Primary vertex X position [cm]
 * - Vy (Float_t): Primary vertex Y position [cm]
 * - Vr (Float_t): Primary vertex radial position [cm] = sqrt(Vx^2 + Vy^2)
 * - refMult (Int_t): Reference multiplicity
 * - runId (Int_t): Run ID
 * - eventId (Int_t): Event ID
 * - centrality (Float_t): Centrality (0-100, calculated or -1 if not calculated)
 * - Qx (Float_t): Event plane Q-vector X component
 * - Qy (Float_t): Event plane Q-vector Y component
 * - psi2 (Float_t): Event plane angle [rad]
 * - nTracks (Int_t): Number of tracks saved for this event
 * - vzVpd (Float_t): VPD vertex Z [cm] (for pileup check)
 * 
 * One entry per event
 */

/*
 * Track Tree Structure:
 * Tree name: "TrackTree"
 * 
 * Branches:
 * - eventIndex (Int_t): Index of the event this track belongs to (matches EventTree entry)
 * - pT (Float_t): Transverse momentum [GeV/c]
 * - eta (Float_t): Pseudorapidity
 * - phi (Float_t): Azimuthal angle [rad]
 * - charge (Short_t): Track charge (+1 or -1)
 * - nHitsFit (Short_t): Number of fitted hits
 * - nHitsMax (Short_t): Maximum number of possible hits
 * - nHitsDedx (Short_t): Number of hits used for dE/dx
 * - DCA (Float_t): Distance of closest approach to primary vertex [cm]
 * - chi2 (Float_t): Track chi2/ndf
 * - nSigmaPion (Float_t): TPC nSigma for pion hypothesis
 * - nSigmaKaon (Float_t): TPC nSigma for kaon hypothesis
 * - nSigmaProton (Float_t): TPC nSigma for proton hypothesis
 * - beta (Float_t): TOF beta (if TOF matched, else -999)
 * - mass2 (Float_t): TOF mass^2 [(GeV/c^2)^2] (if TOF matched, else -999)
 * - tofMatch (Bool_t): Whether track has TOF match
 * 
 * Multiple entries per event (one per track)
 * Use eventIndex to match tracks to events
 */

// Helper structure for reading tracks (optional, for convenience)
struct TrackInfo {
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
};

// Helper structure for reading events (optional, for convenience)
struct EventInfo {
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

