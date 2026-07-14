#ifndef EVENT_MIXER_H
#define EVENT_MIXER_H

#include <vector>
#include <map>
#include <TMath.h>
#include <TRandom.h>
#include "CandidateTypes.h"
#include "CutConfig.h"
#include "TreeReader.h"

// Structure to store event and its tracks for mixing
struct MixingEvent {
  EventCandidate event;
  std::vector<TrackCandidate> tracks;
};

class EventMixer {
public:
  EventMixer();
  ~EventMixer();
  
  // Initialize mixing bins
  void InitializeMixingBins();
  
  // Add event to mixing pool
  void AddEvent(const EventCandidate& evt, const std::vector<TrackCandidate>& tracks);
  
  // Get mixing bin index for an event
  Int_t GetMixingBinIndex(const EventCandidate& evt) const;
  
  // Generate mixed pairs (background)
  // Returns vector of invariant masses from mixed events
  std::vector<Double_t> GenerateMixedPairs(const std::vector<TrackCandidate>& tracks1,
                                            const std::vector<TrackCandidate>& tracks2,
                                            Int_t nPairs = 1000) const;
  
  // Generate mixed pairs with invariant mass calculation
  template<typename MassFunc>
  std::vector<Double_t> GenerateMixedPairsWithMass(const std::vector<TrackCandidate>& tracks1,
                                                    const std::vector<TrackCandidate>& tracks2,
                                                    MassFunc massFunc,
                                                    Int_t nPairs = 1000) const {
    std::vector<Double_t> masses;
    masses.reserve(nPairs);
    
    if (tracks1.empty() || tracks2.empty()) return masses;
    
    for (Int_t i = 0; i < nPairs; i++) {
      Int_t idx1 = (Int_t)(gRandom->Uniform(0, tracks1.size()));
      Int_t idx2 = (Int_t)(gRandom->Uniform(0, tracks2.size()));
      
      Double_t mass = massFunc(tracks1[idx1], tracks2[idx2]);
      masses.push_back(mass);
    }
    
    return masses;
  }
  
  // Clear mixing pool
  void Clear();
  
  // Get number of events in mixing pool
  Int_t GetPoolSize(Int_t binIndex) const;

private:
  // Mixing bins: key = binIndex, value = vector of events
  std::map<Int_t, std::vector<MixingEvent> > mixingPool;
  
  // Bin definitions
  Int_t nVzBins;
  Int_t nCentralityBins;
  Int_t nEventPlaneBins;
  Int_t bufferSize;
  
  Double_t vzMin, vzMax;
  Double_t centralityMin, centralityMax;
  
  // Calculate bin index from event properties
  Int_t CalculateBinIndex(Float_t vz, Float_t centrality, Float_t psi2 = 0.0) const;
  
  // Get random event from mixing pool
  const MixingEvent* GetRandomEvent(Int_t binIndex) const;
};

#endif

