// $Id$
//
//    File: DEventProcessor_fcal_hists.cc
// Created: Mon Apr  3 11:38:03 EDT 2006
// Creator: davidl (on Darwin swire-b241.jlab.org 8.4.0 powerpc)
//

#include "DEventProcessor_fcal_hists.h"

#include <TLorentzVector.h>

#include "DANA/DApplication.h"
#include "FCAL/DFCALCluster.h"
#include "TRACKING/DMCThrown.h"

// The executable should define the ROOTfile global variable. It will
// be automatically linked when dlopen is called.
extern TFile *ROOTfile;

// Routine used to create our DEventProcessor
extern "C"{
void InitProcessors(DApplication *app){
	app->AddProcessor(new DEventProcessor_fcal_hists());
}
} // "C"


#define FCAL_Z_OFFSET 640.0-65.0 // I don't know what this value is ???
//#define FCAL_Z_OFFSET 170.0 // I don't know what this value is ???
#define PI_ZERO_MASS 0.13497

//------------------
// init
//------------------
jerror_t DEventProcessor_fcal_hists::init(void)
{
	// open ROOT file (if needed)
	if(ROOTfile != NULL) ROOTfile->cd();
	
	two_gamma_mass = new TH1F("fcal_two_gamma_mass","two_gamma_mass",100, 0.0, 0.300);
	two_gamma_mass_cut = new TH1F("fcal_two_gamma_mass_cut","two_gamma_mass_cut",100, 0.0, 0.300);
	pi0_zdiff = new TH1F("fcal_pi0_zdiff","Calculated z assuming 2 gammas from pi0",1000, 0.0, 1000.0);
	xy_shower = new TH2F("fcal_xy_shower","xy_shower",100, -100.0, 100., 100 , -100.0, 100.0);
	E_shower = new TH1F("fcal_E_shower","E_shower", 200, 0.0, 6.0);
	
	E_over_Erec_vs_E = new TH2F("fcal_E_over_Erec_vs_E","E_over_Erec_vs_E", 200, 0.0, 10.0, 200, 0.0, 4.0);
	E_over_Erec_vs_R = new TH2F("fcal_E_over_Erec_vs_R","E_over_Erec_vs_R", 200, 0.0, 110.0, 200, 0.0, 4.0);
	E_over_Ereccorr_vs_z = new TH2F("fcal_E_over_Ereccorr_vs_z","E_over_Ereccorr_vs_z", 200, 0.0, 10.0, 200, 0.0, 4.0);

	return NOERROR;
}

//------------------
// brun
//------------------
jerror_t DEventProcessor_fcal_hists::brun(JEventLoop *eventLoop, int runnumber)
{
	return NOERROR;
}

//------------------
// evnt
//------------------
jerror_t DEventProcessor_fcal_hists::evnt(JEventLoop *loop, int eventnumber)
{
	vector<const DFCALCluster*> showers;
	loop->Get(showers);
	
	LockState();
	
	// Single shower params
	for(unsigned int i=0; i<showers.size(); i++){
		const DFCALCluster *s = showers[i];
		xy_shower->Fill(s->getCentroid().X(), s->getCentroid().Y());
		E_shower->Fill(s->getEnergy());
	}
	
	// 2-gamma inv. mass
	for(unsigned int i=0; i<showers.size(); i++){
		const DFCALCluster *s1 = showers[i];
		double x1 = s1->getCentroid().X();
		double y1 = s1->getCentroid().Y();
		double dz = FCAL_Z_OFFSET;
		double R = sqrt(x1*x1 + y1*y1 + dz*dz);
		double E1 = s1->getEnergy();
		TLorentzVector p1(E1*x1/R, E1*y1/R, E1*dz/R, E1);		
		
		for(unsigned int j=i+1; j<showers.size(); j++){
			const DFCALCluster *s2 = showers[j];
			double x2 = s2->getCentroid().X();
			double y2 = s2->getCentroid().Y();
			R = sqrt(x2*x2 + y2*y2 + dz*dz);
			double E2 = s2->getEnergy();
			TLorentzVector p2(E2*x2/R, E2*y2/R, E2*dz/R, E2);		
			
			TLorentzVector ptot = p1+p2;
			two_gamma_mass->Fill(ptot.M());
			if(showers.size()==2)two_gamma_mass_cut->Fill(ptot.M());
			
			// Calculate Z dist. to vertex assuming these photons came
			// from a pi0 decay. Cylindrical coordinates are used here.
			double cos_theta = 1.0 - PI_ZERO_MASS*PI_ZERO_MASS/2.0/E1/E2;
			double cos_theta2 = cos_theta*cos_theta;
			double sin_theta2 = 1.0 - cos_theta2;
			double r1 = sqrt(x1*x1 + y1*y1);
			double r2 = sqrt(x2*x2 + y2*y2);
			double cos_deltaPhi = (x1*x2 + y1*y2)/r1/r2;
			double cos_deltaPhi2 = cos_deltaPhi*cos_deltaPhi;
			
			double A = sin_theta2;
			double B = 2.0*r1*r2*cos_deltaPhi - cos_theta2*(r1*r1 + r2*r2);
			double C = -r1*r1*r2*r2*(cos_theta2 - cos_deltaPhi2);

			double z2 = (-B+sqrt(B*B-4.0*A*C))/2.0/A;
			double z = sqrt(z2);
			pi0_zdiff->Fill(z);

		}
	}
	
	// Compare to thrown values
	vector<const DMCThrown*> mcthrowns;
	loop->Get(mcthrowns);
	for(unsigned int i=0; i<mcthrowns.size(); i++){
		for(unsigned int j=0; j<showers.size(); j++){
			E_over_Erec_vs_E->Fill(showers[j]->getEnergy(), mcthrowns[i]->E/showers[j]->getEnergy());
			double R = sqrt(showers[j]->getCentroid().X()*showers[j]->getCentroid().X() + 
                            showers[j]->getCentroid().Y()*showers[j]->getCentroid().Y());
			E_over_Erec_vs_R->Fill(R, mcthrowns[i]->E/showers[j]->getEnergy());
		}
	}

	UnlockState();	

	return NOERROR;
}

//------------------
// erun
//------------------
jerror_t DEventProcessor_fcal_hists::erun(void)
{
	// Any final calculations on histograms (like dividing them)
	// should be done here. This may get called more than once.
	return NOERROR;
}

//------------------
// fini
//------------------
jerror_t DEventProcessor_fcal_hists::fini(void)
{
	return NOERROR;
}

