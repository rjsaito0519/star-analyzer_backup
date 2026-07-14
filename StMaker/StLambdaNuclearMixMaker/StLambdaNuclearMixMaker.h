#ifndef StLambdaNuclearMixMaker_h
#define StLambdaNuclearMixMaker_h

#include "StMaker.h"
#include "TVector3.h"
#include "TString.h"
#include <vector>
#include <deque>
#include <map>

class StLambdaMaker;
class StNuclearIdMaker;
class HistManager;
class CentralityHelper;
class StPicoEvent;

struct MixLambdaCand {
  TVector3 momentum;
  Double_t invMass;
};

struct MixNuclearCand {
  TVector3 momentum;
  Int_t type; // 0=d, 1=t, 2=3He, 3=4He
};

struct MixedEvent {
  std::vector<MixLambdaCand> lambdas;
  std::vector<MixNuclearCand> nuclei;
};

class StLambdaNuclearMixMaker : public StMaker {
 public:
  StLambdaNuclearMixMaker(const char* name, const char* outName);
  virtual ~StLambdaNuclearMixMaker();

  virtual Int_t Init();
  virtual Int_t Make();
  virtual void Clear(Option_t* opt = "");
  virtual Int_t Finish();

  void WriteHistograms();

 private:
  TString mOutName;
  HistManager* m_histManager;
  CentralityHelper* m_centrality;

  std::map<Int_t, std::deque<MixedEvent> > m_mixingPool;
  Int_t m_maxMixEvents;
  Int_t m_bufferSize;

  Int_t m_cent9;
  Int_t m_cent16;
  Double_t m_refMultCorr;
  Double_t m_centWeight;

  Int_t GetMixingBin(Float_t vz, Int_t cent9) const;
  void FillKstarMixed(Double_t k_star, Double_t q_lab, Int_t type, Int_t cent9 = -1);
  void FillKstarMixedSideband(Double_t k_star, Double_t q_lab, Int_t type, Int_t sbSign, Int_t cent9 = -1);
  void FillKstarMassMixed(Double_t k_star, Double_t invMass, Int_t type, Int_t cent9 = -1);
};

// Factory for CINT
StLambdaNuclearMixMaker* createStLambdaNuclearMixMaker(const char* name, const char* outName);
extern "C" void* createStLambdaNuclearMixMakerC(const char* name, const char* outName);

#endif
