// PulseFitter.cpp
// Author: Andrew Edmonds
// Created: 25th Oct 2013
// -- Pulse fitter module

#include "PulseFitter.h"
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <utility>

#include <sstream>
#include <cmath>

#include "TH1.h"

#include "TF1.h"

#include "TROOT.h"
#include "TStyle.h"

#include "TCanvas.h"
#include "TLine.h"
#include "TLegend.h"

using std::string;
using std::map;
using std::vector;
using std::pair;

static bool verbose = false;

static int entry_counter = 1;

PulseFitter::PulseFitter(char *HistogramDirectoryName) :
  FillHistBase(HistogramDirectoryName){
  dir->cd("/");
}

PulseFitter::~PulseFitter(){
}

int PulseFitter::ProcessEntry(TGlobalData *gData){
  typedef map<string, vector<TPulseIsland*> > TStringPulseIslandMap;
  typedef pair<string, vector<TPulseIsland*> > TStringPulseIslandPair;
  typedef map<string, vector<TPulseIsland*> >::iterator map_iterator;
  typedef vector<TPulseIsland*>::iterator pulse_iterator;
  typedef vector<int>::iterator int_iterator;
  
  gROOT->SetStyle("Plain");
  gStyle->SetCanvasBorderMode(0); // turn off canvas borders
  gStyle->SetPalette(1);

  vector<TPulseIsland*> pulses;
  
  for(map_iterator iter = gData->fPulseIslandToChannelMap.begin();
      iter != gData->fPulseIslandToChannelMap.end(); iter++) {
    	
      if (strcmp((iter->first).data(), "Pulses") == 0)
      	pulses = iter->second;
	  
	  for (pulse_iterator pulseIter = pulses.begin(); pulseIter != pulses.end(); pulseIter++) {
	  	
	  	if (verbose)
	  		std::cout << "Entry: " << entry_counter << " Bank: " << (*pulseIter)->GetBankName() << ", Pulse: " << pulseIter - pulses.begin() << std::endl;
	  	
	  	if (strcmp((*pulseIter)->GetBankName().c_str(), "Nh80") != 0)
	  		continue;
	  		
	  	// Get the samples
	  	std::vector<int> theSamples = (*pulseIter)->GetSamples();
	  	
	  	// Create the histogram
	  	std::stringstream histname;
	  	histname << "Entry" << entry_counter << "_" << (*pulseIter)->GetBankName() << "_Pulse" << pulseIter - pulses.begin();
	  	
	  	int bin_min = 0; int bin_max = theSamples.size(); int n_bins = bin_max;
	  	TH1F* hPulse = new TH1F(histname.str().c_str(), histname.str().c_str(), n_bins, bin_min, bin_max);
	  	
	  	// Fill the histogram
	  	for (int_iterator sampleIter = theSamples.begin(); sampleIter != theSamples.end(); sampleIter++) {
	  		hPulse->Fill(sampleIter - theSamples.begin(), *sampleIter);
	  	}
	  	
	  	std::stringstream canvasname;
	  	canvasname << "Canvas_" << histname.str();
	  	TCanvas* c1 = new TCanvas(canvasname.str().c_str(), canvasname.str().c_str());
	  	
	  	TF1* gaussian = new TF1("gaus", "[0]*TMath::Gaus(x, [1], [2], 0)", hPulse->GetXaxis()->GetXmin(),hPulse->GetXaxis()->GetXmax());
	  	gaussian->SetParameter(0, 250);
	  	gaussian->SetParameter(1, 20);
	  	gaussian->SetParameter(2, 20);
	  	
	  	gaussian->SetLineColor(kRed);
	  	gaussian->SetLineWidth(2);
	  	hPulse->Fit("gaus", "Q");
	  	
	  	TF1* skew_gaussian = new TF1("skew-gaus", "2*[0]*TMath::Gaus(x, [1], [2], 0)*(0.5*(1 + TMath::Erf( ([3]*x)/sqrt(2) )))", hPulse->GetXaxis()->GetXmin(),hPulse->GetXaxis()->GetXmax());
	  	skew_gaussian->SetParameter(0, 250);
	  	skew_gaussian->SetParameter(1, 20);
	  	skew_gaussian->SetParameter(2, 20);
	  	skew_gaussian->SetParameter(3, -1);
	  	
	  	skew_gaussian->SetLineWidth(2);
	  	skew_gaussian->SetLineColor(kBlue);
	  	hPulse->Fit("skew-gaus", "Q");
	  	
	  	hPulse->Draw();
	  	skew_gaussian->Draw("SAMES");
	  	gaussian->Draw("SAMES");
	  	
	  	TLegend* legend = new TLegend(0.5, 0.85, 0.7, 0.75, "");
	  	legend->SetBorderSize(0);
        legend->SetTextSize(0.04);
        legend->SetFillColor(kWhite);
        legend->AddEntry(gaussian, "Gaussian", "l");
        legend->AddEntry(skew_gaussian, "Skewed Gaussian", "l");
        legend->Draw();
        
	  	c1->Update();
	  	c1->Write();
	  	
	  	std::cout << "Difference in parameters:\tdelta_p0 = " << skew_gaussian->GetParameter(0) - gaussian->GetParameter(0)
	  				<< "\tdelta_p1 = " << skew_gaussian->GetParameter(1) - gaussian->GetParameter(1)
	  				<< "\tdelta_p2 = " << skew_gaussian->GetParameter(2) - gaussian->GetParameter(2)
	  				<< "\tdelta_p3 = " << skew_gaussian->GetParameter(3) - 0 << std::endl;
	  				
	  }
  }
  entry_counter++;
  
  return 0;
}