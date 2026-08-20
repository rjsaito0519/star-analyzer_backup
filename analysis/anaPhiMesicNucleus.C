#include "TChain.h"
#include "TStopwatch.h"
#include "TString.h"
#include "TSystem.h"

#include "ConfigManager.h"
#include "StChain.h"
#include "StMaker/StPhiMesicNucleusMaker/StPhiMesicNucleusMaker.h"
#include "StPicoDstMaker/StPicoDstMaker.h"

#include <fstream>
#include <iostream>

StChain* chain = 0;
StPhiMesicNucleusMaker* phiMesicMaker = 0;

void anaPhiMesicNucleus(const Char_t* inputFile = "config/picoDstList/auau3p85GeVfxt.list",
                        const Char_t* outputFile = "rootfile/auau3p85fxt_anaPhiMesicNucleus/auau3p85fxt_anaPhiMesicNucleus_temp.root",
                        const Char_t* jobid = "0",
                        Long64_t nEventsMax = -1,
                        const Char_t* configPath = 0) {
  (void)jobid;
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
      mainConfigPath = TString(pwd) + "/config/mainconf/main_auau3p85fxt_anaPhiMesicNucleus.yaml";
    }
  }

  if (!ConfigManager::GetInstance().LoadConfig(mainConfigPath.Data())) {
    std::cerr << "ERROR: failed to load config: " << mainConfigPath.Data() << std::endl;
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

  phiMesicMaker = new StPhiMesicNucleusMaker("phiMesicNucleus", picoMaker, outputFile);
  chain->AddMaker(picoMaker);
  chain->AddMaker(phiMesicMaker);

  if (chain->Init() == kStErr) {
    std::cerr << "ERROR: chain init failed" << std::endl;
    delete chain;
    chain = 0;
    phiMesicMaker = 0;
    return;
  }

  Long64_t totalEntries = picoMaker->chain() ? picoMaker->chain()->GetEntries() : 0;
  if (totalEntries <= 0) {
    std::cerr << "ERROR: no entries in input file list" << std::endl;
    chain->Finish();
    delete chain;
    chain = 0;
    phiMesicMaker = 0;
    return;
  }
  if (nEvents > totalEntries) nEvents = totalEntries;

  for (Long64_t i = 0; i < nEvents; ++i) {
    if (i % 1000 == 0) std::cout << "Working on event " << i << std::endl;
    chain->Clear();
    Int_t iret = chain->Make(i);
    if (iret) {
      std::cerr << "Bad return code: " << iret << " at event " << i << std::endl;
      break;
    }
  }

  chain->Finish();
  timer.Stop();
  std::cout << "Processed events: " << nEvents << std::endl;
  std::cout << "RealTime: " << timer.RealTime() << " CpuTime: " << timer.CpuTime() << std::endl;

  delete chain;
  chain = 0;
  phiMesicMaker = 0;
}
