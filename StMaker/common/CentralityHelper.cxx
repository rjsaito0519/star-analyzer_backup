#include "CentralityHelper.h"
#include "cuts/CentralityCutConfig.h"
#include "StRefMultCorr/StRefMultCorr.h"
#include "StPicoEvent/StPicoEvent.h"

#include <iostream>

namespace {
  static const Double_t kCent9Percentile[9] = {77.5, 67.5, 57.5, 47.5, 37.5, 27.5, 17.5, 7.5, 2.5};
}

CentralityHelper::CentralityHelper()
    : m_rmc(0),
      m_enabled(kFALSE),
      m_rejectBadRun(kTRUE),
      m_rejectPileup(kTRUE),
      m_useWeight(kTRUE),
      m_requireValidCentrality(kTRUE),
      m_currentRunId(-1),
      m_nBadRun(0),
      m_nPileup(0),
      m_nInvalidCent(0),
      m_nBinRejected(0),
      m_nRefMultRejected(0),
      m_nOk(0) {}

CentralityHelper::~CentralityHelper() {
  Finish();
}

Bool_t CentralityHelper::Init(const CentralityCutConfig& cfg) {
  Finish();
  m_enabled = cfg.enabled;
  if (!m_enabled) return kTRUE;

  m_rejectBadRun = cfg.rejectBadRun;
  m_rejectPileup = cfg.rejectPileup;
  m_useWeight = cfg.useWeight;
  m_requireValidCentrality = cfg.requireValidCentrality;
  m_acceptedCentBins = cfg.acceptedCentBins;

  TString modeName(cfg.mode.c_str());
  modeName.ToLower();
  m_rmc = new StRefMultCorr(modeName, "Def", "Def");
  m_rmc->setVerbose(cfg.verbose);
  m_currentRunId = -1;
  return kTRUE;
}

void CentralityHelper::Finish() {
  if (m_rmc) {
    delete m_rmc;
    m_rmc = 0;
  }
  m_currentRunId = -1;
}

Bool_t CentralityHelper::InitRun(Int_t runId) {
  if (!m_enabled || !m_rmc) return kTRUE;
  if (runId == m_currentRunId) return kTRUE;
  m_currentRunId = runId;
  m_rmc->init(runId);
  return kTRUE;
}

Double_t CentralityHelper::Cent9ToPercentile(Int_t cent9) {
  if (cent9 < 0 || cent9 > 8) return -1.0;
  return kCent9Percentile[cent9];
}

const char* CentralityHelper::RejectReasonString(CentralityRejectReason reason) {
  switch (reason) {
    case kCentralityBadRun: return "badRun";
    case kCentralityPileup: return "pileup";
    case kCentralityInvalidBin: return "invalidCent";
    case kCentralityBinRejected: return "centBinRejected";
    case kCentralityRefMultRejected: return "refMultRejected";
    default: return "unknown";
  }
}

Bool_t CentralityHelper::CheckBadRun(Int_t runId, CentralityRejectReason& reason) {
  reason = kCentralityOk;
  if (!m_enabled || !m_rmc) return kTRUE;
  InitRun(runId);
  if (m_rejectBadRun && m_rmc->isBadRun(runId)) {
    reason = kCentralityBadRun;
    m_nBadRun++;
    return kFALSE;
  }
  return kTRUE;
}

Bool_t CentralityHelper::CheckPileup(Int_t rawMult, Int_t nTofMatch, Double_t vz, CentralityRejectReason& reason) {
  reason = kCentralityOk;
  if (!m_enabled || !m_rmc) return kTRUE;
  if (m_rejectPileup && m_rmc->isPileUpEvent(rawMult, nTofMatch, vz)) {
    reason = kCentralityPileup;
    m_nPileup++;
    return kFALSE;
  }
  return kTRUE;
}

Bool_t CentralityHelper::ComputeBins(StPicoEvent* event, Int_t rawMult, Double_t vz,
                                     Int_t& cent9, Int_t& cent16, Double_t& refMultCorr, Double_t& weight,
                                     CentralityRejectReason& reason) {
  cent9 = -1;
  cent16 = -1;
  refMultCorr = -1.0;
  weight = 1.0;
  reason = kCentralityOk;

  if (!m_enabled || !m_rmc || !event) return kTRUE;

  InitRun(event->runId());
  m_rmc->initEvent(static_cast<UShort_t>(rawMult), vz, event->ZDCx());

  cent16 = m_rmc->getCentralityBin16();
  cent9 = m_rmc->getCentralityBin9();
  refMultCorr = m_rmc->getRefMultCorr();
  weight = m_useWeight ? m_rmc->getWeight() : 1.0;

  if (m_requireValidCentrality && (cent9 < 0 || cent16 < 0)) {
    reason = kCentralityInvalidBin;
    m_nInvalidCent++;
    return kFALSE;
  }
  return kTRUE;
}

Bool_t CentralityHelper::AcceptCentBin(Int_t cent9, Double_t refMultCorr, CentralityRejectReason& reason) {
  reason = kCentralityOk;
  if (!m_enabled) return kTRUE;

  CentralityCutConfig& cfg = CentralityCutConfig::GetInstance();
  if (!cfg.IsCentBinAccepted(cent9)) {
    reason = kCentralityBinRejected;
    m_nBinRejected++;
    return kFALSE;
  }
  if (!cfg.IsRefMultCorrAccepted(cent9, refMultCorr)) {
    reason = kCentralityRefMultRejected;
    m_nRefMultRejected++;
    return kFALSE;
  }
  m_nOk++;
  return kTRUE;
}
