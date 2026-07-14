#ifndef NUCLEAR_ID_DEDX_VS_MOM_H
#define NUCLEAR_ID_DEDX_VS_MOM_H

#include "Rtypes.h"
#include "TMath.h"

// LogPoly dE/dx vs p calibration tables for light nuclei (d, t, 3He, 4He).
// Values from StNuclearIdMaker NuclearPID struct; not YAML-configurable.

namespace NuclearIdDeDxVsMom {

enum SpeciesIndex { kDeuteron = 0, kTriton, kHe3, kHe4, kNSpecies };

struct LogPolyParams {
  Double_t c0;
  Double_t c1;
  Double_t c2;
  Double_t c3;
};

struct PMomentumRange {
  Double_t pMin;
  Double_t pMax;
};

inline const LogPolyParams& Mean(SpeciesIndex sp) {
  static const LogPolyParams kMean[kNSpecies] = {
      {10.2875, -36.6639, 67.63, -45.2483},
      {19.2177, -70.3198, 113.876, -67.0591},
      {28.8747, -68.8637, 104.455, -56.9342},
      {39.974, -103.365, 139.182, -67.1446}};
  return kMean[sp];
}

inline const LogPolyParams& Sigma(SpeciesIndex sp) {
  static const LogPolyParams kSigma[kNSpecies] = {
      {0.771189, -2.63609, 2.8044, 0.0},
      {1.40996, -4.97147, 4.97608, 0.0},
      {1.636, -4.35588, 4.40594, 0.0},
      {2.24127, -7.08328, 8.07124, 0.0}};
  return kSigma[sp];
}

inline const PMomentumRange& PMomentumRangeFor(SpeciesIndex sp) {
  static const PMomentumRange kPRange[kNSpecies] = {
      {0.2, 6.0}, {0.2, 6.0}, {0.4, 5.0}, {0.4, 5.0}};
  return kPRange[sp];
}

inline Double_t LogPoly(Double_t p, const LogPolyParams& par) {
  if (p <= 0) return 0.0;
  const Double_t lp = TMath::Log10(p);
  return par.c0 + par.c1 * lp + par.c2 * lp * lp + par.c3 * lp * lp * lp;
}

inline Bool_t IsValidPMomentum(SpeciesIndex sp, Double_t p) {
  const PMomentumRange& r = PMomentumRangeFor(sp);
  return (p >= r.pMin && p <= r.pMax);
}

inline Double_t GetMean(SpeciesIndex sp, Double_t p) {
  if (!IsValidPMomentum(sp, p)) return 0.0;
  return LogPoly(p, Mean(sp));
}

inline Double_t GetSigma(SpeciesIndex sp, Double_t p) {
  if (!IsValidPMomentum(sp, p)) return 0.0;
  return LogPoly(p, Sigma(sp));
}

inline Double_t GetNSigma(SpeciesIndex sp, Double_t p, Double_t dedx) {
  if (!IsValidPMomentum(sp, p)) return 999.0;
  const Double_t mean = GetMean(sp, p);
  const Double_t sigma = GetSigma(sp, p);
  if (sigma <= 0) return 999.0;
  return (dedx - mean) / sigma;
}

}  // namespace NuclearIdDeDxVsMom

#endif
