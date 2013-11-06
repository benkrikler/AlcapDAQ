#ifndef TimingTest_h__
#define TimingTest_h__

#include "FillHistBase.h"
#include "TGlobalData.h"

#include "TH1.h"

class TimingTest : public FillHistBase{
 public:
  TimingTest(char *HistogramDirectoryName);
  ~TimingTest();
  
  TH1* Calibrate(TH1* hist, double x, double new_x);
  std::vector<double> RemovePedestal(std::vector<int> samples);
  
  double GetPulseTime(std::vector<double> samples);

 private:
  virtual int ProcessEntry(TGlobalData *gData);
};

#endif
