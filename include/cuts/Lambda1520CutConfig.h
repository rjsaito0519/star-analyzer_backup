#ifndef LAMBDA1520_CUT_CONFIG_H
#define LAMBDA1520_CUT_CONFIG_H

#include "Rtypes.h"

class Lambda1520CutConfig {
public:
  static Lambda1520CutConfig& GetInstance();
  Bool_t LoadFromFile(const Char_t* filename);
  
  // Cut values (public member variables)
  Double_t nSigmaProton;
  Double_t nSigmaKaon;
  Double_t minInvMass;
  Double_t maxInvMass;

  // Set default values
  void SetDefaults();
  
private:
  Lambda1520CutConfig();
  ~Lambda1520CutConfig();
  Lambda1520CutConfig(const Lambda1520CutConfig&);
  Lambda1520CutConfig& operator=(const Lambda1520CutConfig&);
  
  Bool_t ParseYamlFile(const Char_t* filename);
};

#endif

