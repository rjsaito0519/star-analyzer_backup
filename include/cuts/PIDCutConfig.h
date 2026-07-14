#ifndef PID_CUT_CONFIG_H
#define PID_CUT_CONFIG_H

#include "Rtypes.h"
#include <string>

class PIDCutConfig {
public:
  static PIDCutConfig& GetInstance();
  Bool_t LoadFromFile(const Char_t* filename);
  
  // Cut values (public member variables)
  Double_t defaultNSigmaCut;
  Double_t nSigmaPion;
  Double_t nSigmaKaon;
  Double_t nSigmaProton;
  Double_t minMass2Pion;
  Double_t maxMass2Pion;
  Double_t minMass2Kaon;
  Double_t maxMass2Kaon;
  Double_t minMass2Proton;
  Double_t maxMass2Proton;
  Bool_t requireTOF;
  Double_t pTofFallbackMax;
  Double_t pMomKaonPID;
  Double_t maxAbsDeltaOneOverBetaKaon;
  Bool_t tofUseMass2Cut;
  Bool_t tofUseDeltaInvBetaCut;
  std::string tofFallbackMode;

  // Set default values
  void SetDefaults();
  
private:
  PIDCutConfig();
  ~PIDCutConfig();
  PIDCutConfig(const PIDCutConfig&);
  PIDCutConfig& operator=(const PIDCutConfig&);
  
  Bool_t ParseYamlFile(const Char_t* filename);
};

#endif

