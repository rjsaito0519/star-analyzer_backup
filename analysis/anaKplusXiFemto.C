// anaKplusXiFemto.C - StChain-based K+-Xi- Femtoscopy analysis macro
// Usage: run via run_anaKplusXiFemto.C wrapper

#include "TROOT.h"
#include "TInterpreter.h"
#include "TSystem.h"
#include "TStopwatch.h"
#include "TString.h"
#include "TChain.h"
#include "StChain.h"
#include "StPicoDstMaker/StPicoDstMaker.h"
#include "StMaker/StXiMaker/StXiMaker.h"
#include "StMaker/StKplusXiFemtoMaker/StKplusXiFemtoMaker.h"
#include "ConfigManager.h"
#include <iostream>
#include <fstream>

StChain* chain = 0;
StXiMaker* xiMaker = 0;
StKplusXiFemtoMaker* femtoMaker = 0;

void anaKplusXiFemto(const Char_t* inputFile = "config/picoDstList/auau19GeV.list",
                     const Char_t* outputFile = "rootfile/auau19_anaKplusXiFemto_temp/auau19_anaKplusXiFemto_temp.root",
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
    mainConfigPath = TString(pwd) + "/config/mainconf/main_auau19_anaKplusXiFemto.yaml";
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

  TString xiOut = "dummy_xi.root";
  xiMaker = new StXiMaker("xi", picoMaker, xiOut.Data());
  femtoMaker = new StKplusXiFemtoMaker("kplusXiFemto", picoMaker, xiMaker, outputFile);

  chain->AddMaker(picoMaker);
  chain->AddMaker(xiMaker);
  chain->AddMaker(femtoMaker);

  if (chain->Init() == kStErr) {
    std::cerr << "ERROR: chain->Init() returned kStErr" << std::endl;
    delete chain;
    chain = 0;
    xiMaker = 0;
    femtoMaker = 0;
    return;
  }

  Long64_t totalEntries = picoMaker->chain() ? picoMaker->chain()->GetEntries() : 0;
  std::cout << "Total entries = " << totalEntries << std::endl;

  if (totalEntries <= 0) {
    std::cerr << "ERROR: no entries found. Check inputFile." << std::endl;
    chain->Finish();
    delete chain;
    chain = 0;
    return;
  }

  if (nEvents > totalEntries) nEvents = totalEntries;

  Int_t status = 0;
  Long64_t eventCounter = 0;
  for (Long64_t i = 0; i < nEvents; i++) {
    if (i % 1000 == 0) {
      std::cout << "Working on eventNumber " << i << std::endl;
    }
    chain->Clear();
    status = chain->Make();
    if (status == kStEOF || status == kStFatal) {
      std::cout << "EOF or Fatal status = " << status << " at event " << i << std::endl;
      break;
    }
    eventCounter++;
  }

  std::cout << "Processed " << eventCounter << " events." << std::endl;

  chain->Finish();
  delete chain;
  chain = 0;

  gSystem->Unlink(xiOut.Data());

  timer.Stop();
  std::cout << "RealTime = " << timer.RealTime() << " s, CpuTime = " << timer.CpuTime() << " s" << std::endl;
}
