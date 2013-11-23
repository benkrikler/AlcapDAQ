/********************************************************************\

Name:         MPulseHeights
Created by:   Andrew Edmonds

Contents:     A module to fill a histogram of the pulse heights from each channel

\********************************************************************/

/* Standard includes */
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <utility>
#include <sstream>

/* MIDAS includes */
#include "midas.h"

/* ROOT includes */
#include <TH1.h>

/* AlCap includes */
#include "TOctalFADCIsland.h"
#include "TOctalFADCBankReader.h"
#include "TGlobalData.h"
#include "TSetupData.h"
#include "TPulseIsland.h"

using std::string;
using std::map;
using std::vector;
using std::pair;

/*-- Module declaration --------------------------------------------*/
INT  MPulseHeights_init(void);
INT  MPulseHeights(EVENT_HEADER*, void*);
vector<string> GetAllFADCBankNames();
double GetClockTickForChannel(string bank_name);

extern HNDLE hDB;
extern TGlobalData* gData;

static vector<TOctalFADCBankReader*> fadc_bank_readers;
map <std::string, TH1I*> height_histograms_map;

TSetupData* setup_data;

ANA_MODULE MPulseHeights_module =
{
	"MPulseHeights",                    /* module name           */
	"Andrew Edmonds",              /* author                */
	MPulseHeights,                      /* event routine         */
	NULL,                          /* BOR routine           */
	NULL,                          /* EOR routine           */
	MPulseHeights_init,                 /* init routine          */
	NULL,                          /* exit routine          */
	NULL,                          /* parameter structure   */
	0,                             /* structure size        */
	NULL,                          /* initial parameters    */
};

/** This method initializes histograms.
*/
INT MPulseHeights_init()
{
  // This histogram has the pulse heights on the X-axis and the number of pulses on the Y-axis
  // One histogram is created for each detector
  // This uses the TH1::kCanRebin mechanism to expand automatically to the
  // number of FADC banks.
  vector<string> bank_names = GetAllFADCBankNames();

  for(unsigned int i=0; i<bank_names.size(); i++) {
    fadc_bank_readers.push_back(new TOctalFADCBankReader(bank_names[i]));
    
    std::string detname = setup_data->GetDetectorName(bank_names[i]);
    std::string histname = "h" + detname + "_Heights";
    std::string histtitle = "Plot of the pulse heights for the " + detname + " detector";
    TH1I* hDetHeights = new TH1I(histname.c_str(), histtitle.c_str(), 100,0,100);
    hDetHeights->GetXaxis()->SetTitle("Pulse Height [ADC value]");
    hDetHeights->GetYaxis()->SetTitle("Arbitrary Unit");
    hDetHeights->SetBit(TH1::kCanRebin);

    std::pair<std::string, TH1I*> thePair(bank_names[i], hDetHeights);
    height_histograms_map.insert(thePair);
  }

  return SUCCESS;
}

/** This method processes one MIDAS block, producing a vector
 * of TOctalFADCIsland objects from the raw Octal FADC data.
 */
INT MPulseHeights(EVENT_HEADER *pheader, void *pevent)
{
	// Get the event number
	int midas_event_number = pheader->serial_number;

	// Some typedefs
	typedef map<string, vector<TPulseIsland*> > TStringPulseIslandMap;
	typedef pair<string, vector<TPulseIsland*> > TStringPulseIslandPair;
	typedef map<string, vector<TPulseIsland*> >::iterator map_iterator;

	// Fetch a reference to the gData structure that stores a map
	// of (bank_name, vector<TPulseIsland*>) pairs
	TStringPulseIslandMap& pulse_islands_map =
		gData->fPulseIslandToChannelMap;

	// Loop over the map and get each bankname, vector pair
	for (map_iterator theMapIter = pulse_islands_map.begin(); theMapIter != pulse_islands_map.end(); theMapIter++) 
	{
	  std::string bankname = theMapIter->first;
	  std::vector<TPulseIsland*> thePulses = theMapIter->second;
			
	  // Loop over the TPulseIslands and plot the histogram
	  for (std::vector<TPulseIsland*>::iterator thePulseIter = thePulses.begin(); thePulseIter != thePulses.end(); thePulseIter++) {
			
	    // Find the relevant bank name from the detector name
	    if (height_histograms_map.find(bankname) != height_histograms_map.end()) {
	      // Fill the histogram
	      height_histograms_map[bankname]->Fill((*thePulseIter)->GetPulseHeight());
		      
	    }
	  }
	}
	return SUCCESS;
}
