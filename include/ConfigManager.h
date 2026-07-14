#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include "Rtypes.h"
#include <map>
#include <string>
#include <vector>

// Forward declarations
class EventCutConfig;
class TrackCutConfig;
class PIDCutConfig;
class V0CutConfig;
class PhiCutConfig;
class LambdaCutConfig;
class Lambda1520CutConfig;
class Sigma1385CutConfig;
class NuclearIdCutConfig;
class MixingConfig;
class CentralityCutConfig;
class FemtoConfig;

class ConfigManager {
public:
  static ConfigManager& GetInstance();
  Bool_t LoadConfig(const Char_t* mainConfigPath = "config/main.yaml");

  /** Return full path for hist config from main.yaml key "hist" or "{makerName}Hist".
   *  Returns empty string if key missing or value empty. */
  std::string GetHistConfigPath(const std::string& makerName = "");

  /** Return anaName from analysis_info (mainconf key "analysis"). Empty if not set. */
  std::string GetAnaName() const;

  // Access to cut config classes
  EventCutConfig& GetEventCuts();
  TrackCutConfig& GetTrackCuts();
  PIDCutConfig& GetPIDCuts();
  V0CutConfig& GetV0Cuts();
  PhiCutConfig& GetPhiCuts();
  LambdaCutConfig& GetLambdaCuts();
  Lambda1520CutConfig& GetLambda1520Cuts();
  Sigma1385CutConfig& GetSigma1385Cuts();
  NuclearIdCutConfig& GetNuclearIdCuts();
  MixingConfig& GetMixingConfig();
  CentralityCutConfig& GetCentralityCuts();
  FemtoConfig& GetFemtoConfig();

private:
  ConfigManager();
  ~ConfigManager();
  ConfigManager(const ConfigManager&);
  ConfigManager& operator=(const ConfigManager&);

  Bool_t ParseMainConfig(const Char_t* filename);
  Bool_t LoadConfigFile(const Char_t* basePath, const Char_t* relativePath, const Char_t* configType);
  /** Check that all paths in m_mainConfigValues exist. On failure, optional missing list gets relative paths (as in main). */
  Bool_t ValidateReferencedFiles(std::vector<std::string>* missing = 0);

  EventCutConfig* eventCuts;
  TrackCutConfig* trackCuts;
  PIDCutConfig* pidCuts;
  V0CutConfig* v0Cuts;
  PhiCutConfig* phiCuts;
  LambdaCutConfig* lambdaCuts;
  Lambda1520CutConfig* lambda1520Cuts;
  Sigma1385CutConfig* sigma1385Cuts;
  NuclearIdCutConfig* nuclearIdCuts;
  MixingConfig* mixingConfig;
  CentralityCutConfig* centralityCuts;
  FemtoConfig* femtoConfig;

  Bool_t isLoaded;
  std::map<std::string, std::string> m_mainConfigValues;  ///< Parsed key-value from main.yaml
  std::string m_configBasePath;  ///< Project root (path before /config/, trailing slash included)
  std::string m_anaName;        ///< From analysis_info (key analysis.anaName)

  Bool_t ParseAnalysisInfoAnaName(const std::string& analysisInfoPath);
};

#endif

