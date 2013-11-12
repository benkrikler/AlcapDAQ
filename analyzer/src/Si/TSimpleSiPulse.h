#ifndef TSIPULSE_H_YAHE2XWS
#define TSIPULSE_H_YAHE2XWS

#include <vector>
#include <string>

#include "TF1.h"

class TOctalFADCIsland;
class TH1I;

class TSimpleSiPulse : public TOctalFADCIsland 
{
	public:
		TSimpleSiPulse(TOctalFADCIsland *island, std::string detname, double pedestal);
		~TSimpleSiPulse();
		TSimpleSiPulse(TOctalFADCIsland *island, std::string detname, 
				unsigned int nped = 7);
	private:
		std::string fDetName;
		double fPedestal;
		double fRMSNoise;
		double fThreshold;
		bool fIsPositive;
		unsigned int fNPedSamples;
		
		TH1I* fWaveform;
		
		bool fIsSlowPulse;
		TF1* fSlowPulseFit;
		
	public:
		double GetPedestal(){return fPedestal;}
		double GetThreshold() {return fThreshold;}
		double GetRMSNoise() {return fRMSNoise;}
		bool IsPositive() {return fIsPositive;};
		bool IsSlowPulse() {return fIsSlowPulse;}
		void PrintInfo();

		TSimpleSiPulse * Invert();

		TH1I * GetWaveform(std::string histname);
		double GetPulseHeight();
		double GetPulseTime();
		
		void SetSlowPulseFit(TF1* fit) { fSlowPulseFit = fit; }
}; // end of class declaration TSimpleSiPulse 

#endif /* end of include guard: TSIPULSE_H_YAHE2XWS */

