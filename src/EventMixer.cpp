#include "EventMixer.h"
#include <TMath.h>
#include <TRandom.h>

EventMixer::EventMixer() {
  const auto& mixingConfig = CutConfig::Mixing::Get();
  nVzBins = mixingConfig.nVzBins;
  nCentralityBins = mixingConfig.nCentralityBins;
  nEventPlaneBins = mixingConfig.nEventPlaneBins;
  bufferSize = mixingConfig.bufferSize;
  
  // Default ranges (can be adjusted)
  vzMin = -100.0;
  vzMax = 100.0;
  centralityMin = 0.0;
  centralityMax = 100.0;
  
  InitializeMixingBins();
}

EventMixer::~EventMixer() {
  Clear();
}

void EventMixer::InitializeMixingBins() {
  mixingPool.clear();
  // Bins will be created on demand when events are added
}

Int_t EventMixer::CalculateBinIndex(Float_t vz, Float_t centrality, Float_t psi2) const {
  // Vz bin
  Int_t vzBin = (Int_t)((vz - vzMin) / (vzMax - vzMin) * nVzBins);
  if (vzBin < 0) vzBin = 0;
  if (vzBin >= nVzBins) vzBin = nVzBins - 1;
  
  // Centrality bin
  Int_t centBin = (Int_t)((centrality - centralityMin) / (centralityMax - centralityMin) * nCentralityBins);
  if (centBin < 0) centBin = 0;
  if (centBin >= nCentralityBins) centBin = nCentralityBins - 1;
  
  // Event plane bin (if used)
  Int_t epBin = 0;
  if (nEventPlaneBins > 1 && psi2 >= 0) {
    epBin = (Int_t)(psi2 / TMath::Pi() * nEventPlaneBins);
    if (epBin >= nEventPlaneBins) epBin = nEventPlaneBins - 1;
  }
  
  // Combined bin index
  Int_t binIndex = vzBin + nVzBins * (centBin + nCentralityBins * epBin);
  
  return binIndex;
}

Int_t EventMixer::GetMixingBinIndex(const EventCandidate& evt) const {
  return CalculateBinIndex(evt.Vz, evt.centrality, evt.psi2);
}

void EventMixer::AddEvent(const EventCandidate& evt, const std::vector<TrackCandidate>& tracks) {
  Int_t binIndex = GetMixingBinIndex(evt);
  
  MixingEvent mixEvt;
  mixEvt.event = evt;
  mixEvt.tracks = tracks;
  
  mixingPool[binIndex].push_back(mixEvt);
  
  // Keep only the last bufferSize events in each bin
  if ((Int_t)mixingPool[binIndex].size() > bufferSize) {
    mixingPool[binIndex].erase(mixingPool[binIndex].begin());
  }
}

const MixingEvent* EventMixer::GetRandomEvent(Int_t binIndex) const {
  auto it = mixingPool.find(binIndex);
  if (it == mixingPool.end() || it->second.empty()) {
    return 0;
  }
  
  Int_t nEvents = it->second.size();
  Int_t randomIndex = (Int_t)(gRandom->Uniform(0, nEvents));
  return &(it->second[randomIndex]);
}

std::vector<Double_t> EventMixer::GenerateMixedPairs(const std::vector<TrackCandidate>& tracks1,
                                                      const std::vector<TrackCandidate>& tracks2,
                                                      Int_t nPairs) const {
  std::vector<Double_t> masses;
  masses.reserve(nPairs);
  
  if (tracks1.empty() || tracks2.empty()) return masses;
  
  // Simple mixing: randomly select from tracks1 and tracks2
  for (Int_t i = 0; i < nPairs; i++) {
    Int_t idx1 = (Int_t)(gRandom->Uniform(0, tracks1.size()));
    Int_t idx2 = (Int_t)(gRandom->Uniform(0, tracks2.size()));
    
    // Calculate invariant mass (assuming massless for default)
    Double_t mass = TreeReader::CalculateInvariantMassMassless(tracks1[idx1], tracks2[idx2]);
    masses.push_back(mass);
  }
  
  return masses;
}

void EventMixer::Clear() {
  mixingPool.clear();
}

Int_t EventMixer::GetPoolSize(Int_t binIndex) const {
  auto it = mixingPool.find(binIndex);
  if (it == mixingPool.end()) return 0;
  return it->second.size();
}

