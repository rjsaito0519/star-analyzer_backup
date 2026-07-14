// anaFemtoPhiDeuteron.C - StChain based phi-deuteron femtoscopy with StFemtoMaker
// Usage: root4star -b -q 'anaFemtoPhiDeuteron.C("input.list","output.root","0",-1)'
// Run from project root: ./script/run_anaFemtoPhiDeuteron.sh

#include "TROOT.h"
#include "TInterpreter.h"
#include "TSystem.h"
#include "TStopwatch.h"
#include "TString.h"
#include "TChain.h"
#include "StChain.h"
#include "StPicoDstMaker/StPicoDstMaker.h"
#include "StMaker/StFemtoMaker/StFemtoMaker.h"
#include "ConfigManager.h"
#include <iostream>
#include <fstream>

StChain* chain = 0;
StFemtoMaker* femtoMaker = 0;

void anaFemtoPhiDeuteron(const Char_t* inputFile = "config/picoDstList/auau19GeV.list",
                       const Char_t* outputFile = "rootfile/auau3p85fxt_anaFemtoPhiDeuteron_temp/auau3p85fxt_anaFemtoPhiDeuteron_temp.root",
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
        mainConfigPath = TString(pwd) + "/config/mainconf/main_auau3p85fxt_anaFemtoPhiDeuteron.yaml";
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

  femtoMaker = new StFemtoMaker("femto", picoMaker, outputFile);
  chain->AddMaker(picoMaker);
  chain->AddMaker(femtoMaker);

  if (chain->Init() == kStErr) {
    std::cerr << "ERROR: chain->Init() returned kStErr" << std::endl;
    delete chain;
    chain = 0;
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
    femtoMaker = 0;
    return;
  }

  if (nEvents > totalEntries) nEvents = totalEntries;

  for (Long64_t i = 0; i < nEvents; i++) {
    if (i % 1000 == 0) std::cout << "Working on event " << i << std::endl;
    chain->Clear();
    Int_t iret = chain->Make(i);
    if (iret) {
      std::cerr << "Bad return code: " << iret << " at event " << i << std::endl;
      break;
    }
  }

  std::cout << "******************************************" << std::endl;
  std::cout << "Work done... chain->Finish()" << std::endl;
  std::cout << "******************************************" << std::endl;
  chain->Finish();

  timer.Stop();
  std::cout << "Processed events: " << nEvents << std::endl;
  std::cout << "RealTime: " << timer.RealTime() << " CpuTime: " << timer.CpuTime() << std::endl;

  delete chain;
  chain = 0;
  femtoMaker = 0;
}
