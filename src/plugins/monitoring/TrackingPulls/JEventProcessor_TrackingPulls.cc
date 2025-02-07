// $Id$
//
//    File: JEventProcessor_TrackingPulls.cc
// Created: Thu Nov  3 14:30:19 EDT 2016
// Creator: mstaib (on Linux ifarm1401 2.6.32-431.el6.x86_64 x86_64)
//

#include "JEventProcessor_TrackingPulls.h"
#include "HistogramTools.h"
#include "PID/DChargedTrack.h"
#include "TRACKING/DTrackTimeBased.h"
#include "TRIGGER/DTrigger.h"

using namespace jana;

// Routine used to create our JEventProcessor
#include <JANA/JApplication.h>
#include <JANA/JFactory.h>
extern "C" {
void InitPlugin(JApplication *app) {
  InitJANAPlugin(app);
  app->AddProcessor(new JEventProcessor_TrackingPulls());
}
}  // "C"

JEventProcessor_TrackingPulls::JEventProcessor_TrackingPulls() {}

JEventProcessor_TrackingPulls::~JEventProcessor_TrackingPulls() {}

jerror_t JEventProcessor_TrackingPulls::init(void) {
  // This is called once at program startup.

  tree_ = new TTree("tracking_pulls", "tracking_pulls");
  tree_->SetAutoSave(1000);
  tree_->Branch("eventnumber", &eventnumber_, "eventnumber/I");
  tree_->Branch("track_index", &track_index_, "track_index/I");
  tree_->Branch("chi2", &chi2_, "chi2/D");
  tree_->Branch("ndf", &ndf_, "ndf/I");
  tree_->Branch("charge", &charge_, "charge/D");
  tree_->Branch("mom", &mom_, "mom/D");
  tree_->Branch("phi", &phi_, "phi/D");
  tree_->Branch("theta", &theta_, "theta/D");
  tree_->Branch("pos_x", &pos_x_, "pos_x/D");
  tree_->Branch("pos_y", &pos_y_, "pos_y/D");
  tree_->Branch("pos_z", &pos_z_, "pos_z/D");
  tree_->Branch("smoothed", &smoothed_, "smoothed/I");
  tree_->Branch("any_nan", &any_nan_, "any_nan/I");
  tree_->Branch("cdc_ring_multi_hits", &cdc_ring_multi_hits_,
                "cdc_ring_multi_hits/I");
  tree_->Branch("fdc_resi", fdc_resi_, Form("fdc_resi[%d]/D", kNumFdcPlanes));
  tree_->Branch("fdc_resic", fdc_resic_,
                Form("fdc_resic[%d]/D", kNumFdcPlanes));
  tree_->Branch("fdc_err", fdc_err_, Form("fdc_err[%d]/D", kNumFdcPlanes));
  tree_->Branch("fdc_errc", fdc_errc_, Form("fdc_errc[%d]/D", kNumFdcPlanes));
  tree_->Branch("fdc_x", fdc_x_, Form("fdc_x[%d]/D", kNumFdcPlanes));
  tree_->Branch("fdc_y", fdc_y_, Form("fdc_y[%d]/D", kNumFdcPlanes));
  tree_->Branch("fdc_z", fdc_z_, Form("fdc_z[%d]/D", kNumFdcPlanes));
  tree_->Branch("fdc_w", fdc_w_, Form("fdc_w[%d]/D", kNumFdcPlanes));
  tree_->Branch("fdc_s", fdc_s_, Form("fdc_s[%d]/D", kNumFdcPlanes));
  tree_->Branch("fdc_d", fdc_d_, Form("fdc_d[%d]/D", kNumFdcPlanes));
  tree_->Branch("fdc_tdrift", fdc_tdrift_,
                Form("fdc_tdrift[%d]/D", kNumFdcPlanes));
  tree_->Branch("fdc_wire", fdc_wire_, Form("fdc_wire[%d]/I", kNumFdcPlanes));
  tree_->Branch("fdc_left_right", fdc_left_right_,
                Form("fdc_left_right[%d]/I", kNumFdcPlanes));
  tree_->Branch("cdc_resi", cdc_resi_, Form("cdc_resi[%d]/D", kNumCdcRings));
  tree_->Branch("cdc_err", cdc_err_, Form("cdc_err[%d]/D", kNumCdcRings));
  tree_->Branch("cdc_z", cdc_z_, Form("cdc_z[%d]/D", kNumCdcRings));
  tree_->Branch("cdc_tdrift", cdc_tdrift_,
                Form("cdc_tdrift[%d]/D", kNumCdcRings));
  tree_->Branch("cdc_straw", cdc_straw_, Form("cdc_straw[%d]/I", kNumCdcRings));
  tree_->Branch("cdc_left_right", cdc_left_right_,
                Form("cdc_left_right[%d]/I", kNumCdcRings));
  tree_->Branch("cdc_phi_intersect", cdc_phi_intersect_,
                Form("cdc_phi_intersect[%d]/D", kNumCdcRings));

  return NOERROR;
}

jerror_t JEventProcessor_TrackingPulls::brun(JEventLoop *eventLoop,
                                             int32_t runnumber) {
  // This is called whenever the run number changes
  return NOERROR;
}

jerror_t JEventProcessor_TrackingPulls::evnt(JEventLoop *loop,
                                             uint64_t eventnumber) {
  unsigned int numstraws[28] = {
      42,  42,  54,  54,  66,  66,  80,  80,  93,  93,  106, 106, 123, 123,
      135, 135, 146, 146, 158, 158, 170, 170, 182, 182, 197, 197, 209, 209};
  static uint32_t evntCount = 0;
  evntCount++;
  // Loop over the tracks, get the tracking pulls, and fill some histograms.
  // Easy peasy

  const DTrigger *locTrigger = NULL;
  loop->GetSingle(locTrigger);
  if (locTrigger->Get_L1FrontPanelTriggerBits() != 0) return NOERROR;

  vector<const DChargedTrack *> chargedTrackVector;
  loop->Get(chargedTrackVector);

  for (size_t i = 0; i < chargedTrackVector.size(); i++) {
    // TODO: Should be changed to use PID FOM when ready
    const DChargedTrackHypothesis *bestHypothesis =
        chargedTrackVector[i]->Get_BestTrackingFOM();

    if (bestHypothesis == NULL) continue;

    auto locTrackTimeBased = bestHypothesis->Get_TrackTimeBased();
    double trackingFOM =
        TMath::Prob(locTrackTimeBased->chisq, locTrackTimeBased->Ndof);

    // Some quality cuts for the tracks we will use
    // Keep this minimal for now and investigate later
    float trackingFOMCut = 0.001;
    int trackingNDFCut = 5;

    if (trackingFOM < trackingFOMCut) continue;
    if (locTrackTimeBased->Ndof < trackingNDFCut) continue;

    double phi = bestHypothesis->momentum().Phi() * TMath::RadToDeg();
    double theta = bestHypothesis->momentum().Theta() * TMath::RadToDeg();
    double pmag = bestHypothesis->momentum().Mag();

    if (pmag < 0.5) continue;

    // Fill some track information
    Fill1DHistogram("TrackingPulls", "TrackInfo", "Tracking FOM", trackingFOM,
                    "Tracking FOM", 200, 0.0, 1.0);
    Fill2DHistogram("TrackingPulls", "TrackInfo", "P Vs. Theta", theta, pmag,
                    "P Vs. #theta; #theta [deg.]; |P| [GeV/c]", 70, 0.0, 140.0,
                    50, 0.0, 10.0);
    Fill2DHistogram("TrackingPulls", "TrackInfo", "Phi Vs. Theta", theta, phi,
                    "#phi Vs. #theta; #theta [deg.];  #phi [deg.]", 70, 0.0,
                    140.0, 180, -180.0, 180.0);
    Fill2DHistogram("TrackingPulls", "TrackInfo", "P Vs. Phi", phi, pmag,
                    "P Vs. #phi; #phi [deg.]; |P| [GeV/c]", 180, -180, 180.0,
                    50, 0.0, 10.0);

    // Get the pulls vector from the track
    auto track = bestHypothesis->Get_TrackTimeBased();

    // Initializes TTree variables.
    for (int j = 0; j < kNumFdcPlanes; ++j) {
      fdc_resi_[j] = -999.9;
      fdc_resic_[j] = -999.9;
      fdc_err_[j] = -999.9;
      fdc_errc_[j] = -999.9;
      fdc_x_[j] = -999.9;
      fdc_y_[j] = -999.9;
      fdc_z_[j] = -999.9;
      fdc_w_[j] = -999.9;
      fdc_s_[j] = -999.9;
      fdc_d_[j] = -999.9;
      fdc_tdrift_[j] = -999.9;
      fdc_wire_[j] = -999;
      fdc_left_right_[j] = -999;
    }
    for (int j = 0; j < kNumCdcRings; ++j) {
      cdc_resi_[j] = -999.9;
      cdc_err_[j] = -999.9;
      cdc_z_[j] = -999.9;
      cdc_tdrift_[j] = -999.9;
      cdc_straw_[j] = -999;
      cdc_left_right_[j] = -999;
      cdc_phi_intersect_[j] = -999.9;
    }
    eventnumber_ = (int)eventnumber;
    track_index_ = (int)i;
    chi2_ = track->chisq;
    ndf_ = track->Ndof;
    charge_ = track->charge();
    mom_ = track->momentum().Mag();
    phi_ = track->momentum().Phi() * TMath::RadToDeg();
    theta_ = track->momentum().Theta() * TMath::RadToDeg();
    pos_x_ = track->position().X();
    pos_y_ = track->position().Y();
    pos_z_ = track->position().Z();
    smoothed_ = (track->IsSmoothed ? 1 : 0);

    if (!track->IsSmoothed) {
      Fill1DHistogram("TrackingPulls", "TrackInfo_SmoothFailure",
                      "Tracking FOM", trackingFOM, "Tracking FOM", 200, 0.0,
                      1.0);
      Fill2DHistogram("TrackingPulls", "TrackInfo_SmoothFailure", "P Vs. Theta",
                      theta, pmag, "P Vs. #theta; #theta [deg.]; |P| [GeV/c]",
                      70, 0.0, 140.0, 50, 0.0, 10.0);
      Fill2DHistogram("TrackingPulls", "TrackInfo_SmoothFailure",
                      "Phi Vs. Theta", theta, phi,
                      "#phi Vs. #theta; #theta [deg.];  #phi [deg.]", 70, 0.0,
                      140.0, 180, -180.0, 180.0);
      Fill2DHistogram("TrackingPulls", "TrackInfo_SmoothFailure", "P Vs. Phi",
                      phi, pmag, "P Vs. #phi; #phi [deg.]; |P| [GeV/c]", 180,
                      -180, 180.0, 50, 0.0, 10.0);
      continue;
    } else {
      Fill1DHistogram("TrackingPulls", "TrackInfo_SmoothSuccess",
                      "Tracking FOM", trackingFOM, "Tracking FOM", 200, 0.0,
                      1.0);
      Fill2DHistogram("TrackingPulls", "TrackInfo_SmoothSuccess", "P Vs. Theta",
                      theta, pmag, "P Vs. #theta; #theta [deg.]; |P| [GeV/c]",
                      70, 0.0, 140.0, 50, 0.0, 10.0);
      Fill2DHistogram("TrackingPulls", "TrackInfo_SmoothSuccess",
                      "Phi Vs. Theta", theta, phi,
                      "#phi Vs. #theta; #theta [deg.];  #phi [deg.]", 70, 0.0,
                      140.0, 180, -180.0, 180.0);
      Fill2DHistogram("TrackingPulls", "TrackInfo_SmoothSuccess", "P Vs. Phi",
                      phi, pmag, "P Vs. #phi; #phi [deg.]; |P| [GeV/c]", 180,
                      -180, 180.0, 50, 0.0, 10.0);
    }

    vector<DTrackFitter::pull_t> pulls = track->pulls;

    // Check for NaNs
    any_nan_ = false;
    for (size_t iPull = 0; iPull < pulls.size(); iPull++) {
      double err = pulls[iPull].err;
      double errc = pulls[iPull].errc;
      if (err != err || errc != errc) {
        any_nan_ = true;
        break;
      }
    }
    if (any_nan_) {
      Fill1DHistogram("TrackingPulls", "TrackInfo_SmoothSuccess_NaN",
                      "Tracking FOM", trackingFOM, "Tracking FOM", 200, 0.0,
                      1.0);
      Fill2DHistogram("TrackingPulls", "TrackInfo_SmoothSuccess_NaN",
                      "P Vs. Theta", theta, pmag,
                      "P Vs. #theta; #theta [deg.]; |P| [GeV/c]", 70, 0.0,
                      140.0, 50, 0.0, 10.0);
      Fill2DHistogram("TrackingPulls", "TrackInfo_SmoothSuccess_NaN",
                      "Phi Vs. Theta", theta, phi,
                      "#phi Vs. #theta; #theta [deg.];  #phi [deg.]", 70, 0.0,
                      140.0, 180, -180.0, 180.0);
      Fill2DHistogram("TrackingPulls", "TrackInfo_SmoothSuccess_NaN",
                      "P Vs. Phi", phi, pmag,
                      "P Vs. #phi; #phi [deg.]; |P| [GeV/c]", 180, -180, 180.0,
                      50, 0.0, 10.0);
      continue;
    }

    for (size_t iPull = 0; iPull < pulls.size(); iPull++) {
      double resi = pulls[iPull].resi;
      double err = pulls[iPull].err;  // estimated error of measurement
      double tdrift = pulls[iPull].tdrift;
      const DCDCTrackHit *cdc_hit = pulls[iPull].cdc_hit;
      const DFDCPseudo *fdc_hit = pulls[iPull].fdc_hit;
      double z = pulls[iPull].z;  // z position at doca
      double resic = pulls[iPull].resic;
      double errc = pulls[iPull].errc;

      Fill1DHistogram("TrackingPulls", "TrackPulls", "All Pulls", resi / err,
                      "Residual/Error", 100, -5.0, 5.0);
      Fill2DHistogram("TrackingPulls", "TrackPulls", "All Pulls Vs. P", pmag,
                      resi / err, ";|P| ;Residual/Error", 100, 0.0, 10.0, 100,
                      -5.0, 5.0);
      Fill2DHistogram("TrackingPulls", "TrackPulls", "All Pulls Vs. Phi", phi,
                      resi / err, ";#phi ;Residual/Error", 180, -180.0, 180.0,
                      100, -5.0, 5.0);
      Fill2DHistogram("TrackingPulls", "TrackPulls", "All Pulls Vs. Theta",
                      theta, resi / err, ";#theta ;Residual/Error", 140, 0.0,
                      140.0, 100, -5.0, 5.0);
      Fill2DHistogram("TrackingPulls", "TrackPulls", "All Pulls Vs. NDF",
                      locTrackTimeBased->Ndof, resi / err,
                      ";Track NDF ;Residual/Error", 140, 0.0, 140.0, 100, -5.0,
                      5.0);
      Fill2DHistogram("TrackingPulls", "TrackPulls",
                      "All Pulls Vs. Tracking FOM", trackingFOM, resi / err,
                      ";Track FOM ;Residual/Error", 140, 0.0, 140.0, 100, -5.0,
                      5.0);
      if (fdc_hit != nullptr) {
        Fill1DHistogram("TrackingPulls", "TrackPulls", "FDC Wire Error", err,
                        "Wire Residual Error", 100, 0.0, 0.1);
        Fill1DHistogram("TrackingPulls", "TrackPulls", "FDC Cathode Error",
                        errc, "Cathode Residual Error", 100, 0.0, 0.1);
      } else {
        Fill1DHistogram("TrackingPulls", "TrackPulls", "CDCError", err,
                        "Residual Error", 100, 0.0, 0.1);
      }

      // Fill some detector specific info
      // Fill them in order = super-hacked
      static int nextPlane = 1;
      static int nextRing = 1;

      if (fdc_hit != nullptr && fdc_hit->wire->layer <= nextPlane) {
        if (fdc_hit->wire->layer == nextPlane) nextPlane++;
        Fill1DHistogram("TrackingPulls", "FDCPulls", "All Wire Pulls",
                        resi / err, "Residual/Error", 100, -5.0, 5.0);
        Fill1DHistogram("TrackingPulls", "FDCPulls", "All Cathode Pulls",
                        resic / errc, "Residual/Error", 100, -5.0, 5.0);
        Fill1DHistogram("TrackingPulls", "FDCPulls", "All Wire Residuals", resi,
                        "Residual", 100, -0.1, 0.1);
        Fill1DHistogram("TrackingPulls", "FDCPulls", "All Cathode Residuals",
                        resic, "Residual", 100, -0.1, 0.1);
        Fill2DHistogram("TrackingPulls", "FDCPulls",
                        "All Wire Residuals Vs. Plane", fdc_hit->wire->layer,
                        resi, ";plane ;Residual", 24, 0.5, 24.5, 100, -0.1,
                        0.1);
        Fill2DHistogram("TrackingPulls", "FDCPulls",
                        "All Cathode Residuals Vs. Plane", fdc_hit->wire->layer,
                        resic, ";plane ;Residual", 24, 0.5, 24.5, 100, -0.1,
                        0.1);
        Fill2DHistogram("TrackingPulls", "FDCPulls", "All Wire Pulls Vs. Plane",
                        fdc_hit->wire->layer, resi / err,
                        ";plane ;Residual/Error", 24, 0.5, 24.5, 100, -5.0,
                        5.0);
        Fill2DHistogram("TrackingPulls", "FDCPulls",
                        "All Cathode Pulls Vs. Plane", fdc_hit->wire->layer,
                        resic / errc, ";plane ;Residual/Error", 24, 0.5, 24.5,
                        100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", "FDCPulls",
                        "All Wire Residuals Vs Drift Time", tdrift, resi,
                        ";Drift Time;Residual", 170, -20.0, 150.0, 100, -0.1,
                        0.1);
        Fill2DHistogram("TrackingPulls", "FDCPulls",
                        "All Wire Pulls Vs Drift Time", tdrift, resi / err,
                        ";Drift Time;Residual/Error", 170, -20.0, 150.0, 100,
                        -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", "FDCPulls", "All Wire Pulls Vs. P",
                        pmag, resi / err, ";|P| ;Residual/Error", 100, 0.0,
                        10.0, 100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", "FDCPulls", "All Wire Pulls Vs. Phi",
                        phi, resi / err, ";#phi ;Residual/Error", 180, -180.0,
                        180.0, 100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", "FDCPulls", "All Wire Pulls Vs. Theta",
                        theta, resi / err, ";#theta ;Residual/Error", 50, 0.0,
                        25.0, 100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", "FDCPulls", "All Cathode Pulls Vs. P",
                        pmag, resic / errc, ";|P| ;Residual/Error", 100, 0.0,
                        10.0, 100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", "FDCPulls",
                        "All Cathode Pulls Vs. Phi", phi, resic / errc,
                        ";#phi ;Residual/Error", 180, -180.0, 180.0, 100, -5.0,
                        5.0);
        Fill2DHistogram("TrackingPulls", "FDCPulls",
                        "All Cathode Pulls Vs. Theta", theta, resic / errc,
                        ";#theta ;Residual/Error", 50, 0.0, 25.0, 100, -5.0,
                        5.0);
        Fill2DHistogram("TrackingPulls", "FDCPulls", "All Wire Residuals Vs. P",
                        pmag, resi, ";|P| ;Residual/Error", 100, 0.0, 10.0, 100,
                        -0.1, 0.1);
        Fill2DHistogram(
            "TrackingPulls", "FDCPulls", "All Wire Residuals Vs. Phi", phi,
            resi, ";#phi ;Residual/Error", 180, -180.0, 180.0, 100, -0.1, 0.1);
        Fill2DHistogram(
            "TrackingPulls", "FDCPulls", "All Wire Residuals Vs. Theta", theta,
            resi, ";#theta ;Residual/Error", 50, 0.0, 25.0, 100, -0.1, 0.1);
        Fill2DHistogram("TrackingPulls", "FDCPulls",
                        "All Cathode Residuals Vs. P", pmag, resic,
                        ";|P| ;Residual/Error", 100, 0.0, 10.0, 100, -0.1, 0.1);
        Fill2DHistogram(
            "TrackingPulls", "FDCPulls", "All Cathode Residuals Vs. Phi", phi,
            resic, ";#phi ;Residual/Error", 180, -180.0, 180.0, 100, -0.1, 0.1);
        Fill2DHistogram("TrackingPulls", "FDCPulls",
                        "All Cathode Residuals Vs. Theta", theta, resic,
                        ";#theta ;Residual/Error", 50, 0.0, 25.0, 100, -0.1,
                        0.1);
        Fill2DHistogram("TrackingPulls", "FDCPulls", "All Wire Pulls Vs. NDF",
                        locTrackTimeBased->Ndof, resi / err,
                        ";Track NDF ;Residual/Error", 50, 0.5, 50.5, 100, -5.0,
                        5.0);
        Fill2DHistogram("TrackingPulls", "FDCPulls",
                        "All Wire Pulls Vs. Tracking FOM", trackingFOM,
                        resi / err, ";Track FOM ;Residual/Error", 100, 0.0, 1.0,
                        100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", "FDCPulls",
                        "All Cathode Pulls Vs. NDF", locTrackTimeBased->Ndof,
                        resic / errc, ";Track NDF ;Residual/Error", 50, 0.5,
                        50.5, 100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", "FDCPulls",
                        "All Cathode Pulls Vs. Tracking FOM", trackingFOM,
                        resic / errc, ";Track FOM ;Residual/Error", 100, 0.0,
                        1.0, 100, -5.0, 5.0);

        // Make the Per-Plane Histograms
        char planeName[256];
        sprintf(planeName, "FDCPulls_Plane%.2i", fdc_hit->wire->layer);

        Fill1DHistogram("TrackingPulls", planeName, "All Wire Pulls",
                        resi / err, "Residual/Error", 100, -5.0, 5.0);
        Fill1DHistogram("TrackingPulls", planeName, "All Cathode Pulls",
                        resic / errc, "Residual/Error", 100, -5.0, 5.0);
        Fill1DHistogram("TrackingPulls", planeName, "All Wire Residuals", resi,
                        "Residual", 100, -0.1, 0.1);
        if (trackingFOM > 0.02) {
          Fill1DHistogram("TrackingPulls", planeName, "wire_residual", resi, "Residual", 200, -0.1, 0.1);
          if (pulls[iPull].left_right == 1) {
            Fill1DHistogram("TrackingPulls", planeName, "wire_residual_right", resi, "Residual", 200, -0.1, 0.1);
          } else if (pulls[iPull].left_right == -1) {
            Fill1DHistogram("TrackingPulls", planeName, "wire_residual_left", resi, "Residual", 200, -0.1, 0.1);
          }
        }
        Fill1DHistogram("TrackingPulls", planeName, "All Cathode Residuals",
                        resic, "Residual", 100, -0.1, 0.1);
        Fill2DHistogram("TrackingPulls", planeName,
                        "All Wire Residuals Vs Drift Time", tdrift, resi,
                        ";Drift Time;Residual", 170, -20.0, 150.0, 100, -0.1,
                        0.1);
        Fill2DHistogram("TrackingPulls", planeName,
                        "All Wire Pulls Vs Drift Time", tdrift, resi / err,
                        ";Drift Time;Residual/Error", 170, -20.0, 150.0, 100,
                        -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", planeName, "All Wire Pulls Vs. P",
                        pmag, resi / err, ";|P| ;Residual/Error", 100, 0.0,
                        10.0, 100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", planeName, "All Wire Pulls Vs. Phi",
                        phi, resi / err, ";#phi ;Residual/Error", 180, -180.0,
                        180.0, 100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", planeName, "All Wire Pulls Vs. Theta",
                        theta, resi / err, ";#theta ;Residual/Error", 50, 0.0,
                        25.0, 100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", planeName, "All Wire Residuals Vs. P",
                        pmag, resi, ";|P| ;Residual", 100, 0.0, 10.0, 100, -0.1,
                        0.1);
        Fill2DHistogram("TrackingPulls", planeName,
                        "All Wire Residuals Vs. Phi", phi, resi,
                        ";#phi ;Residual", 180, -180.0, 180.0, 100, -0.1, 0.1);
        Fill2DHistogram("TrackingPulls", planeName,
                        "All Wire Residuals Vs. Theta", theta, resi,
                        ";#theta ;Residual", 50, 0.0, 25.0, 100, -0.1, 0.1);
        Fill2DHistogram("TrackingPulls", planeName, "All Cathode Pulls Vs. P",
                        pmag, resic / errc, ";|P| ;Residual/Error", 100, 0.0,
                        10.0, 100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", planeName, "All Cathode Pulls Vs. Phi",
                        phi, resic / errc, ";#phi ;Residual/Error", 180, -180.0,
                        180.0, 100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", planeName,
                        "All Cathode Pulls Vs. Theta", theta, resic / errc,
                        ";#theta ;Residual/Error", 50, 0.0, 25.0, 100, -5.0,
                        5.0);
        Fill2DHistogram("TrackingPulls", planeName,
                        "All Cathode Residuals Vs. P", pmag, resic,
                        ";|P| ;Residual", 100, 0.0, 10.0, 100, -0.1, 0.1);
        Fill2DHistogram("TrackingPulls", planeName,
                        "All Cathode Residuals Vs. Phi", phi, resic,
                        ";#phi ;Residual", 180, -180.0, 180.0, 100, -0.1, 0.1);
        Fill2DHistogram("TrackingPulls", planeName,
                        "All Cathode Residuals Vs. Theta", theta, resic,
                        ";#theta ;Residual", 50, 0.0, 25.0, 100, -0.1, 0.1);
        Fill2DHistogram("TrackingPulls", planeName, "Wire Pulls",
                        fdc_hit->wire->wire, resi / err,
                        ";Wire Number ;Residual/Error", 96, 0.5, 96.5, 100,
                        -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", planeName, "Wire Residuals",
                        fdc_hit->wire->wire, resi, ";Wire Number ;Residual", 96,
                        0.5, 96.5, 100, -0.1, 0.1);
        if (fabs(resi / err) < 5.0) {
          Fill2DProfile("TrackingPulls", planeName, "2D Wire Hit Pulls",
                        fdc_hit->xy.X(), fdc_hit->xy.Y(), resi / err,
                        "Mean of Wire Pulls vs. PseudoHit XY", 100, -50., 50.,
                        100, -50., 50.);
        }
        if (fabs(resi) < 0.1) {
          Fill2DProfile("TrackingPulls", planeName, "2D Wire Hit Residuals",
                        fdc_hit->xy.X(), fdc_hit->xy.Y(), resi,
                        "Mean of Wire Residuals vs. PseudoHit XY", 100, -50.,
                        50., 100, -50., 50.);
          Fill2DProfile("TrackingPulls", planeName,
                        "2D Wire Hit Residuals Local", fdc_hit->w, fdc_hit->s,
                        resi,
                        "Mean of Wire Residuals vs. PseudoHit WS;Perpendicular "
                        "Distance to Wire; Distance Along the Wire",
                        100, -50., 50., 100, -50., 50.);
        }
        if (fabs(resic / errc) < 5.0) {
          Fill2DProfile("TrackingPulls", planeName, "2D Cathode Hit Pulls",
                        fdc_hit->xy.X(), fdc_hit->xy.Y(), resic / errc,
                        "Mean of Cathode Pulls vs. PseudoHit XY", 100, -50.,
                        50., 100, -50., 50.);
        }
        if (fabs(resic) < 0.1) {
          Fill2DProfile("TrackingPulls", planeName, "2D Cathode Hit Residuals",
                        fdc_hit->xy.X(), fdc_hit->xy.Y(), resic,
                        "Mean of Cathode Residuals vs. PseudoHit XY", 100, -50.,
                        50., 100, -50., 50.);
          Fill2DProfile(
              "TrackingPulls", planeName, "2D Cathode Hit Residuals Local",
              fdc_hit->w, fdc_hit->s, resic,
              "Mean of Cathode Residuals vs. PseudoHit WS;Perpendicular "
              "Distance to Wire; Distance Along the Wire",
              100, -50., 50., 100, -50., 50.);
        }
        fdc_resi_[fdc_hit->wire->layer - 1] = resi;
        fdc_resic_[fdc_hit->wire->layer - 1] = resic;
        fdc_err_[fdc_hit->wire->layer - 1] = err;
        fdc_errc_[fdc_hit->wire->layer - 1] = errc;
        fdc_x_[fdc_hit->wire->layer - 1] = fdc_hit->xy.X();
        fdc_y_[fdc_hit->wire->layer - 1] = fdc_hit->xy.Y();
        fdc_z_[fdc_hit->wire->layer - 1] = pulls[iPull].z;
        fdc_w_[fdc_hit->wire->layer - 1] = fdc_hit->w;
        fdc_s_[fdc_hit->wire->layer - 1] = fdc_hit->s;
        fdc_d_[fdc_hit->wire->layer - 1] = pulls[iPull].d;
        fdc_tdrift_[fdc_hit->wire->layer - 1] = tdrift;
        fdc_wire_[fdc_hit->wire->layer - 1] = fdc_hit->wire->wire;
        fdc_left_right_[fdc_hit->wire->layer - 1] = pulls[iPull].left_right;
      }

      // Once we are done with the FDC, move on to the CDC.
      if (cdc_hit != nullptr && cdc_hit->wire->ring <= nextRing &&
          (nextPlane == 25 || evntCount > 1000)) {
        if (cdc_hit->wire->ring == nextRing) nextRing++;

        Fill1DHistogram("TrackingPulls", "CDCPulls", "All Residuals", resi,
                        "Residual", 100, -0.1, 0.1);
        Fill1DHistogram("TrackingPulls", "CDCPulls", "All Pulls", resi / err,
                        "Residual/Error", 100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", "CDCPulls", "All Pulls Vs. Ring",
                        cdc_hit->wire->ring, resi / err,
                        ";ring ;Residual/Error", 28, 0.5, 28.5, 100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", "CDCPulls", "All Residuals Vs. Ring",
                        cdc_hit->wire->ring, resi, ";ring ;Residual", 28, 0.5,
                        28.5, 100, -0.1, 0.1);
        Fill2DHistogram("TrackingPulls", "CDCPulls", "All Pulls Vs. tdrift",
                        tdrift, resi / err, ";tdrift [ns] ;Residual/Error", 200,
                        0.0, 1000.0, 100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", "CDCPulls", "All Residuals Vs. tdrift",
                        tdrift, resi, ";tdrift [ns] ;Residual", 200, 0.0,
                        1000.0, 100, -0.1, 0.1);
        Fill2DHistogram("TrackingPulls", "CDCPulls", "All Pulls Vs. P", pmag,
                        resi / err, ";|P| ;Residual/Error", 50, 0.0, 10.0, 100,
                        -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", "CDCPulls", "All Pulls Vs. Phi", phi,
                        resi / err, ";#phi ;Residual/Error", 180, -180.0, 180.0,
                        100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", "CDCPulls", "All Pulls Vs. Theta",
                        theta, resi / err, ";#theta ;Residual/Error", 140, 0.0,
                        140.0, 100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", "CDCPulls", "All Residuals Vs. P",
                        pmag, resi, ";|P| ;Residual", 50, 0.0, 10.0, 100, -0.1,
                        0.1);
        Fill2DHistogram("TrackingPulls", "CDCPulls", "All Residuals Vs. Phi",
                        phi, resi, ";#phi ;Residual", 180, -180.0, 180.0, 100,
                        -0.1, 0.1);
        Fill2DHistogram("TrackingPulls", "CDCPulls", "All Residuals Vs. Theta",
                        theta, resi, ";#theta ;Residual", 140, 0.0, 140.0, 100,
                        -0.1, 0.1);
        Fill2DHistogram("TrackingPulls", "CDCPulls", "All Pulls Vs. NDF",
                        locTrackTimeBased->Ndof, resi / err,
                        ";Track NDF ;Residual/Error", 50, 0.5, 50.5, 100, -5.0,
                        5.0);
        Fill2DHistogram("TrackingPulls", "CDCPulls",
                        "All Pulls Vs. Tracking FOM", trackingFOM, resi / err,
                        ";Track FOM ;Residual/Error", 100, 0.0, 1.0, 100, -5.0,
                        5.0);

        // Make the Per-Ring Histograms
        char ringName[256];
        sprintf(ringName, "CDCPulls_Ring%.2i", cdc_hit->wire->ring);

        Fill1DHistogram("TrackingPulls", ringName, "All Residuals", resi,
                        "Residual", 100, -0.1, 0.1);
        Fill1DHistogram("TrackingPulls", ringName, "All Pulls", resi / err,
                        "Residual/Error", 100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", ringName, "All Pulls Vs. tdrift",
                        tdrift, resi / err, ";tdrift [ns] ;Residual/Error", 200,
                        0.0, 1000.0, 100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", ringName, "All Pulls Vs. z", z,
                        resi / err, ";z [cm] ;Residual/Error", 200, -30., 200.,
                        100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", ringName, "All Residuals Vs. tdrift",
                        tdrift, resi, ";tdrift [ns] ;Residual", 200, 0.0,
                        1000.0, 100, -0.1, 0.1);
        Fill2DHistogram("TrackingPulls", ringName, "All Residuals Vs. z", z,
                        resi, ";z [cm] ;Residual", 200, -30., 200.0, 100, -0.1,
                        0.1);
        Fill2DHistogram("TrackingPulls", ringName, "All Pulls Vs. P", pmag,
                        resi / err, ";|P| ;Residual/Error", 50, 0.0, 10.0, 100,
                        -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", ringName, "All Pulls Vs. Phi", phi,
                        resi / err, ";#phi ;Residual/Error", 180, -180.0, 180.0,
                        100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", ringName, "All Pulls Vs. Theta", theta,
                        resi / err, ";#theta ;Residual/Error", 140, 0.0, 140.0,
                        100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", ringName, "All Residuals Vs. P", pmag,
                        resi, ";|P| ;Residual", 50, 0.0, 10.0, 100, -0.1, 0.1);
        Fill2DHistogram("TrackingPulls", ringName, "All Residuals Vs. Phi", phi,
                        resi, ";#phi ;Residual", 180, -180.0, 180.0, 100, -0.1,
                        0.1);
        Fill2DHistogram("TrackingPulls", ringName, "All Residuals Vs. Theta",
                        theta, resi, ";#theta ;Residual", 140, 0.0, 140.0, 100,
                        -0.1, 0.1);

        double nStraw = numstraws[cdc_hit->wire->ring - 1];
        double phiIntersect =
            (cdc_hit->wire->origin + (z - 92.0) * cdc_hit->wire->udir).Phi() *
            TMath::RadToDeg();

        Fill2DHistogram("TrackingPulls", ringName, "Per Straw Pulls",
                        cdc_hit->wire->straw, resi / err,
                        ";Straw Number ;Residual/Error", nStraw, 0.5,
                        nStraw + 0.5, 100, -5.0, 5.0);
        Fill2DHistogram("TrackingPulls", ringName, "Per Straw Residuals",
                        cdc_hit->wire->straw, resi, ";Straw Number ;Residual",
                        nStraw, 0.5, nStraw + 0.5, 100, -0.1, 0.1);

        if (fabs(resi) < 0.1) {
          Fill2DProfile("TrackingPulls", ringName, "Residual Vs Phi-Theta",
                        theta, phi, resi, ";#theta;#phi", 70, 0.0, 140.0, 180,
                        -180.0, 180.0);
          Fill2DProfile("TrackingPulls", ringName, "Residual Vs Phi-z", z, phi,
                        resi, ";z;#phi", 200, 0.0, 200.0, 180, -180.0, 180.0);
          Fill2DProfile("TrackingPulls", ringName, "Residual Vs PhiIntersect-z",
                        z, phiIntersect, resi, ";z;#phi Intersect", 200, 0.0,
                        200.0, nStraw, -180.0, 180.0);
          Fill2DProfile("TrackingPulls", ringName, "Residual Vs P-Theta", theta,
                        pmag, resi, ";#theta;|P|", 70, 0.0, 140.0, 50, 0.0,
                        10.0);
        }
        cdc_resi_[cdc_hit->wire->ring - 1] = resi;
        cdc_err_[cdc_hit->wire->ring - 1] = err;
        cdc_z_[cdc_hit->wire->ring - 1] = pulls[iPull].z;
        cdc_tdrift_[cdc_hit->wire->ring - 1] = tdrift;
        cdc_straw_[cdc_hit->wire->ring - 1] = cdc_hit->wire->straw;
        cdc_left_right_[cdc_hit->wire->ring - 1] = pulls[iPull].left_right;
        cdc_phi_intersect_[cdc_hit->wire->ring - 1] =
            (cdc_hit->wire->origin + (z - 92.0) * cdc_hit->wire->udir).Phi() *
            TMath::RadToDeg();
      }
    }
    japp->RootWriteLock();
    tree_->Fill();
    japp->RootUnLock();
  }

  return NOERROR;
}

jerror_t JEventProcessor_TrackingPulls::erun(void) {
  // This is called whenever the run number changes, before it is
  // changed to give you a chance to clean up before processing
  // events from the next run number.
  return NOERROR;
}

jerror_t JEventProcessor_TrackingPulls::fini(void) {
  // Called before program exit after event processing is finished.
  return NOERROR;
}
