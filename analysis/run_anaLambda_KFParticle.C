// ROOT5 runner: no KF types are interpreted by CINT.
#include "TROOT.h"
#include "TSystem.h"
#include "TString.h"
#include "TInterpreter.h"
#include <iostream>

TString LambdaKfQuote(const char* value) {
  TString quoted(value ? value : "");
  quoted.ReplaceAll("\\", "\\\\");
  quoted.ReplaceAll("\"", "\\\"");
  quoted.ReplaceAll("\n", "\\n");
  quoted.ReplaceAll("\r", "\\r");
  return TString("\"") + quoted + "\"";
}

void run_anaLambda_KFParticle(const Char_t* inputFile, const Char_t* outputFile,
                              const Char_t* jobid = "0", Long64_t nEventsMax = -1,
                              const Char_t* configPath = 0) {
  // ROOT 5 can recover from SIGSEGV and subsequently exit zero. A crashed KF
  // job must terminate unsuccessfully, not leave a partial output as "success".
  gSystem->ResetSignal(kSigSegmentationViolation, kTRUE);
  gSystem->ResetSignal(kSigBus, kTRUE);
  gSystem->ResetSignal(kSigIllegalInstruction, kTRUE);
  gSystem->ResetSignal(kSigFloatingException, kTRUE);
  const TString pwd = gSystem->WorkingDirectory();
  if (gROOT->LoadMacro("$STAR/StRoot/StMuDSTMaker/COMMON/macros/loadSharedLibraries.C") < 0) {
    gSystem->Exit(1); return;
  }
  Int_t loadError = 0;
  gROOT->ProcessLine("loadSharedLibraries();", &loadError);
  if (loadError) { gSystem->Exit(1); return; }
  const char* starLibraries[] = {"StarRoot", "StBichsel", "StPicoEvent", "StPicoDstMaker"};
  for (UInt_t i = 0; i < sizeof(starLibraries)/sizeof(starLibraries[0]); ++i)
    if (gSystem->Load(starLibraries[i]) < 0) {
      std::cerr << "ERROR: loading " << starLibraries[i] << std::endl;
      gSystem->Exit(1); return;
    }
  const char* libraries[] = {"libStarAnaConfig.so", "libStRefMultCorr.so", "libKFParticle.so",
      "libStKfParticleCommon.so", "libStCommon.so", "libStLambdaKFParticleMaker.so"};
  for (UInt_t i = 0; i < sizeof(libraries)/sizeof(libraries[0]); ++i) {
    const TString path = pwd + "/lib/" + libraries[i];
    if (gSystem->Load(path) < 0) {
      std::cerr << "ERROR: loading " << path << std::endl;
      gSystem->Exit(1); return;
    }
  }
  gInterpreter->AddIncludePath(pwd.Data());
  gInterpreter->AddIncludePath((pwd + "/include").Data());
  gInterpreter->AddIncludePath((pwd + "/StMaker/common").Data());
  gInterpreter->AddIncludePath((pwd + "/StMaker/kfparticle").Data());
  gInterpreter->AddIncludePath("$STAR/StRoot");
  // The macro only sees plain Maker/config APIs, not vendor KF/SIMD headers.
  gSystem->AddLinkedLibs(TString::Format(
      "-L%s/lib -lStLambdaKFParticleMaker -lStKfParticleCommon -lKFParticle "
      "-lStCommon -lStarAnaConfig -lStRefMultCorr -Wl,-rpath,%s/lib", pwd.Data(), pwd.Data()));
  // Process-specific build directories prevent concurrent jobs from replacing
  // one another's dictionary/library. Retain these generated files for diagnostics.
  const TString buildDir = TString::Format("%s/tmp/kf-aclic/%s-%d",
      pwd.Data(), gSystem->HostName(), gSystem->GetPid());
  gSystem->mkdir(buildDir, kTRUE);
  gSystem->SetBuildDir(buildDir, kTRUE);
  // Force ACLiC rebuild for a new ABI; do not delete arbitrary cache globs.
  if (!gSystem->CompileMacro((pwd + "/analysis/anaLambda_KFParticle.C").Data(), "kf")) {
    std::cerr << "ERROR: compiling anaLambda_KFParticle.C" << std::endl;
    gSystem->Exit(1); return;
  }
  const TString call = TString::Format("anaLambda_KFParticle(%s,%s,%s,%lld,%s)",
      LambdaKfQuote(inputFile).Data(), LambdaKfQuote(outputFile).Data(),
      LambdaKfQuote(jobid).Data(), nEventsMax, LambdaKfQuote(configPath).Data());
  Int_t error = 0;
  // Do not invoke nested CINT while initializing a const local: ROOT 5 can
  // leak that declaration context into lazily loaded STAR table macros.
  Long_t result = 1;
  result = gROOT->ProcessLine(call.Data(), &error);
  if (error || result) gSystem->Exit(error ? 1 : static_cast<Int_t>(result));
}
