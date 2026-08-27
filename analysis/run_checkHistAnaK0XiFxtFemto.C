// run_checkHistAnaK0XiFxtFemto.C — reuses checkHistAnaKXiFemto pages for FXT Step 3 smoke.
// Usage: root4star -b -q 'run_checkHistAnaK0XiFxtFemto.C("rootfile/...","auau3p9fxt_anaK0XiFemto","config/mainconf/main_auau3p9fxt_anaK0XiFemto.yaml")'

void run_checkHistAnaK0XiFxtFemto(const Char_t* rootFile,
                                  const Char_t* anaName,
                                  const Char_t* mainconfPath = 0)
{
  const char* pwd = gSystem->Getenv("PWD");
  if (!pwd) pwd = ".";

  if (gSystem->Load(TString(pwd) + "/lib/libStarAnaConfig.so") < 0) {
    std::cerr << "ERROR: failed to load libStarAnaConfig.so" << std::endl;
    return;
  }

  gInterpreter->AddIncludePath(pwd);
  gInterpreter->AddIncludePath(TString::Format("%s/include", pwd));
  gSystem->AddLinkedLibs(TString::Format("-L%s/lib -lStarAnaConfig -Wl,-rpath,%s/lib", pwd, pwd));

  TString acliCBuildDir = TString::Format("%s/.build/aclic", pwd);
  gSystem->mkdir(acliCBuildDir.Data(), kTRUE);
  gSystem->SetBuildDir(acliCBuildDir.Data());

  gROOT->ProcessLine(TString::Format(".L %s/common/macro/checkHistAnaKXiFemto.C+", pwd));
  checkHistAnaKXiFemto(rootFile, anaName, mainconfPath);
}
