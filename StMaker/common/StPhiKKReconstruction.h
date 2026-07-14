#ifndef ST_PHI_KK_RECONSTRUCTION_H
#define ST_PHI_KK_RECONSTRUCTION_H

#include "Rtypes.h"
#include "StarClassLibrary/StPhysicalHelixD.hh"

class TVector3;
class StPicoBTofPidTraits;

/** Shared track fields for phi -> K+ K- reconstruction (helix, TOF, kinematics). */
struct PhiKkTrackState {
  Float_t pT;
  Float_t eta;
  Float_t phi;
  Short_t charge;
  Float_t originX;
  Float_t originY;
  Float_t originZ;
  Float_t momentumX;
  Float_t momentumY;
  Float_t momentumZ;
  Float_t BField;
  Bool_t tofMatch;
  Float_t mass2;
  Float_t deltaOneOverBeta;
};

namespace StPhiKKReconstruction {

Double_t KaonMass();

TVector3 TrackMomentum(const PhiKkTrackState& trk);
StPhysicalHelixD BuildHelix(const PhiKkTrackState& trk);
Double_t CalculateDCA(const PhiKkTrackState& trk1, const PhiKkTrackState& trk2, TVector3& dcaPos1,
                      TVector3& dcaPos2);
Bool_t ReconstructPhi(const PhiKkTrackState& kPlus, const PhiKkTrackState& kMinus, Double_t& invMass,
                     TVector3& phiMom, TVector3& dcaPosPlus, TVector3& dcaPosMinus);
Double_t CalculateInvariantMass(const PhiKkTrackState& trk1, const PhiKkTrackState& trk2, Double_t mass1,
                                Double_t mass2);
Double_t CalculateOpeningAngle(const PhiKkTrackState& trk1, const PhiKkTrackState& trk2);
Double_t CalculatePairRapidity(Double_t invMass, const TVector3& phiMom);
Double_t ApplyRapidityFrame(Double_t yLab);

Bool_t InKaonMass2Window(Float_t mass2);
Bool_t PassTofKaonPid(const PhiKkTrackState& trk);
Bool_t PassKplusTofMass2(Float_t pMag, Bool_t tofMatch, Float_t mass2);
Bool_t PassKminusTofMass2(Float_t pMag, Bool_t tofMatch, Float_t mass2);
Bool_t PassPairTofCut(const PhiKkTrackState& kPlus, const PhiKkTrackState& kMinus);

void FillTofInfo(Float_t& mass2, Float_t& deltaOneOverBeta, Bool_t& tofMatch, StPicoBTofPidTraits* tof,
                 const TVector3& pMom);

}  // namespace StPhiKKReconstruction

#endif
