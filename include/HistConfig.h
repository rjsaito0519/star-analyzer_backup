#ifndef ANALYSIS_CONFIG_H
#define ANALYSIS_CONFIG_H

#include "Rtypes.h"

namespace Config {
  // ----------------------------------------
  // Histogram Binning & Ranges
  // ----------------------------------------

  // Event Level
  const Int_t    nBinsVz = 400;
  const Double_t minVz   = -100.0;
  const Double_t maxVz   = 100.0; // cm

  const Int_t    nBinsVxy = 200;
  const Double_t minVxy   = -5.0;
  const Double_t maxVxy   = 5.0; // cm

  const Int_t    nBinsRefMult = 1000;
  const Double_t minRefMult   = 0.0;
  const Double_t maxRefMult   = 1000.0;

  // RunID mapping (Adjust based on your dataset year)
  // Run19 example range roughly. Check your run log.
  const Int_t    nBinsRun   = 10000;
  const Double_t minRunId   = 19000000; 
  const Double_t maxRunId   = 21000000;

  // Track Level
  const Int_t    nBinsPt = 200;
  const Double_t minPt   = 0.0;
  const Double_t maxPt   = 10.0; // GeV/c

  const Int_t    nBinsEta = 100;
  const Double_t minEta   = -2.5;
  const Double_t maxEta   = 2.5;

  const Int_t    nBinsPhi = 100;
  const Double_t minPhi   = -3.15; // -Pi
  const Double_t maxPhi   = 3.15;  // +Pi

  const Int_t    nBinsDCA = 200;
  const Double_t minDCA   = 0.0;
  const Double_t maxDCA   = 5.0; // cm

  const Int_t    nBinsHits = 60;
  const Double_t minHits   = 0.0;
  const Double_t maxHits   = 60.0;

  // PID
  const Int_t    nBinsDedx = 100; 
  const Double_t minDedx   = 0.0; 
  const Double_t maxDedx   = 1e-6; // keV/cm typically 

  const Int_t    nBinsBeta = 400; 
  const Double_t minBeta   = 0.0; 
  const Double_t maxBeta   = 4.0; 

  const Int_t    nBinsMass2 = 400; 
  const Double_t minMass2   = -0.5; 
  const Double_t maxMass2   = 1.5; 

  // Event Plane
  const Int_t    nBinsPsi = 100;
  const Double_t minPsi   = 0.0;
  const Double_t maxPsi   = 3.15; // 0 to Pi for Psi2
}

#endif
