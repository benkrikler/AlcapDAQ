#include "TPulseIsland.h"

#include <algorithm>

using std::vector;
using std::string;

TPulseIsland::TPulseIsland()
{
  Reset();
}

TPulseIsland::TPulseIsland(
  int timestamp, const vector<int>& samples_vector,
  double clock_tick_in_ns, string bank_name)
{
  Reset();
  fTimeStamp = timestamp;
  fSamples = samples_vector;
  fClockTickInNs = clock_tick_in_ns;
  fBankName = bank_name;
}

void TPulseIsland::Reset(Option_t* o)
{
  fTimeStamp = 0;
  fSamples.clear();
  fClockTickInNs = 0.0;
  fBankName = "";
}

double TPulseIsland::GetPulseHeight() const {

  // max_element returns an iterator to the maximum element in the range.
  return *(std::max_element(fSamples.begin(), fSamples.end() ));
}

double TPulseIsland::GetPulseTime() const {

  return fTimeStamp * fClockTickInNs;
}

double TPulseIsland::GetPedestal(int nPedSamples) const {

  double pedestal = 0;
  for (int iSample = 0; iSample < nPedSamples; iSample++) {
    pedestal += fSamples.at(iSample);
  }

  pedestal /= nPedSamples;

  return pedestal;
}
