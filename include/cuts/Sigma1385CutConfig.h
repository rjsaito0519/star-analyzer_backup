#ifndef SIGMA1385_CUT_CONFIG_H
#define SIGMA1385_CUT_CONFIG_H

#include "Rtypes.h"

class Sigma1385CutConfig {
public:
  static Sigma1385CutConfig& GetInstance();
  Bool_t LoadFromFile(const Char_t* filename);
  
  // Cut values (public member variables)
  Double_t nSigmaPion;
  Double_t nSigmaProton;
  Double_t nSigmaPionForSigma;
  Double_t minInvMass;
  Double_t maxInvMass;

  // Set default values
  void SetDefaults();
  
private:
  Sigma1385CutConfig();
  ~Sigma1385CutConfig();
  Sigma1385CutConfig(const Sigma1385CutConfig&);
  Sigma1385CutConfig& operator=(const Sigma1385CutConfig&);
  
  Bool_t ParseYamlFile(const Char_t* filename);
};

#endif

