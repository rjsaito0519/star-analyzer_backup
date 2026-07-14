#ifndef StLambdaMaker_h
#define StLambdaMaker_h

#include "StMaker.h"
#include "StarClassLibrary/StPhysicalHelixD.hh"
#include "TVector3.h"
#include <vector>

class StPicoDst;
class StPicoDstMaker;
class StPicoEvent;
class StPicoTrack;
class TString;
class HistManager;
class CentralityHelper;
class TVector3;

class StLambdaMaker;

StLambdaMaker* createStLambdaMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName);
extern "C" void* createStLambdaMakerC(const char* name, void* picoMaker, const char* outName);

class StLambdaMaker : public StMaker {
public:
  StLambdaMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName);
  virtual ~StLambdaMaker();

  virtual Int_t Init();
  virtual Int_t Make();
  virtual void Clear(Option_t* opt = "");
  virtual Int_t Finish();

  void WriteHistograms();
  const std::vector<TVector3>& GetLambdaMomList() const { return mLambdaMom; }
  const std::vector<Double_t>& GetLambdaInvMassList() const { return mLambdaInvMass; }
  const std::vector<Int_t>& GetLambdaProtonIdList() const { return mLambdaProtonId; }
  const std::vector<Int_t>& GetLambdaPionIdList() const { return mLambdaPionId; }

private:
  StPicoDstMaker* mPicoDstMaker;
  StPicoDst* mPicoDst;
  TString mOutName;
  Int_t mEventCounter;
  HistManager* m_histManager;
  CentralityHelper* m_centrality;
  Int_t m_cent9;
  Int_t m_cent16;
  Double_t m_refMultCorr;
  Double_t m_centWeight;
  Double_t m_centralityPercent;
  std::vector<TVector3> mLambdaMom;
  std::vector<Double_t> mLambdaInvMass;
  std::vector<Int_t> mLambdaProtonId;
  std::vector<Int_t> mLambdaPionId;

  Bool_t PassEventCuts(Int_t nTracks);
  Bool_t PassProtonCuts(StPicoTrack* trk, const TVector3& pVtx);
  Bool_t PassPionCuts(StPicoTrack* trk, const TVector3& pVtx);
  StPhysicalHelixD MakeHelix(StPicoTrack* trk, Double_t bField);
  Bool_t MakeLambdaHelix(StPicoTrack* p, StPicoTrack* pi, Double_t bField,
                         TVector3& v0, TVector3& momP, TVector3& momPi, Double_t& dca12);
  void FillLambdaCentralityQA(Int_t cent9, Int_t rawMult, Double_t refMultCorr, Int_t nTracks,
                              Int_t nBTOFMatch, Int_t nProtonCand, Int_t nPionCand, Int_t nLambdaPairs);
  void FillLambdaInvMassCentrality(Double_t invMass);
};

#endif
