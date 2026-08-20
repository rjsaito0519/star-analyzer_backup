#include "StHypernucleusReconstructor.h"

#include "StPicoEvent/StPicoTrack.h"
#include "StarClassLibrary/StPhysicalHelixD.hh"
#include "StarClassLibrary/StThreeVectorF.hh"
#include "StarClassLibrary/SystemOfUnits.h"

#include "TMath.h"

namespace {
const Double_t kPionMass = 0.139570;

StPhysicalHelixD BuildHelixFromPico(const StPicoTrack* trk, Double_t bField) {
  // Prefer global momentum (Lambda-like V0 daughters).
  TVector3 g = trk->gMom();
  if (g.Mag() < 1e-6) g = trk->pMom();
  StThreeVectorF p(g.X(), g.Y(), g.Z());
  StThreeVectorF o(trk->origin().X(), trk->origin().Y(), trk->origin().Z());
  return StPhysicalHelixD(p, o, bField * units::kilogauss, (Float_t)trk->charge());
}

void FillSidebandFlags(HyperCandidate& out, const StHypernucleusReconstructor::MassWindow& window) {
  out.passSignal = (out.rawMass >= window.signalMin && out.rawMass <= window.signalMax);
  out.passLeftSideband = (out.rawMass >= window.leftSbMin && out.rawMass <= window.leftSbMax);
  out.passRightSideband = (out.rawMass >= window.rightSbMin && out.rawMass <= window.rightSbMax);
}
}  // namespace

Bool_t StHypernucleusReconstructor::BuildTwoBody(const StPicoTrack* nucTrack,
                                                 NuclearSpecies nucSpecies,
                                                 Int_t nucTrackIndex,
                                                 const StPicoTrack* pionTrack,
                                                 Int_t pionTrackIndex,
                                                 const TVector3& primaryVertex,
                                                 Double_t bField,
                                                 const TopologyCuts& cuts,
                                                 const MassWindow& window,
                                                 HyperCandidate& out) {
  if (!nucTrack || !pionTrack) return kFALSE;
  if (pionTrack->charge() >= 0) return kFALSE;

  StPhysicalHelixD hNuc = BuildHelixFromPico(nucTrack, bField);
  StPhysicalHelixD hPi = BuildHelixFromPico(pionTrack, bField);
  std::pair<Double_t, Double_t> s = hNuc.pathLengths(hPi);
  if (TMath::Abs(s.first) > cuts.maxPathLength || TMath::Abs(s.second) > cuts.maxPathLength) return kFALSE;

  StThreeVectorD dcaA = hNuc.at(s.first);
  StThreeVectorD dcaB = hPi.at(s.second);

  TVector3 decayVertex((dcaA.x() + dcaB.x()) * 0.5, (dcaA.y() + dcaB.y()) * 0.5, (dcaA.z() + dcaB.z()) * 0.5);
  const Double_t dcaDaughters = (dcaA - dcaB).mag();
  if (dcaDaughters < cuts.minDaughterDca || dcaDaughters > cuts.maxDaughterDca) return kFALSE;

  StThreeVectorD pNucAt = hNuc.momentumAt(s.first, bField * units::kilogauss);
  StThreeVectorD pPiAt = hPi.momentumAt(s.second, bField * units::kilogauss);
  TVector3 pNucTrack(pNucAt.x(), pNucAt.y(), pNucAt.z());
  TVector3 pPi(pPiAt.x(), pPiAt.y(), pPiAt.z());

  TLorentzVector p4Nuc = StNuclearIdHelper::NuclearP4(pNucTrack, nucSpecies);
  TLorentzVector p4Pi;
  p4Pi.SetVectM(pPi, kPionMass);
  TLorentzVector p4Mother = p4Nuc + p4Pi;

  TVector3 motherP3 = p4Mother.Vect();
  if (motherP3.Mag() < 1e-5) return kFALSE;
  TVector3 flight = decayVertex - primaryVertex;
  const Double_t dcaToPv = (primaryVertex - decayVertex).Cross(motherP3.Unit()).Mag();
  if (dcaToPv > cuts.maxDcaToPv) return kFALSE;

  const Double_t decayLength = flight.Mag();
  const Double_t transDecay = flight.Perp();
  const Double_t cosPointing = (flight.Mag() > 0.0)
                                   ? flight.Dot(motherP3) / (flight.Mag() * motherP3.Mag())
                                   : -2.0;
  if (cosPointing < cuts.minCosPointing) return kFALSE;
  if (decayLength < cuts.minDecayLength) return kFALSE;
  if (transDecay < cuts.minTransverseDecayLength) return kFALSE;
  if (TMath::Abs(p4Mother.Rapidity()) > cuts.maxAbsRapidity) return kFALSE;

  out = HyperCandidate();
  out.motherType = (nucSpecies == kNucHe4) ? 4 : 3;
  out.motherP4 = p4Mother;
  out.rawMass = p4Mother.M();
  out.decayVertex = decayVertex;
  out.daughterDca = dcaDaughters;
  out.decayLength = decayLength;
  out.transverseDecayLength = transDecay;
  out.dcaToPv = dcaToPv;
  out.cosPointing = cosPointing;
  out.pt = p4Mother.Pt();
  out.y = p4Mother.Rapidity();
  out.eta = p4Mother.Eta();
  out.phi = p4Mother.Phi();
  out.passTopology = kTRUE;

  HyperDaughterRef d1;
  d1.trackId = nucTrack->id();
  d1.trackIndex = nucTrackIndex;
  d1.charge = nucTrack->charge();
  d1.speciesCode = (int)nucSpecies;
  d1.p4 = p4Nuc;
  out.daughters.push_back(d1);

  HyperDaughterRef d2;
  d2.trackId = pionTrack->id();
  d2.trackIndex = pionTrackIndex;
  d2.charge = pionTrack->charge();
  d2.speciesCode = 211;
  d2.p4 = p4Pi;
  out.daughters.push_back(d2);

  FillSidebandFlags(out, window);
  return kTRUE;
}

Bool_t StHypernucleusReconstructor::BuildThreeBodyDaughter(const TLorentzVector& deuteronP4,
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
                                                           HyperCandidate& out) {
  // 3-body: no Lambda-style daughter/V0 DCA. Topology is rapidity only.
  // PV-origin of (hyper + K+) is enforced at parent-candidate level.
  TLorentzVector mother = deuteronP4 + protonP4 + pionP4;
  if (TMath::Abs(mother.Rapidity()) > cuts.maxAbsRapidity) return kFALSE;

  out = HyperCandidate();
  out.motherType = 3;
  out.motherP4 = mother;
  out.rawMass = mother.M();
  out.decayVertex = primaryVertex;
  out.daughterDca = 0.0;
  out.decayLength = 0.0;
  out.transverseDecayLength = 0.0;
  out.dcaToPv = 0.0;
  out.cosPointing = 1.0;
  out.pt = mother.Pt();
  out.y = mother.Rapidity();
  out.eta = mother.Eta();
  out.phi = mother.Phi();
  out.passTopology = kTRUE;

  HyperDaughterRef d;
  d.trackId = deuteronId;
  d.trackIndex = deuteronTrackIdx;
  d.charge = 1;
  d.speciesCode = (int)kNucDeuteron;
  d.p4 = deuteronP4;
  out.daughters.push_back(d);

  d.trackId = protonId;
  d.trackIndex = protonTrackIdx;
  d.charge = 1;
  d.speciesCode = 2212;
  d.p4 = protonP4;
  out.daughters.push_back(d);

  d.trackId = pionId;
  d.trackIndex = pionTrackIdx;
  d.charge = -1;
  d.speciesCode = -211;
  d.p4 = pionP4;
  out.daughters.push_back(d);

  FillSidebandFlags(out, window);
  return kTRUE;
}
