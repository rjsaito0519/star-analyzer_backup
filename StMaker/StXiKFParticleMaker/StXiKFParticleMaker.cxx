#include "StXiKFParticleMaker.h"

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

StXiKFParticleMaker::StXiKFParticleMaker(
    const char* name, StPicoDstMaker* picoMaker, const char* outName)
    : StMaker(name), mPicoDstMaker(picoMaker), mPicoDst(0),
      mOutName(outName), mMainConfigPath(""), mEventSelection(0),
      mEventCounter(0), mHistManager(0), mCentrality(0),
      mKfInterface(0), mOutputFile(0), mCandidateTree(0), mCandidateRow(0),
      mRunId(0), mEventId(0), mMagneticField(0), mSelected(kFALSE),
      mProcessingSucceeded(kFALSE), mReconstructedEvents(0), mCent9(-1),
      mCent16(-1), mRefMultCorr(-1.), mCentWeight(1.), mTracksSeen(0),
      mTracksWithInvalidCovariance(0), mFinderCandidates(0),
      mCandidatesSelected(0) {
  for (Int_t i = 0; i < 14; ++i) mStages[i] = 0;
  for (Int_t i = 0; i < 13; ++i) mEventSelectionCounts[i] = 0;
}

StXiKFParticleMaker::~StXiKFParticleMaker() {
  if (mOutputFile) { mOutputFile->Close(); delete mOutputFile; }
  delete mCandidateRow;
  delete mEventSelection;
  delete mKfInterface;
  delete mCentrality;
  delete mHistManager;
}

StXiKFParticleMaker* createStXiKFParticleMaker(
    const char* name, StPicoDstMaker* picoMaker, const char* outName) {
  return new StXiKFParticleMaker(name, picoMaker, outName);
}

extern "C" void* createStXiKFParticleMakerC(
    const char* name, void* picoMaker, const char* outName) {
  return static_cast<void*>(createStXiKFParticleMaker(
      name, static_cast<StPicoDstMaker*>(picoMaker), outName));
}

Int_t StXiKFParticleMaker::Init() {
  const KfParticleCutConfig& cuts = ConfigManager::GetInstance().GetKfParticleCuts();
  if (!cuts.Validate(std::cerr)) return kStErr;
  if (!cuts.reconstructXi && !cuts.reconstructAntiXi) {
    std::cerr << "ERROR: StXiKFParticleMaker requires reconstructXi and/or reconstructAntiXi"
              << std::endl;
    return kStErr;
  }
  const CentralityCutConfig& centralityCuts = ConfigManager::GetInstance().GetCentralityCuts();
  mEventSelection = new KfEventSelection();
  if (!mEventSelection->Load(mMainConfigPath.Data(), ConfigManager::GetInstance().GetEventCuts(),
                            centralityCuts.mode, centralityCuts.enabled, std::cerr,
                            kFALSE)) return kStErr;
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

  mOutputFile = TFile::Open(mOutName.Data(), "CREATE");
  if (!mOutputFile || mOutputFile->IsZombie()) {
    std::cerr << "ERROR: cannot create KF output (choose a new path): " << mOutName << std::endl;
    return kStErr;
  }
  mOutputFile->cd();
  mCandidateRow = new KfXiCandidate();
  mCandidateTree = new TTree("KfXiCandidates", "Topo Xi; raw parent mass; selected is Maker cut");
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
  mCandidateTree->Branch("decayLengthSignificance", &mCandidateRow->decayLengthSignificance, "decayLengthSignificance/F");
  mCandidateTree->Branch("cosPointing", &mCandidateRow->cosPointing, "cosPointing/F");
  mCandidateTree->Branch("lambdaMass", &mCandidateRow->lambdaMass, "lambdaMass/F");
  mCandidateTree->Branch("protonId", &mCandidateRow->protonId, "protonId/I");
  mCandidateTree->Branch("pionId", &mCandidateRow->pionId, "pionId/I");
  mCandidateTree->Branch("bachelorId", &mCandidateRow->bachelorId, "bachelorId/I");
  mCandidateTree->Branch("pdg", &mCandidateRow->pdg, "pdg/I");
  TH1* stages = mHistManager->Get("hKfStages");
  const char* labels[] = {"read events", "reconstructed events", "raw tracks", "quality tracks",
      "covariance tracks", "PID tracks", "PID hypotheses", "primary hypotheses",
      "Topo particle slots", "Topo Xis", "invalid Xi", "valid raw Xis",
      "selected Xis", "selected anti-Xis"};
  if (stages) for (Int_t i = 0; i < 14; ++i) stages->GetXaxis()->SetBinLabel(i + 1, labels[i]);
  return kStOK;
}

void StXiKFParticleMaker::Clear(Option_t* option) {
  StMaker::Clear(option);
  if (mKfInterface) mKfInterface->Clear();
}

Bool_t StXiKFParticleMaker::PassCandidateCuts(const KfXiCandidate& c) const {
  const KfParticleCutConfig& k = ConfigManager::GetInstance().GetKfParticleCuts();
  if (c.mass < k.minMass || c.mass > k.maxMass) return kFALSE;
  if (k.maxMassError >= 0. && c.massError > k.maxMassError) return kFALSE;
  if (k.maxChi2Ndf >= 0. && c.chi2Ndf > k.maxChi2Ndf) return kFALSE;
  if (k.maxTopoChi2Ndf >= 0. && c.topoChi2Ndf > k.maxTopoChi2Ndf) return kFALSE;
  if (k.maxDaughterDistance >= 0. && c.daughterDistance > k.maxDaughterDistance) return kFALSE;
  if (k.maxDistanceToPv >= 0. && c.distanceToPv > k.maxDistanceToPv) return kFALSE;
  if (k.minDecayLength >= 0. && c.decayLength < k.minDecayLength) return kFALSE;
  if (k.minDecayLengthSignificance >= 0. && c.decayLengthSignificance < k.minDecayLengthSignificance)
    return kFALSE;
  if (k.minVertexLineSignificance >= 0. && c.vertexLineLengthSignificance < k.minVertexLineSignificance)
    return kFALSE;
  if (k.minCosPointing > -1. && c.cosPointing < k.minCosPointing) return kFALSE;
  return kTRUE;
}

Int_t StXiKFParticleMaker::Make() {
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
    mHistManager->Fill("hVz", vz);
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

  if (mCentrality && mCentrality->IsEnabled()) {
    if (!mCentrality->CheckPileup(rawMult, nBTofMatch, vz, reason)) { ++mEventSelectionCounts[8]; return kStOK; }
    if (!mCentrality->ComputeBins(event, rawMult, vz, mCent9, mCent16,
                                  mRefMultCorr, mCentWeight, reason)) { ++mEventSelectionCounts[9]; return kStOK; }
    if (!mCentrality->AcceptCentBin(mCent9, mRefMultCorr, reason)) { ++mEventSelectionCounts[10]; return kStOK; }
  }

  if (!mKfInterface->ProcessEvent(mPicoDst)) {
    ++mEventSelectionCounts[11];
    std::cerr << "[StXiKFParticleMaker] run=" << mRunId << " event=" << mEventId
              << ": " << mKfInterface->LastError() << std::endl;
    return kStErr;
  }
  ++mReconstructedEvents;
  ++mEventSelectionCounts[12];
  const KfParticleEventStats& stats = mKfInterface->Stats();
  mTracksSeen += stats.rawTracks;
  mTracksWithInvalidCovariance += stats.invalidCovariance;
  mFinderCandidates += stats.xiParticles;
  const Long64_t increments[] = {0,1,stats.rawTracks,stats.qualityTracks,
      stats.covarianceTracks,stats.pidTracks,stats.pidHypotheses,stats.primaryHypotheses,
      stats.finderParticles,stats.xiParticles,stats.invalidXiCandidates,stats.validXiCandidates,0,0};
  for (Int_t i = 0; i < 14; ++i) mStages[i] += increments[i];
  const std::vector<KfXiCandidate>& candidates = mKfInterface->XiCandidates();
  for (size_t i = 0; i < candidates.size(); ++i) {
    const KfXiCandidate& c = candidates[i];
    mHistManager->Fill(c.pdg > 0 ? "hKfXiMassRaw" : "hKfAntiXiMassRaw", c.mass);
    mHistManager->Fill("hKfTopoChi2NdfRaw", c.topoChi2Ndf);
    mHistManager->Fill("hKfLambdaMassFromXi", c.lambdaMass);
    *mCandidateRow = c;
    mSelected = PassCandidateCuts(c);
    if (mCandidateTree->Fill() < 0) return kStErr;
    if (!mSelected) continue;
    ++mCandidatesSelected;
    ++mStages[c.pdg > 0 ? 12 : 13];
    const TVector3 momentum(c.px, c.py, c.pz);
    mHistManager->Fill("hXi_InvMass", c.mass);
    mHistManager->Fill(c.pdg > 0 ? "hKfXiMassSelected" : "hKfAntiXiMassSelected", c.mass);
    mHistManager->Fill("hXi_Pt", momentum.Pt());
    mHistManager->Fill("hXi_Eta", momentum.PseudoRapidity());
    mHistManager->Fill("hDCA12", c.daughterDistance);
    mHistManager->Fill("hDCAV0", c.distanceToPv);
    mHistManager->Fill("hCosPointing", c.cosPointing);
    mHistManager->Fill("hXi_InvMass_vs_Pt", momentum.Pt(), c.mass);
    mHistManager->Fill("hXi_InvMass_vs_DecayLength", c.decayLength, c.mass);
    mHistManager->Fill("hXi_InvMass_vs_Y", CandidateRapidity(momentum, c.mass), c.mass);
    mHistManager->Fill("hKfChi2Ndf", c.chi2Ndf);
    mHistManager->Fill("hKfTopoChi2Ndf", c.topoChi2Ndf);
    mHistManager->Fill("hKfDecayLengthSignificance", c.decayLengthSignificance);
    mHistManager->Fill("hKfPdg", c.pdg);
  }
  mHistManager->Fill("hN", 0.);
  return kStOK;
}

Int_t StXiKFParticleMaker::Finish() {
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
  std::cout << "[StXiKFParticleMaker] readEvents=" << mEventCounter
            << " reconstructedEvents=" << mReconstructedEvents
            << " tracks=" << mTracksSeen
            << " invalidCovariance=" << mTracksWithInvalidCovariance
            << " topoXiCandidates=" << mFinderCandidates
            << " selected=" << mCandidatesSelected << std::endl;
  if (mCentrality && mCentrality->IsEnabled()) mCentrality->Finish();
  return result;
}

void StXiKFParticleMaker::WriteHistograms() {
  if (mHistManager) mHistManager->Write();
}
