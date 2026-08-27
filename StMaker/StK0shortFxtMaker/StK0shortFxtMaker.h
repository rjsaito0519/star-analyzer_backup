#ifndef StK0shortFxtMaker_h
#define StK0shortFxtMaker_h

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

class StK0shortFxtMaker;

// FXT K0s reconstruction. Daughter IDs are pico array indices (same space as StXiFxtMaker).
// Optional StXiFxtMaker* enables same-event daughter-share veto at reconstruction time.
StK0shortFxtMaker* createStK0shortFxtMaker(const char* name, StPicoDstMaker* picoMaker,
                                           StXiFxtMaker* xiMaker, const char* outName);
extern "C" void* createStK0shortFxtMakerC(const char* name, void* picoMaker, void* xiMaker,
                                          const char* outName);

class StK0shortFxtMaker : public StMaker {
public:
  StK0shortFxtMaker(const char* name, StPicoDstMaker* picoMaker, StXiFxtMaker* xiMaker,
                    const char* outName);
  virtual ~StK0shortFxtMaker();

  virtual Int_t Init();
  virtual Int_t Make();
  virtual void Clear(Option_t* opt = "");
  virtual Int_t Finish();

  void WriteHistograms();
  const std::vector<TVector3>& GetK0shortMomList() const { return mK0shortMom; }
  const std::vector<Double_t>& GetK0shortInvMassList() const { return mK0shortInvMass; }
  const std::vector<Int_t>& GetK0shortPionPosIdList() const { return mK0shortPionPosId; }
  const std::vector<Int_t>& GetK0shortPionNegIdList() const { return mK0shortPionNegId; }

private:
  StPicoDstMaker* mPicoDstMaker;
  StPicoDst* mPicoDst;
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
  std::vector<TVector3> mK0shortMom;
  std::vector<Double_t> mK0shortInvMass;
  std::vector<Int_t> mK0shortPionPosId;
  std::vector<Int_t> mK0shortPionNegId;

  Bool_t PassEventCuts(Float_t vz, Float_t vr, Int_t nTracks, Int_t refMult, Float_t vzVpd);
  Bool_t PassPionPosCuts(StPicoTrack* trk, const TVector3& pVtx);
  Bool_t PassPionNegCuts(StPicoTrack* trk, const TVector3& pVtx);
  Bool_t TrackUsedByXi(Int_t trackIndex) const;
  StPhysicalHelixD MakeHelix(StPicoTrack* trk, Double_t bField);
  Bool_t MakeK0shortHelix(StPicoTrack* pip, StPicoTrack* pim, Double_t bField, TVector3& v0,
                          TVector3& momPip, TVector3& momPim, Double_t& dca12);
  void FillK0shortCentralityQA(Int_t cent9, Int_t rawMult, Double_t refMultCorr, Int_t nTracks,
                               Int_t nBTOFMatch, Int_t nPionPosCand, Int_t nPionNegCand,
                               Int_t nK0shortPairs);
  void FillK0shortInvMassCentrality(Double_t invMass);
};

#endif
