#include "ConfigManager.h"
#include "cuts/EventCutConfig.h"
#include "cuts/TrackCutConfig.h"
#include "cuts/PIDCutConfig.h"
#include "cuts/V0CutConfig.h"
#include "cuts/PhiCutConfig.h"
#include "cuts/LambdaCutConfig.h"
#include "cuts/Lambda1520CutConfig.h"
#include "cuts/Sigma1385CutConfig.h"
#include "cuts/NuclearIdCutConfig.h"
#include "cuts/MixingConfig.h"
#include "cuts/CentralityCutConfig.h"
#include "cuts/FemtoConfig.h"
#include "YamlParser.h"
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <cstring>

namespace {
  std::string trimWhitespace(const std::string& s) {
    const char* ws = " \t\r\n";
    size_t start = s.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
  }
}

ConfigManager& ConfigManager::GetInstance() {
  static ConfigManager instance;
  return instance;
}

ConfigManager::ConfigManager() 
  : eventCuts(0), trackCuts(0), pidCuts(0), v0Cuts(0),
    phiCuts(0), lambdaCuts(0), lambda1520Cuts(0), sigma1385Cuts(0), nuclearIdCuts(0), mixingConfig(0),
    centralityCuts(0), femtoConfig(0), isLoaded(kFALSE) {
  // Initialize all cut config instances
  eventCuts = &EventCutConfig::GetInstance();
  trackCuts = &TrackCutConfig::GetInstance();
  pidCuts = &PIDCutConfig::GetInstance();
  v0Cuts = &V0CutConfig::GetInstance();
  phiCuts = &PhiCutConfig::GetInstance();
  lambdaCuts = &LambdaCutConfig::GetInstance();
  lambda1520Cuts = &Lambda1520CutConfig::GetInstance();
  sigma1385Cuts = &Sigma1385CutConfig::GetInstance();
  nuclearIdCuts = &NuclearIdCutConfig::GetInstance();
  mixingConfig = &MixingConfig::GetInstance();
  centralityCuts = &CentralityCutConfig::GetInstance();
  femtoConfig = &FemtoConfig::GetInstance();
}

ConfigManager::~ConfigManager() {
  // Note: We don't delete the instances since they are singletons
}

Bool_t ConfigManager::LoadConfig(const Char_t* mainConfigPath) {
  if (isLoaded) {
    std::cerr << "WARNING: Config already loaded, reloading..." << std::endl;
  }
  
  if (!ParseMainConfig(mainConfigPath)) {
    std::cerr << "ERROR: Failed to parse main config file: " << mainConfigPath << std::endl;
    return kFALSE;
  }
  
  isLoaded = kTRUE;
  return kTRUE;
}

Bool_t ConfigManager::ParseMainConfig(const Char_t* filename) {
  std::map<std::string, std::string> values;
  if (!YamlParser::ParseFile(filename, values)) {
    std::cerr << "ERROR: Failed to parse main config file: " << filename << std::endl;
    return kFALSE;
  }
  
  // Base path = project root (path before "/config/" in full path). Supports any main filename (e.g. main.yaml, main_pp500.yaml).
  std::string pathStr(filename);
  std::string basePath;
  size_t configPos = pathStr.find("/config/");
  if (configPos != std::string::npos) {
    basePath = pathStr.substr(0, configPos + 1);  // include trailing slash
  } else {
    basePath = "";
  }

  m_mainConfigValues = values;
  m_configBasePath = basePath;
  m_anaName.clear();

  std::vector<std::string> missing;
  if (!ValidateReferencedFiles(&missing)) {
    std::cerr << "ERROR: The following config files are missing:" << std::endl;
    for (size_t i = 0; i < missing.size(); i++) {
      std::cerr << "  " << missing[i] << std::endl;
    }
    return kFALSE;
  }

  // Load each config file (relativePath is under config/; fullPath = basePath + "config/" + relativePath)
  Bool_t success = kTRUE;

  if (values.find("event") != values.end()) {
    if (!LoadConfigFile(basePath.c_str(), values["event"].c_str(), "event")) {
      success = kFALSE;
    }
  } else {
    std::cerr << "WARNING: 'event' key not found in main config" << std::endl;
  }
  
  if (values.find("track") != values.end()) {
    if (!LoadConfigFile(basePath.c_str(), values["track"].c_str(), "track")) {
      success = kFALSE;
    }
  } else {
    std::cerr << "WARNING: 'track' key not found in main config" << std::endl;
  }
  
  if (values.find("pid") != values.end()) {
    if (!LoadConfigFile(basePath.c_str(), values["pid"].c_str(), "pid")) {
      success = kFALSE;
    }
  } else {
    std::cerr << "WARNING: 'pid' key not found in main config" << std::endl;
  }
  
  if (values.find("v0") != values.end()) {
    if (!LoadConfigFile(basePath.c_str(), values["v0"].c_str(), "v0")) {
      success = kFALSE;
    }
  } else {
    std::cerr << "WARNING: 'v0' key not found in main config" << std::endl;
  }
  
  if (values.find("phi") != values.end()) {
    if (!LoadConfigFile(basePath.c_str(), values["phi"].c_str(), "phi")) {
      success = kFALSE;
    }
  } else if (values.find("maker") == values.end()) {
    std::cerr << "WARNING: 'phi' key not found in main config" << std::endl;
  }

  if (values.find("lambda") != values.end()) {
    if (!LoadConfigFile(basePath.c_str(), values["lambda"].c_str(), "lambda")) {
      success = kFALSE;
    }
  } else {
    std::cerr << "WARNING: 'lambda' key not found in main config" << std::endl;
  }
  
  if (values.find("lambda1520") != values.end()) {
    if (!LoadConfigFile(basePath.c_str(), values["lambda1520"].c_str(), "lambda1520")) {
      success = kFALSE;
    }
  } else {
    std::cerr << "WARNING: 'lambda1520' key not found in main config" << std::endl;
  }
  
  if (values.find("sigma1385") != values.end()) {
    if (!LoadConfigFile(basePath.c_str(), values["sigma1385"].c_str(), "sigma1385")) {
      success = kFALSE;
    }
  } else {
    std::cerr << "WARNING: 'sigma1385' key not found in main config" << std::endl;
  }
  
  if (values.find("nuclearid") != values.end()) {
    if (!LoadConfigFile(basePath.c_str(), values["nuclearid"].c_str(), "nuclearid")) {
      success = kFALSE;
    }
  } else {
    std::cerr << "WARNING: 'nuclearid' key not found in main config" << std::endl;
  }
  
  if (values.find("mixing") != values.end()) {
    if (!LoadConfigFile(basePath.c_str(), values["mixing"].c_str(), "mixing")) {
      success = kFALSE;
    }
  } else {
    std::cerr << "WARNING: 'mixing' key not found in main config" << std::endl;
  }

  if (values.find("centrality") != values.end()) {
    if (!LoadConfigFile(basePath.c_str(), values["centrality"].c_str(), "centrality")) {
      success = kFALSE;
    }
  } else {
    std::cerr << "WARNING: 'centrality' key not found in main config" << std::endl;
  }

  if (values.find("femto") != values.end()) {
    if (!LoadConfigFile(basePath.c_str(), values["femto"].c_str(), "femto")) {
      success = kFALSE;
    }
  }

  if (values.find("maker") != values.end()) {
    if (!LoadConfigFile(basePath.c_str(), values["maker"].c_str(), "maker")) {
      success = kFALSE;
    }
  }

  if (values.find("analysis") != values.end()) {
    std::string analysisRel = trimWhitespace(values["analysis"]);
    if (!analysisRel.empty()) {
      std::string analysisPath = basePath;
      if (!analysisPath.empty() && analysisPath[analysisPath.length() - 1] != '/') {
        analysisPath += "/";
      }
      analysisPath += "config/";
      analysisPath += analysisRel;
      ParseAnalysisInfoAnaName(analysisPath);
    }
  }

  return success;
}

Bool_t ConfigManager::ParseAnalysisInfoAnaName(const std::string& analysisInfoPath) {
  m_anaName.clear();
  std::ifstream file(analysisInfoPath.c_str());
  if (!file.is_open()) {
    std::cerr << "WARNING: Cannot open analysis info file: " << analysisInfoPath << std::endl;
    return kFALSE;
  }
  std::string line;
  while (std::getline(file, line)) {
    if (line.find("anaName") == std::string::npos) continue;
    size_t q1 = line.find('"');
    if (q1 != std::string::npos) {
      size_t q2 = line.find('"', q1 + 1);
      if (q2 != std::string::npos && q2 > q1 + 1) {
        m_anaName = line.substr(q1 + 1, q2 - q1 - 1);
        break;
      }
    }
    q1 = line.find('\'');
    if (q1 != std::string::npos) {
      size_t q2 = line.find('\'', q1 + 1);
      if (q2 != std::string::npos && q2 > q1 + 1) {
        m_anaName = line.substr(q1 + 1, q2 - q1 - 1);
        break;
      }
    }
  }
  file.close();
  return kTRUE;
}

Bool_t ConfigManager::ValidateReferencedFiles(std::vector<std::string>* missing) {
  Bool_t allExist = kTRUE;
  for (std::map<std::string, std::string>::const_iterator it = m_mainConfigValues.begin();
       it != m_mainConfigValues.end(); ++it) {
    std::string value = trimWhitespace(it->second);
    if (value.empty()) continue;

    std::string fullPath;
    if (m_configBasePath.empty()) {
      fullPath = std::string("config/") + value;
    } else {
      fullPath = m_configBasePath;
      if (fullPath[fullPath.length() - 1] != '/') fullPath += "/";
      fullPath += "config/";
      fullPath += value;
    }

    std::ifstream f(fullPath.c_str());
    if (!f.is_open()) {
      allExist = kFALSE;
      if (missing) missing->push_back(value);
    }
  }
  return allExist;
}

std::string ConfigManager::GetAnaName() const {
  return m_anaName;
}

std::string ConfigManager::GetHistConfigPath(const std::string& makerName) {
  std::string key = "hist";
  if (!makerName.empty()) {
    std::string customKey = makerName + "Hist";
    if (m_mainConfigValues.find(customKey) != m_mainConfigValues.end()) {
      key = customKey;
    }
  }
  std::map<std::string, std::string>::const_iterator it = m_mainConfigValues.find(key);
  if (it == m_mainConfigValues.end()) {
    std::cerr << "WARNING: GetHistConfigPath: key '" << key << "' not found in main config" << std::endl;
    return "";
  }
  std::string value = trimWhitespace(it->second);
  if (value.empty()) {
    std::cerr << "WARNING: GetHistConfigPath: key '" << key << "' has empty value" << std::endl;
    return "";
  }
  if (m_configBasePath.empty()) {
    return std::string("config/") + value;
  }
  return m_configBasePath + "config/" + value;
}

Bool_t ConfigManager::LoadConfigFile(const Char_t* basePath, const Char_t* relativePath, const Char_t* configType) {
  // fullPath = basePath (project root with trailing /) + "config/" + relativePath
  std::string fullPath = basePath;
  if (!fullPath.empty() && fullPath[fullPath.length() - 1] != '/') {
    fullPath += "/";
  }
  fullPath += "config/";
  fullPath += relativePath;
  
  // Load based on config type
  if (strcmp(configType, "event") == 0) {
    return eventCuts->LoadFromFile(fullPath.c_str());
  } else if (strcmp(configType, "track") == 0) {
    return trackCuts->LoadFromFile(fullPath.c_str());
  } else if (strcmp(configType, "pid") == 0) {
    return pidCuts->LoadFromFile(fullPath.c_str());
  } else if (strcmp(configType, "v0") == 0) {
    return v0Cuts->LoadFromFile(fullPath.c_str());
  } else if (strcmp(configType, "phi") == 0) {
    return phiCuts->LoadFromFile(fullPath.c_str());
  } else if (strcmp(configType, "lambda") == 0) {
    return lambdaCuts->LoadFromFile(fullPath.c_str());
  } else if (strcmp(configType, "lambda1520") == 0) {
    return lambda1520Cuts->LoadFromFile(fullPath.c_str());
  } else if (strcmp(configType, "sigma1385") == 0) {
    return sigma1385Cuts->LoadFromFile(fullPath.c_str());
  } else if (strcmp(configType, "nuclearid") == 0) {
    return nuclearIdCuts->LoadFromFile(fullPath.c_str());
  } else if (strcmp(configType, "mixing") == 0) {
    return mixingConfig->LoadFromFile(fullPath.c_str());
  } else if (strcmp(configType, "centrality") == 0) {
    return centralityCuts->LoadFromFile(fullPath.c_str());
  } else if (strcmp(configType, "femto") == 0) {
    return femtoConfig->LoadFromFile(fullPath.c_str());
  } else if (strcmp(configType, "maker") == 0) {
    Bool_t ok = phiCuts->LoadFromFile(fullPath.c_str());
    ok = femtoConfig->LoadFromFile(fullPath.c_str()) && ok;
    return ok;
  } else {
    std::cerr << "ERROR: Unknown config type: " << configType << std::endl;
    return kFALSE;
  }
}

EventCutConfig& ConfigManager::GetEventCuts() {
  return *eventCuts;
}

TrackCutConfig& ConfigManager::GetTrackCuts() {
  return *trackCuts;
}

PIDCutConfig& ConfigManager::GetPIDCuts() {
  return *pidCuts;
}

V0CutConfig& ConfigManager::GetV0Cuts() {
  return *v0Cuts;
}

PhiCutConfig& ConfigManager::GetPhiCuts() {
  return *phiCuts;
}

LambdaCutConfig& ConfigManager::GetLambdaCuts() {
  return *lambdaCuts;
}

Lambda1520CutConfig& ConfigManager::GetLambda1520Cuts() {
  return *lambda1520Cuts;
}

Sigma1385CutConfig& ConfigManager::GetSigma1385Cuts() {
  return *sigma1385Cuts;
}

NuclearIdCutConfig& ConfigManager::GetNuclearIdCuts() {
  return *nuclearIdCuts;
}

MixingConfig& ConfigManager::GetMixingConfig() {
  return *mixingConfig;
}

CentralityCutConfig& ConfigManager::GetCentralityCuts() {
  return *centralityCuts;
}

FemtoConfig& ConfigManager::GetFemtoConfig() {
  return *femtoConfig;
}

