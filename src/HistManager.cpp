#include "HistManager.h"
#include "TH1.h"
#include "TH1F.h"
#include "TH1I.h"
#include "TH2F.h"
#include "TH3F.h"
#include "TH3.h"
#include "yaml-cpp/yaml.h"
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <cstring>

namespace {
  struct AxisSpec {
    Int_t nBins;
    Double_t min;
    Double_t max;
  };

  std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    size_t start = s.find_first_not_of(ws);
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(ws);
    return s.substr(start, end - start + 1);
  }

  bool parseAxisSpec(const YAML::Node& node, AxisSpec& out) {
    if (!node.IsMap()) return false;
    if (!node["nBins"]) return false;
    if (!node["min"]) return false;
    if (!node["max"]) return false;
    try {
      out.nBins = node["nBins"].as<Int_t>();
      out.min = node["min"].as<Double_t>();
      out.max = node["max"].as<Double_t>();
      return true;
    } catch (const YAML::Exception& e) {
      std::cerr << "[HistManager] parseAxisSpec error: " << e.what() << std::endl;
      return false;
    }
  }
}

HistManager::HistManager() : m_ownershipReleased(kFALSE) {}

HistManager::~HistManager() {
  if (!m_ownershipReleased) {
    for (std::map<std::string, TH1*>::iterator it = m_histograms.begin(); it != m_histograms.end(); ++it) {
      if (it->second) {
        delete it->second;
        it->second = 0;
      }
    }
  }
  m_histograms.clear();
}

Bool_t HistManager::LoadFromFile(const Char_t* yamlPath) {
  try {
    YAML::Node root = YAML::LoadFile(yamlPath);
    if (!root.IsMap()) {
      std::cerr << "[HistManager] Root must be a map (axes, histograms)." << std::endl;
      return kFALSE;
    }

    std::map<std::string, AxisSpec> axesMap;
    if (root["axes"] && root["axes"].IsMap()) {
      for (YAML::const_iterator it = root["axes"].begin(); it != root["axes"].end(); ++it) {
        std::string name = it->first.as<std::string>();
        AxisSpec spec;
        if (parseAxisSpec(it->second, spec)) {
          axesMap[name] = spec;
        } else {
          std::cerr << "[HistManager] Skipped invalid axis '" << name << "'." << std::endl;
        }
      }
    }

    if (!root["histograms"] || !root["histograms"].IsMap()) {
      std::cerr << "[HistManager] Missing or invalid 'histograms' section." << std::endl;
      return kFALSE;
    }

    // Prevent ROOT from adding our histograms to gDirectory at creation; we manage lifetime
    // and Write() to our TFile in StPhiMaker::Finish(). Otherwise same objects get deleted
    // twice (TFile in Finish + directory cleanup at exit) -> double free in TString.
    // RAII: restore global AddDirectory on return or YAML exception after kFALSE.
    struct AddDirectoryGuard {
      AddDirectoryGuard() { TH1::AddDirectory(kFALSE); }
      ~AddDirectoryGuard() { TH1::AddDirectory(kTRUE); }
    } addDirectoryGuard;

    for (YAML::const_iterator it = root["histograms"].begin(); it != root["histograms"].end(); ++it) {
      const std::string& name = it->first.as<std::string>();
      const YAML::Node& histNode = it->second;

      if (!histNode.IsMap()) {
        std::cerr << "[HistManager] Skipped histogram '" << name << "': not a map." << std::endl;
        continue;
      }

      if (!histNode["title"]) {
        std::cerr << "[HistManager] Skipped histogram '" << name << "': missing 'title'." << std::endl;
        continue;
      }
      std::string title = trim(histNode["title"].as<std::string>());

      std::string typeStr = "TH1F";
      if (histNode["type"]) {
        typeStr = trim(histNode["type"].as<std::string>());
      }

      AxisSpec xSpec, ySpec, zSpec;
      Bool_t hasX = kFALSE;
      Bool_t hasY = kFALSE;
      Bool_t hasZ = kFALSE;

      if (histNode["axis"]) {
        hasX = parseAxisSpec(histNode["axis"], xSpec);
      } else if (histNode["xAxis"]) {
        hasX = parseAxisSpec(histNode["xAxis"], xSpec);
      }
      if (histNode["yAxis"]) {
        hasY = parseAxisSpec(histNode["yAxis"], ySpec);
      }
      if (histNode["zAxis"]) {
        hasZ = parseAxisSpec(histNode["zAxis"], zSpec);
      }
      if (!hasX && histNode["nBins"] && histNode["min"] && histNode["max"]) {
        try {
          xSpec.nBins = histNode["nBins"].as<Int_t>();
          xSpec.min = histNode["min"].as<Double_t>();
          xSpec.max = histNode["max"].as<Double_t>();
          hasX = kTRUE;
        } catch (const YAML::Exception& e) {
          std::cerr << "[HistManager] Skipped histogram '" << name << "': invalid nBins/min/max." << std::endl;
          continue;
        }
      }

      if (!hasX) {
        std::cerr << "[HistManager] Skipped histogram '" << name << "': missing axis (axis, xAxis, or nBins/min/max)." << std::endl;
        continue;
      }

      if (hasY && hasZ) {
        TH3F* h = new TH3F(name.c_str(), title.c_str(),
                           xSpec.nBins, xSpec.min, xSpec.max,
                           ySpec.nBins, ySpec.min, ySpec.max,
                           zSpec.nBins, zSpec.min, zSpec.max);
        m_histograms[name] = h;
      } else if (hasY) {
        TH2F* h = new TH2F(name.c_str(), title.c_str(),
                           xSpec.nBins, xSpec.min, xSpec.max,
                           ySpec.nBins, ySpec.min, ySpec.max);
        if (histNode["yTitle"]) {
          h->GetYaxis()->SetTitle(trim(histNode["yTitle"].as<std::string>()).c_str());
        }
        m_histograms[name] = h;
      } else {
        TH1* h = 0;
        if (typeStr == "TH1I") {
          h = new TH1I(name.c_str(), title.c_str(), xSpec.nBins, xSpec.min, xSpec.max);
        } else {
          h = new TH1F(name.c_str(), title.c_str(), xSpec.nBins, xSpec.min, xSpec.max);
        }
        if (h) m_histograms[name] = h;
      }
    }

    return kTRUE;
  } catch (const YAML::BadFile& e) {
    std::cerr << "[HistManager] Failed to open file: " << yamlPath << " (" << e.what() << ")" << std::endl;
    return kFALSE;
  } catch (const YAML::Exception& e) {
    std::cerr << "[HistManager] YAML error in " << yamlPath << ": " << e.what() << std::endl;
    return kFALSE;
  }
}

TH1* HistManager::Get(const char* name) const {
  if (!name) return 0;
  std::map<std::string, TH1*>::const_iterator it = m_histograms.find(name);
  if (it == m_histograms.end()) return 0;
  return it->second;
}

Bool_t HistManager::HasHistogram(const char* name) const {
  if (!name) return kFALSE;
  return m_histograms.find(name) != m_histograms.end();
}

void HistManager::Fill(const char* name, Double_t x) {
  if (!name) return;
  TH1* h = Get(name);
  if (!h) {
    if (m_missingKeyWarned.find(name) == m_missingKeyWarned.end()) {
      std::cerr << "[HistManager] Fill failed: histogram '" << name << "' not found (not defined in YAML)." << std::endl;
      m_missingKeyWarned.insert(name);
    }
    return;
  }
  h->Fill(x);
}

void HistManager::Fill(const char* name, Double_t x, Double_t y) {
  if (!name) return;
  TH1* h = Get(name);
  if (!h) {
    if (m_missingKeyWarned.find(name) == m_missingKeyWarned.end()) {
      std::cerr << "[HistManager] Fill failed: histogram '" << name << "' not found (not defined in YAML)." << std::endl;
      m_missingKeyWarned.insert(name);
    }
    return;
  }
  h->Fill(x, y);
}

void HistManager::Fill(const char* name, Double_t x, Double_t y, Double_t z) {
  if (!name) return;
  TH1* h = Get(name);
  if (!h) {
    if (m_missingKeyWarned.find(name) == m_missingKeyWarned.end()) {
      std::cerr << "[HistManager] Fill failed: histogram '" << name << "' not found (not defined in YAML)." << std::endl;
      m_missingKeyWarned.insert(name);
    }
    return;
  }
  if (h->InheritsFrom("TH3")) {
    ((TH3*)h)->Fill(x, y, z);
  } else {
    std::cerr << "[HistManager] Fill(x,y,z) failed: histogram '" << name << "' is not TH3." << std::endl;
  }
}

void HistManager::Write() {
  for (std::map<std::string, TH1*>::iterator it = m_histograms.begin(); it != m_histograms.end(); ++it) {
    if (it->second) it->second->Write();
  }
}

void HistManager::WriteContaining(const char* substr) {
  if (!substr) return;
  std::string sub(substr);
  for (std::map<std::string, TH1*>::iterator it = m_histograms.begin(); it != m_histograms.end(); ++it) {
    if (it->second && it->first.find(sub) != std::string::npos) {
      it->second->Write();
    }
  }
}

void HistManager::ReleaseOwnership() {
  m_ownershipReleased = kTRUE;
}
