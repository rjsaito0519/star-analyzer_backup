#ifndef CUT_CONFIG_H
#define CUT_CONFIG_H

#include "Rtypes.h"
#include "ConfigManager.h"
#include "cuts/EventCutConfig.h"
#include "cuts/TrackCutConfig.h"
#include "cuts/PIDCutConfig.h"
#include "cuts/V0CutConfig.h"
#include "cuts/PhiCutConfig.h"
#include "cuts/Lambda1520CutConfig.h"
#include "cuts/Sigma1385CutConfig.h"
#include "cuts/MixingConfig.h"

// Wrapper namespace for backward compatibility
// All values are now loaded from YAML config files via ConfigManager
namespace CutConfig {
  // ----------------------------------------
  // Event Level Cuts
  // ----------------------------------------
  namespace Event {
    inline const EventCutConfig& Get() {
      return ConfigManager::GetInstance().GetEventCuts();
    }
    // Accessor functions for backward compatibility
    inline Double_t minVz() { return Get().minVz; }
    inline Double_t maxVz() { return Get().maxVz; }
    inline Double_t maxVr() { return Get().maxVr; }
    inline Double_t minRefMult() { return Get().minRefMult; }
    inline Double_t maxRefMult() { return Get().maxRefMult; }
    inline Double_t maxVzDiff() { return Get().maxVzDiff; }
  }

  // ----------------------------------------
  // Track Level Cuts
  // ----------------------------------------
  namespace Track {
    inline const TrackCutConfig& Get() {
      return ConfigManager::GetInstance().GetTrackCuts();
    }
    inline Int_t minNHitsFit() { return Get().minNHitsFit; }
    inline Double_t minNHitsRatio() { return Get().minNHitsRatio; }
    inline Int_t minNHitsDedx() { return Get().minNHitsDedx; }
    inline Double_t maxDCA() { return Get().maxDCA; }
    inline Double_t maxEta() { return Get().maxEta; }
    inline Double_t minPt() { return Get().minPt; }
    inline Double_t maxPt() { return Get().maxPt; }
    inline Double_t maxChi2() { return Get().maxChi2; }
  }

  // ----------------------------------------
  // PID Cuts (TPC nSigma)
  // ----------------------------------------
  namespace PID {
    inline const PIDCutConfig& Get() {
      return ConfigManager::GetInstance().GetPIDCuts();
    }
    inline Double_t defaultNSigmaCut() { return Get().defaultNSigmaCut; }
    inline Double_t nSigmaPion() { return Get().nSigmaPion; }
    inline Double_t nSigmaKaon() { return Get().nSigmaKaon; }
    inline Double_t nSigmaProton() { return Get().nSigmaProton; }
    inline Double_t minMass2Pion() { return Get().minMass2Pion; }
    inline Double_t maxMass2Pion() { return Get().maxMass2Pion; }
    inline Double_t minMass2Kaon() { return Get().minMass2Kaon; }
    inline Double_t maxMass2Kaon() { return Get().maxMass2Kaon; }
    inline Double_t minMass2Proton() { return Get().minMass2Proton; }
    inline Double_t maxMass2Proton() { return Get().maxMass2Proton; }
    inline Bool_t requireTOF() { return Get().requireTOF; }
  }

  // ----------------------------------------
  // V0 Topology Cuts (for Lambda reconstruction)
  // ----------------------------------------
  namespace V0 {
    inline const V0CutConfig& Get() {
      return ConfigManager::GetInstance().GetV0Cuts();
    }
    inline Double_t minDaughterDCA() { return Get().minDaughterDCA; }
    inline Double_t maxDaughterDCA() { return Get().maxDaughterDCA; }
    inline Double_t minDecayLength() { return Get().minDecayLength; }
    inline Double_t maxDecayLength() { return Get().maxDecayLength; }
    inline Double_t minPointingAngle() { return Get().minPointingAngle; }
    inline Double_t maxPointingAngle() { return Get().maxPointingAngle; }
    inline Double_t maxDCAtoPV() { return Get().maxDCAtoPV; }
    inline Double_t lambdaMassWindow() { return Get().lambdaMassWindow; }
    inline Double_t lambdaMass() { return Get().lambdaMass; }
  }

  // ----------------------------------------
  // Resonance-specific cuts (can override defaults)
  // ----------------------------------------
  namespace Phi {
    inline const PhiCutConfig& Get() {
      return ConfigManager::GetInstance().GetPhiCuts();
    }
    inline Double_t nSigmaKaon() { return Get().nSigmaKaon; }
    inline Double_t minMass2Kaon() { return Get().minMass2Kaon; }
    inline Double_t maxMass2Kaon() { return Get().maxMass2Kaon; }
    inline Double_t minInvMass() { return Get().minInvMass; }
    inline Double_t maxInvMass() { return Get().maxInvMass; }
  }

  namespace Lambda1520 {
    inline const Lambda1520CutConfig& Get() {
      return ConfigManager::GetInstance().GetLambda1520Cuts();
    }
    inline Double_t nSigmaProton() { return Get().nSigmaProton; }
    inline Double_t nSigmaKaon() { return Get().nSigmaKaon; }
    inline Double_t minInvMass() { return Get().minInvMass; }
    inline Double_t maxInvMass() { return Get().maxInvMass; }
  }

  namespace Sigma1385 {
    inline const Sigma1385CutConfig& Get() {
      return ConfigManager::GetInstance().GetSigma1385Cuts();
    }
    inline Double_t nSigmaPion() { return Get().nSigmaPion; }
    inline Double_t nSigmaProton() { return Get().nSigmaProton; }
    inline Double_t nSigmaPionForSigma() { return Get().nSigmaPionForSigma; }
    inline Double_t minInvMass() { return Get().minInvMass; }
    inline Double_t maxInvMass() { return Get().maxInvMass; }
  }

  // ----------------------------------------
  // Event Mixing Configuration
  // ----------------------------------------
  namespace Mixing {
    inline const MixingConfig& Get() {
      return ConfigManager::GetInstance().GetMixingConfig();
    }
    inline Int_t nVzBins() { return Get().nVzBins; }
    inline Int_t nCentralityBins() { return Get().nCentralityBins; }
    inline Int_t nEventPlaneBins() { return Get().nEventPlaneBins; }
    inline Int_t bufferSize() { return Get().bufferSize; }
  }
}

#endif

