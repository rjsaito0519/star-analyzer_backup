#ifndef CENTRALITY_HELPER_H
#define CENTRALITY_HELPER_H

#include "Rtypes.h"
#include <vector>

class StRefMultCorr;
class StPicoEvent;
class CentralityCutConfig;

enum CentralityRejectReason {
  kCentralityOk = 0,
  kCentralityDisabled,
  kCentralityBadRun,
  kCentralityPileup,
  kCentralityInvalidBin,
  kCentralityBinRejected,
  kCentralityRefMultRejected
};

class CentralityHelper {
public:
  CentralityHelper();
  ~CentralityHelper();

  Bool_t Init(const CentralityCutConfig& cfg);
  void Finish();

  Bool_t IsEnabled() const { return m_enabled; }

  Bool_t InitRun(Int_t runId);

  Bool_t CheckBadRun(Int_t runId, CentralityRejectReason& reason);
  Bool_t CheckPileup(Int_t rawMult, Int_t nTofMatch, Double_t vz, CentralityRejectReason& reason);

  Bool_t ComputeBins(StPicoEvent* event, Int_t rawMult, Double_t vz,
                     Int_t& cent9, Int_t& cent16, Double_t& refMultCorr, Double_t& weight,
                     CentralityRejectReason& reason);

  Bool_t AcceptCentBin(Int_t cent9, Double_t refMultCorr, CentralityRejectReason& reason);

  static Double_t Cent9ToPercentile(Int_t cent9);
  static const char* RejectReasonString(CentralityRejectReason reason);

  Long_t CountBadRun() const { return m_nBadRun; }
  Long_t CountPileup() const { return m_nPileup; }
  Long_t CountInvalidCent() const { return m_nInvalidCent; }
  Long_t CountBinRejected() const { return m_nBinRejected; }
  Long_t CountRefMultRejected() const { return m_nRefMultRejected; }
  Long_t CountOk() const { return m_nOk; }

private:
  StRefMultCorr* m_rmc;
  Bool_t m_enabled;
  Bool_t m_rejectBadRun;
  Bool_t m_rejectPileup;
  Bool_t m_useWeight;
  Bool_t m_requireValidCentrality;
  std::vector<Int_t> m_acceptedCentBins;

  Int_t m_currentRunId;

  Long_t m_nBadRun;
  Long_t m_nPileup;
  Long_t m_nInvalidCent;
  Long_t m_nBinRejected;
  Long_t m_nRefMultRejected;
  Long_t m_nOk;
};

#endif
