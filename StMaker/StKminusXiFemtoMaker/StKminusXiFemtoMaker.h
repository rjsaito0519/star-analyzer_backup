#ifndef StKminusXiFemtoMaker_h
#define StKminusXiFemtoMaker_h

#include "StMaker.h"
#include "TLorentzVector.h"
#include "TVector3.h"
#include <deque>
#include <map>
#include <string>
#include <vector>

class StPicoDst;
class StPicoDstMaker;
class StPicoTrack;
class TString;
class HistManager;
class CentralityHelper;
class StXiMaker;

class StKminusXiFemtoMaker;

StKminusXiFemtoMaker* createStKminusXiFemtoMaker(const char* name, StPicoDstMaker* picoMaker,
                                               StXiMaker* xiMaker, const char* outName);
extern "C" void* createStKminusXiFemtoMakerC(const char* name, void* picoMaker, void* xiMaker,
                                            const char* outName);

class StKminusXiFemtoMaker : public StMaker {
 public:
  StKminusXiFemtoMaker(const char* name, StPicoDstMaker* picoMaker, StXiMaker* xiMaker, const char* outName);
  virtual ~StKminusXiFemtoMaker();

  virtual Int_t Init();
  virtual Int_t Make();
  virtual void Clear(Option_t* opt = "");
  virtual Int_t Finish();

  void WriteHistograms();

 private:
  struct KaonMinusCandidate {
    TVector3 mom;
    Int_t trackId;
    Float_t mass2;
    Bool_t tofMatch;
  };

  struct XiCandidate {
    TVector3 mom;
    Double_t invMass;
    Int_t protonId;
    Int_t lambdaPionId;
    Int_t bachelorPionId;
  };

  struct FemtoMixingEvent {
    std::vector<KaonMinusCandidate> kaonsMinus;
    std::vector<XiCandidate> xis;
  };

  StPicoDstMaker* mPicoDstMaker;
  StPicoDst* mPicoDst;
  StXiMaker* mXiMaker;
  TString mOutName;
  Int_t mEventCounter;
  HistManager* m_histManager;
  CentralityHelper* m_centrality;
  Int_t m_cent9;
  Int_t m_cent16;
  Double_t m_refMultCorr;
  Double_t m_centWeight;
  Double_t m_centralityPercent;

  std::vector<KaonMinusCandidate> mKaonMinusCandidates;
  std::vector<XiCandidate> mXiCandidates;
  std::map<Int_t, std::deque<FemtoMixingEvent> > m_mixingPool;

  Bool_t PassEventCuts(Float_t vz, Float_t vr, Int_t refMult);
  Bool_t PassKaonMinusCuts(StPicoTrack* trk, const TVector3& pVtx, Float_t& mass2, Bool_t& tofMatch) const;
  Double_t ComputeKStar(const TLorentzVector& pA, const TLorentzVector& pB) const;
  TLorentzVector KaonMinusP4(const TVector3& p) const;
  TLorentzVector XiP4(const TVector3& p) const;

  Bool_t ShareTracks(const KaonMinusCandidate& kp, const XiCandidate& xi) const;
  Int_t GetMixingBin(Float_t vz, Int_t cent9) const;
  void CollectKaonMinusCandidates(const TVector3& pVtx);
  void CollectXiCandidates();
  void FillSameEventPairs();
  void FillMixedEventPairs(Float_t vz, Int_t cent9);
  void StoreEventForMixing(Float_t vz, Int_t cent9);
};

#endif
