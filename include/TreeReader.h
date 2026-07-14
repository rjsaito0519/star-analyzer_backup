#ifndef TREE_READER_H
#define TREE_READER_H

#include <TFile.h>
#include <TTree.h>
#include <TMath.h>
#include <TVector3.h>
#include <vector>
#include <iostream>
#include "CutConfig.h"
#include "CandidateTypes.h"

class TreeReader {
public:
  TreeReader();
  ~TreeReader();
  
  // Open file and load trees
  Bool_t OpenFile(const Char_t *filename);
  void CloseFile();
  
  // Get number of events/tracks
  Long64_t GetNEvents() const { return eventTree ? eventTree->GetEntries() : 0; }
  Long64_t GetNTracks() const { return trackTree ? trackTree->GetEntries() : 0; }
  
  // Load event at index
  Bool_t LoadEvent(Long64_t eventIndex);
  
  // Get current event information
  const EventCandidate& GetEvent() const { return currentEvent; }
  
  // Get all tracks for current event
  const std::vector<TrackCandidate>& GetTracks() const { return currentTracks; }
  
  // Apply event cuts
  Bool_t PassEventCuts(const EventCandidate& evt) const;
  
  // Apply track cuts
  Bool_t PassTrackCuts(const TrackCandidate& trk) const;
  
  // PID selection functions
  Bool_t IsPion(const TrackCandidate& trk, Bool_t useTOF = kFALSE) const;
  Bool_t IsKaon(const TrackCandidate& trk, Bool_t useTOF = kFALSE) const;
  Bool_t IsProton(const TrackCandidate& trk, Bool_t useTOF = kFALSE) const;
  
  // Get selected candidates
  std::vector<TrackCandidate> GetPionCandidates(Bool_t useTOF = kFALSE) const;
  std::vector<TrackCandidate> GetKaonCandidates(Bool_t useTOF = kFALSE) const;
  std::vector<TrackCandidate> GetProtonCandidates(Bool_t useTOF = kFALSE) const;
  
  // Calculate invariant mass of two tracks
  static Double_t CalculateInvariantMass(const TrackCandidate& trk1, 
                                          const TrackCandidate& trk2,
                                          Double_t mass1, Double_t mass2);
  
  // Calculate invariant mass assuming massless particles
  static Double_t CalculateInvariantMassMassless(const TrackCandidate& trk1,
                                                  const TrackCandidate& trk2);

private:
  TFile *inputFile;
  TTree *eventTree;
  TTree *trackTree;
  
  // Event tree branches
  Float_t ev_Vz, ev_Vx, ev_Vy, ev_Vr;
  Int_t ev_refMult, ev_runId, ev_eventId;
  Float_t ev_centrality;
  Float_t ev_Qx, ev_Qy, ev_psi2;
  Int_t ev_nTracks;
  Float_t ev_vzVpd;
  
  // Track tree branches
  Int_t tr_eventIndex;
  Float_t tr_pT, tr_eta, tr_phi;
  Short_t tr_charge;
  Short_t tr_nHitsFit, tr_nHitsMax, tr_nHitsDedx;
  Float_t tr_DCA, tr_chi2;
  Float_t tr_nSigmaPion, tr_nSigmaKaon, tr_nSigmaProton;
  Float_t tr_beta, tr_mass2;
  Bool_t tr_tofMatch;
  
  // Current loaded data
  EventCandidate currentEvent;
  std::vector<TrackCandidate> currentTracks;
  Long64_t currentEventIndex;
  
  // Helper function to load tracks for current event
  void LoadTracksForEvent(Long64_t eventIndex);
};

#endif

