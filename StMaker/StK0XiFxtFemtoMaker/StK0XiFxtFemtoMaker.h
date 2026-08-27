#ifndef StK0XiFxtFemtoMaker_h
#define StK0XiFxtFemtoMaker_h

#include "StMaker.h"
#include "TVector3.h"
#include "TLorentzVector.h"
#include <deque>
#include <map>
#include <vector>
#include <string>

class StPicoDst;
class StPicoDstMaker;
class StPicoEvent;
class TString;
class HistManager;
class CentralityHelper;
class StK0shortFxtMaker;
class StXiFxtMaker;

class StK0XiFxtFemtoMaker;

// Interim FXT K0s–Xi femto pairing. Long-term target: StFemtoMaker with 3-daughter cascade.
// Mixing/SB style aligned with StFemtoMaker (MixingConfig + FemtoMixingSampler).
StK0XiFxtFemtoMaker* createStK0XiFxtFemtoMaker(const char* name, StPicoDstMaker* picoMaker,
                                               StK0shortFxtMaker* k0Maker, StXiFxtMaker* xiMaker,
                                               const char* outName);
extern "C" void* createStK0XiFxtFemtoMakerC(const char* name, void* picoMaker, void* k0Maker,
                                            void* xiMaker, const char* outName);

class StK0XiFxtFemtoMaker : public StMaker {
 public:
  StK0XiFxtFemtoMaker(const char* name, StPicoDstMaker* picoMaker, StK0shortFxtMaker* k0Maker,
                      StXiFxtMaker* xiMaker, const char* outName);
  virtual ~StK0XiFxtFemtoMaker();

  virtual Int_t Init();
  virtual Int_t Make();
  virtual void Clear(Option_t* opt = "");
  virtual Int_t Finish();

  void WriteHistograms();

 private:
  struct K0Candidate {
    TVector3 mom;
    Double_t invMass;
    Int_t eventIndex;
    Int_t pionPosId;
    Int_t pionNegId;
  };

  struct XiCandidate {
    TVector3 mom;
    Double_t invMass;
    Int_t eventIndex;
    Int_t protonId;
    Int_t lambdaPionId;
    Int_t bachelorPionId;
  };

  struct FemtoMixingEvent {
    std::vector<K0Candidate> k0shorts;
    std::vector<XiCandidate> xis;
  };

  StPicoDstMaker* mPicoDstMaker;
  StPicoDst* mPicoDst;
  StK0shortFxtMaker* mK0shortMaker;
  StXiFxtMaker* mXiMaker;
  TString mOutName;
  Int_t mEventCounter;
  HistManager* m_histManager;
  CentralityHelper* m_centrality;
  Int_t m_cent9;
  Int_t m_cent16;
  Double_t m_refMultCorr;
  Double_t m_centWeight;
  Double_t m_centralityPercent;

  std::vector<K0Candidate> mK0Candidates;
  std::vector<XiCandidate> mXiCandidates;

  std::map<Int_t, std::deque<FemtoMixingEvent> > m_mixingPool;

  Bool_t PassEventCuts(Float_t vz, Float_t vr, Int_t nTracks, Int_t refMult, Float_t vzVpd);
  Double_t ComputeKStar(const TLorentzVector& pA, const TLorentzVector& pB) const;
  TLorentzVector K0shortP4(const TVector3& p) const;
  TLorentzVector XiP4(const TVector3& p) const;

  // Provenance-aware: shared only if eventIndex and trackIndex both match (StFemtoMaker style).
  Bool_t ShareTracks(const K0Candidate& k0, const XiCandidate& xi) const;
  Int_t EffectiveCentBin(Int_t cent9) const;
  Int_t GetMixingBin(Float_t vz, Int_t cent9) const;
  void FillSameEventPairs();
  void FillMixedEventPairs(Float_t vz, Int_t cent9);
  void StoreEventForMixing(Float_t vz, Int_t cent9);
};

#endif
