#include <stdint.h>
#include <vector>

#include "JEventProcessor_TRD_online.h"
#include <JANA/JApplication.h>

using namespace std;
using namespace jana;

#include <DANA/DApplication.h>

#include <TRIGGER/DL1Trigger.h>
#include <TRD/DTRDDigiHit.h>
#include <TRD/DTRDHit.h>
#include <TRD/DTRDStripCluster.h>
#include <TRD/DTRDPoint.h>

#include <DAQ/Df125FDCPulse.h>
#include <DAQ/DGEMSRSWindowRawData.h>
#include <TRD/DGEMDigiWindowRawData.h>
#include <TRD/DGEMHit.h>
#include <TRD/DGEMStripCluster.h>
#include <TRD/DGEMPoint.h>

#include <TDirectory.h>
#include <TH1.h>
#include <TH2.h>
#include <TProfile.h>

// root hist pointers
const int NTRDplanes = 5;
const int NTRDstrips = 240;
const int NTRDwires = 24;
const int NGEMplanes = 10;
const int NGEMstrips = 256;
const int NGEMpads = 96;
const int NAPVchannels = 128;
const int NGEMsamples = 21;

static TH1I *trd_num_events;
static TH1I *hDigiHit_NHits;
static TH1I *hDigiHit_QualityFactor[NTRDplanes];
static TH1I *hDigiHit_Occupancy[NTRDplanes];
static TH2I *hDigiHit_PeakVsStrip[NTRDplanes];
static TH1I *hDigiHit_PulseTime[NTRDplanes];
static TH1I *hDigiHit_Time[NTRDplanes];
static TH2I *hDigiHit_TimeVsStrip[NTRDplanes];
static TH2I *hDigiHit_TimeVsPeak[NTRDplanes];

static TH1I *hGEMHit_NHits;
static TH1I *hGEMHit_Occupancy[NGEMplanes];
static TH2I *hGEMHit_SampleVsStrip[NGEMplanes];
static TH2I *hGEMHit_PlaneVsStrip;
static TH2I *hGEMSRSAmp_Time, *hGEMSRSAmp_Time_Cluster;
static TH2I *hGEMSRSClusterAmpCorr;

static TH2I *hWireTRDPointAmp_DeltaT, *hGEMTRDPointAmp_DeltaT;
static TH2I *hGEMSRSPointAmp_DeltaT, *hGEMSRSPointAmp_DeltaT_Good;
static TH2I *hWireTRDPoint_WireStrip, *hGEMTRDPoint_XY; 
static TH1I *hWireTRDX_Time, *hGEMTRDX_Time;
static TH2I *hGEMTRDPoint_StripCorr;

static TH2I *hWire_GEMTRDX, *hWire_GEMTRDXstrip, *hWire_GEMTRDX_DeltaT;
static TH2I *hStrip_GEMTRDY, *hStrip_GEMTRDYstrip;
static TH2I *hWire_PadGEMX;
static TH2I *hStrip_PadGEMY;
static TH2I *hWire_GEMSRSXstrip[5], *hStrip_GEMSRSYstrip[5];
static TH2I *hStrip_GEMSRSY[5], *hWire_GEMSRSX[5];
static TH2I *hWire_GEMTRDXchan[11];

static TH2I *hPadGEM_WireTRDX, *hPadGEM_WireTRDY, *hPadGEM_WireTRD_DeltaXY;
static TH2I *hPadGEM_GEMTRDX, *hPadGEM_GEMTRDY, *hPadGEM_GEMTRD_DeltaXY;

//----------------------------------------------------------------------------------


// Routine used to create our JEventProcessor
extern "C"{
    void InitPlugin(JApplication *app){
        InitJANAPlugin(app);
        app->AddProcessor(new JEventProcessor_TRD_online());
    }
}


//----------------------------------------------------------------------------------


JEventProcessor_TRD_online::JEventProcessor_TRD_online() {
}


//----------------------------------------------------------------------------------


JEventProcessor_TRD_online::~JEventProcessor_TRD_online() {
}


//----------------------------------------------------------------------------------

jerror_t JEventProcessor_TRD_online::init(void) {

    // create root folder for TRD and cd to it, store main dir
    TDirectory *mainDir = gDirectory;
    TDirectory *trdDir = gDirectory->mkdir("TRD");
    trdDir->cd();
    // book hists
    trd_num_events = new TH1I("trd_num_events","TRD number of events",1,0.5,1.5);

    // digihit-level hists
    trdDir->cd();
    gDirectory->mkdir("DigiHit")->cd();
    hDigiHit_NHits = new TH1I("DigiHit_NfadcHits","TRD fADC hit multiplicity;raw hits;events",100,0.5,0.5+100);
    
    // histograms for each plane
    for(int i=0; i<NTRDplanes; i++) {
	    if(i<2) { // Wire TRD
		    hDigiHit_Occupancy[i] = new TH1I(Form("DigiHit_Occupancy_WirePlane%d", i),Form("Plane %d TRD Wire hit occupancy;strip;raw hits / counter",i),NTRDwires,-0.5,-0.5+NTRDwires);
		    hDigiHit_QualityFactor[i] = new TH1I(Form("DigiHit_QualityFactor_WirePlane%d", i),Form("Plane %d TRD Wire quality factor;quality factor;raw hits",i),4,-0.5,3.5);
		    hDigiHit_PeakVsStrip[i] = new TH2I(Form("DigiHit_PeakVsStrip_WirePlane%d", i),Form("Plane %d TRD Wire pulse peak vs. strip;strip;pulse peak",i),NTRDwires,-0.5,-0.5+NTRDwires,100,0.0,4100.0);
		    hDigiHit_PulseTime[i] = new TH1I(Form("DigiHit_PulseTime_WirePlane%d", i),Form("Plane %d TRD Wire pulse time;pulse time [62.5 ps];raw hits",i),1000,0.0,5000.0);
		    hDigiHit_Time[i] = new TH1I(Form("DigiHit_Time_WirePlane%d", i),Form("Plane %d TRD Wire pulse time;pulse time [ns];raw hits / 2 ns",i),2500,0.0,5000.0);
		    hDigiHit_TimeVsStrip[i] = new TH2I(Form("DigiHit_TimeVsStrip_WirePlane%d", i),Form("Plane %d TRD Wire pulse time vs. strip;strip;pulse time [ns]",i),NTRDwires,-0.5,-0.5+NTRDwires,100,0.0,1000.0);
		    hDigiHit_TimeVsPeak[i] = new TH2I(Form("DigiHit_TimeVsPeak_WirePlane%d", i),Form("Plane %d TRD Wire time vs. peak;pulse peak;time [ns]",i),410,0.0,4100.0,200,0.0,1000.0);
	    }
	    else if(i<4) { // GEM TRD
		    hDigiHit_Occupancy[i] = new TH1I(Form("DigiHit_Occupancy_GEMPlane%d", i),Form("Plane %d TRD GEM hit occupancy;strip;raw hits / counter",i),NTRDstrips,-0.5,-0.5+NTRDstrips);
		    hDigiHit_QualityFactor[i] = new TH1I(Form("DigiHit_QualityFactor_GEMPlane%d", i),Form("Plane %d TRD GEM hit quality factor;quality factor;raw hits",i),4,-0.5,3.5);
		    hDigiHit_PeakVsStrip[i] = new TH2I(Form("DigiHit_PeakVsStrip_GEMPlane%d", i),Form("Plane %d TRD GEM pulse peak vs. strip;strip;pulse peak",i),NTRDstrips,-0.5,-0.5+NTRDstrips,100,0.0,4100.0);
		    hDigiHit_PulseTime[i] = new TH1I(Form("DigiHit_PulseTime_GEMPlane%d", i),Form("Plane %d TRD GEM pulse time;pulse time [62.5 ps];raw hits",i),1000,0.0,5000.0);
		    hDigiHit_Time[i] = new TH1I(Form("DigiHit_Time_GEMPlane%d", i),Form("Plane %d TRD GEM pulse time;pulse time [ns];raw hits / 2 ns",i),2500,0.0,5000.0);
		    hDigiHit_TimeVsStrip[i] = new TH2I(Form("DigiHit_TimeVsStrip_GEMPlane%d", i),Form("Plane %d TRD GEM pulse time vs. strip;strip;pulse time [ns]",i),NTRDstrips,-0.5,-0.5+NTRDstrips,100,0.0,1000.0);
		    hDigiHit_TimeVsPeak[i] = new TH2I(Form("DigiHit_TimeVsPeak_GEMPlane%d", i),Form("Plane %d TRD GEM time vs. peak;pulse peak;time [ns]",i),410,0.0,4100.0,200,0.0,1000.0);
	    }
	    else {
		   hDigiHit_Occupancy[i] = new TH1I("DigiHit_Occupancy_PadGEM","GEM pad hit occupancy;pad;raw hits / counter",NGEMpads,-0.5,-0.5+NGEMpads);
                    hDigiHit_QualityFactor[i] = new TH1I("DigiHit_QualityFactor_PadGEM","GEM pad quality factor;quality factor;raw hits",4,-0.5,3.5);
                    hDigiHit_PeakVsStrip[i] = new TH2I("DigiHit_PeakVsPad_PadGEM","GEM pad pulse peak vs. strip;strip;pulse peak",NGEMpads,-0.5,-0.5+NGEMpads,100,0.0,4100.0);
                    hDigiHit_PulseTime[i] = new TH1I("DigiHit_PulseTime_PadGEM","GEM pad pulse time;pulse time [62.5 ps];raw hits",1000,0.0,5000.0);
                    hDigiHit_Time[i] = new TH1I("DigiHit_Time_PadGEM","GEM pad pulse time;pulse time [ns];raw hits / 2 ns",2500,0.0,5000.0);
                    hDigiHit_TimeVsStrip[i] = new TH2I("DigiHit_TimeVsPad_PadGEM","GEM pad pulse time vs. pad;pad;pulse time [ns]",NGEMpads,-0.5,-0.5+NGEMpads,100,0.0,1000.0);
                    hDigiHit_TimeVsPeak[i] = new TH2I("DigiHit_TimeVsPeak_PadGEM","GEM pad time vs. peak;pulse peak;time [ns]",410,0.0,4100.0,200,0.0,1000.0);
	    }
    }

    trdDir->cd();
    gDirectory->mkdir("GEMHit")->cd();
    hGEMHit_NHits = new TH1I("GEMHit_NadcHits","GEM ADC hit multiplicity;raw hits;events",10000,0.5,0.5+10000);

    // histograms for each plane
    for(int i=0; i<NGEMplanes; i++) {
	    hGEMHit_Occupancy[i] = new TH1I(Form("GEMHit_Occupancy_Plane%d", i),Form("Plane %d: GEM raw data hit occupancy;strip;raw hits / counter",i),NGEMstrips,-0.5,-0.5+NGEMstrips);
	    hGEMHit_SampleVsStrip[i] = new TH2I(Form("GEMHit_SampleVsStrip_Plane%d", i),Form("Plane %d: GEM ADC time samples vs strip;strip;sample",i),NGEMstrips,-0.5,-0.5+NGEMstrips,NGEMsamples,-0.5,-0.5+NGEMsamples);
    }
    hGEMHit_PlaneVsStrip = new TH2I("GEMHit_PlaneVsStrip","GEM ADC plane vs strip;strip;plane",NGEMstrips,-0.5,-0.5+NGEMstrips,NGEMplanes,-0.5,-0.5+NGEMplanes);

    trdDir->cd();
    gDirectory->mkdir("Correlations")->cd();

    // TRD plane correlations
    hWireTRDPointAmp_DeltaT = new TH2I("WireTRDPointAmp_DeltaT", "Wire TRD Point amplitude vs #Delta t; #Delta t (ns); Pulse Amplitude", 100, -100, 100, 100, 0, 4000);
    hGEMTRDPointAmp_DeltaT = new TH2I("GEMTRDPointAmp_DeltaT", "GEM TRD Point amplitude vs #Delta t; #Delta t (ns); Pulse Amplitude", 100, -100, 100, 100, 0, 4000);
    hWireTRDPoint_WireStrip = new TH2I("WireTRDPoint_WireStrip", "Wire TRD Point Strip (Y) vx Wire (X); X - Wire # ; Y - Strip #", NTRDwires, -0.5, -0.5+NTRDwires, NTRDwires, -0.5, -0.5+NTRDwires);
    hGEMTRDPoint_XY = new TH2I("GEMTRDPoint_XY", "GEM TRD Point Y vx X; X (cm); Y (cm)", 100, -10., 10., 100, -90., -40.);
    hGEMTRDPoint_StripCorr = new TH2I("GEMTRDPoint_StripCorr", "GEM TRD Point Strip (Y) vx (X); X - Strip # ; Y - Strip #", NGEMstrips, -0.5, -0.5+NGEMstrips, NGEMstrips, -0.5, -0.5+NGEMstrips);

    // GEM SRS plane correlations
    hGEMSRSAmp_Time = new TH2I("GEMSRSAmp_Time", "GEM SRS Hit amplitude vs time; time (ns); Pulse Amplitude", 55, 0, 550, 100, 0, 4000);
    hGEMSRSAmp_Time_Cluster = new TH2I("GEMSRSAmp_Time_Cluster", "GEM SRS Hit amplitude vs time; time (ns); Pulse Amplitude", 55, 0, 550, 100, 0, 4000);
    hGEMSRSPointAmp_DeltaT = new TH2I("GEMSRSPointAmp_DeltaT", "GEM SRS Point amplitude vs #Delta t; #Delta t (ns); Pulse Amplitude", 100, -100, 100, 100, 0, 10000);
    hGEMSRSPointAmp_DeltaT_Good = new TH2I("GEMSRSPointAmp_DeltaT_Good", "GEM SRS Point amplitude vs #Delta t; #Delta t (ns); Pulse Amplitude", 100, -100, 100, 100, 0, 10000);
    hGEMSRSClusterAmpCorr = new TH2I("GEMSRSClusterAmpCorr", "GEM SRS Cluster amplitude vs Max strip amplitude; Cluster amplitude; Max Strip Amplitude", 100, 0, 10000, 100, 0, 10000);
    
    // Wire TRD - GEM TRD correlations
    hWire_GEMTRDX = new TH2I("Wire_GEMTRDX", "GEM TRD X position vs TRD wire # ; TRD wire # ; GEM TRD X (cm)", NTRDwires, -0.5, -0.5+NTRDwires, 100., -10., 10.);
    hWire_GEMTRDXstrip = new TH2I("Wire_GEMTRDXstrip", "GEM TRD X strip vs TRD wire # ; TRD wire # ; GEM TRD X strip #", NTRDwires, -0.5, -0.5+NTRDwires, NGEMstrips, -0.5, -0.5+NGEMstrips);
    hWire_GEMTRDX_DeltaT = new TH2I("Wire_GEMTRDX_DeltaT", "GEM TRD X Amplitude vs #Delta t ; #Delta t (ns) ; GEM TRD X Amplitude", 500, -500, 500, 100, 0, 10000);
    hWireTRDX_Time = new TH1I("WireTRDX_Time", "Wire TRD X Time ; t (ns)", 1000, 0, 1000);
    hGEMTRDX_Time = new TH1I("GEMTRDX_Time", "GEM TRD X Time ; t (ns)", 1000, 0, 1000);

    hStrip_GEMTRDY = new TH2I("Strip_GEMTRDY", "GEM TRD Y position vs TRD strip # ; TRD strip # ; GEM TRD Y (cm)", NTRDwires, -0.5, -0.5+NTRDwires, 100, -90., -40.);
    hStrip_GEMTRDYstrip = new TH2I("Strip_GEMTRDYstrip", "GEM TRD Y strip vs TRD strip # ; TRD strip # ; GEM TRD Y strip #", NTRDwires, -0.5, -0.5+NTRDwires, NGEMstrips, -0.5, -0.5+NGEMstrips);

    for(int i=0; i<11; i++) {
	    hWire_GEMTRDXchan[i] = new TH2I(Form("Wire_GEMTRDXchan_slot%d",i), "GEM TRD X channel vs TRD wire # ; TRD wire # ; GEM TRD X channel #", NTRDwires, -0.5, -0.5+NTRDwires, 72, 0, 72);
    }

    // Wire TRD - Pad GEM correlations 
    hWire_PadGEMX = new TH2I("Wire_PadGEMX", "Pad GEM X position vs TRD wire # ; TRD wire # ; Pad GEM X (cm)", NTRDwires, -0.5, -0.5+NTRDwires, 10, 0, 10);
    hStrip_PadGEMY = new TH2I("Strip_PadGEMY", "Pad GEM Y position vs TRD strip # ; TRD strip # ; Pad GEM Y (cm)", NTRDwires, -0.5, -0.5+NTRDwires, 10, 0, 10);

    // Pad GEM correlations with Wire and GEM TRD
    hPadGEM_WireTRDX = new TH2I("PadGEM_WireTRDX", "Wire TRD X vs Pad GEM X; Pad GEM X (cm); Wire TRD X (cm)", 10, 0, 10, 100, -10., 10.);
    hPadGEM_WireTRDY = new TH2I("PadGEM_WireTRDY", "Wire TRD Y vs Pad GEM Y; Pad GEM Y (cm); Wire TRD Y (cm)", 10, 0, 10, 100, -90., -40.);
    hPadGEM_WireTRD_DeltaXY = new TH2I("PadGEM_WireTRD_DeltaXY", "Position difference Pad GEM - Wire TRD Y vs X; #Delta X (cm); #Delta Y (cm)", 100, -25, 25, 100., -25, 25);
    hPadGEM_GEMTRDX = new TH2I("PadGEM_GEMTRDX", "GEM TRD X vs Pad GEM X; Pad GEM X (cm); GEM TRD X (cm)", 10, 0, 10, 100., -10., 10.);
    hPadGEM_GEMTRDY = new TH2I("PadGEM_GEMTRDY", "GEM TRD Y vs Pad GEM Y; Pad GEM Y (cm); GEM TRD Y (cm)", 10, 0, 10, 100, -90., -40.);
    hPadGEM_GEMTRD_DeltaXY = new TH2I("PadGEM_GEMTRD_DeltaXY", "Position difference Pad GEM - GEM TRD Y vs X; #Delta X (cm); #Delta Y (cm)", 100, -25, 25, 100, -25, 25);
    
    for(int i=0; i<5; i++) {
	    hWire_GEMSRSXstrip[i] = new TH2I(Form("Wire_GEMSRSXstrip_%d",i), Form("Package %d: GEM SRS X strip vs TRD wire # ; TRD wire # ; GEM SRS X strip #",i), NTRDwires, -0.5, -0.5+NTRDwires, 256, -0.5, -0.5+NGEMstrips);
	    hStrip_GEMSRSYstrip[i] = new TH2I(Form("Strip_GEMSRSYstrip_%d",i), Form("Package %d GEM SRS Y strip vs TRD strip # ; TRD strip # ; GEM SRS Y stip",i), NTRDwires, -0.5, -0.5+NTRDwires, 256, -0.5, -0.5+NGEMstrips);

	    hWire_GEMSRSX[i] = new TH2I(Form("Wire_GEMSRSX_%d",i), Form("Package %d GEM SRS X position vs TRD wire # ; TRD wire # ; GEM SRS X (cm)",i), NTRDwires, -0.5, -0.5+NTRDwires, 100., 0., 10.5);
	    hStrip_GEMSRSY[i] = new TH2I(Form("Strip_GEMSRSY_%d",i), Form("Package %d GEM SRS Y position vs TRD strip # ; TRD strip # ; GEM SRS Y (cm)",i), NTRDwires, -0.5, -0.5+NTRDwires, 100., 0., 10.5);
    }
    
    // back to main dir
    mainDir->cd();

    return NOERROR;
}

//----------------------------------------------------------------------------------


jerror_t JEventProcessor_TRD_online::brun(JEventLoop *eventLoop, int32_t runnumber) {
    // This is called whenever the run number changes

    // special conditions for different geometries
    if(runnumber < 70000) wirePlaneOffset = 0;
    else wirePlaneOffset = 4;

    DApplication* dapp = dynamic_cast<DApplication*>(eventLoop->GetJApplication());
    const DGeometry *geom = dapp->GetDGeometry(runnumber);
    vector<double> z_trd;
    geom->GetTRDZ(z_trd);

    return NOERROR;
}


//----------------------------------------------------------------------------------


jerror_t JEventProcessor_TRD_online::evnt(JEventLoop *eventLoop, uint64_t eventnumber) {
    // This is called for every event. Use of common resources like writing
    // to a file or filling a histogram should be mutex protected. Using
    // loop-Get(...) to get reconstructed objects (and thereby activating the
    // reconstruction algorithm) should be done outside of any mutex lock
    // since multiple threads may call this method at the same time.

/*
    // Get trigger words and filter on PS trigger (if it exists?)
    const DL1Trigger *trig_words = nullptr;
    uint32_t trig_mask, fp_trig_mask;
    try {
        eventLoop->GetSingle(trig_words);
    } catch(...) {};
    if (trig_words != nullptr) {
        trig_mask = trig_words->trig_mask;
        fp_trig_mask = trig_words->fp_trig_mask;
    }
    else {
        trig_mask = 0;
        fp_trig_mask = 0;
    }
    int trig_bits = fp_trig_mask > 0 ? 10 + fp_trig_mask:trig_mask;
    // Select PS-triggered events
    if (trig_bits != 8) {
        return NOERROR;
    }
*/

    vector<const DTRDDigiHit*> digihits;
    eventLoop->Get(digihits);
    vector<const DTRDHit*> hits;
    eventLoop->Get(hits);
    vector<const DTRDStripCluster*> clusters;
    eventLoop->Get(clusters);
    vector<const DTRDPoint*> points;
    eventLoop->Get(points);

    vector<const DGEMDigiWindowRawData*> windowrawdata;
    eventLoop->Get(windowrawdata);
    vector<const DGEMHit*> gem_hits;
    eventLoop->Get(gem_hits);
    vector<const DGEMStripCluster*> gem_clusters;
    eventLoop->Get(gem_clusters);
    vector<const DGEMPoint*> gem_points;
    eventLoop->Get(gem_points);

    vector<const DGEMPoint*> padgem_points;
    eventLoop->Get(padgem_points,"PAD");

    // FILL HISTOGRAMS
    // Since we are filling histograms local to this plugin, it will not interfere with other ROOT operations: can use plugin-wide ROOT fill lock
    japp->RootFillLock(this); //ACQUIRE ROOT FILL LOCK

    ///////////////////////////
    // TRD DigiHits and Hits //
    ///////////////////////////

    if (digihits.size() > 0) trd_num_events->Fill(1);
    hDigiHit_NHits->Fill(digihits.size());
    for (const auto& hit : digihits) {
	    int plane = hit->plane;
	    int iplane = plane - wirePlaneOffset; // TRD plane indexing
	    if(iplane < 0) iplane = 4;
	    int strip = hit->strip;
	    hDigiHit_Occupancy[iplane]->Fill(strip);
	    hDigiHit_QualityFactor[iplane]->Fill(hit->QF);
	    hDigiHit_PeakVsStrip[iplane]->Fill(strip,hit->pulse_peak);
	    hDigiHit_PulseTime[iplane]->Fill(hit->pulse_time);
	    double t_ns = 0.8*hit->pulse_time;
	    hDigiHit_Time[iplane]->Fill(t_ns);
	    hDigiHit_TimeVsStrip[iplane]->Fill(strip,t_ns);
	    hDigiHit_TimeVsPeak[iplane]->Fill(hit->pulse_peak,t_ns);
    }

    ////////////////////////
    // GEM SRS histograms //
    ////////////////////////

    if (windowrawdata.size() > 0) trd_num_events->Fill(2);
    hGEMHit_NHits->Fill(windowrawdata.size());

    for (const auto& window : windowrawdata) {
	const DGEMSRSWindowRawData* srswindow;
	window->GetSingle(srswindow);
	
	int plane = window->plane;
	int strip = window->strip;
	hGEMHit_Occupancy[plane]->Fill(strip);
	
	// loop over samples for each channel
	vector<uint16_t> samples = srswindow->samples;
	for(uint isample=0; isample<samples.size(); isample++) {
		hGEMHit_PlaneVsStrip->Fill(1.*strip, 1.*plane, 1.*samples[isample]);
	}

	uint16_t pedestal = 0; //samples[0];
	int max_adc_zs = 0;
	for(uint isample=0; isample<samples.size(); isample++) {
		int adc_zs = (samples[isample]-pedestal);
		if(adc_zs > max_adc_zs) 
			max_adc_zs = adc_zs;
	}		    
	
	// fill all hits in channels with a large signal
	if(max_adc_zs > 400) {
		for(uint isample=0; isample<samples.size(); isample++) {
			int adc_zs = (samples[isample]-pedestal);
			hGEMHit_SampleVsStrip[plane]->Fill(strip,isample,adc_zs);
		}
	}	
    }

    //////////////////////
    // Wire/GEM Summary //
    //////////////////////

    // Wire TRD hits correlation in XY plane
    for (const auto& hit : hits) {
	    if(hit->plane != 0 && hit->plane != 4) continue;

	    for (const auto& hit_strip : hits) {
		    if(hit_strip->plane != 1 && hit_strip->plane != 5) continue;
		    
		    hWireTRDPoint_WireStrip->Fill(hit->strip, hit_strip->strip);
	    }
    }

    // GEM TRD hits correlation in XY plane
    for (const auto& hit : hits) {
	    if(hit->plane != 6) continue;

	    for (const auto& hit_strip : hits) {
		    if(hit_strip->plane != 7) continue;
		    
		    hGEMTRDPoint_StripCorr->Fill(hit->strip, hit_strip->strip);
	    }
    }


    // check SRS GEM hits in known good region
    for (const auto& hit : gem_hits) {
	    hGEMSRSAmp_Time->Fill(hit->t, hit->pulse_height);
    }

    // SRS GEM cluster time comparison
    for (const auto& cluster : gem_clusters) {
	    const DGEMHit *max_hit;
	    for (const auto& hit : cluster->members) {
		    //if(hit->plane == 7 && hit->strip > 100 && hit->strip < 130) 
		    hGEMSRSAmp_Time_Cluster->Fill(hit->t, hit->pulse_height);
		    //if(!max_hit) max_hit = hit;
		    //if(hit->pulse_height > max_hit->pulse_height) max_hit = hit;
	    }

	    // compare max strip to cluster time and position
	    //hGEMSRSClusterAmpCorr->Fill(cluster->q_tot, max_hit->pulse_height);
    }

    // Wire TRD points correlation
    for (const auto& point : points) {
	    if(point->detector == 2) { // Wire TRD
		    hWireTRDPointAmp_DeltaT->Fill(point->t_x - point->t_y, point->dE_amp);
	    }
	    if(point->detector == 3) {
		    hGEMTRDPoint_XY->Fill(point->x, point->y);
		    hGEMTRDPointAmp_DeltaT->Fill(point->t_x - point->t_y, point->dE_amp);
	    }
    }

    // SRS GEM points correlation
    for (const auto& point : gem_points) {
	    hGEMSRSPointAmp_DeltaT->Fill(point->t_x - point->t_y, point->dE_amp);
	    
	    vector<const DGEMStripCluster*> gem_clusters;
	    point->Get(gem_clusters);
	    for (const auto& cluster : gem_clusters) {
		    for (const auto& hit : cluster->members) {
			    if(hit->plane == 7 && hit->strip > 100 && hit->strip < 130) {
				    hGEMSRSPointAmp_DeltaT_Good->Fill(point->t_x - point->t_y, point->dE_amp);
				    break;
			    }
		    }
	    }
    }

    ///////////////////////////
    // Wire-GEM correlations //
    ///////////////////////////

    ////////////////////////////////////////
    // Wire TRD wire # correlation with X //
    ////////////////////////////////////////
    for (const auto& hit : hits) {
	    if(hit->plane != 0 && hit->plane != 4) continue; // only Wire TRD
	    int wire = hit->strip;

	    // GEM TRD points
	    for (const auto& point : points) {
		    if(point->detector == 3)
			    hWire_GEMTRDX->Fill(wire, point->x);
	    }
	    
	    // GEM TRD hits
	    for (const auto& gemtrd_hit : hits) {
		    if(gemtrd_hit->plane != 2 && gemtrd_hit->plane != 6) continue; 
		    double locDeltaT = gemtrd_hit->t - hit->t;
		    
		    hWire_GEMTRDX_DeltaT->Fill(locDeltaT, gemtrd_hit->pulse_height);
		    hGEMTRDX_Time->Fill(gemtrd_hit->t);
		    hWireTRDX_Time->Fill(hit->t);

		    const DTRDDigiHit *gemtrd_digihit;
		    gemtrd_hit->GetSingle(gemtrd_digihit);
		    const Df125FDCPulse *gemtrd_pulse;
		    gemtrd_digihit->GetSingle(gemtrd_pulse);
		    int slot = gemtrd_pulse->slot;
		    int channel = gemtrd_pulse->channel;

		    // some rough time matching of hits
		    if(locDeltaT > 0 && locDeltaT < 300) {
			    hWire_GEMTRDXstrip->Fill(wire, gemtrd_hit->strip);
			    hWire_GEMTRDXchan[slot]->Fill(wire, channel);
		    }
	    }

	    // Pad GEM hits
	    for (const auto& padgem_point : padgem_points) {
		    if(padgem_point->detector != 1) 
			    continue;

		    const DTRDHit *padgem_hit;
		    padgem_point->GetSingle(padgem_hit);
		    hWire_PadGEMX->Fill(wire, (padgem_hit->strip-4)%10);
	    }

	    // GEM SRS hit
	    for (const auto& gem_hit : gem_hits) {
		    if(gem_hit->plane%2 != 0) continue; // skip Y strips
		    hWire_GEMSRSXstrip[gem_hit->plane/2]->Fill(wire, gem_hit->strip);
	    }

	    // GEM SRS points
	    for (const auto& point : gem_points) {
		    hWire_GEMSRSX[point->detector]->Fill(wire, point->x);
	    }
    }

    /////////////////////////////////////////
    // Wire TRD strip # correlation with Y //
    /////////////////////////////////////////
    for (const auto& hit : hits) {
	    if(hit->plane != 1 && hit->plane != 5) continue; // only Wire TRD stips
	    int strip = hit->strip;

	    // GEM TRD points
	    for (const auto& point : points) {
		    if(point->detector == 3)
			    hStrip_GEMTRDY->Fill(strip, point->y);
	    }

	    // GEM TRD hits
	    for (const auto& gemtrd_hit : hits) {
		    if(gemtrd_hit->plane != 3 && gemtrd_hit->plane != 7) continue; 
		    double locDeltaT = gemtrd_hit->t - hit->t;
		    
		    hStrip_GEMTRDYstrip->Fill(strip, gemtrd_hit->strip);
	    }

	    // Pad GEM hits
	    for (const auto& padgem_point : padgem_points) {
		    if(padgem_point->detector != 1) 
			    continue;

		    const DTRDHit *padgem_hit;
		    padgem_point->GetSingle(padgem_hit);
		    hStrip_PadGEMY->Fill(strip, padgem_hit->strip/10);
	    }
	    
	    // GEM SRS hit
	    for (const auto& gem_hit : gem_hits) {
		    if(gem_hit->plane%2 == 0) continue; // skip X strips
		    hStrip_GEMSRSYstrip[gem_hit->plane/2]->Fill(strip, gem_hit->strip);
	    }

	    // GEM SRS points
	    for (const auto& point : gem_points) {
		    hStrip_GEMSRSY[point->detector]->Fill(strip, point->y);
	    }
    }

    // Pad GEM Point correlations
    for (const auto& padgem_point : padgem_points) {
	    if(padgem_point->detector != 1) continue;

	    const DTRDHit *padgem_hit;
	    padgem_point->GetSingle(padgem_hit);

	    // Wire and GEM TRD points
	    for (const auto& point : points) {
		    if(point->detector == 2) { // Wire TRD
			    hPadGEM_WireTRDX->Fill((padgem_hit->strip-4)%10, point->x);
			    hPadGEM_WireTRDY->Fill(padgem_hit->strip/10, point->y);
			    hPadGEM_WireTRD_DeltaXY->Fill(padgem_point->x-point->x, padgem_point->y-point->y);
		    }
		    else if(point->detector == 3) { // GEM TRD
			    hPadGEM_GEMTRDX->Fill((padgem_hit->strip-4)%10, point->x);
			    hPadGEM_GEMTRDY->Fill(padgem_hit->strip/10, point->y);
			    hPadGEM_GEMTRD_DeltaXY->Fill(padgem_point->x-point->x, padgem_point->y-point->y);
		    }	    
	    }

	    
    }

    japp->RootFillUnLock(this); //RELEASE ROOT FILL LOCK

    return NOERROR;
}
//----------------------------------------------------------------------------------


jerror_t JEventProcessor_TRD_online::erun(void) {
    // This is called whenever the run number changes, before it is
    // changed to give you a chance to clean up before processing
    // events from the next run number.
    return NOERROR;
}


//----------------------------------------------------------------------------------


jerror_t JEventProcessor_TRD_online::fini(void) {
    // Called before program exit after event processing is finished.
    return NOERROR;
}

//----------------------------------------------------------------------------------
//----------------------------------------------------------------------------------
