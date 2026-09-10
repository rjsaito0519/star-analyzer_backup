#include "KfEventSelection.h"
#include "cuts/EventCutConfig.h"
#include "StPicoEvent/StPicoEvent.h"
#include "TVector3.h"
#include "yaml-cpp/yaml.h"

#include <cctype>
#include <cmath>
#include <iomanip>
#include <limits>
#include <ostream>
#include <set>
#include <stdexcept>

namespace {
void Require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

std::string Lower(const std::string& value) {
  std::string result(value);
  for (size_t i = 0; i < result.size(); ++i)
    result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
  return result;
}

void UniqueMap(const YAML::Node& node, const std::string& location) {
  Require(node.IsMap(), location + " must be a mapping");
  std::set<std::string> seen;
  for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
    Require(it->first.IsScalar(), location + " has a non-scalar key");
    const std::string key = it->first.as<std::string>();
    Require(seen.insert(key).second, location + " has duplicate key: " + key);
  }
}

std::string RequiredString(const YAML::Node& node, const std::string& location) {
  Require(node.IsScalar(), location + " must be a scalar string");
  const std::string value = node.as<std::string>();
  Require(!value.empty(), location + " must not be empty");
  return value;
}

double FiniteNumber(const YAML::Node& node, const std::string& location) {
  Require(node.IsScalar(), location + " must be a number");
  const double value = node.as<double>();
  Require(std::isfinite(value), location + " must be finite");
  return value;
}

void NumberPair(const YAML::Node& node, const std::string& location,
                double& first, double& second) {
  Require(node.IsSequence() && node.size() == 2, location + " must contain exactly two numbers");
  first = FiniteNumber(node[0], location + "[0]");
  second = FiniteNumber(node[1], location + "[1]");
}

std::string ConfigReference(const std::string& mainPath, const std::string& reference) {
  // Match the project's config/ reference convention, with explicit absolute
  // references accepted. Never infer paths from workDir or the environment.
  if (reference[0] == '/') return reference;
  const size_t position = mainPath.find("/config/");
  const std::string prefix = position == std::string::npos ? "" : mainPath.substr(0, position + 1);
  return prefix + "config/" + reference;
}

struct VertexProfile {
  double minVz, maxVz, centerX, centerY, radius;
};

VertexProfile ReadProfile(const YAML::Node& node, const std::string& location) {
  UniqueMap(node, location);
  for (YAML::const_iterator it = node.begin(); it != node.end(); ++it) {
    const std::string key = it->first.as<std::string>();
    Require(key == "vzRange" || key == "center" || key == "radius",
            location + " has unknown key: " + key);
  }
  VertexProfile result;
  NumberPair(node["vzRange"], location + ".vzRange", result.minVz, result.maxVz);
  NumberPair(node["center"], location + ".center", result.centerX, result.centerY);
  result.radius = FiniteNumber(node["radius"], location + ".radius");
  Require(result.minVz < result.maxVz, location + ".vzRange must be strictly increasing");
  Require(result.radius > 0., location + ".radius must be positive");
  return result;
}

// A diagnostic center is not an acceptance window. The sparse Imp5 schema
// intentionally cannot carry radius, Vz, RefMult or VPD cut settings.
VertexProfile ReadQaProfile(const YAML::Node& node, const std::string& location) {
  UniqueMap(node, location);
  for (YAML::const_iterator it = node.begin(); it != node.end(); ++it)
    Require(it->first.as<std::string>() == "center",
            location + " has unknown key: " + it->first.as<std::string>());
  VertexProfile result;
  result.minVz = result.maxVz = result.radius = 0.;
  NumberPair(node["center"], location + ".center", result.centerX, result.centerY);
  return result;
}
}

KfEventSelection::KfEventSelection()
    : mLoaded(false), mLegacyLambdaCuts(false),
      mMinVz(0.), mMaxVz(0.), mCenterX(0.), mCenterY(0.), mRadius(0.),
      mMinRefMult(0.), mMaxRefMult(0.), mMaxVzDiff(0.), mMaxAbsVzVpd(0.), mMaxNTr(0) {}

bool KfEventSelection::Load(const char* mainConfigPath, const EventCutConfig& base,
                            const std::string& centralityMode, bool centralityEnabled,
                            std::ostream& errors, bool legacyLambdaCuts) {
  // Any failed initial load or reload leaves Check fail-closed. Do not retain
  // a formerly valid effective mode after a configuration error.
  mLoaded = false;
  mLegacyLambdaCuts = false;
  mMode.clear();
  mMainPath = mainConfigPath ? mainConfigPath : "";
  mAnalysisPath.clear();
  mEventPath.clear();
  mVertexSource.clear();
  try {
    Require(!mMainPath.empty(), "mainconf path is required");
    const YAML::Node main = YAML::LoadFile(mMainPath);
    UniqueMap(main, "mainconf");
    mAnalysisPath = ConfigReference(mMainPath, RequiredString(main["analysis"], "mainconf.analysis"));
    mEventPath = ConfigReference(mMainPath, RequiredString(main["event"], "mainconf.event"));

    const YAML::Node analysisInfo = YAML::LoadFile(mAnalysisPath);
    UniqueMap(analysisInfo, "analysis_info");
    const YAML::Node analysis = analysisInfo["analysis"];
    UniqueMap(analysis, "analysis_info.analysis");
    const std::string mode = Lower(RequiredString(analysis["mode"], "analysis.mode"));
    Require(mode == "refmult" || mode == "fxtmult",
            "analysis.mode must be refmult (collider) or fxtmult (fixed target)");
    Require(!centralityEnabled || Lower(centralityMode) == mode,
            "enabled centrality.mode differs from analysis.mode");

    const YAML::Node event = YAML::LoadFile(mEventPath);
    UniqueMap(event, "event cuts");
    const YAML::Node qaProfiles = event["qaVertexByMode"];
    const bool useQaProfile = qaProfiles.IsDefined();
    VertexProfile vertex;
    int maximumTracks = base.maxNTr;
    if (useQaProfile) {
      Require(legacyLambdaCuts,
              "event.qaVertexByMode is only allowed with lambda_imp5");
      Require(!event["vertexByMode"].IsDefined(),
              "event cannot define both vertexByMode and qaVertexByMode");
      for (YAML::const_iterator it = event.begin(); it != event.end(); ++it) {
        const std::string key = it->first.as<std::string>();
        Require(key == "maxNTr" || key == "qaVertexByMode",
                "sparse Imp5 event cuts have unknown/inactive key: " + key);
      }
      Require(event["maxNTr"].IsScalar(), "sparse Imp5 event.maxNTr must be an integer");
      maximumTracks = event["maxNTr"].as<int>();
      Require(maximumTracks >= 0, "sparse Imp5 event.maxNTr must be nonnegative");
      UniqueMap(qaProfiles, "event.qaVertexByMode");
      for (YAML::const_iterator it = qaProfiles.begin(); it != qaProfiles.end(); ++it) {
        const std::string key = it->first.as<std::string>();
        Require(key == "refmult" || key == "fxtmult",
                "event.qaVertexByMode has unknown mode: " + key);
        ReadQaProfile(it->second, "event.qaVertexByMode." + key);
      }
      vertex = ReadQaProfile(qaProfiles[mode], "event.qaVertexByMode." + mode);
      mVertexSource = "event.qaVertexByMode." + mode;
    } else {
      // Retain archived Imp5 inputs as well as the standard mode-aware schema.
      const YAML::Node profiles = event["vertexByMode"];
      UniqueMap(profiles, "event.vertexByMode");
      for (YAML::const_iterator it = profiles.begin(); it != profiles.end(); ++it) {
        const std::string key = it->first.as<std::string>();
        Require(key == "refmult" || key == "fxtmult",
                "event.vertexByMode has unknown mode: " + key);
        ReadProfile(it->second, "event.vertexByMode." + key);
      }
      vertex = ReadProfile(profiles[mode], "event.vertexByMode." + mode);
      mVertexSource = "event.vertexByMode." + mode;
    }

    if (!legacyLambdaCuts) {
      Require(std::isfinite(base.minRefMult) && std::isfinite(base.maxRefMult) &&
              base.minRefMult <= base.maxRefMult, "invalid base RefMult range");
      Require(std::isfinite(base.maxVzDiff) && base.maxVzDiff >= 0. &&
              std::isfinite(base.maxAbsVzVpd) && base.maxAbsVzVpd >= 0., "invalid base VPD cuts");
    }
    mMinVz = vertex.minVz;
    mMaxVz = vertex.maxVz;
    mCenterX = vertex.centerX;
    mCenterY = vertex.centerY;
    mRadius = vertex.radius;
    mMinRefMult = legacyLambdaCuts ? 0. : base.minRefMult;
    mMaxRefMult = legacyLambdaCuts ? 0. : base.maxRefMult;
    mMaxVzDiff = legacyLambdaCuts ? 0. : base.maxVzDiff;
    mMaxAbsVzVpd = legacyLambdaCuts ? 0. : base.maxAbsVzVpd;
    mMaxNTr = maximumTracks;
    mMode = mode;
    mLegacyLambdaCuts = legacyLambdaCuts;
    mLoaded = true;
  } catch (const std::exception& error) {
    errors << "ERROR: KF event selection " << mMainPath << ": " << error.what() << '\n';
    return false;
  }
  return true;
}

KfEventSelection::Result KfEventSelection::Check(const StPicoEvent& event, int nTracks) const {
  if (!mLoaded) return kInvalidVertex;
  const TVector3 vertex = event.primaryVertex();
  if (!std::isfinite(vertex.X()) || !std::isfinite(vertex.Y()) || !std::isfinite(vertex.Z()))
    return kInvalidVertex;
  // Explicit Imp5 compatibility only: StLambdaMaker::PassEventCuts checks
  // maxNTr, not the other EventCutConfig fields. Keep finite-PV safety for KF.
  // The mode profile remains loaded for diagnostic coordinates/provenance.
  if (mLegacyLambdaCuts) {
    if (mMaxNTr > 0 && nTracks > mMaxNTr) return kTrackCount;
    return kAccepted;
  }
  if (vertex.Z() < mMinVz || vertex.Z() > mMaxVz) return kVz;
  const double radius = ComputeVr(vertex.X(), vertex.Y());
  if (!std::isfinite(radius)) return kInvalidVertex;
  if (radius > mRadius) return kVr;
  if (event.refMult() < mMinRefMult || event.refMult() > mMaxRefMult) return kRefMult;
  const double vzVpd = event.vzVpd();
  // Preserve the existing KF/other EventCutConfig users' optional valid-VPD
  // convention and inclusive cut boundaries. No collision-mode VPD guess.
  if (std::isfinite(vzVpd) && std::fabs(vzVpd) < mMaxAbsVzVpd &&
      std::fabs(vertex.Z() - vzVpd) > mMaxVzDiff) return kVpd;
  if (mMaxNTr > 0 && nTracks > mMaxNTr) return kTrackCount;
  return kAccepted;
}

double KfEventSelection::ComputeVr(double x, double y) const {
  if (!mLoaded) return std::numeric_limits<double>::quiet_NaN();
  const double dx = x - mCenterX;
  const double dy = y - mCenterY;
  return std::sqrt(dx * dx + dy * dy);
}

void KfEventSelection::Dump(std::ostream& output) const {
  const std::streamsize precision = output.precision();
  output << std::setprecision(17)
         << "KFEventSelection.loaded: " << (mLoaded ? "true" : "false") << '\n'
         << "KFEventSelection.mainconf: " << mMainPath << '\n'
         << "KFEventSelection.analysisInfo: " << mAnalysisPath << '\n'
         << "KFEventSelection.eventConfig: " << mEventPath << '\n'
         << "KFEventSelection.mode: " << mMode << '\n';
  if (mLoaded) {
    output << "KFEventSelection.policy: "
           << (mLegacyLambdaCuts ? "legacy_lambda_imp5" : "mode_vertex") << '\n'
           << "KFEventSelection.vertexCutsApplied: " << (mLegacyLambdaCuts ? "false" : "true") << '\n'
           << "KFEventSelection.refMultCutsApplied: " << (mLegacyLambdaCuts ? "false" : "true") << '\n'
           << "KFEventSelection.vpdCutsApplied: " << (mLegacyLambdaCuts ? "false" : "true") << '\n'
           << "KFEventSelection.vertexSource: " << mVertexSource << '\n';
    if (!mLegacyLambdaCuts)
      output << "KFEventSelection.minVz: " << mMinVz << '\n'
             << "KFEventSelection.maxVz: " << mMaxVz << '\n';
    output << "KFEventSelection.vtxCenterX: " << mCenterX << '\n'
           << "KFEventSelection.vtxCenterY: " << mCenterY << '\n';
    if (!mLegacyLambdaCuts)
      output << "KFEventSelection.maxVr: " << mRadius << '\n'
             << "KFEventSelection.minRefMult: " << mMinRefMult << '\n'
             << "KFEventSelection.maxRefMult: " << mMaxRefMult << '\n'
             << "KFEventSelection.maxVzDiff: " << mMaxVzDiff << '\n'
             << "KFEventSelection.maxAbsVzVpd: " << mMaxAbsVzVpd << '\n';
    output << "KFEventSelection.maxNTr: " << mMaxNTr << '\n';
  }
  output.precision(precision);
}
