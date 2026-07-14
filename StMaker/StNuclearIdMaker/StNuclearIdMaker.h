#ifndef StNuclearIdMaker_h
#define StNuclearIdMaker_h

#include "StMaker.h"
#include "TString.h"
#include "TVector3.h"
#include <vector>

class StPicoDst;
class StPicoDstMaker;
class StPicoEvent;
class StPicoTrack;
class StPicoBTofPidTraits;
class HistManager;
class CentralityHelper;

class StNuclearIdMaker : public StMaker {
 public:
  StNuclearIdMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName);
  virtual ~StNuclearIdMaker();

  virtual Int_t Init();
  virtual Int_t Make();
  virtual void Clear(Option_t* opt = "");
  virtual Int_t Finish();

  void WriteHistograms();
  const std::vector<TVector3>& GetNuclearMomList() const { return mNuclearMom; }
  const std::vector<Int_t>& GetNuclearIdList() const { return mNuclearId; }
  const std::vector<Int_t>& GetNuclearTypeList() const { return mNuclearType; }

  void FillKstar(Double_t k_star, Double_t q_lab, Int_t type, Int_t cent9 = -1);
  void FillKstarSideband(Double_t k_star, Double_t q_lab, Int_t type, Int_t sbSign, Int_t cent9 = -1);
  void FillKstarMass(Double_t k_star, Double_t invMass, Int_t type, Int_t cent9 = -1);

 private:
  StPicoDstMaker* mPicoDstMaker;
  StPicoDst* mPicoDst;
  TString mOutName;
  HistManager* m_histManager;
  std::vector<TVector3> mNuclearMom;
  std::vector<Int_t> mNuclearId;
  std::vector<Int_t> mNuclearType;

  CentralityHelper* m_centrality;
  Int_t m_cent9;
  Int_t m_cent16;
  Double_t m_refMultCorr;
  Double_t m_centWeight;
  Double_t m_centralityPercent;
};

// Factory for CINT
StNuclearIdMaker* createStNuclearIdMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName);
extern "C" void* createStNuclearIdMakerC(const char* name, void* picoMaker, const char* outName);

#endif
