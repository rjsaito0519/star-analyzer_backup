#ifndef ST_PHI_MESIC_NUCLEUS_MAKER_H
#define ST_PHI_MESIC_NUCLEUS_MAKER_H

#include "StMaker.h"
#include "StHypernucleusReconstructor.h"
#include "StNuclearIdHelper.h"
#include "StarClassLibrary/StPhysicalHelixD.hh"
#include "TLorentzVector.h"
#include "TString.h"
#include "TVector3.h"
#include <deque>
#include <map>
#include <string>
#include <vector>

class StPicoDst;
class StPicoDstMaker;
class StPicoEvent;
class StPicoTrack;
class HistManager;
class CentralityHelper;
class MixingConfig;
class PhiMesicNucleusConfig;

class StPhiMesicNucleusMaker : public StMaker {
public:
  StPhiMesicNucleusMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName);
  virtual ~StPhiMesicNucleusMaker();

  virtual Int_t Init();
  virtual Int_t Make();
  virtual void Clear(Option_t* opt = "");
  virtual Int_t Finish();

private:
  struct TrackRef {
    Int_t trackId;
    Int_t trackIndex;
    Int_t charge;
    std::string speciesKey;
    TLorentzVector p4;
    Double_t dcaToPv;
    Double_t m2;
    Double_t nSigma;
    Bool_t hasTof;
  };

  struct LambdaCandidate {
    TLorentzVector p4;
    Double_t invMass;
    TVector3 decayVertex;
    Int_t protonTrackId;
    Int_t protonTrackIndex;
    Int_t pionTrackId;
    Int_t pionTrackIndex;
    Double_t daughterDca;
    Double_t v0DcaToPv;
    Double_t decayLength;
    Double_t cosPointing;
    Double_t rapidity;
    Double_t pt;
    Bool_t passTopology;
    Bool_t passSignal;
    Bool_t passLeftSideband;
    Bool_t passRightSideband;
  };

  struct ParentCandidate {
    std::string channelKey;
    TLorentzVector p4;
    Double_t mass;
    Double_t deltaM;
    Double_t pt;
    Double_t rapidity;
    Double_t intermediateMass;
    std::vector<Int_t> daughterTrackIds;
    Bool_t passSignal;
    Bool_t passLeftSideband;
    Bool_t passRightSideband;
  };

  struct EventSnapshot {
    Int_t eventId;
    Int_t cent9;
    Int_t vzBin;
    std::vector<TrackRef> kplus;
    std::vector<LambdaCandidate> lambdas;
    std::vector<HyperCandidate> hyper2Body;
    std::vector<HyperCandidate> hyper3Body;
    std::vector<HyperCandidate> hyper4;
  };

  struct EventContext {
    Bool_t accepted;
    Int_t eventId;
    Int_t runId;
    Int_t cent9;
    Int_t cent16;
    Int_t rawMult;
    Double_t refMultCorr;
    Double_t centWeight;
    TVector3 pv;
    Double_t bField;
    Int_t nTracks;
  };

  StPicoDstMaker* mPicoDstMaker;
  StPicoDst* mPicoDst;
  TString mOutName;
  Int_t mEventCounter;
  HistManager* m_histManager;
  CentralityHelper* m_centrality;

  Int_t m_currentCent9;
  Int_t m_currentCent16;
  Double_t m_currentRefMultCorr;
  Double_t m_currentCentWeight;

  std::vector<TrackRef> m_kplusTracks;
  std::vector<TrackRef> m_protonTracks;       // Lambda daughters (min DCA)
  std::vector<TrackRef> m_pionMinusTracks;    // Lambda + hyper-2body pi (min DCA)
  std::vector<TrackRef> m_protonLooseTracks;  // hyper-3body: no min DCA
  std::vector<TrackRef> m_pionLooseTracks;    // hyper-3body: no min DCA
  std::vector<TrackRef> m_deuteronTracks;     // direct parent: primary-like
  std::vector<TrackRef> m_deuteronLooseTracks; // hyper-3body: no DCA cut
  std::vector<TrackRef> m_tritonTracks;       // direct parent: primary-like
  std::vector<TrackRef> m_he3Tracks;          // hyper-2body: secondary min DCA
  std::vector<TrackRef> m_he4Tracks;          // hyper-2body: secondary min DCA

  std::vector<LambdaCandidate> m_lambdas;
  std::vector<HyperCandidate> m_hypertriton2Body;
  std::vector<HyperCandidate> m_hypertriton3Body;
  std::vector<HyperCandidate> m_hyperhydrogen4;
  std::vector<ParentCandidate> m_parents;

  std::map<std::string, std::deque<EventSnapshot> > m_mixingPool;

  Bool_t BuildEventContext(EventContext& ctx);
  void SelectTracksAndPid(const EventContext& ctx);
  void SelectNuclearTracks(const EventContext& ctx);
  void BuildLambdaCandidates(const EventContext& ctx);
  void BuildHyperCandidates(const EventContext& ctx);
  void BuildParentCandidates(const EventContext& ctx);
  void FillMixedEventParents(const EventContext& ctx);
  void StoreEventForMixing(const EventContext& ctx);
  void FillQaAndPhysicsHists(const EventContext& ctx);

  Bool_t PassTrackQuality(const StPicoTrack* trk, const TVector3& pv) const;
  Bool_t PassKplusPid(const StPicoTrack* trk) const;
  Bool_t PassLambdaProtonCuts(const StPicoTrack* trk, const TVector3& pv) const;
  Bool_t PassLambdaPionCuts(const StPicoTrack* trk, const TVector3& pv) const;
  Bool_t PassHyperTwoBodyPionCuts(const StPicoTrack* trk, const TVector3& pv) const;
  Bool_t PassHyperThreeBodyLightCuts(const StPicoTrack* trk, Int_t expectedChargeSign) const;
  Bool_t PassHyperTwoBodyNuclearCuts(const StPicoTrack* trk, const TVector3& pv) const;
  Bool_t PassPrimaryLikeBachelor(const StPicoTrack* trk, const TVector3& pv) const;
  Bool_t PassNuclearDedxPid(NuclearSpecies species, const StPicoTrack* trk) const;
  Bool_t RejectSharedTracks(const std::vector<Int_t>& ids) const;
  Bool_t PassParentPvOrigin(const TrackRef& bachelor) const;

  static Int_t ComputeVzBin(Double_t vz, Double_t minVz, Double_t maxVz, Int_t nBins);
  static std::string BuildMixingKey(Int_t cent9, Int_t vzBin);

  // Lambda V0 reconstruction using LambdaCutConfig (same cuts as StLambdaMaker).
  Bool_t ReconstructLambda(const TrackRef& proton, const TrackRef& pion, const EventContext& ctx, LambdaCandidate& out);
};

#endif
