// $Id$
//
//    File: JEventProcessor_HLDetectorTiming.cc
// Created: Mon Jan 12 14:37:56 EST 2015
// Creator: mstaib (on Linux egbert 2.6.32-431.20.3.el6.x86_64 x86_64)
//

#include "JEventProcessor_HLDetectorTiming.h"
using namespace jana;

// Routine used to create our JEventProcessor
#include <JANA/JApplication.h>
#include <JANA/JFactory.h>

#include "PID/DChargedTrack.h"
#include "PID/DEventRFBunch.h"
#include "TTAB/DTTabUtilities.h"
#include "TTAB/DTranslationTable.h"
#include "FCAL/DFCALGeometry.h"
#include "BCAL/DBCALGeometry.h"
#include "CCAL/DCCALGeometry.h"
#include "TRIGGER/DTrigger.h"
#include "RF/DRFTime.h"
#include "HistogramTools.h"

#include "PAIR_SPECTROMETER/DPSCHit.h"
#include "PAIR_SPECTROMETER/DPSHit.h"
#include "CCAL/DCCALHit.h"
#include "CCAL/DCCALShower.h"
#include "DIRC/DDIRCPmtHit.h"
#include "DIRC/DDIRCPmtHit_factory.h"

extern "C"{
void InitPlugin(JApplication *app){
    InitJANAPlugin(app);
    app->AddProcessor(new JEventProcessor_HLDetectorTiming());
    app->AddFactoryGenerator(new DFactoryGenerator_p2pi()); //register the factory generator
}
} // "C"

static int Get_FDCTDC_crate_slot(int mod, string &act_crate, int &act_slot){ //expected mod range from 1 to 48
  int LH_module=(mod-1)%2; //low (1-48) or high (49-96) wire number (0/1)
  int package=(mod-1)/12; //package number (0-3)
  int cell=(mod-1-package*12)/2;  //cell number (0-5)

  int rotation = -45 + 90*LH_module -60*cell;
  if(rotation<-180)rotation+=360;

  int crate=0; // (0-3) actual crates are ROCFDC1,4,13,14
  if(package<2){
    crate=0;
    if(rotation>0)crate=3;
  } else {
    crate=1;
    if(rotation>0)crate=2;
  }

  int slot=0; //(0-11) actual slots are 3-10,13-16
  if(rotation<0){
    if(cell==0){
      slot=0;
    } else if (cell==1) {
      slot=1+LH_module;
    } else if (cell==2) {
      slot=3+LH_module;
    } else {
      slot=5;
    }
  } else {
    if(cell==0){
      slot=0;
    } else if (cell==3) {
      slot=1;
    } else if (cell==4) {
      slot=2+LH_module;
    } else {
      slot=4+LH_module;
    }
  } 
  slot+=(package%2)*6;

  //string act_crate="ROCFDC1";
  act_crate="ROCFDC1";
  if(crate==1)act_crate="ROCFDC4";
  if(crate==2)act_crate="ROCFDC13";
  if(crate==3)act_crate="ROCFDC14";
  //int act_slot=slot+3;
  act_slot=slot+3;
  if(act_slot>10)act_slot+=2;

  //cout<<"        "<<act_crate<<endl;
  //cout<<" actual slot="<<act_slot<<endl;

  return crate*12+slot+1; //returns modules in crate/slot sequence (1-48)

}


//------------------
// JEventProcessor_HLDetectorTiming (Constructor)
//------------------
JEventProcessor_HLDetectorTiming::JEventProcessor_HLDetectorTiming()
{

}

//------------------
// ~JEventProcessor_HLDetectorTiming (Destructor)
//------------------
JEventProcessor_HLDetectorTiming::~JEventProcessor_HLDetectorTiming()
{

}

//------------------
// init
//------------------
jerror_t JEventProcessor_HLDetectorTiming::init(void)
{
    BEAM_CURRENT = 50; // Assume that there is beam until first EPICs event. Set from EPICS evio data, can override on command line

    fBeamEventCounter = 0;
    dMaxDIRCChannels = 108*64;

    REQUIRE_BEAM = 0;
    BEAM_EVENTS_TO_KEEP = 1000000000; // Set enormously high
    DO_ROUGH_TIMING = 0;
    DO_CDC_TIMING = 0;
    DO_TDC_ADC_ALIGN = 0;
    DO_TRACK_BASED = 0;
    DO_VERIFY = 1;
    DO_OPTIONAL = 0;
    DO_REACTION = 0;
    DO_HIGH_RESOLUTION = 0;

    USE_RF_BUNCH = 1;
    TRIGGER_MASK = 0;

    NO_TRACKS = false;
    NO_FIELD = true;
    CCAL_CALIB = false;
    STRAIGHT_TRACK = false;

    if(gPARMS){
        gPARMS->SetDefaultParameter("HLDETECTORTIMING:DO_ROUGH_TIMING", DO_ROUGH_TIMING, "Set to > 0 to do rough timing of all detectors");
        gPARMS->SetDefaultParameter("HLDETECTORTIMING:DO_CDC_TIMING", DO_CDC_TIMING, "Set to > 0 to do CDC Per channel Alignment");
        gPARMS->SetDefaultParameter("HLDETECTORTIMING:DO_TDC_ADC_ALIGN", DO_TDC_ADC_ALIGN, "Set to > 0 to do TDC/ADC alignment of SC,TOF,TAGM,TAGH");
        gPARMS->SetDefaultParameter("HLDETECTORTIMING:DO_TRACK_BASED", DO_TRACK_BASED, "Set to > 0 to do Track Based timing corrections");
        gPARMS->SetDefaultParameter("HLDETECTORTIMING:DO_HIGH_RESOLUTION", DO_HIGH_RESOLUTION, "Set to > 0 to increase the resolution of the track Based timing corrections");
        gPARMS->SetDefaultParameter("HLDETECTORTIMING:DO_VERIFY", DO_VERIFY, "Set to > 0 to verify timing with current constants");
        gPARMS->SetDefaultParameter("HLDETECTORTIMING:REQUIRE_BEAM", REQUIRE_BEAM, "Set to 0 to skip beam current check");
        gPARMS->SetDefaultParameter("HLDETECTORTIMING:BEAM_EVENTS_TO_KEEP", BEAM_EVENTS_TO_KEEP, "Set to the number of beam on events to use");
        gPARMS->SetDefaultParameter("HLDETECTORTIMING:DO_OPTIONAL", DO_OPTIONAL, "Set to >0 to enable optional histograms ");
        gPARMS->SetDefaultParameter("HLDETECTORTIMING:DO_REACTION", DO_REACTION, "Set to >0 to run DReaction");
        gPARMS->SetDefaultParameter("HLDETECTORTIMING:USE_RF_BUNCH", USE_RF_BUNCH, "Set to 0 to disable use of 2 vote RF Bunch");
        gPARMS->SetDefaultParameter("HLDETECTORTIMING:NO_TRACKS", NO_TRACKS, "Don't use tracking information for timing calibrations");
        gPARMS->SetDefaultParameter("HLDETECTORTIMING:CCAL_CALIB", CCAL_CALIB, "Perform CCAL calibrations");
        gPARMS->SetDefaultParameter("HLDETECTORTIMING:TRIGGER_MASK", TRIGGER_MASK, "Set to >0 to override use of standard physics trigger");
        gPARMS->SetDefaultParameter("HLDETECTORTIMING:STRAIGHT_TRACK", STRAIGHT_TRACK, "Set to >0 to change better for straight track data (field-off, drift chambers-on)");
    }

    // Would like the code with no arguments to simply verify the current status of the calibration
    if (DO_ROUGH_TIMING > 0 || DO_CDC_TIMING > 0 || DO_TDC_ADC_ALIGN > 0 || DO_TRACK_BASED > 0) DO_VERIFY = 0;

    // Increase range for initial search
    if(DO_TDC_ADC_ALIGN){
        NBINS_TDIFF = 2800; MIN_TDIFF = -150.0; MAX_TDIFF = 550.0;
    }
    else{
        NBINS_TDIFF = 200; MIN_TDIFF = -40.0; MAX_TDIFF = 40.0;
    }

    // DEBUG
    //NBINS_TDIFF = 2800; MIN_TDIFF = -200.0; MAX_TDIFF = 500.0;


    if (DO_TRACK_BASED){
        if (DO_HIGH_RESOLUTION) {
	    NBINS_TAGGER_TIME = 400; MIN_TAGGER_TIME = -20; MAX_TAGGER_TIME = 20;
	    NBINS_MATCHING = 1000; MIN_MATCHING_T = -10; MAX_MATCHING_T = 10;
	} else {
	    NBINS_TAGGER_TIME = 1600; MIN_TAGGER_TIME = -200; MAX_TAGGER_TIME = 400;
	    //NBINS_MATCHING = 1000; MIN_MATCHING_T = -100; MAX_MATCHING_T = 400;
	    NBINS_MATCHING = 800; MIN_MATCHING_T = -100; MAX_MATCHING_T = 100;
	}
    } else if (DO_VERIFY){
        NBINS_TAGGER_TIME = 200; MIN_TAGGER_TIME = -20; MAX_TAGGER_TIME = 20;
        NBINS_MATCHING = 1000; MIN_MATCHING_T = -10; MAX_MATCHING_T = 10;
    } else{
        NBINS_TAGGER_TIME = 100; MIN_TAGGER_TIME = -50; MAX_TAGGER_TIME = 50;
        NBINS_MATCHING = 100; MIN_MATCHING_T = -10; MAX_MATCHING_T = 10;
    }

    NBINS_RF_COMPARE = 200; MIN_RF_COMPARE = -2.2; MAX_RF_COMPARE = 2.2;

    return NOERROR;
}

//------------------
// brun
//------------------
jerror_t JEventProcessor_HLDetectorTiming::brun(JEventLoop *eventLoop, int32_t runnumber)
{
    // This is called whenever the run number changes
    DApplication* app = dynamic_cast<DApplication*>(eventLoop->GetJApplication());
    DGeometry* geom = app->GetDGeometry(runnumber);
    geom->GetTargetZ(Z_TARGET);

    return NOERROR;
}

//------------------
// evnt
//------------------
jerror_t JEventProcessor_HLDetectorTiming::evnt(JEventLoop *loop, uint64_t eventnumber)
{
    // select events with physics events, i.e., not LED and other front panel triggers
    vector<const DTPOLHit *> tpolHitVector;
    loop->Get(tpolHitVector);
    
    //if (tpolHitVector.size() > 0) cout<<tpolHitVector.size()<<endl;
    //Loop over TPOL hits. Not in physics event trigger.
    //If loop over below, size of tpolHitVector is always 0. 
    //Might want check for the specific trigger implemented.
    for (unsigned int j = 0; j < tpolHitVector.size(); j++){
        if (tpolHitVector[j]->w_samp1 > 160.0 || tpolHitVector[j]->pulse_peak < 60.0) continue;
        unsigned int NSECTORS = 32;
        unsigned int nsamples = tpolHitVector[j]->nsamples;
        Fill2DHistogram("HLDetectorTiming","TPOL","TPOL_time_per_sector",tpolHitVector[j]->sector,tpolHitVector[j]->t_proxy,"TPOL time vs. sector; Sector; Time [ns]",NSECTORS,0.5,NSECTORS+0.5,nsamples+25,0.0,4.0*nsamples+100);

        Fill1DHistogram("HLDetectorTiming","TPOL","TPOL_time",tpolHitVector[j]->t_proxy,"TPOL time; Time [ns]; Entries",nsamples+25,0.0,4.0*nsamples+100);
    }

   DApplication* app = dynamic_cast<DApplication*>(loop->GetJApplication());
   //   DGeometry* geom = app->GetDGeometry(loop->GetJEvent().GetRunNumber());
   // Check for magnetic field
   const DMagneticFieldMap *bfield=app->GetBfield(loop->GetJEvent().GetRunNumber());
   bool locIsNoFieldFlag = (dynamic_cast<const DMagneticFieldMapNoField*>(bfield) != NULL);

    const DTrigger* locTrigger = NULL; 
    loop->GetSingle(locTrigger); 
    
    // make sure no "special" front-panel trigger events are used (e.g. LED, random pulser...)
    if(locTrigger->Get_L1FrontPanelTriggerBits() != 0) 
      return NOERROR;

	// allow the user to select which trigger select events to use for calibrations
	if( TRIGGER_MASK > 0) {
	    if( !((locTrigger->Get_L1TriggerBits())&TRIGGER_MASK) )
        	return NOERROR;
	} else {
		// but default to the main physics trigger
    	if(!locTrigger->Get_IsPhysicsEvent())
	    	return NOERROR;
	}

    // Get the particleID object for each run
    vector<const DParticleID *> locParticleID_algos;
    loop->Get(locParticleID_algos);
    if(locParticleID_algos.size()<1){
        _DBG_<<"Unable to get a DParticleID object! NO PID will be done!"<<endl;
        return RESOURCE_UNAVAILABLE;
    }
    auto locParticleID = locParticleID_algos[0];

    // We want to be use some of the tools available in the RFTime factory 
    // Specifically steping the RF back to a chosen time
    vector<const DRFTime *> locRFTimes;
    loop->Get(locRFTimes);      // make sure brun() gets called for this factory!
    auto dRFTimeFactory = static_cast<DRFTime_factory*>(loop->GetFactory("DRFTime"));

    vector<const DDIRCGeometry*> locDIRCGeometryVec;
    loop->Get(locDIRCGeometryVec);
    // next line commented out to supress warning
    //    const DDIRCGeometry* locDIRCGeometry = locDIRCGeometryVec[0];

    // Initialize DIRC LUT
	const DDIRCLut* dDIRCLut = nullptr;
    loop->GetSingle(dDIRCLut);

    // Get the EPICs events and update beam current. Skip event if current too low (<10 nA).
    vector<const DEPICSvalue *> epicsValues;
    loop->Get(epicsValues);
    for(unsigned int j = 0; j < epicsValues.size(); j++){
        const DEPICSvalue *thisValue = epicsValues[j];
        if (strcmp((thisValue->name).c_str(), "IBCAD00CRCUR6") == 0){
            BEAM_CURRENT = thisValue->fval;
            Fill1DHistogram("HLDetectorTiming", "", "Beam Current",
                    BEAM_CURRENT,
                    "Beam Current; Beam Current [nA]; Entries",
                    100, 0, 200);
        }
        //cout << "EPICS Name " <<  (thisValue->name).c_str() << " Value " << thisValue->fval << endl;
    }
    // There is a caveat here when running multithreaded
    // Another thread might be the one to catch the EPICS event
    // and there is no way to reject events that may have come from earilier
    // Expect number of entries in histograms to vary slightly over the same file with many threads
    if (BEAM_CURRENT < 10.0) {
        Fill1DHistogram("HLDetectorTiming", "" , "Beam Events",
                0, "Beam On Events (0 = no beam, 1 = beam > 10nA)",
                2, -0.5, 1.5);
        if (REQUIRE_BEAM){
            return NOERROR; // Skip events where we can't verify the beam current
        }
    }
    else{
        Fill1DHistogram("HLDetectorTiming", "" , "Beam Events",
                1, "Beam On Events (0 = no beam, 1 = beam > 10nA)",
                2, -0.5, 1.5);
        fBeamEventCounter++;
    }

    if (fBeamEventCounter >= BEAM_EVENTS_TO_KEEP) { // Able to specify beam ON events instead of just events
        cout<< "Maximum number of Beam Events reached" << endl;
        japp->Quit();
        return NOERROR;
    }

    // Get the objects from the event loop
    vector<const DCDCHit *> cdcHitVector;
    vector<const DFDCHit *> fdcHitVector;
    vector<const DSCHit *> scHitVector;
    vector<const DBCALUnifiedHit *> bcalUnifiedHitVector;
    vector<const DTOFHit *> tofHitVector;
    vector<const DTOFPoint *> tofPointVector;
    vector<const DFCALHit *> fcalHitVector;
    vector<const DCCALHit *> ccalHitVector;
    vector<const DDIRCPmtHit *> dircPmtHitVector;
    vector<const DTAGMHit *> tagmHitVector;
    vector<const DTAGHHit *> taghHitVector;
    vector<const DPSHit *> psHitVector;
    vector<const DPSCHit *> pscHitVector;

    loop->Get(cdcHitVector);
    loop->Get(fdcHitVector);
    loop->Get(scHitVector);
    loop->Get(bcalUnifiedHitVector);
    loop->Get(tofHitVector);
    loop->Get(tofPointVector);
    loop->Get(fcalHitVector);
    if(CCAL_CALIB) {
      loop->Get(ccalHitVector);
    }
    loop->Get(dircPmtHitVector);
    loop->Get(psHitVector);
    loop->Get(pscHitVector);
    loop->Get(tagmHitVector, "Calib");
    loop->Get(taghHitVector, "Calib");

    // TTabUtilities object used for RF time conversion
    const DTTabUtilities* locTTabUtilities = NULL;
    loop->GetSingle(locTTabUtilities);

    unsigned int i = 0;
    int nBins = 2000;
    float xMin = -500, xMax = 1500;
    for (i = 0; i < cdcHitVector.size(); i++){
        Fill1DHistogram ("HLDetectorTiming", "CDC", "CDCHit time", cdcHitVector[i]->t, 
                "CDCHit time;t [ns];", nBins, xMin, xMax);
        if(DO_VERIFY || DO_CDC_TIMING){
            int nStraws = 3522;
            Fill2DHistogram("HLDetectorTiming", "CDC", "CDCHit time per Straw Raw", 
                    cdcHitVector[i]->t, GetCCDBIndexCDC(cdcHitVector[i]),
                    "Hit time for each CDC wire; t [ns]; CCDB Index",
                    750, -500, 1000, nStraws, 0.5, nStraws + 0.5);
        }
    }

    for (i = 0; i < fdcHitVector.size(); i++){
        if(fdcHitVector[i]->type == 0 ) {
            Fill1DHistogram ("HLDetectorTiming", "FDC", "FDCHit Wire time", fdcHitVector[i]->t,
                    "FDCHit Wire time;t [ns];", nBins, xMin, xMax);
	    // Keep track of module/crate level shifts
	    // two F1TDC modules per wire layer
	    int module = 2 * fdcHitVector[i]->gLayer - 1;  // layers start counting at 1
	    if(fdcHitVector[i]->element > 48)
		    module++;
	    Fill2DHistogram ("HLDetectorTiming", "FDC", "FDCHit Wire time vs. module",
			     module, fdcHitVector[i]->t,
			     "FDCHit Wire time; module/slot; t [ns];", 
			     48, 0.5, 48.5, 400, -200, 600);

        }
        else{
            Fill1DHistogram ("HLDetectorTiming", "FDC", "FDCHit Cathode time", fdcHitVector[i]->t,
                    "FDCHit Cathode time;t [ns];", nBins, xMin, xMax);
        }
    }

    for (i = 0; i < scHitVector.size(); i++){
        //if(!scHitVector[i]->has_fADC || !scHitVector[i]->has_TDC) continue;
        Fill1DHistogram ("HLDetectorTiming", "SC", "SCHit time", scHitVector[i]->t,
                "SCHit time;t [ns];", nBins, xMin, xMax);
    }

    for (i = 0; i < dircPmtHitVector.size(); i++){
        Fill1DHistogram ("HLDetectorTiming", "DIRC", "DIRCHit time", dircPmtHitVector[i]->t,
                "DIRCHit time;t [ns];", nBins, xMin, xMax);
                
        if(dircPmtHitVector[i]->ch < DDIRCPmtHit_factory::DIRC_MAX_CHANNELS) {
            Fill2DHistogram ("HLDetectorTiming", "DIRC", "DIRCHit North Per Channel TDC Hit Time",
                            dircPmtHitVector[i]->ch, dircPmtHitVector[i]->t,
                            "DIRCHit North Per Channel TDC Hit Time; channel ID; t_{TDC} [ns] ",
                            DDIRCPmtHit_factory::DIRC_MAX_CHANNELS, 0.5, 
                            (double)DDIRCPmtHit_factory::DIRC_MAX_CHANNELS+0.5, 500, -50, 150);
        } else {
            Fill2DHistogram ("HLDetectorTiming", "DIRC", "DIRCHit South Per Channel TDC Hit Time",
                            dircPmtHitVector[i]->ch-DDIRCPmtHit_factory::DIRC_MAX_CHANNELS, dircPmtHitVector[i]->t,
                            "DIRCHit South Per Channel TDC Hit Time; channel ID; t_{TDC} [ns] ",
                            DDIRCPmtHit_factory::DIRC_MAX_CHANNELS, 0.5, 
                            (double)DDIRCPmtHit_factory::DIRC_MAX_CHANNELS+0.5, 500, -50, 150);
        }
    }
    
    for (i = 0; i < bcalUnifiedHitVector.size(); i++){
        int the_cell = (bcalUnifiedHitVector[i]->module - 1) * 16 + (bcalUnifiedHitVector[i]->layer - 1) * 4 + bcalUnifiedHitVector[i]->sector;
        // There is one less layer of TDCs so the numbering relects this
        int the_tdc_cell = (bcalUnifiedHitVector[i]->module - 1) * 12 + (bcalUnifiedHitVector[i]->layer - 1) * 4 + bcalUnifiedHitVector[i]->sector;
        // Get the underlying associated objects
        const DBCALHit * thisADCHit;
        const DBCALTDCHit * thisTDCHit;
        bcalUnifiedHitVector[i]->GetSingle(thisADCHit);
        bcalUnifiedHitVector[i]->GetSingle(thisTDCHit);

        if (thisADCHit != NULL){ //This should never be NULL but might as well check
            Fill1DHistogram ("HLDetectorTiming", "BCAL", "BCALHit ADC time", thisADCHit->t,
                    "BCALHit ADC time; t_{ADC} [ns]; Entries", nBins, xMin, xMax);

            //if (DO_OPTIONAL){
                if (bcalUnifiedHitVector[i]->end == 0){
                    Fill2DHistogram ("HLDetectorTiming", "BCAL", "BCALHit Upstream Per Channel ADC Hit Time",
                            the_cell, thisADCHit->t,
                            "BCALHit Upstream Per Channel Hit Time; cellID; t_{ADC} [ns] ",
                            768, 0.5, 768.5, 250, -50, 50);
                }
                else{
                    Fill2DHistogram ("HLDetectorTiming", "BCAL", "BCALHit Downstream Per Channel ADC Hit Time",
                            the_cell, thisADCHit->t,
                            "BCALHit Downstream Per Channel Hit Time; cellID; t_{ADC} [ns] ",
                            768, 0.5, 768.5, 250, -50, 50);
                }
                //}
        }

        if (thisTDCHit != NULL){
            Fill1DHistogram ("HLDetectorTiming", "BCAL", "BCALHit TDC time", thisTDCHit->t,
                    "BCALHit TDC time; t_{TDC} [ns]; Entries", nBins, xMin, xMax);

            if (DO_OPTIONAL){
                if (bcalUnifiedHitVector[i]->end == 0){
                    Fill2DHistogram ("HLDetectorTiming", "BCAL", "BCALHit Upstream Per Channel TDC Hit Time",
                            the_tdc_cell, thisTDCHit->t,
                            "BCALHit Upstream Per Channel TDC Hit Time; cellID; t_{TDC} [ns] ",
                            576, 0.5, 576.5, 350, -50, 300);
                }
                else{
                    Fill2DHistogram ("HLDetectorTiming", "BCAL", "BCALHit Downstream Per Channel TDC Hit Time",
                            the_tdc_cell, thisTDCHit->t,
                            "BCALHit Downstream Per Channel TDC Hit Time; cellID; t_{TDC} [ns] ",
                            576, 0.5, 576.5, 350, -50, 300);
                }
            }
        }

        if (thisADCHit != NULL && thisTDCHit != NULL){
            if (bcalUnifiedHitVector[i]->end == 0){
                Fill2DHistogram ("HLDetectorTiming", "BCAL", "BCALHit Upstream Per Channel TDC-ADC Hit Time",
                        the_tdc_cell, thisTDCHit->t - thisADCHit->t,
                        "BCALHit Upstream Per Channel TDC-ADC Hit Time; cellID; t_{TDC} - t_{ADC} [ns] ",
                        576, 0.5, 576.5, NBINS_TDIFF, MIN_TDIFF, MAX_TDIFF);
            }
            else{
                Fill2DHistogram ("HLDetectorTiming", "BCAL", "BCALHit Downstream Per Channel TDC-ADC Hit Time",
                        the_tdc_cell, thisTDCHit->t - thisADCHit->t,
                        "BCALHit Downstream Per Channel TDC-ADC Hit Time; cellID; t_{TDC} - t_{ADC} [ns] ",
                        576, 0.5, 576.5, NBINS_TDIFF, MIN_TDIFF, MAX_TDIFF);
            }
        }
    }

    for (i = 0; i < tofHitVector.size(); i++){
        Fill1DHistogram ("HLDetectorTiming", "TOF", "TOFHit time", tofHitVector[i]->t,
                "TOFHit time;t [ns];", nBins, xMin, xMax);
    }

    for (i = 0; i < psHitVector.size(); i++){
	int nColumns = 145*2;
        Fill1DHistogram ("HLDetectorTiming", "PS", "PSHit time", psHitVector[i]->t, 
                "PSHit time;t [ns];", nBins, xMin, xMax);

	Fill2DHistogram("HLDetectorTiming", "PS", "PSHit time per Column", 
			psHitVector[i]->t, psHitVector[i]->column+psHitVector[i]->arm*nColumns/2, //GetCCDBIndexPS(psHitVector[i]),
			"Hit time for each PS column; t [ns]; CCDB Index",
			nBins, xMin, xMax, nColumns, 0.5, nColumns + 0.5);
    }

    // from FCAL_online:  find energy weighted average time for FCAL hits, useful as a t0
    double fcalHitETot = 0;
    double fcalHitEwtT = 0;
    for (i = 0; i < fcalHitVector.size(); i++){
        fcalHitETot += fcalHitVector[i]->E;
        fcalHitEwtT += fcalHitVector[i]->E * fcalHitVector[i]->t;
    }
    fcalHitEwtT /= fcalHitETot;
    
	// extract the FCAL Geometry
	vector<const DFCALGeometry*> fcalGeomVect;
	loop->Get( fcalGeomVect );
	if (fcalGeomVect.size() < 1){
        cout << "FCAL Geometry not available?" << endl;
        return OBJECT_NOT_AVAILABLE;
	}
	const DFCALGeometry& fcalGeom = *(fcalGeomVect[0]);

    for (i = 0; i < fcalHitVector.size(); i++){
        Fill1DHistogram ("HLDetectorTiming", "FCAL", "FCALHit time", fcalHitVector[i]->t,
                         "FCALHit time;t [ns];", nBins, xMin, xMax);
        
        Fill2DHistogram("HLDetectorTiming", "FCAL", "FCALHit Occupancy",
                        fcalHitVector[i]->row, fcalHitVector[i]->column, 
                        "FCAL Hit Occupancy; column; row",
                        61, -1.5, 59.5, 61, -1.5, 59.5);
        double locTime = ( fcalHitVector[i]->t - fcalHitEwtT )*k_to_nsec;
        //Fill2DHistogram("HLDetectorTiming", "FCAL", "FCALHit Local Time",
        Fill2DWeightedHistogram("HLDetectorTiming", "FCAL", "FCALHit Local Time",
                                fcalHitVector[i]->row, fcalHitVector[i]->column, locTime,
                                "FCAL Hit Local Time [ns]; column; row",
                                61, -1.5, 59.5, 61, -1.5, 59.5);
        
        if (DO_OPTIONAL){
            Fill2DHistogram("HLDetectorTiming", "FCAL", "FCALHit Per Channel Time",
                            fcalGeom.channel(fcalHitVector[i]->row, fcalHitVector[i]->column), fcalHitVector[i]->t,
                            "FCAL Per Channel Hit time; channel; t [ns]",
                            fcalGeom.numChannels(), 0.5, fcalGeom.numChannels() + 0.5, 250, -50, 50); 
        }
    }

    Fill1DHistogram ("HLDetectorTiming", "FCAL", "FCAL total energy", fcalHitETot,
                         "FCAL total energy;", 400, 0, 8000);

    if(CCAL_CALIB) {
      // Do the same thing for the CCAL as a start
      double ccalHitETot = 0;
      double ccalHitEwtT = 0;
      for (i = 0; i < ccalHitVector.size(); i++){
	ccalHitETot += ccalHitVector[i]->E;
	ccalHitEwtT += ccalHitVector[i]->E * ccalHitVector[i]->t;
      }
      ccalHitEwtT /= ccalHitETot;
    
      // extract the CCAL Geometry
      vector<const DCCALGeometry*> ccalGeomVect;
      loop->Get( ccalGeomVect );
      if (ccalGeomVect.size() < 1){
	cout << "CCAL Geometry not available?" << endl;
      } else {
        for (i = 0; i < ccalHitVector.size(); i++){
	  Fill1DHistogram ("HLDetectorTiming", "CCAL", "CCALHit time", ccalHitVector[i]->t,
			   "CCALHit time;t [ns];", nBins, xMin, xMax);
	  // next line commented out to suppress warning
	  //            const DCCALGeometry& ccalGeom = *(ccalGeomVect[0]);
	  for (i = 0; i < ccalHitVector.size(); i++) {
	    Fill2DHistogram("HLDetectorTiming", "CCAL", "CCALHit Occupancy",
			    ccalHitVector[i]->row, ccalHitVector[i]->column, 
			    "CCAL Hit Occupancy; column; row",
			    13, -1.5, 11.5, 13, -1.5, 11.5);
	    double locTime = ( ccalHitVector[i]->t - ccalHitEwtT )*k_to_nsec;
	    //Fill2DHistogram("HLDetectorTiming", "CCAL", "CCALHit Local Time",
	    Fill2DWeightedHistogram("HLDetectorTiming", "CCAL", "CCALHit Local Time",
				    ccalHitVector[i]->row, ccalHitVector[i]->column, locTime,
				    "CCAL Hit Local Time [ns]; column; row",
				    13, -1.5, 11.5, 13, -1.5, 11.5);
	  }
        }
      }
    }

    for (i = 0; i < tagmHitVector.size(); i++){
        Fill1DHistogram ("HLDetectorTiming", "TAGM", "TAGMHit time", tagmHitVector[i]->t,
                "TAGMHit time;t [ns];", nBins, xMin, xMax);
    }
    for (i = 0; i < taghHitVector.size(); i++){
        Fill1DHistogram ("HLDetectorTiming", "TAGH", "TAGHHit time", taghHitVector[i]->t,
                "TAGHHit time;t [ns];", nBins, xMin, xMax);
    }

    // The detectors with both TDCs and ADCs need these two to be aligned
    // These detectors are the SC,TAGM,TAGH,TOF,PSC

    // Break these histograms up into hits coming from the TDC and hits coming from the ADC
    for (i = 0; i < scHitVector.size(); i++){
        int nSCCounters = 30;
        const DSCHit *thisSCHit = scHitVector[i];
        if (thisSCHit->has_fADC && !thisSCHit->has_TDC){
            Fill1DHistogram ("HLDetectorTiming", "SC", "SCHit ADC time", scHitVector[i]->t,
                    "SCHit ADC only time;t [ns];", nBins, xMin, xMax);
            // Manual loop over hits to match out of time
            for (auto hit = scHitVector.begin(); hit != scHitVector.end(); hit++){
                if ((*hit)->has_TDC && !(*hit)->has_fADC){
                    if (scHitVector[i]->sector == (*hit)->sector){
                        Fill2DHistogram("HLDetectorTiming", "SC", "SCHit TDC_ADC Difference",
                                scHitVector[i]->sector, (*hit)->t_TDC - scHitVector[i]->t_fADC,
                                "SC #Deltat TDC-ADC; Sector ;t_{TDC} - t_{ADC} [ns]", nSCCounters, 0.5, nSCCounters + 0.5, NBINS_TDIFF, MIN_TDIFF, MAX_TDIFF);
                    }
                }
            }
        }
        else if (!thisSCHit->has_fADC && thisSCHit->has_TDC){
            Fill1DHistogram ("HLDetectorTiming", "SC", "SCHit TDC time", scHitVector[i]->t,
                    "SCHit TDC only time;t [ns];", nBins, xMin, xMax);
        }
        else{
            Fill1DHistogram ("HLDetectorTiming", "SC", "SCHit Matched time", scHitVector[i]->t,
                    "SCHit Matched ADC/TDC time;t [ns];", nBins, xMin, xMax);
            Fill1DHistogram ("HLDetectorTiming", "SC", "SCHit ADC time", scHitVector[i]->t_fADC,
                    "SCHit ADC only time;t [ns];", nBins, xMin, xMax);
            Fill1DHistogram ("HLDetectorTiming", "SC", "SCHit TDC time", scHitVector[i]->t_TDC,
                    "SCHit TDC only time;t [ns];", nBins, xMin, xMax);

            Fill2DHistogram("HLDetectorTiming", "SC", "SCHit TDC_ADC Difference",
                    scHitVector[i]->sector, scHitVector[i]->t_TDC - scHitVector[i]->t_fADC,
                    "SC #Deltat TDC-ADC; Sector ;t_{TDC} - t_{ADC} [ns]", nSCCounters, 0.5, nSCCounters + 0.5, NBINS_TDIFF, MIN_TDIFF, MAX_TDIFF);
            Fill2DHistogram("HLDetectorTiming", "SC", "SCHit Matched time per Counter",
                            scHitVector[i]->sector, scHitVector[i]->t,
                            "SCHit Matched ADC/TDC time; Sector ;t [ns]", nSCCounters, 0.5, nSCCounters + 0.5, 50, -50, 50);
        }

    }
    for (i = 0; i < tagmHitVector.size(); i++){

        const DTAGMHit *thisTAGMHit = tagmHitVector[i];
        int nTAGMCounters = 122; // 102 + 20 including 4 fully read out columns

        if(thisTAGMHit->has_fADC && !thisTAGMHit->has_TDC){
            Fill1DHistogram ("HLDetectorTiming", "TAGM", "TAGMHit ADC time", tagmHitVector[i]->t,
                    "TAGMHit ADC only time;t [ns];", nBins, xMin, xMax);
            // Manual loop over hits to match out of time
            for (auto hit = tagmHitVector.begin(); hit != tagmHitVector.end(); hit++){
                if ((*hit)->has_TDC && !(*hit)->has_fADC){
                    if (GetCCDBIndexTAGM(tagmHitVector[i]) == GetCCDBIndexTAGM(*hit)){
                        Fill2DHistogram("HLDetectorTiming", "TAGM", "TAGMHit TDC_ADC Difference",
                                GetCCDBIndexTAGM(tagmHitVector[i]), (*hit)->t - tagmHitVector[i]->time_fadc,
					//GetCCDBIndexTAGM(tagmHitVector[i]), (*hit)->time_tdc - tagmHitVector[i]->time_fadc,
                                "TAGM #Deltat TDC-ADC; Column ;t_{TDC} - t_{ADC} [ns]", nTAGMCounters, 0.5, nTAGMCounters + 0.5, NBINS_TDIFF, MIN_TDIFF, MAX_TDIFF);
                    }
                }
            }
        }
        else if (!thisTAGMHit->has_fADC && thisTAGMHit->has_TDC){
            Fill1DHistogram ("HLDetectorTiming", "TAGM", "TAGMHit TDC time", tagmHitVector[i]->t,
                    "TAGMHit TDC only time;t [ns];", nBins, xMin, xMax);
        }
        else{
            Fill1DHistogram ("HLDetectorTiming", "TAGM", "TAGMHit Matched time", tagmHitVector[i]->t,
                    "TAGMHit Matched ADC/TDC time;t [ns];", nBins, xMin, xMax);
            Fill1DHistogram ("HLDetectorTiming", "TAGM", "TAGMHit ADC time", tagmHitVector[i]->time_fadc,
                    "TAGMHit ADC only time;t [ns];", nBins, xMin, xMax);
            Fill1DHistogram ("HLDetectorTiming", "TAGM", "TAGMHit TDC time", tagmHitVector[i]->t,
                    "TAGMHit TDC only time;t [ns];", nBins, xMin, xMax);

            Fill2DHistogram("HLDetectorTiming", "TAGM", "TAGMHit TDC_ADC Difference",
                    GetCCDBIndexTAGM(tagmHitVector[i]), tagmHitVector[i]->t - tagmHitVector[i]->time_fadc,
                    "TAGM #Deltat TDC-ADC; Column ;t_{TDC} - t_{ADC} [ns]", nTAGMCounters, 0.5, nTAGMCounters + 0.5, NBINS_TDIFF, MIN_TDIFF, MAX_TDIFF);
            if (DO_OPTIONAL){
                Fill2DHistogram("HLDetectorTiming", "TAGM", "TAGM Per Channel TDC Time",
                        GetCCDBIndexTAGM(tagmHitVector[i]), tagmHitVector[i]->t,
                        "TAGM Per Channel TDC time; Column ;t_{TDC} [ns]", nTAGMCounters, 0.5, nTAGMCounters + 0.5, 100, -50, 50);
            }

        }

    }
    for (i = 0; i < taghHitVector.size(); i++){

        const DTAGHHit *thisTAGHHit = taghHitVector[i];
        int nTAGHCounters = 274;

        if(thisTAGHHit->has_fADC && !thisTAGHHit->has_TDC){
            Fill1DHistogram ("HLDetectorTiming", "TAGH", "TAGHHit ADC time", taghHitVector[i]->t,
                    "TAGHHit ADC only time;t [ns];", nBins, xMin, xMax);
            // Manual loop over hits to match out of time
            for (auto hit = taghHitVector.begin(); hit != taghHitVector.end(); hit++){
                if ((*hit)->has_TDC && !(*hit)->has_fADC){
                    if (taghHitVector[i]->counter_id == (*hit)->counter_id){
                        Fill2DHistogram("HLDetectorTiming", "TAGH", "TAGHHit TDC_ADC Difference",
                                taghHitVector[i]->counter_id, (*hit)->time_tdc - taghHitVector[i]->time_fadc,
                                "TAGH #Deltat TDC-ADC; Counter ID ;t_{TDC} - t_{ADC} [ns]", nTAGHCounters, 0.5, nTAGHCounters + 0.5, NBINS_TDIFF, MIN_TDIFF, MAX_TDIFF);
                    }
                }
            }
        }
        else if (!thisTAGHHit->has_fADC && thisTAGHHit->has_TDC){
            Fill1DHistogram ("HLDetectorTiming", "TAGH", "TAGHHit TDC time", taghHitVector[i]->t,
                    "TAGHHit TDC only time;t [ns];", nBins, xMin, xMax);
        }
        else{
            Fill1DHistogram ("HLDetectorTiming", "TAGH", "TAGHHit Matched time", taghHitVector[i]->t,
                    "TAGHHit Matched ADC/TDC time;t [ns];", nBins, xMin, xMax);
            Fill1DHistogram ("HLDetectorTiming", "TAGH", "TAGHHit ADC time", taghHitVector[i]->time_fadc,
                    "TAGHHit ADC only time;t [ns];", nBins, xMin, xMax);
            Fill1DHistogram ("HLDetectorTiming", "TAGH", "TAGHHit TDC time", taghHitVector[i]->time_tdc,
                    "TAGHHit TDC only time;t [ns];", nBins, xMin, xMax);

            // We want to look at the timewalk within these ADC/TDC detectors
            Fill2DHistogram("HLDetectorTiming", "TAGH", "TAGHHit TDC_ADC Difference",
                    taghHitVector[i]->counter_id, taghHitVector[i]->time_tdc - taghHitVector[i]->time_fadc,
                    "TAGH #Deltat TDC-ADC; Counter ID ;t_{TDC} - t_{ADC} [ns]", nTAGHCounters, 0.5, nTAGHCounters + 0.5, NBINS_TDIFF, MIN_TDIFF, MAX_TDIFF);
        }
    }
    for (i = 0; i < tofHitVector.size(); i++){
        const DTOFHit *thisTOFHit = tofHitVector[i];
        int nTOFCounters = 176;
        if(thisTOFHit->has_fADC && !thisTOFHit->has_TDC){
            Fill1DHistogram ("HLDetectorTiming", "TOF", "TOFHit ADC time", tofHitVector[i]->t,
                    "TOFHit ADC only time;t [ns];", nBins, xMin, xMax);
            // Manual loop over hits to match out of time
            for (auto hit = tofHitVector.begin(); hit != tofHitVector.end(); hit++){
                if ((*hit)->has_TDC && !(*hit)->has_fADC){
                    if (GetCCDBIndexTOF(tofHitVector[i]) == GetCCDBIndexTOF(*hit)){
                        Fill2DHistogram("HLDetectorTiming", "TOF", "TOFHit TDC_ADC Difference",
                                GetCCDBIndexTOF(tofHitVector[i]), (*hit)->t_TDC - tofHitVector[i]->t_fADC,
                                "TOF #Deltat TDC-ADC; CDCB Index ;t_{TDC} - t_{ADC} [ns]", nTOFCounters, 0.5, nTOFCounters + 0.5, NBINS_TDIFF, MIN_TDIFF, MAX_TDIFF);
                    }
                }
            }
        }
        else if (!thisTOFHit->has_fADC && thisTOFHit->has_TDC){
            Fill1DHistogram ("HLDetectorTiming", "TOF", "TOFHit TDC time", tofHitVector[i]->t,
                    "TOFHit TDC only time;t [ns];", nBins, xMin, xMax);
        }
        else{
            Fill1DHistogram ("HLDetectorTiming", "TOF", "TOFHit Matched time", tofHitVector[i]->t,
                    "TOFHit Matched ADC/TDC time;t [ns];", nBins, xMin, xMax);
            Fill1DHistogram ("HLDetectorTiming", "TOF", "TOFHit ADC time", tofHitVector[i]->t_fADC,
                    "TOFHit ADC only time;t [ns];", nBins, xMin, xMax);
            Fill1DHistogram ("HLDetectorTiming", "TOF", "TOFHit TDC time", tofHitVector[i]->t_TDC,
                    "TOFHit TDC only time;t [ns];", nBins, xMin, xMax);

            Fill2DHistogram("HLDetectorTiming", "TOF", "TOFHit TDC_ADC Difference",
                    GetCCDBIndexTOF(tofHitVector[i]), tofHitVector[i]->t_TDC - tofHitVector[i]->t_fADC,
                    "TOF #Deltat TDC-ADC; CDCB Index ;t_{TDC} - t_{ADC} [ns]", nTOFCounters, 0.5, nTOFCounters + 0.5, NBINS_TDIFF, MIN_TDIFF, MAX_TDIFF);
        }
    }
    for (i = 0; i < pscHitVector.size(); i++){
        int nPSCCounters = 16;
        const DPSCHit *thisPSCHit = pscHitVector[i];
        if (thisPSCHit->has_fADC && !thisPSCHit->has_TDC){
            Fill1DHistogram ("HLDetectorTiming", "PS", "PSCHit ADC time", pscHitVector[i]->t,
                             "PSCHit ADC only time;t [ns];", nBins, xMin, xMax);
            // Manual loop over hits to match out of time
            for (auto hit = pscHitVector.begin(); hit != pscHitVector.end(); hit++){
                if ((*hit)->has_TDC && !(*hit)->has_fADC){
                    if ( (pscHitVector[i]->arm == (*hit)->arm) && (pscHitVector[i]->module == (*hit)->module) ) {
                        Fill2DHistogram("HLDetectorTiming", "PS", "PSCHit TDC_ADC Difference",
                                        pscHitVector[i]->module+pscHitVector[i]->arm*nPSCCounters/2, (*hit)->time_tdc - pscHitVector[i]->time_fadc,
                                        "PSC #Deltat TDC-ADC; Sector ;t_{TDC} - t_{ADC} [ns]", nPSCCounters, 0.5, nPSCCounters + 0.5, NBINS_TDIFF, MIN_TDIFF, MAX_TDIFF);
                    }
                }
            }
        }
        else if (!thisPSCHit->has_fADC && thisPSCHit->has_TDC){
            Fill1DHistogram ("HLDetectorTiming", "PS", "PSCHit TDC time", pscHitVector[i]->t,
                             "PSCHit TDC only time;t [ns];", nBins, xMin, xMax);
        }
        else{
            Fill1DHistogram ("HLDetectorTiming", "PS", "PSCHit Matched time", pscHitVector[i]->t,
                             "PSCHit Matched ADC/TDC time;t [ns];", nBins, xMin, xMax);
            Fill1DHistogram ("HLDetectorTiming", "PS", "PSCHit ADC time", pscHitVector[i]->time_fadc,
                             "PSCHit ADC only time;t [ns];", nBins, xMin, xMax);
            Fill1DHistogram ("HLDetectorTiming", "PS", "PSCHit TDC time", pscHitVector[i]->time_tdc,
                             "PSCHit TDC only time;t [ns];", nBins, xMin, xMax);
            
            Fill2DHistogram("HLDetectorTiming", "PS", "PSCHit TDC_ADC Difference",
                            pscHitVector[i]->module+pscHitVector[i]->arm*nPSCCounters/2, pscHitVector[i]->time_tdc - pscHitVector[i]->time_fadc,
                            "PSC #Deltat TDC-ADC; Sector ;t_{TDC} - t_{ADC} [ns]", nPSCCounters, 0.5, nPSCCounters + 0.5, NBINS_TDIFF, MIN_TDIFF, MAX_TDIFF);
        }
    }

    // Next the relative times between detectors using tracking
    // By the time we get to this point, our first guess at the timing should be fairly good. 
    // Certainly good enough to take a pass at the time based tracking
    // This will be the final alignment step for now

    // We want to plot the delta t at the target between the SC hit and the tagger hits
    // Some limits for these
    float nBinsE = 160, EMin = 3.0, EMax = 12.0;


    const DEventRFBunch *thisRFBunch = NULL;
    
    if(NO_TRACKS) {
	    // If the drift chambers are turned off, we'll need to use the neutral showers
	    loop->GetSingle(thisRFBunch, "CalorimeterOnly");
    } else {
	    loop->GetSingle(thisRFBunch, "Calibrations"); // SC only hits
    }
    if (thisRFBunch->dNumParticleVotes < 2) return NOERROR;

    // Loop over TAGM hits
    for (unsigned int j = 0 ; j < tagmHitVector.size(); j++){
        int nTAGMColumns = 122; // Not really just columns, but a name is a name
        if(tagmHitVector[j]->has_fADC){
            char name [200];
            char title[500];
            sprintf(name, "Column %.3i Row %.1i", tagmHitVector[j]->column, tagmHitVector[j]->row);
            sprintf(title, "TAGM Column %i t_{ADC} - t_{RF}; t_{ADC} - t_{RF} [ns]; Entries", tagmHitVector[j]->column);
            double locShiftedTime = dRFTimeFactory->Step_TimeToNearInputTime(thisRFBunch->dTime, tagmHitVector[j]->time_fadc);
            Fill1DHistogram("HLDetectorTiming", "TAGM_ADC_RF_Compare", name,
                    tagmHitVector[j]->time_fadc - locShiftedTime,
                    title,
                    NBINS_RF_COMPARE, MIN_RF_COMPARE, MAX_RF_COMPARE);
        }

        if(tagmHitVector[j]->has_TDC){
            char name [200];
            char title[500];
            sprintf(name, "Column %.3i Row %.1i", tagmHitVector[j]->column, tagmHitVector[j]->row);
            sprintf(title, "TAGM Column %i t_{TDC} - t_{RF}; t_{TDC} - t_{RF} [ns]; Entries", tagmHitVector[j]->column);
            double locShiftedTime = dRFTimeFactory->Step_TimeToNearInputTime(thisRFBunch->dTime, tagmHitVector[j]->t);
            Fill1DHistogram("HLDetectorTiming", "TAGM_TDC_RF_Compare", name,
                    tagmHitVector[j]->t - locShiftedTime,
                    title,
                    NBINS_RF_COMPARE, MIN_RF_COMPARE, MAX_RF_COMPARE);
        }

        Fill2DHistogram("HLDetectorTiming", "TRACKING", "TAGM - RFBunch Time",
                GetCCDBIndexTAGM(tagmHitVector[j]), tagmHitVector[j]->t - thisRFBunch->dTime,
                "#Deltat TAGM-RFBunch; CCDB Index ;t_{TAGM} - t_{SC @ target} [ns]",
                nTAGMColumns, 0.5, nTAGMColumns + 0.5, NBINS_TAGGER_TIME,MIN_TAGGER_TIME,MAX_TAGGER_TIME);
        Fill2DHistogram("HLDetectorTiming", "TRACKING", "Tagger - RFBunch Time",
                tagmHitVector[j]->t - thisRFBunch->dTime, tagmHitVector[j]->E,
                "Tagger - RFBunch Time; #Deltat_{Tagger - SC} [ns]; Energy [GeV]",
                NBINS_TAGGER_TIME,MIN_TAGGER_TIME,MAX_TAGGER_TIME, nBinsE, EMin, EMax);
        Fill1DHistogram("HLDetectorTiming", "TRACKING", "Tagger - RFBunch 1D Time",
                tagmHitVector[j]->t - thisRFBunch->dTime,
                "Tagger - RFBunch Time; #Deltat_{Tagger - RFBunch} [ns]; Entries",
			//160, -20, 20);
                    800, -50, 50);
        if (tagmHitVector[j]->row == 0){
            Fill1DHistogram("HLDetectorTiming", "TRACKING", "TAGM - RFBunch 1D Time",
                    tagmHitVector[j]->t - thisRFBunch->dTime,
                    "TAGM - RFBunch Time; #Deltat_{TAGM - RFBunch} [ns]; Entries",
			    //480, -30, 30);
                    800, -50, 50);
        }
    }

    // Loop over TAGH hits
    for (unsigned int j = 0 ; j < taghHitVector.size(); j++){
        int nTAGHCounters = 274;

        if(taghHitVector[j]->has_fADC){
            char name [200];
            char title[500];
            sprintf(name, "Counter ID %.3i", taghHitVector[j]->counter_id);
            sprintf(title, "TAGH Counter ID %i t_{ADC} - t_{RF}; t_{ADC} - t_{RF} [ns]; Entries", taghHitVector[j]->counter_id);
            double locShiftedTime = dRFTimeFactory->Step_TimeToNearInputTime(thisRFBunch->dTime, taghHitVector[j]->time_fadc);
            Fill1DHistogram("HLDetectorTiming", "TAGH_ADC_RF_Compare", name,
                    taghHitVector[j]->time_fadc - locShiftedTime,
                    title,
                    NBINS_RF_COMPARE, MIN_RF_COMPARE, MAX_RF_COMPARE);
        }
        if(taghHitVector[j]->has_TDC){
            char name [200];
            char title[500];
            sprintf(name, "Counter ID %.3i", taghHitVector[j]->counter_id);
            sprintf(title, "TAGH Counter ID %i t_{TDC} - t_{RF}; t_{TDC} - t_{RF} [ns]; Entries", taghHitVector[j]->counter_id);
            double locShiftedTime = dRFTimeFactory->Step_TimeToNearInputTime(thisRFBunch->dTime, taghHitVector[j]->time_tdc);
            Fill1DHistogram("HLDetectorTiming", "TAGH_TDC_RF_Compare", name,
                    taghHitVector[j]->time_tdc - locShiftedTime,
                    title,
                    NBINS_RF_COMPARE, MIN_RF_COMPARE, MAX_RF_COMPARE);
        }

        Fill2DHistogram("HLDetectorTiming", "TRACKING", "TAGH - RFBunch Time",
                taghHitVector[j]->counter_id, taghHitVector[j]->t - thisRFBunch->dTime,
                "#Deltat TAGH-RFBunch; Counter ID ;t_{TAGH} - t_{RFBunch} [ns]",
                nTAGHCounters, 0.5, nTAGHCounters + 0.5, NBINS_TAGGER_TIME,MIN_TAGGER_TIME,MAX_TAGGER_TIME);

        Fill2DHistogram("HLDetectorTiming", "TRACKING", "Tagger - RFBunch Time",
                taghHitVector[j]->t - thisRFBunch->dTime, taghHitVector[j]->E,
                "Tagger - RFBunch Time; #Deltat_{Tagger - RFBunch} [ns]; Energy [GeV]",
                NBINS_TAGGER_TIME,MIN_TAGGER_TIME,MAX_TAGGER_TIME, nBinsE, EMin, EMax);

        Fill1DHistogram("HLDetectorTiming", "TRACKING", "Tagger - RFBunch 1D Time",
                taghHitVector[j]->t - thisRFBunch->dTime,
                "Tagger - RFBunch Time; #Deltat_{Tagger - RFBunch} [ns]; Entries",
                480, -30, 30);
    }

    // now loop over neutral showers to align calorimeters
    vector<const DNeutralShower *> neutralShowerVector;
    loop->Get(neutralShowerVector);
    
    DVector3 locTargetCenter(0.,0.,Z_TARGET);

    for (i = 0; i <  neutralShowerVector.size(); i++){
	    double locPathLength = (neutralShowerVector[i]->dSpacetimeVertex.Vect() - locTargetCenter).Mag();
	    double locDeltaT = neutralShowerVector[i]->dSpacetimeVertex.T() - locPathLength/29.9792458 - thisRFBunch->dTime;
	    
        //cout << locDeltaT << endl;

	    // to eliminate low-energy tails and other reconstruction problems, require minimum energies
	    //   E(FCAL) > 200 MeV,  E(BCAL) > 100 MeV
	    if(neutralShowerVector[i]->dDetectorSystem == SYS_FCAL) {
		    Fill2DHistogram("HLDetectorTiming", "TRACKING", "FCAL - RF Time vs. Energy (Neutral)",  neutralShowerVector[i]->dEnergy, locDeltaT,
				    "Shower Energy [GeV]; t_{FCAL} - t_{RF} at Target (Neutral); t_{FCAL} - t_{RF} [ns]; Entries",
				    100, 0., 10., NBINS_MATCHING, MIN_MATCHING_T, MAX_MATCHING_T);
		    if(neutralShowerVector[i]->dEnergy > 0.2) {
			    Fill1DHistogram("HLDetectorTiming", "TRACKING", "FCAL - RF Time (Neutral)",  locDeltaT,
					    "t_{FCAL} - t_{RF} at Target (Neutral); t_{FCAL} - t_{RF} [ns]; Entries",
					    NBINS_MATCHING, MIN_MATCHING_T, MAX_MATCHING_T);
		    }
		    
		    // if we're not using tracking, then align the TOF using hits matched between the TOF and FCAL
		    if(NO_TRACKS) {
		    	for( vector< const DTOFPoint* >::const_iterator tof = tofPointVector.begin(); 
					tof != tofPointVector.end(); tof++ ) {
					
					const DTOFPoint* tof_hit = *tof;
					
					// select double-ended hits
					if( tof_hit->dHorizontalBarStatus != 3 || tof_hit->dVerticalBarStatus != 3 )
						continue;
						
					double dx = tof_hit->pos.X() - neutralShowerVector[i]->dSpacetimeVertex.X();
					double dy = tof_hit->pos.Y() - neutralShowerVector[i]->dSpacetimeVertex.Y();
					
	    			double locTOFPathLength = (tof_hit->pos - locTargetCenter).Mag();
					double locTOFDeltaT = tof_hit->t - locTOFPathLength/29.9792458 - thisRFBunch->dTime;
					
					// match the hits
					if( ( fabs(dx - TOF_X_MEAN) < 2.*TOF_X_SIG ) && 
		    	    	( fabs(dy - TOF_Y_MEAN) < 2.*TOF_Y_SIG ) ) {
					   Fill1DHistogram("HLDetectorTiming", "TRACKING", "TOF - RF Time (No Tracks)",
							 locTOFDeltaT,
							 "t_{TOF} - t_{RF} at Target (No Tracks); t_{TOF} - t_{RF} at Target [ns]; Entries",
							 NBINS_MATCHING, MIN_MATCHING_T, MAX_MATCHING_T);
		    	    }

				}
		    } 
	    } else {
		    Fill2DHistogram("HLDetectorTiming", "TRACKING", "BCAL - RF Time vs. Energy (Neutral)",  neutralShowerVector[i]->dEnergy, locDeltaT,
				    "Shower Energy [GeV];t_{BCAL} - t_{RF} at Target (Neutral); t_{BCAL} - t_{RF} [ns]; Entries",
                            100, 0., 10., NBINS_MATCHING, MIN_MATCHING_T, MAX_MATCHING_T);
		    if(neutralShowerVector[i]->dEnergy > 0.1) {
			    Fill1DHistogram("HLDetectorTiming", "TRACKING", "BCAL - RF Time (Neutral)",  locDeltaT,
					    "t_{BCAL} - t_{RF} at Target (Neutral); t_{BCAL} - t_{RF} [ns]; Entries",
                                NBINS_MATCHING, MIN_MATCHING_T, MAX_MATCHING_T);
		    }
	    }
	    
    } // End of loop over neutral showers

    vector<const DCCALShower *> ccalShowerVector;
    loop->Get(ccalShowerVector);
    
    for (i = 0; i <  ccalShowerVector.size(); i++){
	    DVector3 locShowerPos(ccalShowerVector[i]->x, ccalShowerVector[i]->y, ccalShowerVector[i]->z);
	    //DVector3 locShowerPos(ccalShowerVector[i]->x, ccalShowerVector[i]->y, 1279.77);
	    double locPathLength = (locShowerPos - locTargetCenter).Mag();
	    double locDeltaT = ccalShowerVector[i]->time - locPathLength/29.9792458 - thisRFBunch->dTime;

	    Fill2DHistogram("HLDetectorTiming", "TRACKING", "CCAL - RF Time vs. Energy (Neutral)",  ccalShowerVector[i]->E, locDeltaT,
			    "Shower Energy [GeV];t_{CCAL} - t_{RF} at Target (Neutral); t_{CCAL} - t_{RF} [ns]; Entries",
			    100, 0., 10., 500, -20, 20);
	    
	    // to eliminate low-energy tails and other reconstruction problems, require minimum energies
	    if(ccalShowerVector[i]->E > 0.1) {
		    Fill1DHistogram("HLDetectorTiming", "TRACKING", "CCAL - RF Time (Neutral)",  locDeltaT,
				    "t_{CCAL} - t_{RF} at Target (Neutral); t_{CCAL} - t_{RF} [ns]; Entries",
				    2000, -50, 50);
		    //NBINS_MATCHING, MIN_MATCHING_T, MAX_MATCHING_T);
	    }
    }
    
    // we went this far just to align the tagger with the RF time, nothing else to do without tracks
    if(NO_TRACKS) 
	    return NOERROR;

    if (!DO_TRACK_BASED && !DO_VERIFY ) return NOERROR; // Before this stage we aren't really ready yet, so just return

    // Try using the detector matches
    // Loop over the charged tracks

    vector<const DChargedTrack *> chargedTrackVector;
    loop->Get(chargedTrackVector);

    for (i = 0; i < chargedTrackVector.size(); i++){

        const DChargedTrackHypothesis *pionHypothesis;

        // We only want negative particles to kick out protons
		if(!locIsNoFieldFlag) {
	        if (chargedTrackVector[i]->Get_Charge() > 0) continue;
	        pionHypothesis = chargedTrackVector[i]->Get_Hypothesis(PiMinus);
		} else {
	        pionHypothesis = chargedTrackVector[i]->Get_Hypothesis(PiPlus);
		}

        if (pionHypothesis == NULL) continue;

		auto locTrackTimeBased = pionHypothesis->Get_TrackTimeBased();
        double trackingFOM = TMath::Prob(locTrackTimeBased->chisq, locTrackTimeBased->Ndof);
        // Some quality cuts for the tracks we will use
        // Keep this minimal for now and investigate later
        //float trackingFOMCut = 0.01;
        //float trackingFOMCut =0.0027;
		float trackingFOMCut = 2.87E-7;
		float trackingNDFCut = 5;
		if(STRAIGHT_TRACK) {
        	trackingFOMCut = 1.E-10;
        	trackingNDFCut = 5;
		}
		
        if( trackingFOM < trackingFOMCut ) continue;
        if( locTrackTimeBased->Ndof < trackingNDFCut) continue;

        //////////////////////////////////////////
        // get best matches to SC/TOF/FCAL/BCAL //
        //////////////////////////////////////////
        auto locSCHitMatchParams       = pionHypothesis->Get_SCHitMatchParams();
        auto locTOFHitMatchParams      = pionHypothesis->Get_TOFHitMatchParams();
        auto locFCALShowerMatchParams  = pionHypothesis->Get_FCALShowerMatchParams();
        auto locBCALShowerMatchParams  = pionHypothesis->Get_BCALShowerMatchParams();

        // We will only use tracks matched to the start counter for our calibration since this will be our reference for t0
        if (locSCHitMatchParams == NULL) continue;

        // the idea will be to fix the SC time and reference the other PID detectors off of this

        // These "flightTime" corrected time are essentially that detector's estimate of the target time
        float targetCenterCorrection = ((pionHypothesis->position()).Z() - Z_TARGET) / SPEED_OF_LIGHT;
        float flightTimeCorrectedSCTime = locSCHitMatchParams->dHitTime - locSCHitMatchParams->dFlightTime - targetCenterCorrection; 
        char name [200];
        char title[500];
        sprintf(name, "Sector %.2i", locSCHitMatchParams->dSCHit->sector);
        sprintf(title, "SC Sector %i t_{Target} - t_{RF}; t_{Target} - t_{RF} [ns]; Entries", locSCHitMatchParams->dSCHit->sector);
        double locShiftedTime = dRFTimeFactory->Step_TimeToNearInputTime(thisRFBunch->dTime, flightTimeCorrectedSCTime);
		double locSCDeltaT = flightTimeCorrectedSCTime - thisRFBunch->dTime;
        Fill1DHistogram("HLDetectorTiming", "SC_Target_RF_Compare_all", name,
			flightTimeCorrectedSCTime - locShiftedTime,
			title,
			NBINS_RF_COMPARE, MIN_RF_COMPARE, MAX_RF_COMPARE);
		Fill1DHistogram("HLDetectorTiming", "TRACKING", "SC - RF Time (all)",
			flightTimeCorrectedSCTime - thisRFBunch->dTime,
			"t_{SC} - t_{RF} at Target; t_{SC} - t_{RF} at Target [ns]; Entries",
			NBINS_MATCHING, MIN_MATCHING_T, MAX_MATCHING_T);

		// Stay away from the nose section, since the propagation time corrections are not stable there.
		// cut corresponds to ~50 cm path length through the SC - not too far into the nose section
		// but enough to get some statistics
	
		// need to get the projected hit position at the SC in order to cut on it
		DVector3 IntersectionPoint, IntersectionMomentum;	
		vector<DTrackFitter::Extrapolation_t> extrapolations = locTrackTimeBased->extrapolations.at(SYS_START);
		shared_ptr<DSCHitMatchParams> locSCHitMatchParams2;
		// comment out definition of sc_match_pid to suppress warning
		//		bool sc_match_pid = locParticleID->Cut_MatchDistance(extrapolations, locSCHitMatchParams->dSCHit, locSCHitMatchParams->dSCHit->t, locSCHitMatchParams2, 
		//								   true, &IntersectionPoint, &IntersectionMomentum);
		double locSCzIntersection = IntersectionPoint.z();
		if( locSCzIntersection < 83. ) {
			Fill1DHistogram("HLDetectorTiming", "SC_Target_RF_Compare", name,
					flightTimeCorrectedSCTime - locShiftedTime,
					title,
					NBINS_RF_COMPARE, MIN_RF_COMPARE, MAX_RF_COMPARE);
			Fill1DHistogram("HLDetectorTiming", "TRACKING", "SC - RF Time",
					flightTimeCorrectedSCTime - thisRFBunch->dTime,
					"t_{SC} - t_{RF} at Target; t_{SC} - t_{RF} at Target [ns]; Entries",
					NBINS_MATCHING, MIN_MATCHING_T, MAX_MATCHING_T);
			Fill2DHistogram("HLDetectorTiming", "TRACKING", "SC - RF Time vs. Sector",
					locSCHitMatchParams->dSCHit->sector, locSCDeltaT,
					"t_{SC} - t_{RF} at Target; Sector; t_{SC} - t_{RF} at Target [ns];",
					30, 0.5, 30.5, 800, -20., 20.);
            }

        // Get the pulls vector from the track
		auto thisTimeBasedTrack = pionHypothesis->Get_TrackTimeBased();

        vector<DTrackFitter::pull_t> pulls = thisTimeBasedTrack->pulls;
        double earliestCDCTime = 10000.;
        double earliestFDCTime = 10000.;
        for (size_t iPull = 0; iPull < pulls.size(); iPull++){
            if ( pulls[iPull].cdc_hit != nullptr && pulls[iPull].tdrift < earliestCDCTime) earliestCDCTime = pulls[iPull].tdrift;
            if ( pulls[iPull].fdc_hit != nullptr && pulls[iPull].tdrift < earliestFDCTime) earliestFDCTime = pulls[iPull].tdrift;
         }

        // Do this the old way for the CDC
        vector < const DCDCTrackHit *> cdcTrackHitVector;
        pionHypothesis->Get_TrackTimeBased()->Get(cdcTrackHitVector);
        if (cdcTrackHitVector.size() != 0){
           float earliestTime = 10000; // Initialize high
           for (unsigned int iCDC = 0; iCDC < cdcTrackHitVector.size(); iCDC++){
              if (cdcTrackHitVector[iCDC]->tdrift < earliestTime) earliestTime = cdcTrackHitVector[iCDC]->tdrift;
           }

           Fill1DHistogram("HLDetectorTiming", "TRACKING", "Earliest CDC Time Minus Matched SC Time",
                 earliestTime - locSCHitMatchParams->dHitTime,
                 "Earliest CDC Time Minus Matched SC Time; t_{CDC} - t_{SC} [ns];",
                 400, -50, 150);

        }

        // Loop over TAGM hits
        for (unsigned int j = 0 ; j < tagmHitVector.size(); j++){
           int nTAGMColumns = 122;
           // We want to look at the timewalk within these ADC/TDC detectors

           Fill2DHistogram("HLDetectorTiming", "TRACKING", "TAGM - SC Target Time",
                 GetCCDBIndexTAGM(tagmHitVector[j]), tagmHitVector[j]->t - flightTimeCorrectedSCTime,
                 "#Deltat TAGM-SC; Column ;t_{TAGM} - t_{SC @ target} [ns]", nTAGMColumns, 0.5, nTAGMColumns + 0.5, NBINS_TAGGER_TIME,MIN_TAGGER_TIME,MAX_TAGGER_TIME);

           Fill2DHistogram("HLDetectorTiming", "TRACKING", "Tagger - SC Target Time",
                 tagmHitVector[j]->t - flightTimeCorrectedSCTime, tagmHitVector[j]->E,
                 "Tagger - SC Target Time; #Deltat_{Tagger - SC} [ns]; Energy [GeV]",
                 NBINS_TAGGER_TIME,MIN_TAGGER_TIME,MAX_TAGGER_TIME, nBinsE, EMin, EMax);   


           Fill1DHistogram("HLDetectorTiming", "TRACKING", "Tagger - SC 1D Target Time",
                 tagmHitVector[j]->t - flightTimeCorrectedSCTime,
                 "Tagger - SC Time at Target; #Deltat_{Tagger - SC} [ns]; Entries",
                 160, -20, 20);
        }
        // Loop over TAGH hits
        for (unsigned int j = 0 ; j < taghHitVector.size(); j++){
           int nTAGHCounters = 274;
           Fill2DHistogram("HLDetectorTiming", "TRACKING", "TAGH - SC Target Time",
                 taghHitVector[j]->counter_id, taghHitVector[j]->t - flightTimeCorrectedSCTime,
                 "#Deltat TAGH-SC; Counter ID ;t_{TAGH} - t_{SC @ target} [ns]", nTAGHCounters, 0.5, nTAGHCounters + 0.5, NBINS_TAGGER_TIME,MIN_TAGGER_TIME,MAX_TAGGER_TIME);

           Fill2DHistogram("HLDetectorTiming", "TRACKING", "Tagger - SC Target Time",
                 taghHitVector[j]->t - flightTimeCorrectedSCTime, taghHitVector[j]->E,
                 "Tagger - SC Target Time; #Deltat_{Tagger - SC} [ns]; Energy [GeV]",
                 NBINS_TAGGER_TIME,MIN_TAGGER_TIME,MAX_TAGGER_TIME, nBinsE, EMin, EMax);

           Fill1DHistogram("HLDetectorTiming", "TRACKING", "Tagger - SC 1D Target Time",
                 taghHitVector[j]->t - flightTimeCorrectedSCTime,
                 "Tagger - SC Time at Target; #Deltat_{Tagger - SC} [ns]; Entries",
                 160, -20, 20);
        }


        if (locTOFHitMatchParams != NULL){
           // Now check the TOF matching. Do this on a full detector level.
           float flightTimeCorrectedTOFTime = locTOFHitMatchParams->dHitTime - locTOFHitMatchParams->dFlightTime - targetCenterCorrection;

           Fill1DHistogram("HLDetectorTiming", "TRACKING", "TOF - SC Target Time",
                 flightTimeCorrectedTOFTime - flightTimeCorrectedSCTime,
                 "t_{TOF} - t_{SC} at Target; t_{TOF} - t_{SC} at Target [ns]; Entries",
                 NBINS_MATCHING, MIN_MATCHING_T, MAX_MATCHING_T);
 
          Fill1DHistogram("HLDetectorTiming", "TRACKING", "TOF - RF Time",
                 flightTimeCorrectedTOFTime - thisRFBunch->dTime,
                 "t_{TOF} - t_{RF} at Target; t_{TOF} - t_{RF} at Target [ns]; Entries",
                 NBINS_MATCHING, MIN_MATCHING_T, MAX_MATCHING_T);
           // Fill the following when there is a SC/TOF match
           Fill1DHistogram("HLDetectorTiming", "TRACKING", "Earliest Flight-time Corrected FDC Time",
                 earliestFDCTime,
                 "Earliest Flight-time corrected FDC Time; t_{FDC} [ns];",
                 200, -50, 150);
                 
		    // get DIRC match parameters (contains LUT information)
			const DDetectorMatches* locDetectorMatches = NULL;
			loop->GetSingle(locDetectorMatches);
			DDetectorMatches &locDetectorMatch = (DDetectorMatches&)locDetectorMatches[0];
		    shared_ptr<const DDIRCMatchParams> locDIRCMatchParams;
		    bool foundDIRC = locParticleID->Get_DIRCMatchParams(locTrackTimeBased, locDetectorMatches, locDIRCMatchParams);

        	// For DIRC calibrations, select tracks which have a good TOF match
		    if(foundDIRC && locTOFHitMatchParams->dDeltaXToHit < 10.0 && locTOFHitMatchParams->dDeltaYToHit < 10.0) {

				// Get match parameters
				TVector3 posInBar = locDIRCMatchParams->dExtrapolatedPos; 
				TVector3 momInBar = locDIRCMatchParams->dExtrapolatedMom;
				// next line commented out to suppress warning
				//				double locExpectedThetaC = locDIRCMatchParams->dExpectedThetaC;
				double locExtrapolatedTime = locDIRCMatchParams->dExtrapolatedTime;
				// next line commented out to suppress warning
				//				int locBar = locDIRCGeometry->GetBar(posInBar.Y());

				Particle_t locPID = locTrackTimeBased->PID();
				double locMass = ParticleMass(locPID);
				double locAngle = dDIRCLut->CalcAngle(momInBar.Mag(), locMass);
				map<Particle_t, double> locExpectedAngle = dDIRCLut->CalcExpectedAngles(momInBar.Mag());

				// get map of DIRCMatches to PMT hits
				map<shared_ptr<const DDIRCMatchParams>, vector<const DDIRCPmtHit*> > locDIRCTrackMatchParamsMap;
				locDetectorMatch.Get_DIRCTrackMatchParamsMap(locDIRCTrackMatchParamsMap);
				map<Particle_t, double> logLikelihoodSum;

		  		// loop over associated hits for LUT diagnostic plots
		  		for(uint loc_i=0; loc_i<dircPmtHitVector.size(); loc_i++) {
				        bool locIsReflected = false;
			  		vector<pair<double, double>> locDIRCPhotons = dDIRCLut->CalcPhoton(dircPmtHitVector[loc_i], locExtrapolatedTime, posInBar, momInBar, locExpectedAngle, locAngle, locPID, locIsReflected, logLikelihoodSum);
			  		double locHitTime = dircPmtHitVector[loc_i]->t - locExtrapolatedTime;
			  		int locChannel = dircPmtHitVector[loc_i]->ch%dMaxDIRCChannels;

			  		if(locDIRCPhotons.size() > 0) {
						// loop over candidate photons
						for(uint loc_j = 0; loc_j<locDIRCPhotons.size(); loc_j++) {
							double locDeltaT = locDIRCPhotons[loc_j].first - locHitTime;
		
							if(locChannel < DDIRCPmtHit_factory::DIRC_MAX_CHANNELS) {
								Fill2DHistogram ("HLDetectorTiming", "DIRC", "DIRCHit North Per Channel t_{DIRC} - t_{track}",
												locChannel, locDeltaT,
												"DIRCHit North Per Channel t_{DIRC} - t_{track}; channel ID; t_{DIRC} - t_{track} [ns] ",
												DDIRCPmtHit_factory::DIRC_MAX_CHANNELS, 0.5, 
												(double)DDIRCPmtHit_factory::DIRC_MAX_CHANNELS+0.5, 400, -50, 50);
							} else {
								Fill2DHistogram ("HLDetectorTiming", "DIRC", "DIRCHit South Per Channel t_{DIRC} - t_{track}",
												locChannel-DDIRCPmtHit_factory::DIRC_MAX_CHANNELS, locDeltaT,
												"DIRCHit South Per Channel t_{DIRC} - t_{track}; channel ID; t_{DIRC} - t_{track} [ns] ",
												DDIRCPmtHit_factory::DIRC_MAX_CHANNELS, 0.5, 
                                                (double)DDIRCPmtHit_factory::DIRC_MAX_CHANNELS+0.5, 400, -50, 50);
							}
						}
					}
				}
			}
		}
        if (locBCALShowerMatchParams != NULL){
           float flightTimeCorrectedBCALTime = locBCALShowerMatchParams->dBCALShower->t - locBCALShowerMatchParams->dFlightTime - targetCenterCorrection;

           Fill1DHistogram("HLDetectorTiming", "TRACKING", "BCAL - SC Target Time",
                 flightTimeCorrectedBCALTime - flightTimeCorrectedSCTime,
                 "t_{BCAL} - t_{SC} at Target; t_{BCAL} - t_{SC} [ns]; Entries",
                 NBINS_MATCHING, MIN_MATCHING_T, MAX_MATCHING_T);

           Fill1DHistogram("HLDetectorTiming", "TRACKING", "BCAL - RF Time",
                 flightTimeCorrectedBCALTime - thisRFBunch->dTime,
                 "t_{BCAL} - t_{RF} at Target; t_{BCAL} - t_{RF} [ns]; Entries",
                 NBINS_MATCHING, MIN_MATCHING_T, MAX_MATCHING_T);


           // Add histogram suggested by Mark Dalton
           Fill2DHistogram("HLDetectorTiming", "TRACKING", "BCAL - SC Target Time Vs Correction",
                 locBCALShowerMatchParams->dFlightTime, flightTimeCorrectedBCALTime - flightTimeCorrectedSCTime,
                 "t_{BCAL} - t_{SC} at Target; Flight time [ns]; t_{BCAL} - t_{SC} [ns]",
                 100, 0, 20, 50, -10, 10);

           // Fill the following when there is a SC/BCAL match.
           Fill1DHistogram("HLDetectorTiming", "TRACKING", "Earliest Flight-time Corrected CDC Time",
                 earliestCDCTime,
                 "Earliest Flight-time Corrected CDC Time; t_{CDC} [ns];",
                 200, -50, 150);
        }
        if (locFCALShowerMatchParams != NULL){
           float flightTimeCorrectedFCALTime = locFCALShowerMatchParams->dFCALShower->getTime() - locFCALShowerMatchParams->dFlightTime - targetCenterCorrection;

           Fill1DHistogram("HLDetectorTiming", "TRACKING", "FCAL - SC Target Time",
                 flightTimeCorrectedFCALTime - flightTimeCorrectedSCTime,
                 "t_{FCAL} - t_{SC} at Target; t_{FCAL} - t_{SC} [ns]; Entries",
                 NBINS_MATCHING, MIN_MATCHING_T, MAX_MATCHING_T);

           Fill1DHistogram("HLDetectorTiming", "TRACKING", "FCAL - RF Time",
                 flightTimeCorrectedFCALTime - thisRFBunch->dTime,
                 "t_{FCAL} - t_{RF} at Target; t_{FCAL} - t_{RF} [ns]; Entries",
                 NBINS_MATCHING, MIN_MATCHING_T, MAX_MATCHING_T);
        }

    } // End of loop over time based tracks

    if (DO_REACTION){
       // Trigger the analysis
       vector<const DAnalysisResults*> locAnalysisResultsVector;
       loop->Get(locAnalysisResultsVector);
       // Get the time from the results and fill histograms
       deque<const DParticleCombo*> locPassedParticleCombos;
       locAnalysisResultsVector[0]->Get_PassedParticleCombos(locPassedParticleCombos);

       for (i=0; i < locPassedParticleCombos.size(); i++){
          double taggerTime = locPassedParticleCombos[i]->Get_ParticleComboStep(0)->Get_InitialParticle_Measured()->time();
          //cout << "We have a tagger time of " << taggerTime << endl;
          //Find matching hit by time
          for (unsigned int j = 0 ; j < tagmHitVector.size(); j++){
             if (taggerTime == tagmHitVector[j]->t){
                int nTAGMColumns = 122;
                // We want to look at the timewalk within these ADC/TDC detectors
                Fill2DHistogram("HLDetectorTiming", "TRACKING", "TAGM - RFBunch Time p2pi",
                      GetCCDBIndexTAGM(tagmHitVector[j]), tagmHitVector[j]->t - thisRFBunch->dTime,
                      "#Deltat TAGM-RFBunch; Column ;t_{TAGM} - t_{SC @ target} [ns]",
                      nTAGMColumns, 0.5, nTAGMColumns + 0.5, NBINS_TAGGER_TIME,MIN_TAGGER_TIME,MAX_TAGGER_TIME);
                Fill2DHistogram("HLDetectorTiming", "TRACKING", "Tagger - RFBunch Time p2pi",
                      tagmHitVector[j]->t - thisRFBunch->dTime, tagmHitVector[j]->E,
                      "Tagger - RFBunch Time; #Deltat_{Tagger - SC} [ns]; Energy [GeV]",
                      NBINS_TAGGER_TIME,MIN_TAGGER_TIME,MAX_TAGGER_TIME, nBinsE, EMin, EMax);
                Fill1DHistogram("HLDetectorTiming", "TRACKING", "Tagger - RFBunch 1D Time p2pi",
                      tagmHitVector[j]->t - thisRFBunch->dTime,
                      "Tagger - RFBunch Time; #Deltat_{Tagger - RFBunch} [ns]; Entries",
                      160, -20, 20);
             }
          }
          // Loop over TAGH hits
          for (unsigned int j = 0 ; j < taghHitVector.size(); j++){
             if (taggerTime == taghHitVector[j]->t){
                int nTAGHCounters = 274;
                Fill2DHistogram("HLDetectorTiming", "TRACKING", "TAGH -  RFBunch Time p2pi",
                      taghHitVector[j]->counter_id, taghHitVector[j]->t - thisRFBunch->dTime,
                      "#Deltat TAGH-RFBunch; Counter ID ;t_{TAGH} - t_{RFBunch} [ns]",
                      nTAGHCounters, 0.5, nTAGHCounters + 0.5, NBINS_TAGGER_TIME,MIN_TAGGER_TIME,MAX_TAGGER_TIME);

                Fill2DHistogram("HLDetectorTiming", "TRACKING", "Tagger - RFBunch Time p2pi",
                      taghHitVector[j]->t - thisRFBunch->dTime, taghHitVector[j]->E,
                      "Tagger - RFBunch Time; #Deltat_{Tagger - RFBunch} [ns]; Energy [GeV]",
                      NBINS_TAGGER_TIME,MIN_TAGGER_TIME,MAX_TAGGER_TIME, nBinsE, EMin, EMax);

                Fill1DHistogram("HLDetectorTiming", "TRACKING", "Tagger - RFBunch 1D Time p2pi",
                                taghHitVector[j]->t - thisRFBunch->dTime,
                                "Tagger - RFBunch Time; #Deltat_{Tagger - RFBunch} [ns]; Entries",
                                160, -20, 20);
             }
          }
       }
    }
    
    return NOERROR;
}


//------------------
// erun
//------------------
jerror_t JEventProcessor_HLDetectorTiming::erun(void)
{
   // This is called whenever the run number changes, before it is
   // changed to give you a chance to clean up before processing
   // events from the next run number.

  // set some histogram properties


  TH2I *fdc_time_module_hist = (TH2I*)gDirectory->Get("HLDetectorTiming/FDC/FDCHit Wire time vs. module");
  if(fdc_time_module_hist != NULL) {
    string act_crate;
    int act_slot;
    for(int ibin=1; ibin<=48; ibin++){
      int mod = Get_FDCTDC_crate_slot(ibin, act_crate, act_slot);
      if (mod) {} // gratuitious check of return value to suppress warning
      stringstream ss;
      ss << act_crate << "/" << act_slot;
      fdc_time_module_hist->GetXaxis()->SetBinLabel(ibin, ss.str().c_str());
    }
    fdc_time_module_hist->LabelsOption("v");
  }
  

   return NOERROR;
}

//------------------
// fini
//------------------
jerror_t JEventProcessor_HLDetectorTiming::fini(void)
{
   // Called before program exit after event processing is finished.
   //Here is where we do the fits to the data to see if we have a reasonable alignment
   SortDirectories(); //Defined in HistogramTools.h

   return NOERROR;
}

int JEventProcessor_HLDetectorTiming::GetCCDBIndexTOF(const DTOFHit *thisHit){
   // Returns the CCDB index of a particular hit
   // This 
   int plane = thisHit->plane;
   int bar = thisHit->bar;
   int end = thisHit->end;
   // 44 bars per plane
   int CCDBIndex = plane * 88 + end * 44 + bar; 
   return CCDBIndex;
}

int JEventProcessor_HLDetectorTiming::GetCCDBIndexBCAL(const DBCALHit *thisHit){
   return 0;
}

int JEventProcessor_HLDetectorTiming::GetCCDBIndexTAGM(const DTAGMHit *thisHit){
   // Since there are a few counters where each row is read out seperately this is a bit of a mess
   int row = thisHit->row;
   int column = thisHit->column;

   int CCDBIndex = column + row;
   if (column > 9) CCDBIndex += 5;
   if (column > 27) CCDBIndex += 5;
   if (column > 81) CCDBIndex += 5;
   if (column > 99) CCDBIndex += 5;

   return CCDBIndex;
}

int JEventProcessor_HLDetectorTiming::GetCCDBIndexCDC(const DCDCHit *thisHit){

   int ring = thisHit->ring;
   int straw = thisHit->straw;

   int CCDBIndex = GetCCDBIndexCDC(ring, straw);
   return CCDBIndex;
}

int JEventProcessor_HLDetectorTiming::GetCCDBIndexCDC(int ring, int straw){

   //int Nstraws[28] = {42, 42, 54, 54, 66, 66, 80, 80, 93, 93, 106, 106, 123, 123, 135, 135, 146, 146, 158, 158, 170, 170, 182, 182, 197, 197, 209, 209};
   int StartIndex[28] = {0, 42, 84, 138, 192, 258, 324, 404, 484, 577, 670, 776, 882, 1005, 1128, 1263, 1398, 1544, 1690, 1848, 2006, 2176, 2346, 2528, 2710, 2907, 3104, 3313};

   int CCDBIndex = StartIndex[ring - 1] + straw;
   return CCDBIndex;
}

