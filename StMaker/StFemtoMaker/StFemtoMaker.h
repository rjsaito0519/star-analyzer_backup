#ifndef StFemtoMaker_h
#define StFemtoMaker_h

#include "StMaker.h"
#include "StPhiKKReconstruction.h"
#include "FemtoCandidate.h"
#include "cuts/FemtoConfig.h"
#include "StarClassLibrary/StPhysicalHelixD.hh"

#include <deque>
#include <map>
#include <string>
#include <vector>

class StPicoDst;
class StPicoDstMaker;
class StPicoEvent;
class StPicoTrack;
class TString;
class HistManager;
class CentralityHelper;
class TVector3;

class StFemtoMaker;

StFemtoMaker* createStFemtoMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName);
extern "C" void* createStFemtoMakerC(const char* name, void* picoMaker, const char* outName);

class StFemtoMaker : public StMaker {
 public:
  StFemtoMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName);
  virtual ~StFemtoMaker();

  virtual Int_t Init();
  virtual Int_t Make();
  virtual void Clear(Option_t* opt = "");
  virtual Int_t Finish();

  void WriteHistograms();

 private:
  struct TrackState {
    Float_t pT, eta, phi;
    Short_t charge;
    Float_t nSigmaKaon;
    Float_t nSigmaProton;
    Float_t DCA;
    Short_t nHitsFit, nHitsMax, nHitsDedx;
    Float_t chi2;
    Bool_t tofMatch;
    Float_t mass2;
    Float_t deltaOneOverBeta;
    Float_t tofBeta;
    Float_t originX, originY, originZ;
    Float_t momentumX, momentumY, momentumZ;
    Float_t BField;
    Int_t trackIndex;
  };

  struct He4TrackState {
    TrackState trk;
    Float_t nSigmaHe4;
  };

  struct DeuteronTrackState {
    TrackState trk;
    Float_t nSigmaDeuteron;
  };

  struct TritonTrackState {
    TrackState trk;
    Float_t nSigmaTriton;
  };

  struct He3TrackState {
    TrackState trk;
    Float_t nSigmaHe3;
  };

  struct FemtoMixingEvent {
    FemtoCandidateStore candidates;
  };

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
  Double_t m_psi2;

  FemtoCandidateStore m_eventCandidates;
  std::vector<FemtoCandidate> m_phiQaLoose;
  std::vector<FemtoCandidate> m_phiQaPreMassLoose;
  std::vector<FemtoCandidate> m_phiQaPreMassTofStrict;
  std::map<Int_t, std::deque<FemtoMixingEvent> > m_mixingPool;

  Bool_t PassEventCuts(Float_t vz, Float_t vr, Int_t refMult, Float_t vzVpd);
  Bool_t PassTrackCuts(StPicoTrack* trk, TVector3& pVtx);
  Bool_t PassKaonCuts(StPicoTrack* trk, TVector3& pVtx);
  Bool_t PassKaonMinusBaseCuts(StPicoTrack* trk, TVector3& pVtx);
  Bool_t PassProtonCuts(StPicoTrack* trk, TVector3& pVtx);

  static PhiKkTrackState ToPhiKkTrack(const TrackState& trk);
  void BuildTrackState(TrackState& track, StPicoTrack* pico, StPicoEvent* event, TVector3& pVtx, Int_t index);
  void FillTofInfo(TrackState& track, StPicoTrack* trk, const TVector3& pMom, Int_t btofIndex);
  Bool_t PassTofKaonPid(const TrackState& trk) const;
  Bool_t PassTofProtonPid(const TrackState& trk) const;
  Bool_t PassFemtoProtonCuts(const TrackState& trk) const;
  Bool_t PassFemtoKaonMinusCuts(const TrackState& trk) const;
  Bool_t PassFemtoHe4Cuts(const He4TrackState& h4) const;
  Bool_t PassFemtoDeuteronCuts(const DeuteronTrackState& d) const;
  Bool_t PassFemtoTritonCuts(const TritonTrackState& t) const;
  Bool_t PassFemtoHe3Cuts(const He3TrackState& h3) const;
  Bool_t IsKaon(const TrackState& trk);
  Bool_t IsProton(const TrackState& trk);
  Double_t ProtonRapidityCm(const TrackState& trk) const;
  Double_t KaonMinusRapidityCm(const TrackState& trk) const;
  Double_t He4RapidityCm(const TrackState& trk) const;
  Double_t DeuteronRapidityCm(const TrackState& trk) const;
  Double_t TritonRapidityCm(const TrackState& trk) const;
  Double_t He3RapidityCm(const TrackState& trk) const;
  void FillProtonPreFemtoQa(const TrackState& trk);
  void FillProtonFemtoQa(const TrackState& trk);
  void FillKaonMinusPreFemtoQa(const TrackState& trk);
  void FillKaonMinusFemtoQa(const TrackState& trk);
  void FillHe4PreFemtoQa(const TrackState& trk, Float_t nSigmaHe4);
  void FillHe4FemtoQa(const TrackState& trk);
  void FillDeuteronPreFemtoQa(const TrackState& trk, Float_t nSigmaDeuteron);
  void FillDeuteronFemtoQa(const TrackState& trk);
  void FillTritonPreFemtoQa(const TrackState& trk, Float_t nSigmaTriton);
  void FillTritonFemtoQa(const TrackState& trk);
  void FillHe3PreFemtoQa(const TrackState& trk, Float_t nSigmaHe3);
  void FillHe3FemtoQa(const TrackState& trk);
  void FillPhiPairKinematicsQa(Double_t invMass, const TVector3& phiMom, Double_t openingAngle, Double_t pairRapidity);
  void FillPhiCandidatePreCutQa(Double_t invMass, Double_t pt, Double_t pairRapidity);

  TVector3 TrackMomentum(const TrackState& trk) const;
  StPhysicalHelixD BuildHelix(const TrackState& trk);
  Double_t CalculateDCA(const TrackState& trk1, const TrackState& trk2, TVector3& dcaPos1, TVector3& dcaPos2);
  Bool_t ReconstructPhi(const TrackState& kPlus, const TrackState& kMinus, Double_t& invMass, TVector3& phiMom,
                        TVector3& dcaPosPlus, TVector3& dcaPosMinus);
  Double_t CalculateOpeningAngle(const TrackState& trk1, const TrackState& trk2);
  Double_t CalculatePairRapidity(Double_t invMass, const TVector3& phiMom);
  Double_t ApplyRapidityFrame(Double_t yLab) const;
  Bool_t PassPairTofCut(const TrackState& kPlus, const TrackState& kMinus) const;

  void BuildTrackPidCandidates(const std::string& speciesKey, const std::string& particleKey,
                               const std::vector<TrackState>& protonTracks,
                               const std::vector<TrackState>& kaonMinusTracks,
                               const std::vector<He4TrackState>& he4Tracks,
                               const std::vector<DeuteronTrackState>& deuteronTracks,
                               const std::vector<TritonTrackState>& tritonTracks,
                               const std::vector<He3TrackState>& he3Tracks, Int_t eventIndex);
  void BuildResonanceCandidates(const std::string& speciesKey, const std::string& particleKey,
                                const std::vector<TrackState>& kaonsPlus, const std::vector<TrackState>& kaonsMinus,
                                Int_t eventIndex);
  void BuildRotatedPhiCandidates(const std::string& speciesKey, const std::vector<TrackState>& kaonsPlus,
                                 const std::vector<TrackState>& kaonsMinus, Int_t eventIndex);
  FemtoCandidate MakeProtonCandidate(const TrackState& trk, Int_t eventIndex, const std::string& speciesKey) const;
  FemtoCandidate MakeKaonMinusCandidate(const TrackState& trk, Int_t eventIndex, const std::string& speciesKey) const;
  FemtoCandidate MakeHe4Candidate(const He4TrackState& trk, Int_t eventIndex, const std::string& speciesKey) const;
  FemtoCandidate MakeDeuteronCandidate(const DeuteronTrackState& trk, Int_t eventIndex,
                                       const std::string& speciesKey) const;
  FemtoCandidate MakeTritonCandidate(const TritonTrackState& trk, Int_t eventIndex,
                                     const std::string& speciesKey) const;
  FemtoCandidate MakeHe3Candidate(const He3TrackState& trk, Int_t eventIndex, const std::string& speciesKey) const;
  FemtoCandidate MakePhiCandidate(const TrackState& kPlus, const TrackState& kMinus, Double_t invMass,
                                  const TVector3& phiMom, Double_t openingAngle, Double_t pairRapidity,
                                  Double_t dcaKK, Int_t eventIndex, const std::string& speciesKey) const;

  Double_t ComputeKStar(const TLorentzVector& pA, const TLorentzVector& pB) const;
  TLorentzVector ProtonP4(const TVector3& p) const;
  TLorentzVector KaonP4(const TVector3& p) const;
  TLorentzVector CandidateP4(const FemtoCandidate& cand) const;
  Bool_t TracksOverlap(const FemtoCandidate& phiCand, const FemtoCandidate& trkCand) const;
  static Double_t ComputeMomentumAngleRad(const TVector3& pA, const TVector3& pB);
  std::string PhiPairMomAngleHistKey(const std::string& channel, Bool_t vsMkk, Bool_t tofStrict) const;
  std::string PhiPairMomAngleHistKeyWithSuffix(const std::string& channel, Bool_t vsMkk, const std::string& suffix) const;
  void FillPhiBachelorPairAngleQa();

  Int_t GetMixingBin(Float_t vz, Int_t cent9, Double_t psi2) const;
  void FillSameEventPairs(const FemtoConfig::ChannelDef& ch);
  void FillMixedEventPairs(const FemtoConfig::ChannelDef& ch, Float_t vz, Int_t cent9, Double_t psi2);
  void StoreEventForMixing(Float_t vz, Int_t cent9, Double_t psi2);
  void FillCandidateQA();
  void FillCentralityEventQA(Int_t cent9, Int_t rawMult, Double_t refMultCorr, Int_t nTracks, Int_t nBTOFMatch,
                             Int_t nKaonPlus, Int_t nKaonMinus, Int_t nPhiCandidates, Int_t nProtons, Int_t nHe4,
                             Int_t nDeuteron, Int_t nTriton, Int_t nHe3);
  std::string HistName(const std::string& prefix, const std::string& channelName) const;
};

#endif
