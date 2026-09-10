// Compiled StChain entry point. Physics stays in the Maker/STAR Pico adapter.
#include "TChain.h"
#include "TStopwatch.h"
#include "TString.h"
#include "TSystem.h"
#include "StChain.h"
#include "StPicoDstMaker/StPicoDstMaker.h"
#include "StMaker/StLambdaKFParticleMaker/StLambdaKFParticleMaker.h"
#include "ConfigManager.h"
#include "cuts/KfParticleCutConfig.h"
#include <cstring>
#include <fstream>
#include <sstream>
#include <iostream>

Int_t anaLambda_KFParticle(
    const Char_t* inputFile = "config/picoDstList/auau19GeV.list",
    const Char_t* outputFile = "rootfile/auau19_anaLambda_KFParticle_temp/auau19_anaLambda_KFParticle_temp.root",
    const Char_t* jobid = "0", Long64_t nEventsMax = -1, const Char_t* configPath = 0) {
  TStopwatch timer;
  timer.Start();
  if (!inputFile || !*inputFile || !outputFile || !*outputFile || nEventsMax == 0 || nEventsMax < -1) {
    std::cerr << "ERROR: input/output required; nEvents must be positive or -1" << std::endl;
    return 1;
  }
  const char* pwd = gSystem->WorkingDirectory();
  TString config;
  if (configPath && *configPath) config = configPath;
  else if (gSystem->Getenv("STAR_ANA_MAINCONF")) config = gSystem->Getenv("STAR_ANA_MAINCONF");
  else config = "config/mainconf/main_auau19_anaLambda_KFParticle.yaml";
  if (config(0) != '/') config = TString(pwd) + "/" + config;
  if (!ConfigManager::GetInstance().LoadConfig(config.Data())) return 1;
  if (!ConfigManager::GetInstance().GetKfParticleCuts().Validate(std::cerr)) return 1;
  if (!gSystem->AccessPathName(outputFile)) {
    std::cerr << "ERROR: output already exists; choose a new file: " << outputFile << std::endl;
    return 1;
  }
  Long64_t expectedFiles = 0;
  if (TString(inputFile).Contains(".list")) {
    std::ifstream list(inputFile);
    if (!list) { std::cerr << "ERROR: cannot open input list " << inputFile << std::endl; return 2; }
    std::string line;
    while (std::getline(list, line)) {
      const std::string::size_type comment = line.find('#');
      if (comment != std::string::npos) line.erase(comment);
      std::istringstream fields(line);
      std::string file;
      if (!(fields >> file)) continue;
      if (file.find(".picoDst.root") == std::string::npos) {
        std::cerr << "ERROR: unsupported input-list entry: " << file << std::endl; return 2;
      }
      ++expectedFiles;
    }
    if (!expectedFiles) { std::cerr << "ERROR: input list is empty" << std::endl; return 2; }
  }
  gSystem->mkdir(gSystem->DirName(outputFile), kTRUE);
  StChain* chain = new StChain();
  StPicoDstMaker* pico = new StPicoDstMaker(StPicoDstMaker::IoRead, inputFile, "picoDst");
  pico->SetStatus("*", 0);
  pico->SetStatus("Event", 1);
  pico->SetStatus("Track", 1);
  pico->SetStatus("TrackCovMatrix", 1);
  pico->SetStatus("BTofHit", 1);
  pico->SetStatus("BTofPidTraits", 1);
  StLambdaKFParticleMaker* maker = new StLambdaKFParticleMaker("lambdaKFParticle", pico, outputFile);
  maker->SetMainConfigPath(config.Data());
  // StMaker construction registers both Makers with the current StChain.
  const Int_t init = chain->Init();
  if (init != kStOK) {
    std::cerr << "ERROR: chain Init failed: " << init << std::endl;
    delete chain;
    return 1;
  }
  TChain* input = pico->chain();
  const Long64_t total = input ? input->GetEntries() : 0;
  Long64_t requested = nEventsMax > 0 ? nEventsMax : total;
  if (requested > total) requested = total;
  Int_t status = 0;
  if (expectedFiles && (!input || input->GetListOfFiles()->GetEntries() != expectedFiles)) {
    std::cerr << "ERROR: Pico reader skipped input files; expected=" << expectedFiles
              << " attached=" << (input ? input->GetListOfFiles()->GetEntries() : 0) << std::endl;
    status = 2;
  }
  if (total <= 0) {
    std::cerr << "ERROR: no input events; this is not a reconstruction test" << std::endl;
    status = 2;
  }
  Long64_t completed = 0;
  for (Long64_t i = 0; !status && i < requested; ++i) {
    // Check every underlying file, not just the first, before any KF processing.
    if (input->LoadTree(i) < 0 || !input->GetTree()) {
      std::cerr << "ERROR: unreadable input tree at entry " << i << std::endl;
      status = 2;
      break;
    }
    const char* required[] = {"Event", "Track", "TrackCovMatrix", "BTofPidTraits"};
    const Int_t nRequired = ConfigManager::GetInstance().GetKfParticleCuts().useTof ? 4 : 3;
    for (Int_t b = 0; b < nRequired; ++b) {
      const TString dotted = TString(required[b]) + ".";
      if (!input->GetTree()->GetBranch(required[b]) && !input->GetTree()->GetBranch(dotted)) {
        std::cerr << "ERROR: input lacks required branch " << required[b] << " at entry " << i << std::endl;
        status = 2;
        break;
      }
    }
    if (status) break;
    if (i % 1000 == 0) std::cout << "Working on event " << i << std::endl;
    chain->Clear();
    const Int_t result = chain->Make(i);
    if (result != kStOK) {
      std::cerr << "ERROR: chain Make code " << result << " at event " << i << std::endl;
      status = 3;
      break;
    }
    ++completed;
  }
  if (!status && maker->GetReconstructedEvents() <= 0) {
    std::cerr << "ERROR: no events reached successful KF processing; inspect event/centrality selection" << std::endl;
    status = 2;
  }
  maker->SetProcessingSucceeded(status == 0);
  if (chain->Finish() != kStOK) status = 4;
  timer.Stop();
  std::cout << "KF job=" << jobid << " requested=" << requested << " completed=" << completed
            << " reconstructed=" << maker->GetReconstructedEvents()
            << " status=" << status << " RealTime=" << timer.RealTime()
            << " CpuTime=" << timer.CpuTime() << std::endl;
  delete chain;
  return status;
}
