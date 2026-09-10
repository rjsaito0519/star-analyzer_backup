#include "StPicoKFParticleInterface.h"

#include "KFParticle.h"
#include "KFParticleFinder.h"
#include "KFParticleTopoReconstructor.h"
#include "KFParticleSnapshot.h"
#include "KFPTrack.h"
#include "KFPVertex.h"
#include "KFVertex.h"
#include "cuts/KfParticleCutConfig.h"
#include "StBichsel/StdEdxPull.h"
#include "StPicoEvent/StPicoBTofPidTraits.h"
#include "StPicoEvent/StPicoDst.h"
#include "StPicoEvent/StPicoEvent.h"
#include "StPicoEvent/StPicoTrack.h"
#include "TVector3.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

namespace kfp = star_analyzer_kfp;

namespace {
const int kPdg[3] = {211, 321, 2212};
// PDG hypotheses used by the reference StPicoTrack::dEdxPull calls, not cuts.
const Float_t kMass[3] = {0.139570f, 0.493677f, 0.938272f};

bool FiniteVector(const TVector3& v) {
  return std::isfinite(v.X()) && std::isfinite(v.Y()) && std::isfinite(v.Z());
}

bool FiniteParticle(const kfp::KFParticle& p) {
  for (int i = 0; i < 8; ++i)
    if (!std::isfinite(p.Parameters()[i])) return false;
  for (int i = 0; i < 36; ++i)
    if (!std::isfinite(p.CovarianceMatrix()[i])) return false;
  return std::isfinite(p.GetChi2());
}

double Polynomial(const std::vector<double>& coefficients, int row, double p) {
  double result = coefficients[row * 5 + 4];
  for (int term = 3; term >= 0; --term)
    result = result * p + coefficients[row * 5 + term];
  return result;
}

struct TrackPid {
  TrackPid() : index(-1), hasTof(false), m2(0.), dcaToPv(0.) {
    for (int i = 0; i < 3; ++i) tpc[i] = tof[i] = 0.;
  }
  int index;
  bool hasTof;
  double m2, dcaToPv, tpc[3], tof[3];
};
}

KfParticleEventStats::KfParticleEventStats()
    : rawTracks(0), qualityTracks(0), covarianceTracks(0), pidTracks(0),
      pidHypotheses(0), protonHypotheses(0), pionHypotheses(0),
      unknownHypotheses(0), primaryHypotheses(0), finderParticles(0),
      lambdaParticles(0), validCandidates(0), xiParticles(0),
      validXiCandidates(0), invalidXiCandidates(0), invalidCandidates(0),
      invalidCovariance(0), invalidPid(0), invalidTrackIds(0), tofTracks(0) {}

class StPicoKFParticleInterface::Impl {
public:
  explicit Impl(const KfParticleCutConfig& configuration)
      : cuts(configuration), helper(configuration) {
    std::ostringstream errors;
    if (!cuts.Validate(errors)) {
      configurationError = errors.str();
      return;
    }
    // For NDF=2 the inverse chi-square survival probability is exactly
    // -2 ln(prob). SetChi2PrimaryCut also sets Finder ChiPrimary2D, hence the
    // explicit Finder value MUST be applied after this probability setting.
    topo.SetChi2PrimaryCut(-2. * std::log(cuts.primaryProbCut));
    kfp::KFParticleFinder* finder = topo.GetKFParticleFinder();
    finder->SetMaxDistanceBetweenParticlesCut(cuts.finderMaxDaughterDistance);
    finder->SetLCut(cuts.finderLCut);
    finder->SetChiPrimaryCut2D(cuts.finderChiPrimary2D);
    finder->SetChi2Cut2D(cuts.finderChi2Ndf2D);
    finder->SetLdLCut2D(cuts.finderLdL2D);
    finder->SetLdLCutXiOmega(cuts.finderLdLXiOmega);
    finder->SetChi2TopoCutXiOmega(cuts.finderChi2TopoXiOmega);
    finder->SetChi2CutXiOmega(cuts.finderChi2XiOmega);
    // Ξ reconstruction requires Λ in the Finder list.
    finder->AddDecayToReconstructionList(3122);
    if (cuts.reconstructAntiLambda || cuts.reconstructAntiXi)
      finder->AddDecayToReconstructionList(-3122);
    if (cuts.reconstructXi) finder->AddDecayToReconstructionList(3312);
    if (cuts.reconstructAntiXi) finder->AddDecayToReconstructionList(-3312);
  }

  void Clear() {
    stats = KfParticleEventStats();
    candidates.clear();
    xiCandidates.clear();
    error.clear();
    trackPid.clear();
    topo.Clear();
  }

  bool Fail(const std::string& reason) {
    candidates.clear();
    xiCandidates.clear();
    error = reason;
    return false;
  }

  bool Quality(const StPicoTrack& track) const {
    if (cuts.selectionProfile == "lambda_imp5") {
      // Match StLambdaMaker, not the generic (unused there) track YAML.
      if (!track.charge() || track.nHitsFit() < cuts.minNHitsFit) return false;
      if (track.nHitsMax() > 0 &&
          double(track.nHitsFit()) / double(track.nHitsMax()) < cuts.minNHitsRatio)
        return false;
      // Numerical safety for KF conversion, not an extra pT/eta acceptance.
      const TVector3 p = track.gMom();
      return FiniteVector(p) && p.Mag2() > 0.;
    }
    if (!track.charge() || track.nHitsFit() < cuts.minNHitsFit ||
        track.nHitsDedx() < cuts.minNHitsDedx) return false;
    if (cuts.minNHitsRatio > 0. &&
        (track.nHitsMax() <= 0 ||
         track.nHitsFit() < cuts.minNHitsRatio * track.nHitsMax())) return false;
    const double dedxError = track.dEdxError();
    if (!std::isfinite(dedxError) || dedxError < cuts.minDedxError ||
        dedxError > cuts.maxDedxError) return false;
    const TVector3 p = track.gMom();
    if (!FiniteVector(p) || !std::isfinite(p.Eta()) || p.Mag2() <= 0.) return false;
    return p.Pt() >= cuts.minPt && p.Pt() <= cuts.maxPt &&
           p.Eta() >= cuts.minEta && p.Eta() <= cuts.maxEta;
  }

  bool Pid(StPicoDst* dst, const StPicoTrack& track, double momentum,
           TrackPid& result, std::vector<int>& hypotheses) {
    if (cuts.selectionProfile == "lambda_imp5") {
      // Independent stored nSigma tests, inclusive at the legacy boundary.
      // No kaon/PID competition or TOF lookup, even with a stored TOF index.
      result.tpc[0] = track.nSigmaPion();
      result.tpc[2] = track.nSigmaProton();
      const TVector3 pv = dst->event()->primaryVertex();
      result.dcaToPv = track.gDCA(pv.X(), pv.Y(), pv.Z());
      if (!std::isfinite(result.dcaToPv)) return false;
      for (int species = 0; species < 3; species += 2) {
        if (!cuts.reconstructAntiLambda &&
            ((species == 0 && track.charge() > 0) ||
             (species == 2 && track.charge() < 0))) continue;
        const double limit = species == 0 ? cuts.nSigmaPion : cuts.nSigmaProton;
        const double minimumDca = species == 0 ? cuts.imp5MinDCAPion : cuts.imp5MinDCAProton;
        if (!std::isfinite(result.tpc[species]) ||
            std::fabs(result.tpc[species]) > limit || result.dcaToPv < minimumDca) continue;
        hypotheses.push_back(kPdg[species] * (track.charge() > 0 ? 1 : -1));
      }
      if (hypotheses.empty()) hypotheses.push_back(-1);
      return true;
    }
    if (cuts.pidProfile == "dedx_pull") {
      if (!std::isfinite(track.dEdx()) || track.dEdx() <= 0. ||
          !std::isfinite(track.dEdxError()) || track.dEdxError() <= 0.) return false;
      // Identical units, fit=1 and |charge|=1 to the SL24y __TFG__ guarded
      // StPicoTrack::dEdxPull, called through public APIs only.
      const Float_t picoMomentum = track.gMom().Mag();
      const Float_t measured = 1.e-6 * track.dEdx();
      const Float_t resolution = track.dEdxError();
      for (int species = 0; species < 3; ++species) {
        const Float_t betaGamma = picoMomentum / kMass[species];
        result.tpc[species] = static_cast<Float_t>(
            StdEdxPull::Eval(measured, resolution, betaGamma, 1, 1));
      }
    } else if (cuts.pidProfile == "pico_nsigma") {
      result.tpc[0] = track.nSigmaPion();
      result.tpc[1] = track.nSigmaKaon();
      result.tpc[2] = track.nSigmaProton();
    } else {
      error = "Unknown KF PID profile: " + cuts.pidProfile;
      return false;
    }
    for (int species = 0; species < 3; ++species)
      if (!std::isfinite(result.tpc[species]) || result.tpc[species] == -999.)
        return false; // -999 is the SL24y failed-pull sentinel, not a measurement.

    const int tofIndex = track.bTofPidTraitsIndex();
    if (cuts.useTof && tofIndex >= 0) {
      if (static_cast<unsigned int>(tofIndex) >= dst->numberOfBTofPidTraits()) {
        error = "BTofPidTraits index is out of range; check the enabled branch and input consistency";
        return false;
      }
      const StPicoBTofPidTraits* tof = dst->btofPidTraits(tofIndex);
      if (!tof) {
        error = "BTofPidTraits contains a null entry referenced by a track";
        return false;
      }
      const double beta = tof->btofBeta();
      // A missing/invalid beta is not a measurement. In soft mode it does not
      // veto an otherwise valid TPC hypothesis; valid beta can exceed one.
      if (std::isfinite(beta) && beta > 0. && beta * beta > 1.e-6) {
        result.m2 = momentum * momentum * (1. / (beta * beta) - 1.);
        result.hasTof = std::isfinite(result.m2);
      }
    }

    bool tofAccepted[3] = {false, false, false};
    if (result.hasTof) {
      ++stats.tofTracks;
      const double x = std::min(momentum, cuts.tofPMax);
      int bestSpecies = 0;
      for (int species = 0; species < 3; ++species) {
        const int row = species + (track.charge() < 0 ? 3 : 0);
        const double mean = Polynomial(cuts.tofMean, row, x);
        const double sigma = Polynomial(cuts.tofSigma, row, x);
        if (!std::isfinite(mean) || !std::isfinite(sigma) || sigma <= 0.) {
          error = "TOF calibration gives an invalid width; check the selected profile and coefficients";
          return false;
        }
        result.tof[species] = (result.m2 - mean) / sigma;
        if (!std::isfinite(result.tof[species])) return false;
        tofAccepted[species] = std::fabs(result.tof[species]) < cuts.tofNSigma;
        if (std::fabs(result.tof[species]) < std::fabs(result.tof[bestSpecies]))
          bestSpecies = species;
      }
      if (cuts.strictTofPid)
        for (int species = 0; species < 3; ++species)
          if (species != bestSpecies) tofAccepted[species] = false;
    }
    const double tpcCut[3] = {cuts.nSigmaPion, cuts.nSigmaKaon, cuts.nSigmaProton};
    const bool requireKaonTof = cuts.cleanKaonsWithTof &&
        momentum > cuts.tofKaonPMin && momentum < cuts.tofKaonPMax;
    for (int species = 0; species < 3; ++species) {
      if (std::fabs(result.tpc[species]) >= tpcCut[species]) continue;
      if (species == 1 && requireKaonTof && !tofAccepted[1]) continue;
      if (result.hasTof && !tofAccepted[species]) continue;
      hypotheses.push_back(kPdg[species] * (track.charge() > 0 ? 1 : -1));
    }
    // Preserve the reference Interface's unknown-PID input hypothesis. It is
    // not counted as accepted p/pi PID and does not silently become a proton.
    if (hypotheses.empty()) hypotheses.push_back(-1);
    return true;
  }

  bool Candidate(const kfp::KFParticle& raw,
                 const std::vector<kfp::KFParticle>& particles,
                 const kfp::KFVertex& pv, const TVector3& picoPv,
                 KfLambdaCandidate& out, StPicoDst* dst) {
    if (raw.NDaughters() != 2 || !FiniteParticle(raw) || raw.GetNDF() <= 0 ||
        raw.GetChi2() < 0.f) return false;
    const kfp::KFParticle* proton = 0;
    const kfp::KFParticle* pion = 0;
    const int sign = raw.GetPDG() > 0 ? 1 : -1;
    for (int daughter = 0; daughter < 2; ++daughter) {
      const int particleIndex = raw.DaughterIds()[daughter];
      if (particleIndex < 0 || static_cast<size_t>(particleIndex) >= particles.size())
        return false;
      const kfp::KFParticle& particle = particles[particleIndex];
      if (particle.NDaughters() != 1) return false;
      if (particle.GetPDG() == 2212 * sign && particle.GetQ() == sign)
        proton = &particle;
      else if (particle.GetPDG() == -211 * sign && particle.GetQ() == -sign)
        pion = &particle;
      else return false;
    }
    if (!proton || !pion) return false;
    out.protonId = proton->DaughterIds()[0];
    out.pionId = pion->DaughterIds()[0];
    const std::map<int, TrackPid>::const_iterator protonInput = trackPid.find(out.protonId);
    const std::map<int, TrackPid>::const_iterator pionInput = trackPid.find(out.pionId);
    if (out.protonId == out.pionId || protonInput == trackPid.end() ||
        pionInput == trackPid.end()) return false;
    const TrackPid& protonPid = protonInput->second;
    const TrackPid& pionPid = pionInput->second;
    out.protonIndex = protonPid.index;
    out.pionIndex = pionPid.index;
    if (cuts.selectionProfile == "lambda_imp5") {
      out.protonDcaToPv = protonPid.dcaToPv;
      out.pionDcaToPv = pionPid.dcaToPv;
      helper.FillImp5PathLengths(dst, out);
    }
    if (out.protonIndex == out.pionIndex) return false;
    out.protonPidPull = protonPid.tpc[2];
    out.pionPidPull = pionPid.tpc[0];
    out.protonHasTof = protonPid.hasTof;
    out.pionHasTof = pionPid.hasTof;
    out.protonTofPull = protonPid.tof[2];
    out.pionTofPull = pionPid.tof[0];
    out.protonTofM2 = protonPid.m2;
    out.pionTofM2 = pionPid.m2;
    out.pdg = raw.GetPDG();
    out.x = raw.GetX(); out.y = raw.GetY(); out.z = raw.GetZ();
    out.px = raw.GetPx(); out.py = raw.GetPy(); out.pz = raw.GetPz();
    out.chi2Ndf = raw.GetChi2() / raw.GetNDF();
    if (raw.GetMass(out.mass, out.massError) != 0 || out.massError <= 0.f) return false;
    out.daughterDistance = proton->GetDistanceFromParticle(*pion);
    out.distanceToPv = raw.GetDistanceFromVertex(pv);
    raw.GetDistanceToVertexLine(pv, out.vertexLineLength, out.vertexLineLengthError);
    if (out.vertexLineLengthError <= 0.f) return false;
    out.vertexLineLengthSignificance = out.vertexLineLength / out.vertexLineLengthError;
    // Never constrain the parent mass used for the spectrum. Constrain only a
    // distinct copy to the input production vertex for topology/lifetime QA.
    kfp::KFParticle topological = raw;
    topological.SetProductionVertex(pv);
    if (!FiniteParticle(topological) || topological.GetNDF() <= 0 ||
        topological.GetChi2() < 0.f) return false;
    out.topoChi2Ndf = topological.GetChi2() / topological.GetNDF();
    if (topological.GetDecayLength(out.decayLength, out.decayLengthError) != 0 ||
        out.decayLengthError <= 0.f) return false;
    out.decayLengthSignificance = out.decayLength / out.decayLengthError;
    const TVector3 flight(out.x - picoPv.X(), out.y - picoPv.Y(), out.z - picoPv.Z());
    const TVector3 momentum(out.px, out.py, out.pz);
    const double denominator = flight.Mag() * momentum.Mag();
    if (!std::isfinite(denominator) || denominator <= 0.) return false;
    out.cosPointing = std::max(-1., std::min(1., flight.Dot(momentum) / denominator));
    const float values[] = {out.mass, out.massError, out.chi2Ndf, out.topoChi2Ndf,
      out.daughterDistance, out.distanceToPv, out.vertexLineLength,
      out.vertexLineLengthError, out.vertexLineLengthSignificance, out.decayLength,
      out.decayLengthError, out.decayLengthSignificance, out.cosPointing};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
      if (!std::isfinite(values[i])) return false;
    return true;
  }

  bool XiCandidate(const kfp::KFParticle& raw,
                   const std::vector<kfp::KFParticle>& particles,
                   const kfp::KFVertex& pv, const TVector3& picoPv,
                   KfXiCandidate& out, StPicoDst* dst) {
    // Minimal parent-only fill first so mass QA works even if daughter-id
    // bookkeeping differs across Finder versions. Optional daughter recovery
    // follows when NDaughters==2 with {bachelor track id, Lambda particle index}.
    if (!FiniteParticle(raw) || raw.GetNDF() <= 0 || raw.GetChi2() < 0.f) return false;
    const int sign = raw.GetPDG() > 0 ? 1 : -1;
    out = KfXiCandidate();
    out.pdg = raw.GetPDG();
    out.lambdaPdg = 3122 * sign;
    out.x = raw.GetX(); out.y = raw.GetY(); out.z = raw.GetZ();
    out.px = raw.GetPx(); out.py = raw.GetPy(); out.pz = raw.GetPz();
    out.chi2Ndf = raw.GetChi2() / raw.GetNDF();
    if (raw.GetMass(out.mass, out.massError) != 0 || out.massError <= 0.f) return false;
    out.distanceToPv = raw.GetDistanceFromVertex(pv);
    raw.GetDistanceToVertexLine(pv, out.vertexLineLength, out.vertexLineLengthError);
    if (out.vertexLineLengthError <= 0.f) return false;
    out.vertexLineLengthSignificance = out.vertexLineLength / out.vertexLineLengthError;
    kfp::KFParticle topological = raw;
    topological.SetProductionVertex(pv);
    if (!FiniteParticle(topological) || topological.GetNDF() <= 0 ||
        topological.GetChi2() < 0.f) return false;
    out.topoChi2Ndf = topological.GetChi2() / topological.GetNDF();
    if (topological.GetDecayLength(out.decayLength, out.decayLengthError) != 0 ||
        out.decayLengthError <= 0.f) return false;
    out.decayLengthSignificance = out.decayLength / out.decayLengthError;
    const TVector3 flight(out.x - picoPv.X(), out.y - picoPv.Y(), out.z - picoPv.Z());
    const TVector3 momentum(out.px, out.py, out.pz);
    const double denominator = flight.Mag() * momentum.Mag();
    if (!std::isfinite(denominator) || denominator <= 0.) return false;
    out.cosPointing = std::max(-1., std::min(1., flight.Dot(momentum) / denominator));
    out.daughterDistance = -1.f;
    out.lambdaMass = 0.f;
    out.lambdaMassError = -1.f;

    if (raw.NDaughters() == 2 && dst) {
      const int id0 = raw.DaughterIds()[0];
      const int id1 = raw.DaughterIds()[1];
      const kfp::KFParticle* lambda = 0;
      int bachelorTrackId = -1;
      if (id1 >= 0 && static_cast<size_t>(id1) < particles.size() &&
          particles[id1].GetPDG() == 3122 * sign) {
        lambda = &particles[id1];
        bachelorTrackId = id0;
      } else if (id0 >= 0 && static_cast<size_t>(id0) < particles.size() &&
                 particles[id0].GetPDG() == 3122 * sign) {
        lambda = &particles[id0];
        bachelorTrackId = id1;
      }
      if (lambda) {
        // Intermediate Lambda mass is available as soon as the V0 daughter is
        // identified; do not wait for full bachelor/proton/pion ID recovery.
        float unused = 0.f;
        if (lambda->GetMass(out.lambdaMass, unused) == 0) {
          out.lambdaMassError = unused;
        }
        out.daughterDistance = lambda->GetDistanceFromVertex(pv);
      }
      if (lambda && lambda->NDaughters() == 2 && bachelorTrackId >= 0) {
        const kfp::KFParticle* proton = 0;
        const kfp::KFParticle* pion = 0;
        for (int daughter = 0; daughter < 2; ++daughter) {
          const int particleIndex = lambda->DaughterIds()[daughter];
          if (particleIndex < 0 || static_cast<size_t>(particleIndex) >= particles.size())
            continue;
          const kfp::KFParticle& particle = particles[particleIndex];
          if (particle.NDaughters() != 1) continue;
          if (particle.GetPDG() == 2212 * sign && particle.GetQ() == sign)
            proton = &particle;
          else if (particle.GetPDG() == -211 * sign && particle.GetQ() == -sign)
            pion = &particle;
        }
        const std::map<int, TrackPid>::const_iterator bachelorInput =
            trackPid.find(bachelorTrackId);
        if (proton && pion && bachelorInput != trackPid.end()) {
          out.protonId = proton->DaughterIds()[0];
          out.pionId = pion->DaughterIds()[0];
          out.bachelorId = bachelorTrackId;
          const std::map<int, TrackPid>::const_iterator protonInput = trackPid.find(out.protonId);
          const std::map<int, TrackPid>::const_iterator pionInput = trackPid.find(out.pionId);
          if (protonInput != trackPid.end() && pionInput != trackPid.end()) {
            out.protonIndex = protonInput->second.index;
            out.pionIndex = pionInput->second.index;
            out.bachelorIndex = bachelorInput->second.index;
            out.protonPidPull = protonInput->second.tpc[2];
            out.pionPidPull = pionInput->second.tpc[0];
            out.bachelorPidPull = bachelorInput->second.tpc[0];
          }
        }
      }
    }

    const float values[] = {out.mass, out.massError, out.chi2Ndf, out.topoChi2Ndf,
      out.distanceToPv, out.vertexLineLength, out.vertexLineLengthError,
      out.vertexLineLengthSignificance, out.decayLength, out.decayLengthError,
      out.decayLengthSignificance, out.cosPointing};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i)
      if (!std::isfinite(values[i])) return false;
    return true;
  }

  bool ProcessEvent(StPicoDst* dst) {
    Clear();
    if (!configurationError.empty()) return Fail(configurationError);
    std::ostringstream errors;
    if (!cuts.Validate(errors)) return Fail(errors.str());
    if (!dst || !dst->picoArray(StPicoArrays::Event) || !dst->event())
      return Fail("PicoDst or Event is missing");
    if (!dst->picoArray(StPicoArrays::Track) ||
        !dst->picoArray(StPicoArrays::TrackCovMatrix))
      return Fail("Track and TrackCovMatrix branch arrays are required");
    if (cuts.useTof && !dst->picoArray(StPicoArrays::BTofPidTraits))
      return Fail("The enabled TOF profile requires the BTofPidTraits branch array");
    stats.rawTracks = dst->numberOfTracks();
    if (dst->numberOfTrackCovMatrices() != stats.rawTracks)
      return Fail("TrackCovMatrix count differs from Track count; the covariance branch is required");
    if (cuts.useTof && (cuts.tofMean.size() != 30 || cuts.tofSigma.size() != 30))
      return Fail("KF TOF calibration requires 30 mean and 30 width coefficients");
    const StPicoEvent* event = dst->event();
    const TVector3 position = event->primaryVertex();
    const TVector3 uncertainty = event->primaryVertexError();
    if (!FiniteVector(position) || !FiniteVector(uncertainty) ||
        uncertainty.X() <= 0. || uncertainty.Y() <= 0. || uncertainty.Z() <= 0. ||
        !std::isfinite(event->bField()))
      return Fail("Input primary vertex, its errors, or the magnetic field is invalid");
    // Set the current event's field BEFORE track-to-PV deviations are computed.
    // It is in PicoDst/STAR units (kG), with no additional Tesla conversion.
    topo.SetField(event->bField());
    kfp::KFPVertex vertex;
    vertex.SetXYZ(position.X(), position.Y(), position.Z());
    vertex.SetCovarianceMatrix(uncertainty.X() * uncertainty.X(), 0.f,
        uncertainty.Y() * uncertainty.Y(), 0.f, 0.f, uncertainty.Z() * uncertainty.Z());
    const kfp::KFVertex pv(vertex);
    std::vector<kfp::KFParticle> inputs;
    std::vector<int> pdgs, hftHits, primaryTracks;
    inputs.reserve(stats.rawTracks * 3);
    pdgs.reserve(stats.rawTracks * 3);
    hftHits.reserve(stats.rawTracks * 3);
    std::map<int, int> originalIds;
    for (UInt_t index = 0; index < stats.rawTracks; ++index) {
      const StPicoTrack* track = dst->track(index);
      if (!track) return Fail("Track branch contains a null entry");
      // IDs need not be contiguous or equal to the PicoDst array index.
      if (!originalIds.insert(std::make_pair(track->id(), index)).second) {
        ++stats.invalidTrackIds;
        return Fail("Duplicate StPicoTrack::id(): cannot unambiguously recover daughters");
      }
      if (!Quality(*track)) continue;
      const int nHft = static_cast<int>(track->hasPxl1Hit()) +
          static_cast<int>(track->hasPxl2Hit()) + static_cast<int>(track->hasIstHit());
      if (cuts.useHftTracksOnly && nHft < 3) continue;
      ++stats.qualityTracks;
      kfp::KFPTrack input;
      if (!helper.BuildTrack(dst, index, input)) { ++stats.invalidCovariance; continue; }
      ++stats.covarianceTracks;
      TrackPid pid;
      pid.index = index;
      std::vector<int> hypotheses;
      if (!Pid(dst, *track, input.GetP(), pid, hypotheses)) {
        ++stats.invalidPid;
        if (!error.empty()) return Fail(error);
        continue;
      }
      trackPid.insert(std::make_pair(track->id(), pid));
      if (hypotheses[0] != -1) ++stats.pidTracks;
      for (size_t h = 0; h < hypotheses.size(); ++h) {
        const int pdg = hypotheses[h];
        kfp::KFParticle particle(input, pdg);
        const float chiPrimary = particle.GetDeviationFromVertex(pv);
        if (!FiniteParticle(particle) || !std::isfinite(chiPrimary) || chiPrimary < 0.f) {
          ++stats.invalidPid;
          continue;
        }
        // As in reference AddTrackToParticleList: do NOT square this API's
        // return value. The opt-in Imp5 profile separately selects original
        // daughter gDCA in PID; the default reference has no such prefilter.
        if (chiPrimary < cuts.interfaceChiPrimaryCut) {
          primaryTracks.push_back(inputs.size());
          ++stats.primaryHypotheses;
        }
        particle.SetId(track->id());
        inputs.push_back(particle);
        pdgs.push_back(pdg);
        hftHits.push_back(nHft);
        ++stats.pidHypotheses;
        if (std::abs(pdg) == 2212) ++stats.protonHypotheses;
        if (std::abs(pdg) == 211) ++stats.pionHypotheses;
        if (pdg == -1) ++stats.unknownHypotheses;
      }
    }
    // Same STAR Interface sequence, with the external Pico PV. No PV refit,
    // custom p x pi loop, extra competition pass, or scalar fallback is performed.
    // Upstream ReconstructParticles itself runs SelectParticleCandidates:
    // Lambda must already pass PV-constrained chi2/NDF < 3. Keep that behavior.
    topo.CleanPV();
    topo.Init(inputs, &pdgs, &hftHits);
    topo.GetKFParticleFinder()->Init(topo.NPrimaryVertices());
    topo.FillPVIndices();
    topo.AddPV(pv, primaryTracks);
    topo.FillPVIndices();
    topo.SortTracks();
    topo.ReconstructParticles();
    const std::vector<kfp::KFParticle>& particles = topo.GetParticles();
    stats.finderParticles = particles.size();
    for (size_t i = 0; i < particles.size(); ++i) {
      const int pdg = particles[i].GetPDG();
      if (std::abs(pdg) == 3122) {
        ++stats.lambdaParticles;
        KfLambdaCandidate candidate;
        if (!Candidate(particles[i], particles, pv, position, candidate, dst)) {
          ++stats.invalidCandidates;
          continue;
        }
        candidates.push_back(candidate);
      } else if (std::abs(pdg) == 3312) {
        ++stats.xiParticles;
        KfXiCandidate candidate;
        if (!XiCandidate(particles[i], particles, pv, position, candidate, dst)) {
          ++stats.invalidXiCandidates;
          continue;
        }
        xiCandidates.push_back(candidate);
      }
    }
    stats.validCandidates = candidates.size();
    stats.validXiCandidates = xiCandidates.size();
    return true;
  }

  const KfParticleCutConfig& cuts;
  KfParticleHelper helper;
  kfp::KFParticleTopoReconstructor topo;
  KfParticleEventStats stats;
  std::vector<KfLambdaCandidate> candidates;
  std::vector<KfXiCandidate> xiCandidates;
  std::map<int, TrackPid> trackPid;
  std::string error;
  std::string configurationError;
};

StPicoKFParticleInterface::StPicoKFParticleInterface(const KfParticleCutConfig& cuts)
    : mImpl(new Impl(cuts)) {}
StPicoKFParticleInterface::~StPicoKFParticleInterface() { delete mImpl; }
Bool_t StPicoKFParticleInterface::ProcessEvent(StPicoDst* dst) { return mImpl->ProcessEvent(dst); }
void StPicoKFParticleInterface::Clear() { mImpl->Clear(); }
const std::vector<KfLambdaCandidate>& StPicoKFParticleInterface::Candidates() const {
  return mImpl->candidates;
}
const std::vector<KfXiCandidate>& StPicoKFParticleInterface::XiCandidates() const {
  return mImpl->xiCandidates;
}
const KfParticleEventStats& StPicoKFParticleInterface::Stats() const { return mImpl->stats; }
const std::string& StPicoKFParticleInterface::LastError() const { return mImpl->error; }

const char* StPicoKFParticleInterface::BackendDescription() {
  return "finder_topo; core=.DEV2 sha256=" STAR_ANALYZER_KFP_SOURCE_ID
      "; snapshot=" STAR_ANALYZER_KFP_SNAPSHOT_DATE
      "; namespace=star_analyzer_kfp; __ROOT__,KFParticleStandalone,HomogeneousField,SSE4.1"
      "; Pico source=xwu2/KFTree_lambda/StKFParticleInterface"
      "; Lambda and optional Xi (PDG +/-3312); parent mass unconstrained";
}
