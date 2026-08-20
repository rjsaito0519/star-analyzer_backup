#ifndef ST_HYPERNUCLEUS_RECONSTRUCTOR_H
#define ST_HYPERNUCLEUS_RECONSTRUCTOR_H

#include "StNuclearIdHelper.h"
#include "Rtypes.h"
#include "TLorentzVector.h"
#include "TVector3.h"
#include <vector>

struct HyperDaughterRef {
  Int_t trackId;
  Int_t trackIndex;
  Int_t charge;
  Int_t speciesCode;
  TLorentzVector p4;
};

struct HyperCandidate {
  Int_t motherType;  // 3 = 3LambdaH, 4 = 4LambdaH
  std::vector<HyperDaughterRef> daughters;
  TLorentzVector motherP4;
  Double_t rawMass;
  TVector3 decayVertex;
  Double_t daughterDca;
  Double_t decayLength;
  Double_t transverseDecayLength;
  Double_t dcaToPv;
  Double_t cosPointing;
  Double_t pt;
  Double_t y;
  Double_t eta;
  Double_t phi;
  Bool_t passTopology;
  Bool_t passSignal;
  Bool_t passLeftSideband;
  Bool_t passRightSideband;

  HyperCandidate()
      : motherType(0),
        rawMass(0.0),
        daughterDca(0.0),
        decayLength(0.0),
        transverseDecayLength(0.0),
        dcaToPv(0.0),
        cosPointing(-2.0),
        pt(0.0),
        y(0.0),
        eta(0.0),
        phi(0.0),
        passTopology(kFALSE),
        passSignal(kFALSE),
        passLeftSideband(kFALSE),
        passRightSideband(kFALSE) {}
};

class StPicoTrack;

class StHypernucleusReconstructor {
public:
  struct TopologyCuts {
    Double_t minDaughterDca;
    Double_t maxDaughterDca;
    Double_t maxDcaToPv;
    Double_t minDecayLength;
    Double_t minTransverseDecayLength;
    Double_t minCosPointing;
    Double_t maxAbsRapidity;
    Double_t maxPathLength;  // |helix pathLengths| upper bound (Lambda-like)
  };

  struct MassWindow {
    Double_t signalMin;
    Double_t signalMax;
    Double_t leftSbMin;
    Double_t leftSbMax;
    Double_t rightSbMin;
    Double_t rightSbMax;
  };

  static Bool_t BuildTwoBody(const StPicoTrack* nucTrack,
                             NuclearSpecies nucSpecies,
                             Int_t nucTrackIndex,
                             const StPicoTrack* pionTrack,
                             Int_t pionTrackIndex,
                             const TVector3& primaryVertex,
                             Double_t bField,
                             const TopologyCuts& cuts,
                             const MassWindow& window,
                             HyperCandidate& out);

  static Bool_t BuildThreeBodyDaughter(const TLorentzVector& deuteronP4,
                                       Int_t deuteronId,
                                       Int_t deuteronTrackIdx,
                                       const TLorentzVector& protonP4,
                                       Int_t protonId,
                                       Int_t protonTrackIdx,
                                       const TLorentzVector& pionP4,
                                       Int_t pionId,
                                       Int_t pionTrackIdx,
                                       const TVector3& primaryVertex,
                                       const TopologyCuts& cuts,
                                       const MassWindow& window,
                                       HyperCandidate& out);
};

#endif
