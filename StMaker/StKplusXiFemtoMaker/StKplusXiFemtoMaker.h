#ifndef StKplusXiFemtoMaker_h
#define StKplusXiFemtoMaker_h

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

class StKplusXiFemtoMaker;

StKplusXiFemtoMaker* createStKplusXiFemtoMaker(const char* name, StPicoDstMaker* picoMaker,
                                               StXiMaker* xiMaker, const char* outName);
extern "C" void* createStKplusXiFemtoMakerC(const char* name, void* picoMaker, void* xiMaker,
                                            const char* outName);

class StKplusXiFemtoMaker : public StMaker {
 public:
  StKplusXiFemtoMaker(const char* name, StPicoDstMaker* picoMaker, StXiMaker* xiMaker, const char* outName);
  virtual ~StKplusXiFemtoMaker();

  virtual Int_t Init();
  virtual Int_t Make();
  virtual void Clear(Option_t* opt = "");
  virtual Int_t Finish();

  void WriteHistograms();

 private:
  struct KaonPlusCandidate {
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
    std::vector<KaonPlusCandidate> kaonsPlus;
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

  std::vector<KaonPlusCandidate> mKaonPlusCandidates;
  std::vector<XiCandidate> mXiCandidates;
  std::map<Int_t, std::deque<FemtoMixingEvent> > m_mixingPool;

  Bool_t PassEventCuts(Float_t vz, Float_t vr, Int_t refMult);
  Bool_t PassKaonPlusCuts(StPicoTrack* trk, const TVector3& pVtx, Float_t& mass2, Bool_t& tofMatch) const;
  Double_t ComputeKStar(const TLorentzVector& pA, const TLorentzVector& pB) const;
  TLorentzVector KaonPlusP4(const TVector3& p) const;
  TLorentzVector XiP4(const TVector3& p) const;

  Bool_t ShareTracks(const KaonPlusCandidate& kp, const XiCandidate& xi) const;
  Int_t GetMixingBin(Float_t vz, Int_t cent9) const;
  void CollectKaonPlusCandidates(const TVector3& pVtx);
  void CollectXiCandidates();
  void FillSameEventPairs();
  void FillMixedEventPairs(Float_t vz, Int_t cent9);
  void StoreEventForMixing(Float_t vz, Int_t cent9);
};

#endif
