#ifndef ST_XI_KF_PARTICLE_MAKER_H
#define ST_XI_KF_PARTICLE_MAKER_H

#include "StMaker.h"
#include "TString.h"
#include "TVector3.h"
#include <vector>

class CentralityHelper;
class KfEventSelection;
class HistManager;
class StPicoKFParticleInterface;
struct KfXiCandidate;
class StPicoDst;
class StPicoEvent;
class StPicoDstMaker;
class TFile;
class TTree;

class StXiKFParticleMaker;
StXiKFParticleMaker* createStXiKFParticleMaker(
    const char* name, StPicoDstMaker* picoMaker, const char* outName);
extern "C" void* createStXiKFParticleMakerC(
    const char* name, void* picoMaker, const char* outName);

class StXiKFParticleMaker : public StMaker {
public:
  StXiKFParticleMaker(const char* name, StPicoDstMaker* picoMaker, const char* outName);
  virtual ~StXiKFParticleMaker();
  virtual Int_t Init();
  virtual Int_t Make();
  virtual void Clear(Option_t* option = "");
  virtual Int_t Finish();

  void SetProcessingSucceeded(Bool_t value) { mProcessingSucceeded = value; }
  Long64_t GetReadEvents() const { return mEventCounter; }
  Long64_t GetReconstructedEvents() const { return mReconstructedEvents; }
  void SetMainConfigPath(const char* path) { mMainConfigPath = path ? path : ""; }

private:
  Bool_t PassCandidateCuts(const KfXiCandidate& candidate) const;
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
  KfXiCandidate* mCandidateRow;
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
};
#endif
