#ifndef CENTRALITY_CUT_CONFIG_H
#define CENTRALITY_CUT_CONFIG_H

#include "Rtypes.h"
#include <vector>
#include <string>

class CentralityCutConfig {
public:
  static CentralityCutConfig& GetInstance();
  Bool_t LoadFromFile(const Char_t* filename);

  void SetDefaults();

  Bool_t enabled;
  std::string mode;  // "refmult" or "fxtmult"
  Bool_t rejectBadRun;
  Bool_t rejectPileup;
  Bool_t useWeight;
  Bool_t requireValidCentrality;
  Bool_t verbose;
  Bool_t fillCentralityQA;
  std::vector<Int_t> acceptedCentBins;

  // Optional: reject events in cent9MaxRefMultCorrBin when refMult_corr > cent9MaxRefMultCorr.
  // Disabled when cent9MaxRefMultCorrBin < 0 or cent9MaxRefMultCorr <= 0.
  Int_t cent9MaxRefMultCorrBin;
  Double_t cent9MaxRefMultCorr;

  Bool_t IsCentBinAccepted(Int_t cent9) const;
  Bool_t IsRefMultCorrAccepted(Int_t cent9, Double_t refMultCorr) const;

private:
  CentralityCutConfig();
  ~CentralityCutConfig();
  CentralityCutConfig(const CentralityCutConfig&);
  CentralityCutConfig& operator=(const CentralityCutConfig&);

  Bool_t ParseYamlFile(const Char_t* filename);
  static void ParseAcceptedBins(const std::string& value, std::vector<Int_t>& out);
};

#endif
