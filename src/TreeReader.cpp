#include "TreeReader.h"
#include <TMath.h>
#include <iostream>

TreeReader::TreeReader() 
  : inputFile(0), eventTree(0), trackTree(0), currentEventIndex(-1) {
}

TreeReader::~TreeReader() {
  CloseFile();
}

Bool_t TreeReader::OpenFile(const Char_t *filename) {
  inputFile = TFile::Open(filename, "READ");
  if (!inputFile || inputFile->IsZombie()) {
    std::cerr << "ERROR: Cannot open file " << filename << std::endl;
    return kFALSE;
  }
  
  eventTree = (TTree*)inputFile->Get("EventTree");
  trackTree = (TTree*)inputFile->Get("TrackTree");
  
  if (!eventTree || !trackTree) {
    std::cerr << "ERROR: Cannot find EventTree or TrackTree in file" << std::endl;
    return kFALSE;
  }
  
  // Set branch addresses for event tree
  eventTree->SetBranchAddress("Vz", &ev_Vz);
  eventTree->SetBranchAddress("Vx", &ev_Vx);
  eventTree->SetBranchAddress("Vy", &ev_Vy);
  eventTree->SetBranchAddress("Vr", &ev_Vr);
  eventTree->SetBranchAddress("refMult", &ev_refMult);
  eventTree->SetBranchAddress("runId", &ev_runId);
  eventTree->SetBranchAddress("eventId", &ev_eventId);
  eventTree->SetBranchAddress("centrality", &ev_centrality);
  eventTree->SetBranchAddress("Qx", &ev_Qx);
  eventTree->SetBranchAddress("Qy", &ev_Qy);
  eventTree->SetBranchAddress("psi2", &ev_psi2);
  eventTree->SetBranchAddress("nTracks", &ev_nTracks);
  eventTree->SetBranchAddress("vzVpd", &ev_vzVpd);
  
  // Set branch addresses for track tree
  trackTree->SetBranchAddress("eventIndex", &tr_eventIndex);
  trackTree->SetBranchAddress("pT", &tr_pT);
  trackTree->SetBranchAddress("eta", &tr_eta);
  trackTree->SetBranchAddress("phi", &tr_phi);
  trackTree->SetBranchAddress("charge", &tr_charge);
  trackTree->SetBranchAddress("nHitsFit", &tr_nHitsFit);
  trackTree->SetBranchAddress("nHitsMax", &tr_nHitsMax);
  trackTree->SetBranchAddress("nHitsDedx", &tr_nHitsDedx);
  trackTree->SetBranchAddress("DCA", &tr_DCA);
  trackTree->SetBranchAddress("chi2", &tr_chi2);
  trackTree->SetBranchAddress("nSigmaPion", &tr_nSigmaPion);
  trackTree->SetBranchAddress("nSigmaKaon", &tr_nSigmaKaon);
  trackTree->SetBranchAddress("nSigmaProton", &tr_nSigmaProton);
  trackTree->SetBranchAddress("beta", &tr_beta);
  trackTree->SetBranchAddress("mass2", &tr_mass2);
  trackTree->SetBranchAddress("tofMatch", &tr_tofMatch);
  
  return kTRUE;
}

void TreeReader::CloseFile() {
  if (inputFile) {
    inputFile->Close();
    delete inputFile;
    inputFile = 0;
  }
  eventTree = 0;
  trackTree = 0;
  currentTracks.clear();
  currentEventIndex = -1;
}

Bool_t TreeReader::LoadEvent(Long64_t eventIndex) {
  if (!eventTree || eventIndex < 0 || eventIndex >= eventTree->GetEntries()) {
    return kFALSE;
  }
  
  eventTree->GetEntry(eventIndex);
  
  // Fill event structure
  currentEvent.Vz = ev_Vz;
  currentEvent.Vx = ev_Vx;
  currentEvent.Vy = ev_Vy;
  currentEvent.Vr = ev_Vr;
  currentEvent.refMult = ev_refMult;
  currentEvent.runId = ev_runId;
  currentEvent.eventId = ev_eventId;
  currentEvent.centrality = ev_centrality;
  currentEvent.Qx = ev_Qx;
  currentEvent.Qy = ev_Qy;
  currentEvent.psi2 = ev_psi2;
  currentEvent.nTracks = ev_nTracks;
  currentEvent.vzVpd = ev_vzVpd;
  
  // Load tracks for this event
  LoadTracksForEvent(eventIndex);
  
  currentEventIndex = eventIndex;
  return kTRUE;
}

void TreeReader::LoadTracksForEvent(Long64_t eventIndex) {
  currentTracks.clear();
  
  if (!trackTree) return;
  
  Long64_t nTracks = trackTree->GetEntries();
  for (Long64_t i = 0; i < nTracks; i++) {
    trackTree->GetEntry(i);
    if (tr_eventIndex == eventIndex) {
      TrackCandidate trk;
      trk.eventIndex = tr_eventIndex;
      trk.pT = tr_pT;
      trk.eta = tr_eta;
      trk.phi = tr_phi;
      trk.charge = tr_charge;
      trk.nHitsFit = tr_nHitsFit;
      trk.nHitsMax = tr_nHitsMax;
      trk.nHitsDedx = tr_nHitsDedx;
      trk.DCA = tr_DCA;
      trk.chi2 = tr_chi2;
      trk.nSigmaPion = tr_nSigmaPion;
      trk.nSigmaKaon = tr_nSigmaKaon;
      trk.nSigmaProton = tr_nSigmaProton;
      trk.beta = tr_beta;
      trk.mass2 = tr_mass2;
      trk.tofMatch = tr_tofMatch;
      
      currentTracks.push_back(trk);
    }
  }
}

Bool_t TreeReader::PassEventCuts(const EventCandidate& evt) const {
  const auto& eventCuts = CutConfig::Event::Get();
  if (evt.Vz < eventCuts.minVz || evt.Vz > eventCuts.maxVz) return kFALSE;
  if (evt.Vr > eventCuts.maxVr) return kFALSE;
  if (evt.refMult < eventCuts.minRefMult) return kFALSE;
  if (evt.refMult > eventCuts.maxRefMult) return kFALSE;
  if (TMath::Abs(evt.Vz - evt.vzVpd) > eventCuts.maxVzDiff && 
      TMath::Abs(evt.vzVpd) < 200) return kFALSE;
  return kTRUE;
}

Bool_t TreeReader::PassTrackCuts(const TrackCandidate& trk) const {
  const auto& trackCuts = CutConfig::Track::Get();
  if (trk.nHitsFit < trackCuts.minNHitsFit) return kFALSE;
  if ((Float_t)trk.nHitsFit / (Float_t)trk.nHitsMax < trackCuts.minNHitsRatio) return kFALSE;
  if (trk.nHitsDedx < trackCuts.minNHitsDedx) return kFALSE;
  if (trk.DCA > trackCuts.maxDCA) return kFALSE;
  if (TMath::Abs(trk.eta) > trackCuts.maxEta) return kFALSE;
  if (trk.pT < trackCuts.minPt) return kFALSE;
  if (trk.pT > trackCuts.maxPt) return kFALSE;
  if (trk.chi2 > trackCuts.maxChi2) return kFALSE;
  return kTRUE;
}

Bool_t TreeReader::IsPion(const TrackCandidate& trk, Bool_t useTOF) const {
  if (!PassTrackCuts(trk)) return kFALSE;
  const auto& pidCuts = CutConfig::PID::Get();
  if (TMath::Abs(trk.nSigmaPion) > pidCuts.nSigmaPion) return kFALSE;
  
  if (useTOF && trk.tofMatch) {
    if (trk.mass2 < pidCuts.minMass2Pion || 
        trk.mass2 > pidCuts.maxMass2Pion) return kFALSE;
  }
  
  return kTRUE;
}

Bool_t TreeReader::IsKaon(const TrackCandidate& trk, Bool_t useTOF) const {
  if (!PassTrackCuts(trk)) return kFALSE;
  const auto& pidCuts = CutConfig::PID::Get();
  if (TMath::Abs(trk.nSigmaKaon) > pidCuts.nSigmaKaon) return kFALSE;
  
  if (useTOF && trk.tofMatch) {
    if (trk.mass2 < pidCuts.minMass2Kaon || 
        trk.mass2 > pidCuts.maxMass2Kaon) return kFALSE;
  }
  
  return kTRUE;
}

Bool_t TreeReader::IsProton(const TrackCandidate& trk, Bool_t useTOF) const {
  if (!PassTrackCuts(trk)) return kFALSE;
  const auto& pidCuts = CutConfig::PID::Get();
  if (TMath::Abs(trk.nSigmaProton) > pidCuts.nSigmaProton) return kFALSE;
  
  if (useTOF && trk.tofMatch) {
    if (trk.mass2 < pidCuts.minMass2Proton || 
        trk.mass2 > pidCuts.maxMass2Proton) return kFALSE;
  }
  
  return kTRUE;
}

std::vector<TrackCandidate> TreeReader::GetPionCandidates(Bool_t useTOF) const {
  std::vector<TrackCandidate> candidates;
  for (const auto& trk : currentTracks) {
    if (IsPion(trk, useTOF)) {
      candidates.push_back(trk);
    }
  }
  return candidates;
}

std::vector<TrackCandidate> TreeReader::GetKaonCandidates(Bool_t useTOF) const {
  std::vector<TrackCandidate> candidates;
  for (const auto& trk : currentTracks) {
    if (IsKaon(trk, useTOF)) {
      candidates.push_back(trk);
    }
  }
  return candidates;
}

std::vector<TrackCandidate> TreeReader::GetProtonCandidates(Bool_t useTOF) const {
  std::vector<TrackCandidate> candidates;
  for (const auto& trk : currentTracks) {
    if (IsProton(trk, useTOF)) {
      candidates.push_back(trk);
    }
  }
  return candidates;
}

Double_t TreeReader::CalculateInvariantMass(const TrackCandidate& trk1,
                                            const TrackCandidate& trk2,
                                            Double_t mass1, Double_t mass2) {
  TVector3 p1 = trk1.GetMomentum();
  TVector3 p2 = trk2.GetMomentum();
  
  Double_t E1 = TMath::Sqrt(mass1*mass1 + p1.Mag2());
  Double_t E2 = TMath::Sqrt(mass2*mass2 + p2.Mag2());
  
  Double_t E = E1 + E2;
  TVector3 p = p1 + p2;
  
  return TMath::Sqrt(E*E - p.Mag2());
}

Double_t TreeReader::CalculateInvariantMassMassless(const TrackCandidate& trk1,
                                                     const TrackCandidate& trk2) {
  TVector3 p1 = trk1.GetMomentum();
  TVector3 p2 = trk2.GetMomentum();
  
  TVector3 p = p1 + p2;
  Double_t p1Mag = p1.Mag();
  Double_t p2Mag = p2.Mag();
  
  // For massless particles: m^2 = 2*p1*p2*(1 - cos(theta))
  Double_t cosTheta = (p1 * p2) / (p1Mag * p2Mag);
  return TMath::Sqrt(2.0 * p1Mag * p2Mag * (1.0 - cosTheta));
}

