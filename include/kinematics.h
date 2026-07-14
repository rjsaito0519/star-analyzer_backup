#ifndef KINEMATICS_H
#define KINEMATICS_H

/**
 * Expected 1/beta for a particle of mass [GeV/c^2] and momentum p [GeV/c].
 * 1/beta = E/p = sqrt(m^2 + p^2) / p.
 * Returns -1 if p <= 0.
 */
double OneOverBetaExpected(double mass, double p);

/**
 * Delta(1/beta) = 1/beta_measured - 1/beta_expected(mass, p).
 * Returns the signed difference; use fabs() for a cut on |Delta(1/beta)|.
 * If betaMeasured <= 0 or p <= 0, returns 999.0 so caller can skip.
 */
double DeltaOneOverBeta(double betaMeasured, double mass, double p);

/**
 * CM rapidity of the NN system in the lab frame for fixed-target kinematics.
 * sqrtSNNGeV is sqrt(s_NN) [GeV]; nucleonMassGeV is the nucleon mass [GeV/c^2].
 * On success writes y_shift and returns true; y_shift is undefined if false.
 * y_shift = atanh(beta_cm) with beta_cm = p_beam / (E_beam + m_target), target at rest.
 */
bool BeamRapidityShiftFromSqrtSNN(double sqrtSNNGeV, double nucleonMassGeV, double* yShiftOut);

#endif
