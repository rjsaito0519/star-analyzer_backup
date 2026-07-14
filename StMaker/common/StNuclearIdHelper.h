#ifndef ST_NUCLEAR_ID_HELPER_H
#define ST_NUCLEAR_ID_HELPER_H

#include "Rtypes.h"
#include <TVector3.h>
#include <TLorentzVector.h>

class StPicoDst;
class StPicoTrack;

enum NuclearSpecies { kNucDeuteron = 0, kNucTriton, kNucHe3, kNucHe4 };

struct NuclearTrackState {
  Float_t pT;
  Float_t eta;
  Float_t phi;
  Float_t pMag;
  Float_t dedx;
  Bool_t tofMatch;
  Float_t mass2;
  Float_t beta;
  Int_t trackIndex;

  NuclearTrackState()
      : pT(0.0f),
        eta(0.0f),
        phi(0.0f),
        pMag(0.0f),
        dedx(0.0f),
        tofMatch(kFALSE),
        mass2(-999.0f),
        beta(-1.0f),
        trackIndex(-1) {}
};

class StNuclearIdHelper {
 public:
  static Double_t SpeciesMass(NuclearSpecies sp);
  static Int_t ChargeNumber(NuclearSpecies sp);
  static Double_t EffectiveMomentum(Double_t pTrack, NuclearSpecies sp);
  static TVector3 EffectiveMomentum(const TVector3& pTrack, NuclearSpecies sp);
  static Double_t GetNSigma(NuclearSpecies sp, Double_t p, Double_t dedx);
  static Bool_t PassTpc(NuclearSpecies sp, const NuclearTrackState& state);
  static Bool_t PassTof(NuclearSpecies sp, const NuclearTrackState& state);
  static Bool_t IsDeuteron(const NuclearTrackState& state);
  static Bool_t IsTriton(const NuclearTrackState& state);
  static Bool_t IsHe3(const NuclearTrackState& state);
  static Bool_t IsHe4(const NuclearTrackState& state);
  static TLorentzVector NuclearP4(const TVector3& pTrack, NuclearSpecies sp);
  static void FillFromPico(NuclearTrackState& state, StPicoTrack* trk, StPicoDst* picoDst);
};

#endif
