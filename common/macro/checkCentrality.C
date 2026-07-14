// checkCentrality.C - Standalone StRefMultCorr QA on picoDst (mainconf-driven mode).
// Invoke via: ./script/checkCentrality.sh <picoDst_or_list> <mainconf_path> [output.root] [maxEvents]

#include "TFile.h"
#include "TH1.h"
#include "TH2.h"
#include "TString.h"
#include "TSystem.h"
#include "TVector3.h"

#include "ConfigManager.h"
#include "cuts/CentralityCutConfig.h"
#include "StRefMultCorr/StRefMultCorr.h"
#include "StPicoDstMaker/StPicoDstReader.h"
#include "StPicoEvent/StPicoDst.h"
#include "StPicoEvent/StPicoEvent.h"

#include <iostream>

void checkCentrality(const Char_t* inFileName,
                     const Char_t* mainconfPath,
                     const Char_t* oFileName = "centrality_qa.root",
                     Long64_t nEventsMax = -1) {
  if (!inFileName || !mainconfPath) {
    std::cerr << "Usage: checkCentrality(input, mainconf, [output.root], [maxEvents])" << std::endl;
    return;
  }

  if (!ConfigManager::GetInstance().LoadConfig(mainconfPath)) {
    std::cerr << "ERROR: failed to load mainconf " << mainconfPath << std::endl;
    return;
  }

  CentralityCutConfig& centCfg = ConfigManager::GetInstance().GetCentralityCuts();
  if (!centCfg.enabled) {
    std::cerr << "WARNING: centrality disabled in config; exiting." << std::endl;
    return;
  }

  TString modeName(centCfg.mode.c_str());
  modeName.ToLower();
  std::cout << "[checkCentrality] mode=" << modeName.Data() << " mainconf=" << mainconfPath << std::endl;

  gROOT->LoadMacro("$STAR/StRoot/StMuDSTMaker/COMMON/macros/loadSharedLibraries.C");
  loadSharedLibraries();
  gSystem->Load("StPicoEvent");
  gSystem->Load("StPicoDstMaker");

  StRefMultCorr* rmc = new StRefMultCorr(modeName, "Def", "Def");
  rmc->setVerbose(centCfg.verbose);

  StPicoDstReader* picoReader = new StPicoDstReader(inFileName);
  picoReader->Init();
  picoReader->SetStatus("*", 0);
  picoReader->SetStatus("Event", 1);
  picoReader->SetStatus("BTofPidTraits", 1);

  if (!picoReader->chain()) {
    std::cerr << "ERROR: no picoDst chain" << std::endl;
    return;
  }

  Long64_t events2read = picoReader->chain()->GetEntries();
  if (nEventsMax > 0 && nEventsMax < events2read) events2read = nEventsMax;
  std::cout << "[checkCentrality] events to read: " << events2read << std::endl;

  TH1F* hRefMult = new TH1F("hRefMult", "raw multiplicity;mult", 500, -0.5, 499.5);
  TH1F* hRefMultCorr = new TH1F("hRefMultCorr", "corrected multiplicity;mult_{corr}", 500, -0.5, 499.5);
  TH1F* hCent9 = new TH1F("hCent9", "cent9;cent9", 9, -0.5, 8.5);
  TH1F* hCent16 = new TH1F("hCent16", "cent16;cent16", 16, -0.5, 15.5);
  TH2F* hTofMatchVsRefMultBefore =
      new TH2F("hTofMatchVsRefMultBefore", ";nBTOFMatch;mult", 500, -0.5, 499.5, 500, -0.5, 499.5);
  TH2F* hTofMatchVsRefMultAfter =
      new TH2F("hTofMatchVsRefMultAfter", ";nBTOFMatch;mult", 500, -0.5, 499.5, 500, -0.5, 499.5);
  TH2F* hWeightVsRefMultCorr =
      new TH2F("hWeightVsRefMultCorr", ";mult_{corr};weight", 500, -0.5, 499.5, 125, 0.5, 3.0);

  Long64_t nBadRun = 0, nPileup = 0, nInvalidCent = 0, nOk = 0;
  Int_t lastRunId = -1;

  for (Long64_t iEvent = 0; iEvent < events2read; iEvent++) {
    if (!picoReader->readPicoEvent(iEvent)) break;

    StPicoDst* dst = picoReader->picoDst();
    StPicoEvent* event = dst ? dst->event() : 0;
    if (!event) continue;

    const Int_t runId = event->runId();
    if (runId != lastRunId) {
      rmc->init(runId);
      lastRunId = runId;
    }

    Int_t rawMult = event->refMult();
    if (modeName == "fxtmult") rawMult = event->fxtMult();

    TVector3 pVtx = event->primaryVertex();
    const Int_t nBTofMatched = event->nBTOFMatch();
    hTofMatchVsRefMultBefore->Fill(nBTofMatched, rawMult);

    if (centCfg.rejectBadRun && rmc->isBadRun(runId)) {
      nBadRun++;
      continue;
    }
    if (centCfg.rejectPileup && rmc->isPileUpEvent(rawMult, nBTofMatched, pVtx.Z())) {
      nPileup++;
      continue;
    }

    rmc->initEvent(static_cast<UShort_t>(rawMult), pVtx.Z(), event->ZDCx());
    const Int_t cent9 = rmc->getCentralityBin9();
    const Int_t cent16 = rmc->getCentralityBin16();
    if (centCfg.requireValidCentrality && (cent9 < 0 || cent16 < 0)) {
      nInvalidCent++;
      continue;
    }
    if (!centCfg.IsCentBinAccepted(cent9)) continue;

    const Double_t refMultCorr = rmc->getRefMultCorr();
    const Double_t weight = centCfg.useWeight ? rmc->getWeight() : 1.0;

    hRefMult->Fill(rawMult);
    hRefMultCorr->Fill(refMultCorr);
    hCent9->Fill(cent9, weight);
    hCent16->Fill(cent16, weight);
    hTofMatchVsRefMultAfter->Fill(nBTofMatched, rawMult);
    hWeightVsRefMultCorr->Fill(refMultCorr, weight);
    nOk++;
  }

  picoReader->Finish();

  std::cout << "[checkCentrality] summary: ok=" << nOk << " badRun=" << nBadRun << " pileup=" << nPileup
            << " invalidCent=" << nInvalidCent << std::endl;

  TFile* oFile = TFile::Open(oFileName, "RECREATE");
  if (oFile && !oFile->IsZombie()) {
    hRefMult->Write();
    hRefMultCorr->Write();
    hCent9->Write();
    hCent16->Write();
    hTofMatchVsRefMultBefore->Write();
    hTofMatchVsRefMultAfter->Write();
    hWeightVsRefMultCorr->Write();
    oFile->Close();
    std::cout << "[checkCentrality] wrote " << oFileName << std::endl;
  }
  delete oFile;
  delete rmc;
  delete picoReader;
}
