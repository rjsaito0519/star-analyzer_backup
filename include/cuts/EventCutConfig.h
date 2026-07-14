#ifndef EVENT_CUT_CONFIG_H
#define EVENT_CUT_CONFIG_H

#include "Rtypes.h"

class EventCutConfig {
public:
  static EventCutConfig& GetInstance();
  Bool_t LoadFromFile(const Char_t* filename);
  
  // Cut values (public member variables)
  Double_t minVz;  // cm; event accepted when minVz <= vz <= maxVz
  Double_t maxVz;
  Double_t maxVr;
  Double_t vtxCenterX;  // cm; beam spot center for Vr cut
  Double_t vtxCenterY;  // cm
  Double_t minRefMult;
  Double_t maxRefMult;
  Double_t maxVzDiff;
  Double_t maxAbsVzVpd;  // cm; use VPD (e.g. for hVzDiff / vzDiff cut) only when |vzVpd| < this
  Int_t maxNTr;  // 0 or negative = no limit; event skipped when nTracks > maxNTr

  // Set default values
  void SetDefaults();

  Double_t ComputeVr(Double_t vx, Double_t vy) const;
  
private:
  EventCutConfig();
  ~EventCutConfig();
  EventCutConfig(const EventCutConfig&);  // Copy constructor (disabled)
  EventCutConfig& operator=(const EventCutConfig&);  // Assignment operator (disabled)
  
  Bool_t ParseYamlFile(const Char_t* filename);
};

#endif

