#ifndef V0_CUT_CONFIG_H
#define V0_CUT_CONFIG_H

#include "Rtypes.h"

class V0CutConfig {
public:
  static V0CutConfig& GetInstance();
  Bool_t LoadFromFile(const Char_t* filename);
  
  // Cut values (public member variables)
  Double_t minDaughterDCA;
  Double_t maxDaughterDCA;
  Double_t minDecayLength;
  Double_t maxDecayLength;
  Double_t minPointingAngle;
  Double_t maxPointingAngle;
  Double_t maxDCAtoPV;
  Double_t lambdaMassWindow;
  Double_t lambdaMass;

  // Set default values
  void SetDefaults();
  
private:
  V0CutConfig();
  ~V0CutConfig();
  V0CutConfig(const V0CutConfig&);
  V0CutConfig& operator=(const V0CutConfig&);
  
  Bool_t ParseYamlFile(const Char_t* filename);
};

#endif

