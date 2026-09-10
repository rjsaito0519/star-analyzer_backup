#ifndef STAR_ANALYZER_KF_EVENT_SELECTION_H
#define STAR_ANALYZER_KF_EVENT_SELECTION_H

#include <iosfwd>
#include <string>

class EventCutConfig;
class StPicoEvent;

// KF-only, immutable effective event cuts. Shared EventCutConfig and its
// singleton are deliberately not modified by analysis.mode profile selection.
class KfEventSelection {
public:
  enum Result {
    kAccepted = 0, kInvalidVertex, kVz, kVr, kRefMult, kVpd, kTrackCount,
    kResultCount
  };

  KfEventSelection();
  bool Load(const char* mainConfigPath, const EventCutConfig& base,
            const std::string& centralityMode, bool centralityEnabled,
            std::ostream& errors, bool legacyLambdaCuts = false);
  Result Check(const StPicoEvent& event, int nTracks) const;
  double ComputeVr(double x, double y) const;
  void Dump(std::ostream& output) const;
  const std::string& Mode() const { return mMode; }

private:
  bool mLoaded;
  bool mLegacyLambdaCuts;
  std::string mMode;
  std::string mMainPath;
  std::string mAnalysisPath;
  std::string mEventPath;
  std::string mVertexSource;
  double mMinVz, mMaxVz, mCenterX, mCenterY, mRadius;
  double mMinRefMult, mMaxRefMult, mMaxVzDiff, mMaxAbsVzVpd;
  int mMaxNTr;
};

#endif
