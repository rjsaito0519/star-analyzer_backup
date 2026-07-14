// anaLambdaNuclearId.C - StChain based integrated Lambda and NuclearId analysis macro
// Usage: root4star -b -q 'anaLambdaNuclearId.C("input.list","output.root","0",-1)'
//        anaLambdaNuclearId.C("input.list","output.root","0",-1,"config/mainconf/main_auau19_anaLambdaNuclearId.yaml")
// Run from project root: ./script/run_anaLambdaNuclearId.sh

#include "TROOT.h"
#include "TInterpreter.h"
#include "TSystem.h"
#include "TStopwatch.h"
#include "TString.h"
#include "TFile.h"
#include "TChain.h"
#include "StChain.h"
#include "StPicoDstMaker/StPicoDstMaker.h"
#include "StMaker/StLambdaMaker/StLambdaMaker.h"
#include "StMaker/StNuclearIdMaker/StNuclearIdMaker.h"
#include "StMaker/StLambdaNuclearMixMaker/StLambdaNuclearMixMaker.h"
#include "ConfigManager.h"
#include <iostream>
#include <fstream>
#include <vector>
#include "TLorentzVector.h"
#include "TMath.h"
#include "StPicoEvent/StPicoDst.h"
#include "StPicoEvent/StPicoEvent.h"
#include "StPicoEvent/StPicoTrack.h"
#include "StMaker/common/CentralityHelper.h"
#include "cuts/EventCutConfig.h"
#include "cuts/NuclearIdCutConfig.h"
#include "cuts/CentralityCutConfig.h"

namespace {
  const Double_t kProtonMass = 0.938272;
  const Double_t kPionMass   = 0.139570;

  const Double_t M_d   = 1.87561;
  const Double_t M_t   = 2.80892;
  const Double_t M_3He = 2.80839;
  const Double_t M_4He = 3.72742;
}

StChain* chain = 0;
StLambdaMaker* lambdaMaker = 0;
StNuclearIdMaker* nuclearidMaker = 0;
StLambdaNuclearMixMaker* mixMaker = 0;

void anaLambdaNuclearId(const Char_t* inputFile = "config/picoDstList/auau19GeV_lambda.list",
            const Char_t* outputFile = "rootfile/auau19_anaLambdaNuclearId_temp/auau19_anaLambdaNuclearId_temp.root",
            const Char_t* jobid = "0",
            Long64_t nEventsMax = -1,
            const Char_t* configPath = 0)
{
  TStopwatch timer;
  timer.Start();

  Long64_t nEvents = (nEventsMax > 0) ? nEventsMax : 10000000;

  const char* pwd = gSystem->Getenv("PWD");
  if (!pwd) pwd = ".";

  TString mainConfigPath;
  if (configPath && strlen(configPath) > 0) {
    mainConfigPath = configPath;
    if (mainConfigPath(0) != '/') mainConfigPath = TString(pwd) + "/" + mainConfigPath;
  } else {
    const char* env_conf = gSystem->Getenv("STAR_ANA_MAINCONF");
    if (env_conf && strlen(env_conf) > 0) {
      mainConfigPath = env_conf;
      if (mainConfigPath(0) != '/') mainConfigPath = TString(pwd) + "/" + mainConfigPath;
    } else {
      TString fallbackPath = TString(pwd) + "/.current_mainconf";
      std::ifstream infile(fallbackPath.Data());
      std::string line;
      if (infile.is_open() && std::getline(infile, line)) {
        mainConfigPath = line.c_str();
        if (mainConfigPath(0) != '/') mainConfigPath = TString(pwd) + "/" + mainConfigPath;
      } else {
        mainConfigPath = TString(pwd) + "/config/mainconf/main_auau19_anaLambdaNuclearId.yaml";
      }
    }
  }
  if (!ConfigManager::GetInstance().LoadConfig(mainConfigPath.Data())) {
    std::cerr << "ERROR: Failed to load config: " << mainConfigPath.Data() << std::endl;
    return;
  }

  chain = new StChain();
  StPicoDstMaker* picoMaker = new StPicoDstMaker(StPicoDstMaker::IoRead, inputFile, "picoDst");
  picoMaker->SetStatus("*", 0);
  picoMaker->SetStatus("Event", 1);
  picoMaker->SetStatus("Track", 1);
  picoMaker->SetStatus("BTofHit", 1);
  picoMaker->SetStatus("BTofPidTraits", 1);
  picoMaker->SetStatus("BbcHit", 1);
  picoMaker->SetStatus("EpdHit", 1);
  picoMaker->SetStatus("MtdHit", 1);
  picoMaker->SetStatus("BTowHit", 1);
  picoMaker->SetStatus("ETofPidTraits", 1);

  // Pass empty string as output file so the makers don't write during their Finish()
  lambdaMaker = new StLambdaMaker("lambda", picoMaker, "");
  nuclearidMaker = new StNuclearIdMaker("nuclearid", picoMaker, "");
  mixMaker = new StLambdaNuclearMixMaker("mix", "");

  chain->AddMaker(picoMaker);
  chain->AddMaker(lambdaMaker);
  chain->AddMaker(nuclearidMaker);
  chain->AddMaker(mixMaker);

  if (chain->Init() == kStErr) {
    std::cerr << "ERROR: chain->Init() returned kStErr" << std::endl;
    delete chain;
    chain = 0;
    lambdaMaker = 0;
    nuclearidMaker = 0;
    mixMaker = 0;
    return;
  }

  Long64_t totalEntries = picoMaker->chain() ? picoMaker->chain()->GetEntries() : 0;
  std::cout << "Total entries = " << totalEntries << std::endl;

  if (totalEntries <= 0) {
    std::cerr << "ERROR: no entries found. Check inputFile." << std::endl;
    chain->Finish();
    delete chain;
    chain = 0;
    lambdaMaker = 0;
    nuclearidMaker = 0;
    mixMaker = 0;
    return;
  }

  if (nEvents > totalEntries) nEvents = totalEntries;

  // Initialize CentralityHelper for event-level QA and cuts
  CentralityHelper centrality;
  if (!centrality.Init(ConfigManager::GetInstance().GetCentralityCuts())) {
    std::cerr << "WARNING: CentralityHelper initialization failed in macro" << std::endl;
  }
  for (Long64_t i = 0; i < nEvents; i++) {
    if (i % 1000 == 0) std::cout << "Working on event " << i << std::endl;
    chain->Clear();
    Int_t iret = chain->Make(i);
    if (iret) {
      std::cerr << "Bad return code: " << iret << " at event " << i << std::endl;
      break;
    }
    StPicoDst* picoDst = picoMaker->picoDst();
    if (!picoDst) continue;
    StPicoEvent* event = picoDst->event();
    if (!event) continue;

    TVector3 pVtx = event->primaryVertex();
    Int_t refMult = event->refMult();
    const Int_t runId = event->runId();
    const Int_t nBTOFMatch = event->nBTOFMatch();
    const Double_t vz = pVtx.Z();

    CentralityCutConfig& centCfg = ConfigManager::GetInstance().GetCentralityCuts();
    Int_t rawMult = refMult;
    if (centCfg.enabled) {
      TString mode(centCfg.mode.c_str());
      mode.ToLower();
      if (mode == "fxtmult") {
        rawMult = event->fxtMult();
      }
    }

    Int_t cent9 = -1;
    Int_t cent16 = -1;
    Double_t refMultCorr = -1.0;
    Double_t centWeight = 1.0;

    CentralityRejectReason centReason = kCentralityOk;
    if (centrality.IsEnabled()) {
      if (!centrality.CheckBadRun(runId, centReason)) continue;
      if (!centrality.CheckPileup(rawMult, nBTOFMatch, vz, centReason)) continue;
      if (!centrality.ComputeBins(event, rawMult, vz, cent9, cent16, refMultCorr, centWeight, centReason)) continue;
      if (!centrality.AcceptCentBin(cent9, refMultCorr, centReason)) continue;
    }

    // Pass event cuts from EventCutConfig
    // EventCutConfig& evCuts = ConfigManager::GetInstance().GetEventCuts();
    // Int_t nTr = picoDst->numberOfTracks();
    // if (evCuts.maxNTr > 0 && nTr > evCuts.maxNTr) continue;

    // double vr = pVtx.Perp();
    // if (vz < evCuts.minVz || vz > evCuts.maxVz) continue;
    // if (vr >= evCuts.maxVr) continue;
    // if (evCuts.maxVzDiff > 0) {
    //   double vzVpd = event->vzVpd();
    //   if (fabs(vzVpd) < evCuts.maxAbsVzVpd) {
    //     if (fabs(vz - vzVpd) >= evCuts.maxVzDiff) continue;
    //   }
    // }

    // 1. Retrieve reconstructed Lambda and Nuclear candidates from the makers
    const std::vector<TVector3>& lamMoms = lambdaMaker->GetLambdaMomList();
    const std::vector<Double_t>& lamMasses = lambdaMaker->GetLambdaInvMassList();
    const std::vector<Int_t>& lamProtonIds = lambdaMaker->GetLambdaProtonIdList();
    const std::vector<Int_t>& lamPionIds = lambdaMaker->GetLambdaPionIdList();

    const std::vector<TVector3>& nucMoms = nuclearidMaker->GetNuclearMomList();
    const std::vector<Int_t>& nucIds = nuclearidMaker->GetNuclearIdList();
    const std::vector<Int_t>& nucTypes = nuclearidMaker->GetNuclearTypeList();

    NuclearIdCutConfig& nucCuts = ConfigManager::GetInstance().GetNuclearIdCuts();

    if (lamMoms.size() > 0 || nucMoms.size() > 0) {
    //  std::cout << "DEBUG Event " << i << ": lamMoms=" << lamMoms.size() << ", nucMoms=" << nucMoms.size() << std::endl;
    }

    const Double_t kLambdaMass = 1.115683; // Lambda PDG mass

    // 2. Correlate Lambdas and Nuclei
    for (size_t il = 0; il < lamMoms.size(); il++) {
      double invMass = lamMasses[il];
      double delta   = invMass - nucCuts.MeanLambda;
      double window  = nucCuts.nsigmaLambda * nucCuts.sigmaLambda;

      // Classify Lambda candidate into signal or sideband regions
      bool isSig   = (TMath::Abs(delta) <= window);
      bool isSBPos = (!isSig && delta > window  && delta <= 4.0 * window);
      bool isSBNeg = (!isSig && delta < -window && delta >= -4.0 * window);

      // Construct Lambda TLorentzVector using PDG mass
      TLorentzVector lambdaMom;
      lambdaMom.SetVectM(lamMoms[il], kLambdaMass);

      for (size_t in = 0; in < nucMoms.size(); in++) {
        // Self-correlation cut: nucleus track cannot be one of the Lambda daughter tracks
        if (nucIds[in] == lamProtonIds[il] || nucIds[in] == lamPionIds[il]) continue;

        // Construct Nucleus TLorentzVector using PDG mass
        double mass = 0;
        int nucType = nucTypes[in];
        if (nucType == 0) {
          mass = M_d;
        } else if (nucType == 1) {
          mass = M_t;
        } else if (nucType == 2) {
          mass = M_3He;
        } else if (nucType == 3) {
          mass = M_4He;
        }

        TLorentzVector nMom;
        nMom.SetVectM(nucMoms[in], mass);

        // Lab frame relative momentum q_lab = |p_lambda - p_nucleus|
        TVector3 p3_lam = lambdaMom.Vect();
        TVector3 p3_nuc = nMom.Vect();
        double q_lab = (p3_lam - p3_nuc).Mag();

        // Rest frame relative momentum k* (momentum of either particle in rest frame of the pair)
        TLorentzVector pair = lambdaMom + nMom;
        TVector3 boostVec = -pair.BoostVector();
        TLorentzVector lamRest = lambdaMom;
        lamRest.Boost(boostVec);
        double k_star = lamRest.P();

        // Fill histograms based on region
        if (nuclearidMaker) {
          nuclearidMaker->FillKstarMass(k_star, invMass, nucType, cent9);

          if (!isSig && !isSBPos && !isSBNeg) continue;

          if (isSig) {
            nuclearidMaker->FillKstar(k_star, q_lab, nucType, cent9);
          } else if (isSBPos) {
            nuclearidMaker->FillKstarSideband(k_star, q_lab, nucType, +1, cent9);
          } else if (isSBNeg) {
            nuclearidMaker->FillKstarSideband(k_star, q_lab, nucType, -1, cent9);
          }
        }
      }
    }
  }

  std::cout << "******************************************" << std::endl;
  std::cout << "Work done... chain->Finish()" << std::endl;
  std::cout << "******************************************" << std::endl;
  chain->Finish();

  // Create single combined ROOT file and write histograms from all makers.
  // NOTE: nuclearidMaker and mixMaker both load from the same YAML, so they share
  // histogram names (e.g. hKstar_d). To avoid ROOT cycle-overwriting the true-event
  // histograms with the mix maker's 0-entry copies, each maker writes to its own
  // TDirectory subdirectory.
  if (outputFile && strlen(outputFile) > 0) {
    // Ensure parent directories exist
    gSystem->mkdir(gSystem->DirName(outputFile), kTRUE);
    TFile* fout = new TFile(outputFile, "RECREATE");
    if (fout && !fout->IsZombie()) {
      if (lambdaMaker) {
        std::cout << "Writing Lambda histograms..." << std::endl;
        fout->cd();
        lambdaMaker->WriteHistograms();
      }
      if (nuclearidMaker) {
        std::cout << "Writing NuclearId histograms..." << std::endl;
        TDirectory* dirNuc = fout->mkdir("true");
        dirNuc->cd();
        nuclearidMaker->WriteHistograms();
      }
      if (mixMaker) {
        std::cout << "Writing Mix histograms..." << std::endl;
        TDirectory* dirMix = fout->mkdir("mix");
        dirMix->cd();
        mixMaker->WriteHistograms();
      }

      fout->Close();
      delete fout;
      std::cout << "Successfully saved all histograms to " << outputFile << std::endl;
    } else {
      std::cerr << "ERROR: Failed to open output ROOT file: " << outputFile << std::endl;
    }
  }



  timer.Stop();
  std::cout << "Processed events: " << nEvents << std::endl;
  std::cout << "RealTime: " << timer.RealTime() << " CpuTime: " << timer.CpuTime() << std::endl;

  delete chain;
  chain = 0;
  lambdaMaker = 0;
  nuclearidMaker = 0;
  mixMaker = 0;
}
