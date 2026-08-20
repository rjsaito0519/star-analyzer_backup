#ifndef StXiFxtMaker_h
#define StXiFxtMaker_h

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

class StXiFxtMaker;

StXiFxtMaker* createStXiFxtMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName);
extern "C" void* createStXiFxtMakerC(const char* name, void* picoMaker, const char* outName);

// FXT-common Xi reconstruction (all FXT √s). Cuts come from YAML, not from energy switches.
class StXiFxtMaker : public StMaker {
public:
  StXiFxtMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName);
  virtual ~StXiFxtMaker();

  virtual Int_t Init();
  virtual Int_t Make();
  virtual void Clear(Option_t* opt = "");
  virtual Int_t Finish();

  void WriteHistograms();
  const std::vector<TVector3>& GetXiMomList() const { return mXiMom; }
  const std::vector<Double_t>& GetXiInvMassList() const { return mXiInvMass; }
  const std::vector<Int_t>& GetXiProtonIdList() const { return mXiProtonId; }
  const std::vector<Int_t>& GetXiLambdaPionIdList() const { return mXiLambdaPionId; }
  const std::vector<Int_t>& GetXiBachelorPionIdList() const { return mXiBachelorPionId; }

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

  std::vector<TVector3> mXiMom;
  std::vector<Double_t> mXiInvMass;
  std::vector<Int_t> mXiProtonId;
  std::vector<Int_t> mXiLambdaPionId;
  std::vector<Int_t> mXiBachelorPionId;

  Bool_t PassEventCuts(Float_t vz, Float_t vr, Int_t nTracks, Int_t refMult, Float_t vzVpd);
  Bool_t PassProtonCuts(StPicoTrack* trk, const TVector3& pVtx);
  Bool_t PassLambdaPionCuts(StPicoTrack* trk, const TVector3& pVtx);
  Bool_t PassBachelorPionCuts(StPicoTrack* trk, const TVector3& pVtx);

  StPhysicalHelixD MakeHelix(StPicoTrack* trk, Double_t bField);
  Bool_t MakeLambdaHelix(StPicoTrack* p, StPicoTrack* pi, Double_t bField,
                         TVector3& v2, TVector3& momP, TVector3& momPi, Double_t& dca12);
  Bool_t MakeXiHelix(const TVector3& v2, const TVector3& momLam, StPicoTrack* pi_bach, Double_t bField,
                     TVector3& v1, TVector3& momXi, Double_t& dcaCascade, Double_t& pathLengthLam);

  void FillXiCentralityQA(Int_t cent9, Int_t rawMult, Double_t refMultCorr, Int_t nTracks,
                          Int_t nBTOFMatch, Int_t nProtonCand, Int_t nPionCand, Int_t nXiPairs);
};

#endif
