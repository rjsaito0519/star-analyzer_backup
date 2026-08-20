#ifndef MIXING_CONFIG_H
#define MIXING_CONFIG_H

#include "Rtypes.h"
#include <string>

class MixingConfig {
public:
  static MixingConfig& GetInstance();
  Bool_t LoadFromFile(const Char_t* filename);
  
  // Config values (public member variables)
  Int_t nVzBins;
  Int_t nCentralityBins;
  Int_t nEventPlaneBins;
  Int_t bufferSize;

  // mixingMode: randomSample (uniform subset of eligible pairs) | bufferAll (all eligible pairs)
  std::string mixingMode;
  // 0 = unlimited pairs per event (randomSample); ignored by bufferAll
  Int_t maxMixedPairsPerEvent;
  // Mix current A with buffer B and, when true, buffer A with current B
  Bool_t mixBothDirections;

  // Set default values
  void SetDefaults();
  Bool_t IsBufferAllMode() const;
  
private:
  MixingConfig();
  ~MixingConfig();
  MixingConfig(const MixingConfig&);
  MixingConfig& operator=(const MixingConfig&);
  
  Bool_t ParseYamlFile(const Char_t* filename);
};

#endif

