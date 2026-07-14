#ifndef TRACK_CUT_CONFIG_H
#define TRACK_CUT_CONFIG_H

#include "Rtypes.h"

class TrackCutConfig {
public:
  static TrackCutConfig& GetInstance();
  Bool_t LoadFromFile(const Char_t* filename);
  
  // Cut values (public member variables)
  Int_t minNHitsFit;
  Double_t minNHitsRatio;
  Int_t minNHitsDedx;
  Double_t maxDCA;
  Double_t minEta;
  Double_t maxEta;
  Double_t minPt;
  Double_t maxPt;
  Double_t maxChi2;
  Bool_t requirePrimaryTrack;

  // Set default values
  void SetDefaults();
  
private:
  TrackCutConfig();
  ~TrackCutConfig();
  TrackCutConfig(const TrackCutConfig&);
  TrackCutConfig& operator=(const TrackCutConfig&);
  
  Bool_t ParseYamlFile(const Char_t* filename);
};

#endif

