#include "StLambdaKFParticleMaker.h"

#include "CentralityHelper.h"
#include "ConfigManager.h"
#include "HistManager.h"
#include "KfParticleHelper.h"
#include "KfEventSelection.h"
#include "StPicoKFParticleInterface.h"
#include "cuts/CentralityCutConfig.h"
#include "cuts/EventCutConfig.h"
#include "cuts/KfParticleCutConfig.h"

#include "StPicoDstMaker/StPicoDstMaker.h"
#include "StPicoEvent/StPicoDst.h"
#include "StPicoEvent/StPicoEvent.h"
#include "StPicoEvent/StPicoTrack.h"

#include "TFile.h"
#include "TH1.h"
#include "TTree.h"
#include "TNamed.h"
#include "TObjString.h"
#include "TMath.h"
#include "TString.h"
#include "TVector3.h"

#include <cmath>
#include <iostream>
#include <sstream>
#include <vector>

namespace {
Double_t CandidateRapidity(const TVector3& momentum, Double_t mass) {
  const Double_t energy = TMath::Sqrt(momentum.Mag2() + mass * mass);
  const Double_t denominator = energy - momentum.Z();
  return denominator > 0. ? 0.5 * TMath::Log((energy + momentum.Z()) / denominator) : 0.;
}

}

StLambdaKFParticleMaker::StLambdaKFParticleMaker(
    const char* name, StPicoDstMaker* picoMaker, const char* outName)
    : StMaker(name), mPicoDstMaker(picoMaker), mPicoDst(0),
      mOutName(outName), mMainConfigPath(""), mEventSelection(0),
      mEventCounter(0), mHistManager(0), mCentrality(0),
      mKfInterface(0), mOutputFile(0), mCandidateTree(0), mCandidateRow(0),
      mRunId(0), mEventId(0), mMagneticField(0), mSelected(kFALSE), mProcessingSucceeded(kFALSE), mReconstructedEvents(0), mCent9(-1), mCent16(-1), mRefMultCorr(-1.),
      mCentWeight(1.), mTracksSeen(0), mTracksWithInvalidCovariance(0),
      mFinderCandidates(0), mCandidatesSelected(0) {
  for (Int_t i = 0; i < 14; ++i) mStages[i] = 0;
  for (Int_t i = 0; i < 13; ++i) mEventSelectionCounts[i] = 0;
}

StLambdaKFParticleMaker::~StLambdaKFParticleMaker() {
  if (mOutputFile) { mOutputFile->Close(); delete mOutputFile; }
  delete mCandidateRow;
  delete mEventSelection;
  delete mKfInterface;
  delete mCentrality;
  delete mHistManager;
}

StLambdaKFParticleMaker* createStLambdaKFParticleMaker(
    const char* name, StPicoDstMaker* picoMaker, const char* outName) {
  return new StLambdaKFParticleMaker(name, picoMaker, outName);
}

extern "C" void* createStLambdaKFParticleMakerC(
    const char* name, void* picoMaker, const char* outName) {
  return static_cast<void*>(createStLambdaKFParticleMaker(
      name, static_cast<StPicoDstMaker*>(picoMaker), outName));
}

Int_t StLambdaKFParticleMaker::Init() {
  const KfParticleCutConfig& cuts = ConfigManager::GetInstance().GetKfParticleCuts();
  if (!cuts.Validate(std::cerr)) return kStErr;
  const CentralityCutConfig& centralityCuts = ConfigManager::GetInstance().GetCentralityCuts();
  mEventSelection = new KfEventSelection();
  if (!mEventSelection->Load(mMainConfigPath.Data(), ConfigManager::GetInstance().GetEventCuts(),
                            centralityCuts.mode, centralityCuts.enabled, std::cerr,
                            cuts.selectionProfile == "lambda_imp5")) return kStErr;
  mEventSelection->Dump(std::cout);
  cuts.Dump(std::cout);
  std::cout << StPicoKFParticleInterface::BackendDescription() << std::endl;
  const std::string histPath = ConfigManager::GetInstance().GetHistConfigPath(GetName());
  if (histPath.empty()) { std::cerr << "ERROR: KF histogram config missing" << std::endl; return kStErr; }
  mHistManager = new HistManager();
  if (!mHistManager->LoadFromFile(histPath.c_str())) return kStErr;
  TH1* eventSelection = mHistManager->Get("hKfEventSelection");
  const char* eventLabels[] = {"read events", "bad run", "invalid vertex", "Vz", "Vr",
      "refMult", "VPD difference", "track count", "pileup", "centrality invalid",
      "centrality bin", "KF error", "reconstructed"};
  if (!eventSelection || eventSelection->GetNbinsX() != 13) {
    std::cerr << "ERROR: KF histogram config requires 13-bin hKfEventSelection" << std::endl;
    return kStErr;
  }
  for (Int_t i = 0; i < 13; ++i) eventSelection->GetXaxis()->SetBinLabel(i + 1, eventLabels[i]);
  mCentrality = new CentralityHelper();
  if (!mCentrality->Init(ConfigManager::GetInstance().GetCentralityCuts())) return kStErr;
  mKfInterface = new StPicoKFParticleInterface(cuts);

  // Open once so the candidate tree flushes baskets instead of accumulating
  // an entire production sample in memory. Never overwrite an existing output.
  mOutputFile = TFile::Open(mOutName.Data(), "CREATE");
  if (!mOutputFile || mOutputFile->IsZombie()) {
    std::cerr << "ERROR: cannot create KF output (choose a new path): " << mOutName << std::endl;
    return kStErr;
  }
  mOutputFile->cd();
  mCandidateRow = new KfLambdaCandidate();
  mCandidateTree = new TTree("KfLambdaCandidates", "Topo output; raw parent mass; selected is the additional Maker cut");
  mCandidateTree->Branch("runId", &mRunId, "runId/I");
  mCandidateTree->Branch("eventId", &mEventId, "eventId/I");
  mCandidateTree->Branch("magneticField", &mMagneticField, "magneticField/F");
  mCandidateTree->Branch("cent9", &mCent9, "cent9/I");
  mCandidateTree->Branch("selected", &mSelected, "selected/O");
  mCandidateTree->Branch("x", &mCandidateRow->x, "x/F");
  mCandidateTree->Branch("y", &mCandidateRow->y, "y/F");
  mCandidateTree->Branch("z", &mCandidateRow->z, "z/F");
  mCandidateTree->Branch("px", &mCandidateRow->px, "px/F");
  mCandidateTree->Branch("py", &mCandidateRow->py, "py/F");
  mCandidateTree->Branch("pz", &mCandidateRow->pz, "pz/F");
  mCandidateTree->Branch("mass", &mCandidateRow->mass, "mass/F");
  mCandidateTree->Branch("massError", &mCandidateRow->massError, "massError/F");
  mCandidateTree->Branch("chi2Ndf", &mCandidateRow->chi2Ndf, "chi2Ndf/F");
  mCandidateTree->Branch("topoChi2Ndf", &mCandidateRow->topoChi2Ndf, "topoChi2Ndf/F");
  mCandidateTree->Branch("daughterDistance", &mCandidateRow->daughterDistance, "daughterDistance/F");
  mCandidateTree->Branch("distanceToPv", &mCandidateRow->distanceToPv, "distanceToPv/F");
  mCandidateTree->Branch("decayLength", &mCandidateRow->decayLength, "decayLength/F");
  mCandidateTree->Branch("decayLengthError", &mCandidateRow->decayLengthError, "decayLengthError/F");
  mCandidateTree->Branch("decayLengthSignificance", &mCandidateRow->decayLengthSignificance, "decayLengthSignificance/F");
  mCandidateTree->Branch("vertexLineLength", &mCandidateRow->vertexLineLength, "vertexLineLength/F");
  mCandidateTree->Branch("vertexLineLengthError", &mCandidateRow->vertexLineLengthError, "vertexLineLengthError/F");
  mCandidateTree->Branch("vertexLineLengthSignificance", &mCandidateRow->vertexLineLengthSignificance, "vertexLineLengthSignificance/F");
  mCandidateTree->Branch("cosPointing", &mCandidateRow->cosPointing, "cosPointing/F");
  mCandidateTree->Branch("protonPidPull", &mCandidateRow->protonPidPull, "protonPidPull/F");
  mCandidateTree->Branch("pionPidPull", &mCandidateRow->pionPidPull, "pionPidPull/F");
  mCandidateTree->Branch("protonId", &mCandidateRow->protonId, "protonId/I");
  mCandidateTree->Branch("pionId", &mCandidateRow->pionId, "pionId/I");
  mCandidateTree->Branch("protonIndex", &mCandidateRow->protonIndex, "protonIndex/I");
  mCandidateTree->Branch("pionIndex", &mCandidateRow->pionIndex, "pionIndex/I");
  mCandidateTree->Branch("protonHasTof", &mCandidateRow->protonHasTof, "protonHasTof/O");
  mCandidateTree->Branch("pionHasTof", &mCandidateRow->pionHasTof, "pionHasTof/O");
  mCandidateTree->Branch("protonTofM2", &mCandidateRow->protonTofM2, "protonTofM2/F");
  mCandidateTree->Branch("pionTofM2", &mCandidateRow->pionTofM2, "pionTofM2/F");
  mCandidateTree->Branch("protonTofPull", &mCandidateRow->protonTofPull, "protonTofPull/F");
  mCandidateTree->Branch("pionTofPull", &mCandidateRow->pionTofPull, "pionTofPull/F");
  mCandidateTree->Branch("protonDcaToPv", &mCandidateRow->protonDcaToPv, "protonDcaToPv/D");
  mCandidateTree->Branch("pionDcaToPv", &mCandidateRow->pionDcaToPv, "pionDcaToPv/D");
  mCandidateTree->Branch("protonHelixPathLength", &mCandidateRow->protonHelixPathLength, "protonHelixPathLength/D");
  mCandidateTree->Branch("pionHelixPathLength", &mCandidateRow->pionHelixPathLength, "pionHelixPathLength/D");
  mCandidateTree->Branch("imp5PathValid", &mCandidateRow->imp5PathValid, "imp5PathValid/O");
  mCandidateTree->Branch("pdg", &mCandidateRow->pdg, "pdg/I");
  TH1* stages = mHistManager->Get("hKfStages");
  const char* labels[] = {"read events", "reconstructed events", "raw tracks", "quality tracks",
      "covariance tracks", "PID tracks", "PID hypotheses", "primary hypotheses",
      "Topo particle slots", "Topo Lambdas", "invalid candidates", "valid raw Lambdas",
      "selected Lambdas", "selected anti-Lambdas"};
  if (stages) for (Int_t i = 0; i < 14; ++i) stages->GetXaxis()->SetBinLabel(i + 1, labels[i]);
  return kStOK;
}

void StLambdaKFParticleMaker::Clear(Option_t* option) {
  StMaker::Clear(option);
  mLambdaMom.clear();
  mLambdaMass.clear();
  mProtonIds.clear();
  mPionIds.clear();
  mPdgs.clear();
  if (mKfInterface) mKfInterface->Clear();
}

Bool_t StLambdaKFParticleMaker::PassCandidateCuts(const KfLambdaCandidate& c) const {
  const KfParticleCutConfig& k = ConfigManager::GetInstance().GetKfParticleCuts();
  if (k.selectionProfile == "lambda_imp5" &&
      (!c.imp5PathValid || !std::isfinite(c.protonHelixPathLength) ||
       !std::isfinite(c.pionHelixPathLength) ||
       std::fabs(c.protonHelixPathLength) > k.imp5MaxPathLength ||
       std::fabs(c.pionHelixPathLength) > k.imp5MaxPathLength)) return kFALSE;
  if (c.mass < k.minMass || c.mass > k.maxMass) return kFALSE;
  if (k.maxMassError >= 0. && c.massError > k.maxMassError) return kFALSE;
  if (k.maxChi2Ndf >= 0. && c.chi2Ndf > k.maxChi2Ndf) return kFALSE;
  if (k.maxTopoChi2Ndf >= 0. && c.topoChi2Ndf > k.maxTopoChi2Ndf) return kFALSE;
  if (k.maxDaughterDistance >= 0. && c.daughterDistance > k.maxDaughterDistance) return kFALSE;
  if (k.maxDistanceToPv >= 0. && c.distanceToPv > k.maxDistanceToPv) return kFALSE;
  if (k.minDecayLength >= 0. && c.decayLength < k.minDecayLength) return kFALSE;
  if (k.minDecayLengthSignificance >= 0. && c.decayLengthSignificance < k.minDecayLengthSignificance) return kFALSE;
  if (k.minVertexLineSignificance >= 0. && c.vertexLineLengthSignificance < k.minVertexLineSignificance) return kFALSE;
  if (k.minCosPointing > -1. && c.cosPointing < k.minCosPointing) return kFALSE;
  return kTRUE;
}

void StLambdaKFParticleMaker::FillCentralityQa(
    Int_t cent9, Int_t rawMult, Double_t refMultCorr, Int_t nTracks,
    Int_t nBTofMatch, Int_t nProtons, Int_t nPions, Int_t nPairs) {
  if (!mHistManager || cent9 < 0) return;
  const Double_t x = static_cast<Double_t>(cent9);
  mHistManager->Fill("hRawMult_vs_Cent9", x, rawMult);
  if (refMultCorr >= 0.) {
    mHistManager->Fill("hRefMultCorr_vs_Cent9", x, refMultCorr);
    mHistManager->Fill("hRawMult_vs_RefMultCorr", refMultCorr, rawMult);
  }
  mHistManager->Fill("hNTracks_vs_Cent9", x, nTracks);
  mHistManager->Fill("hTofMatchMult_vs_Cent9", x, nBTofMatch);
  mHistManager->Fill("hNProtonCand_vs_Cent9", x, nProtons);
  mHistManager->Fill("hNPionCand_vs_Cent9", x, nPions);
  mHistManager->Fill("hNLambdaPairs_vs_Cent9", x, nPairs);
}

void StLambdaKFParticleMaker::FillCandidateCentrality(Double_t mass) {
  if (!mHistManager || mCent9 < 0 || mCent9 > 8) return;
  const CentralityCutConfig& cent =
      ConfigManager::GetInstance().GetCentralityCuts();
  if (!cent.fillCentralityQA) return;
  mHistManager->Fill("hLambda_InvMass_vs_Cent9", mCent9, mass);
  if (mRefMultCorr >= 0.)
    mHistManager->Fill("hLambda_InvMass_vs_RefMultCorr", mRefMultCorr, mass);
  static const char* names[] = {
      "hLambda_InvMass_CentBin0", "hLambda_InvMass_CentBin1",
      "hLambda_InvMass_CentBin2", "hLambda_InvMass_CentBin3",
      "hLambda_InvMass_CentBin4", "hLambda_InvMass_CentBin5",
      "hLambda_InvMass_CentBin6", "hLambda_InvMass_CentBin7",
      "hLambda_InvMass_CentBin8"};
  mHistManager->Fill(names[mCent9], mass);
}

Int_t StLambdaKFParticleMaker::Make() {
  if (!mPicoDstMaker || !mKfInterface || !mEventSelection) return kStErr;
  mPicoDst = mPicoDstMaker->picoDst();
  if (!mPicoDst) return kStWarn;
  StPicoEvent* event = mPicoDst->event();
  if (!event) return kStWarn;

  ++mEventCounter;
  ++mEventSelectionCounts[0];
  ++mStages[0];
  mRunId = event->runId();
  mEventId = event->eventId();
  mMagneticField = event->bField();
  const TVector3 primaryVertex = event->primaryVertex();
  const Int_t refMult = event->refMult();
  const Int_t runId = event->runId();
  const Int_t nBTofMatch = event->nBTOFMatch();
  const Double_t vz = primaryVertex.Z();
  const Int_t nTracks = mPicoDst->numberOfTracks();

  const CentralityCutConfig& cent =
      ConfigManager::GetInstance().GetCentralityCuts();
  Int_t rawMult = refMult;
  TString centralityMode(cent.mode.c_str());
  centralityMode.ToLower();
  if (cent.enabled && centralityMode == "fxtmult") rawMult = event->fxtMult();

  mCent9 = -1;
  mCent16 = -1;
  mRefMultCorr = -1.;
  mCentWeight = 1.;

  if (mHistManager) {
    mHistManager->Fill("hRefMultVsNTOFMatch", nBTofMatch, rawMult);
    mHistManager->Fill("hVz", vz);
    if (std::isfinite(primaryVertex.X()) && std::isfinite(primaryVertex.Y())) {
      mHistManager->Fill("hKfVertexXYBefore", primaryVertex.X(), primaryVertex.Y());
      mHistManager->Fill("hKfVertexRadiusBefore", mEventSelection->ComputeVr(primaryVertex.X(), primaryVertex.Y()));
    }
    mHistManager->Fill("hRefMult", refMult);
    mHistManager->Fill("hKfTrackCovCount", mPicoDst->numberOfTrackCovMatrices());
  }

  CentralityRejectReason reason = kCentralityOk;
  if (mCentrality && mCentrality->IsEnabled() &&
      !mCentrality->CheckBadRun(runId, reason)) { ++mEventSelectionCounts[1]; return kStOK; }
  const KfEventSelection::Result eventResult = mEventSelection->Check(*event, nTracks);
  if (eventResult != KfEventSelection::kAccepted) {
    ++mEventSelectionCounts[static_cast<Int_t>(eventResult) + 1];
    return kStOK;
  }
  mHistManager->Fill("hKfVertexXYAfterEvent", primaryVertex.X(), primaryVertex.Y());
  mHistManager->Fill("hKfVertexRadiusAfterEvent", mEventSelection->ComputeVr(primaryVertex.X(), primaryVertex.Y()));
  mHistManager->Fill("hKfVertexZAfterEvent", vz);

  if (mHistManager && cent.fillCentralityQA)
    mHistManager->Fill("hRawMult", rawMult);

  if (mCentrality && mCentrality->IsEnabled()) {
    if (!mCentrality->CheckPileup(rawMult, nBTofMatch, vz, reason)) { ++mEventSelectionCounts[8]; return kStOK; }
    if (!mCentrality->ComputeBins(event, rawMult, vz, mCent9, mCent16,
                                  mRefMultCorr, mCentWeight, reason)) { ++mEventSelectionCounts[9]; return kStOK; }
    if (mHistManager) {
      mHistManager->Fill("hCentralityRaw", mCent9);
      mHistManager->Fill("hRefMultCorr", mRefMultCorr);
      mHistManager->Fill("hCentralityVsVz", vz, mCent9);
      mHistManager->Fill("hRefMultWeight", mCentWeight);
      mHistManager->Fill("hRefMultVsNTOFMatchAfter", nBTofMatch, rawMult);
    }
    if (!mCentrality->AcceptCentBin(mCent9, mRefMultCorr, reason)) { ++mEventSelectionCounts[10]; return kStOK; }
    if (mHistManager) {
      const Double_t weight = cent.useWeight ? mCentWeight : 1.;
      TH1* hCent = mHistManager->Get("hCentrality");
      if (hCent) hCent->Fill(mCent9, weight);
      TH1* hCent16 = mHistManager->Get("hCentrality16");
      if (hCent16) hCent16->Fill(mCent16, weight);
    }
  }

  if (!mKfInterface->ProcessEvent(mPicoDst)) {
    ++mEventSelectionCounts[11];
    std::cerr << "[StLambdaKFParticleMaker] run=" << mRunId << " event=" << mEventId
              << ": " << mKfInterface->LastError() << std::endl;
    return kStErr;
  }
  ++mReconstructedEvents;
  ++mEventSelectionCounts[12];
  const KfParticleEventStats& stats = mKfInterface->Stats();
  mTracksSeen += stats.rawTracks;
  mTracksWithInvalidCovariance += stats.invalidCovariance;
  mFinderCandidates += stats.lambdaParticles;
  const Long64_t increments[] = {0,1,stats.rawTracks,stats.qualityTracks,
      stats.covarianceTracks,stats.pidTracks,stats.pidHypotheses,stats.primaryHypotheses,
      stats.finderParticles,stats.lambdaParticles,stats.invalidCandidates,stats.validCandidates,0,0};
  for (Int_t i = 0; i < 14; ++i) mStages[i] += increments[i];
  const std::vector<KfLambdaCandidate>& candidates = mKfInterface->Candidates();
  for (size_t i = 0; i < candidates.size(); ++i) {
    const KfLambdaCandidate& c = candidates[i];
    mHistManager->Fill(c.pdg > 0 ? "hKfLambdaMassRaw" : "hKfAntiLambdaMassRaw", c.mass);
    mHistManager->Fill("hKfTopoChi2NdfRaw", c.topoChi2Ndf);
    *mCandidateRow = c;
    mSelected = PassCandidateCuts(c);
    if (mCandidateTree->Fill() < 0) return kStErr;
    if (!mSelected) continue;
    ++mCandidatesSelected;
    ++mStages[c.pdg > 0 ? 12 : 13];
    const TVector3 momentum(c.px, c.py, c.pz);
    mLambdaMom.push_back(momentum);
    mLambdaMass.push_back(c.mass);
    mProtonIds.push_back(c.protonId);
    mPionIds.push_back(c.pionId);
    mPdgs.push_back(c.pdg);
    // Preserve the previous combined histogram; add explicitly signed QA.
    mHistManager->Fill("hLambda_InvMass", c.mass);
    mHistManager->Fill(c.pdg > 0 ? "hKfLambdaMassSelected" : "hKfAntiLambdaMassSelected", c.mass);
    mHistManager->Fill("hLambda_Pt", momentum.Pt());
    mHistManager->Fill("hLambda_Eta", momentum.PseudoRapidity());
    mHistManager->Fill("hLambda_Phi", momentum.Phi());
    mHistManager->Fill("hDCA12", c.daughterDistance);
    mHistManager->Fill("hDCAV0", c.distanceToPv);
    mHistManager->Fill("hCosPointing", c.cosPointing);
    mHistManager->Fill("hKfProtonPidPull", c.protonPidPull);
    mHistManager->Fill("hKfPionPidPull", c.pionPidPull);
    mHistManager->Fill("hNSigmaProton", mPicoDst->track(c.protonIndex)->nSigmaProton());
    mHistManager->Fill("hNSigmaPion", mPicoDst->track(c.pionIndex)->nSigmaPion());
    mHistManager->Fill("hLambda_InvMass_vs_Pt", momentum.Pt(), c.mass);
    mHistManager->Fill("hLambda_InvMass_vs_DecayLength", c.decayLength, c.mass);
    mHistManager->Fill("hLambda_InvMass_vs_Y", CandidateRapidity(momentum, c.mass), c.mass);
    mHistManager->Fill("hDCAV0_vs_InvMass", c.mass, c.distanceToPv);
    mHistManager->Fill("hCosPointing_vs_InvMass", c.mass, c.cosPointing);
    mHistManager->Fill("hKfMassError", c.massError);
    mHistManager->Fill("hKfChi2Ndf", c.chi2Ndf);
    mHistManager->Fill("hKfTopoChi2Ndf", c.topoChi2Ndf);
    mHistManager->Fill("hKfVertexLineSignificance", c.vertexLineLengthSignificance);
    mHistManager->Fill("hKfDecayLengthSignificance", c.decayLengthSignificance);
    mHistManager->Fill("hKfPdg", c.pdg);
    FillCandidateCentrality(c.mass);
  }
  if (mHistManager && mCent9 >= 0) {
    FillCentralityQa(mCent9, rawMult, mRefMultCorr, nTracks, nBTofMatch,
                    stats.protonHypotheses, stats.pionHypotheses, stats.lambdaParticles);
  }
  mHistManager->Fill("hN", 0.);
  return kStOK;
}

Int_t StLambdaKFParticleMaker::Finish() {
  Int_t result = kStOK;
  if (mOutputFile && !mOutputFile->IsZombie()) {
    mOutputFile->cd();
    TH1* stages = mHistManager ? mHistManager->Get("hKfStages") : 0;
    if (stages) for (Int_t i = 0; i < 14; ++i) stages->SetBinContent(i+1, mStages[i]);
    TH1* eventSelection = mHistManager ? mHistManager->Get("hKfEventSelection") : 0;
    if (eventSelection) for (Int_t i = 0; i < 13; ++i) {
      eventSelection->SetBinContent(i + 1, mEventSelectionCounts[i]);
      std::cout << "[KF event selection] " << eventSelection->GetXaxis()->GetBinLabel(i + 1)
                << "=" << mEventSelectionCounts[i] << std::endl;
    }
    WriteHistograms();
    if (mCandidateTree && mCandidateTree->Write("", TObject::kOverwrite) <= 0) result = kStErr;
    std::ostringstream configuration;
    ConfigManager::GetInstance().GetKfParticleCuts().Dump(configuration);
    TObjString config(configuration.str().c_str());
    if (config.Write("KFParticleEffectiveConfiguration") <= 0) result = kStErr;
    std::ostringstream eventConfiguration;
    mEventSelection->Dump(eventConfiguration);
    TObjString eventConfig(eventConfiguration.str().c_str());
    if (eventConfig.Write("KFEventSelectionConfiguration") <= 0) result = kStErr;
    TNamed backend("KFParticleBackend", StPicoKFParticleInterface::BackendDescription());
    if (backend.Write() <= 0) result = kStErr;
    mOutputFile->Flush();
    if (mOutputFile->TestBit(TFile::kWriteError)) result = kStErr;
    TNamed status("KFRunStatus", mProcessingSucceeded && result == kStOK ? "completed" : "incomplete");
    if (status.Write() <= 0) result = kStErr;
    mOutputFile->Close();
    if (mOutputFile->TestBit(TFile::kWriteError)) result = kStErr;
    delete mOutputFile;
    mOutputFile = 0;
    mCandidateTree = 0;
  }
  std::cout << "[StLambdaKFParticleMaker] readEvents=" << mEventCounter
            << " reconstructedEvents=" << mReconstructedEvents
            << " tracks=" << mTracksSeen
            << " invalidCovariance=" << mTracksWithInvalidCovariance
            << " topoLambdaCandidates=" << mFinderCandidates
            << " selected=" << mCandidatesSelected << std::endl;
  if (mCentrality && mCentrality->IsEnabled()) mCentrality->Finish();
  return result;
}

void StLambdaKFParticleMaker::WriteHistograms() {
  if (mHistManager) mHistManager->Write();
}
