#ifndef HIST_MANAGER_H
#define HIST_MANAGER_H

#include "Rtypes.h"
#include <map>
#include <set>
#include <string>

class TH1;

/**
 * Loads histogram definitions from a flat key-value YAML and creates TH1/TH2/TH3.
 * Fill by name; missing keys are logged once and Fill is skipped.
 */
class HistManager {
public:
  HistManager();
  ~HistManager();

  /** Load YAML and create histograms. Path can be absolute or relative. */
  Bool_t LoadFromFile(const Char_t* yamlPath);

  /** Get histogram by name. Returns nullptr if not found. */
  TH1* Get(const char* name) const;

  /** Return true if a histogram with the given name was loaded from YAML. */
  Bool_t HasHistogram(const char* name) const;

  /** Fill 1D histogram. No-op and log once if name not found. */
  void Fill(const char* name, Double_t x);

  /** Fill 2D histogram. No-op and log once if name not found. */
  void Fill(const char* name, Double_t x, Double_t y);

  /** Fill 3D histogram. No-op and log once if name not found. */
  void Fill(const char* name, Double_t x, Double_t y, Double_t z);

  /** Write all owned histograms to current TDirectory. */
  void Write();

  /** Write only histograms whose names contain the given substring. */
  void WriteContaining(const char* substr);

  /** Relinquish ownership so destructor will not delete histograms (call after Write() when TFile will own them). */
  void ReleaseOwnership();

private:
  HistManager(const HistManager&);
  HistManager& operator=(const HistManager&);

  std::map<std::string, TH1*> m_histograms;
  std::set<std::string> m_missingKeyWarned;
  Bool_t m_ownershipReleased;
};

#endif
