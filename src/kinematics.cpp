#include "kinematics.h"
#include "TMath.h"

double OneOverBetaExpected(double mass, double p) {
  if (p <= 0.) return -1.;
  double E = TMath::Sqrt(mass * mass + p * p);
  return E / p;
}

double DeltaOneOverBeta(double betaMeasured, double mass, double p) {
  const double kInvalid = 999.0;
  if (betaMeasured <= 1e-6 || p <= 1e-6) return kInvalid;
  double oneOverBetaMeas = 1.0 / betaMeasured;
  double oneOverBetaExp = OneOverBetaExpected(mass, p);
  if (oneOverBetaExp < 0.) return kInvalid;
  return oneOverBetaMeas - oneOverBetaExp;
}

bool BeamRapidityShiftFromSqrtSNN(double sqrtSNNGeV, double nucleonMassGeV, double* yShiftOut) {
  if (!yShiftOut || sqrtSNNGeV <= 0. || nucleonMassGeV <= 0.) return false;

  const double m = nucleonMassGeV;
  const double s = sqrtSNNGeV * sqrtSNNGeV;
  const double sMin = 4.0 * m * m;
  if (s <= sMin) return false;

  const double eBeam = (s - 2.0 * m * m) / (2.0 * m);
  if (eBeam <= m) return false;

  const double pBeamSq = eBeam * eBeam - m * m;
  if (pBeamSq <= 0.) return false;
  const double pBeam = TMath::Sqrt(pBeamSq);

  const double betaCm = pBeam / (eBeam + m);
  if (betaCm <= 0. || betaCm >= 1.) return false;

  *yShiftOut = 0.5 * TMath::Log((1.0 + betaCm) / (1.0 - betaCm));
  return true;
}
