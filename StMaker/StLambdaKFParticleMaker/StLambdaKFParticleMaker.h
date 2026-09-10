#ifndef ST_LAMBDA_KF_PARTICLE_MAKER_H
#define ST_LAMBDA_KF_PARTICLE_MAKER_H

#include "StMaker.h"
#include "TString.h"
#include "TVector3.h"
#include <vector>

class CentralityHelper;
class KfEventSelection;
class HistManager;
class StPicoKFParticleInterface;
struct KfLambdaCandidate;
class StPicoDst;
class StPicoEvent;
class StPicoDstMaker;
class TFile;
class TTree;

class StLambdaKFParticleMaker;
StLambdaKFParticleMaker* createStLambdaKFParticleMaker(
    const char* name, StPicoDstMaker* picoMaker, const char* outName);
extern "C" void* createStLambdaKFParticleMakerC(
    const char* name, void* picoMaker, const char* outName);

class StLambdaKFParticleMaker : public StMaker {
public:
  StLambdaKFParticleMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName);
  virtual ~StLambdaKFParticleMaker();
  virtual Int_t Init();
  virtual Int_t Make();
  virtual void Clear(Option_t* option = "");
  virtual Int_t Finish();

  const std::vector<TVector3>& GetLambdaMomList() const { return mLambdaMom; }
  const std::vector<Double_t>& GetLambdaInvMassList() const { return mLambdaMass; }
  const std::vector<Int_t>& GetLambdaProtonIdList() const { return mProtonIds; }
  const std::vector<Int_t>& GetLambdaPionIdList() const { return mPionIds; }
  const std::vector<Int_t>& GetLambdaPdgList() const { return mPdgs; }
  void SetProcessingSucceeded(Bool_t value) { mProcessingSucceeded = value; }
  Long64_t GetReadEvents() const { return mEventCounter; }
  Long64_t GetReconstructedEvents() const { return mReconstructedEvents; }
  void SetMainConfigPath(const char* path) { mMainConfigPath = path ? path : ""; }

private:
  Bool_t PassCandidateCuts(const KfLambdaCandidate& candidate) const;
  void FillCentralityQa(Int_t cent9, Int_t rawMult, Double_t refMultCorr,
                        Int_t nTracks, Int_t nBTofMatch, Int_t nProtons, Int_t nPions, Int_t nPairs);
  void FillCandidateCentrality(Double_t mass);
  void WriteHistograms();

  StPicoDstMaker* mPicoDstMaker;
  StPicoDst* mPicoDst;
  TString mOutName;
  TString mMainConfigPath;
  KfEventSelection* mEventSelection;
  Long64_t mEventSelectionCounts[13];
  Long64_t mEventCounter;
  HistManager* mHistManager;
  CentralityHelper* mCentrality;
  StPicoKFParticleInterface* mKfInterface;
  TFile* mOutputFile;
  TTree* mCandidateTree;
  KfLambdaCandidate* mCandidateRow;
  Int_t mRunId;
  Int_t mEventId;
  Float_t mMagneticField;
  Bool_t mSelected;
  Bool_t mProcessingSucceeded;
  Long64_t mReconstructedEvents;
  Int_t mCent9;
  Int_t mCent16;
  Double_t mRefMultCorr;
  Double_t mCentWeight;
  Long64_t mTracksSeen;
  Long64_t mTracksWithInvalidCovariance;
  Long64_t mFinderCandidates;
  Long64_t mCandidatesSelected;
  Long64_t mStages[14];
  std::vector<TVector3> mLambdaMom;
  std::vector<Double_t> mLambdaMass;
  std::vector<Int_t> mProtonIds;
  std::vector<Int_t> mPionIds;
  std::vector<Int_t> mPdgs;
};
#endif
