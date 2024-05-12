// 1PPScalib.cpp   calibrate SampleHz using 1PPS to FrontMic

#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <corecrt_share.h>

#include "waveIn.h"

#define AudDeviceName "FrontMic" 

// TODO: update to latest waveIn (from WWVB) or merge into WWVB for initial calibration
//    WWVB PCM wants accuracy < ~10% of 60 kHz cycle over 0.5 second ~3 ppm
//    so calibrate for > 2 seconds 

// 1PPS has up to 25ns jitter (inconsequential) and can skip pulses if satellite signals are weak

// Note that typical mic input has ~4Hz high-pass filter (DC blocking)
//   recording is impulse response

double bufferStartSeconds;

const int MaxSeconds = 3600;

int   samplePos[MaxSeconds];
short sampleVal[MaxSeconds];
short index[MaxSeconds];  // keet ordered by sampleVal
int second; // 1PPS

void audioReadyCallback(int b) {
  const int EdgeThreshold = 22560 * 9 / 10; // adjust to ~90% of high sammpleVals

  static bool once;
  for (int s = 0; s < BufferSamples; ++s) {
    if (wavInBuf[b][s] > EdgeThreshold) { 
      if (!once) {
        once = true;
        printf("PC clock off %+.3f s\n\n", fmod(bufferStartSeconds + s / SampleHz + 0.5, 1) - 0.5); // assume < 500 ms
        // compare with http://nist.time.gov
      }

      samplePos[second] = s-1;  // rising edge
      sampleVal[second] = wavInBuf[b][s-1];

      int i;
      for (i = 0; i < second; ++i)
        if (sampleVal[second] <= sampleVal[index[i]]) { // found index
          memmove(index + i + 1, index + i, (second - i) * sizeof(index[0]));
          break;
        }      
      index[i] = second;

      double sumHz = 0, sumWeight = 0;
      for (int i = 0; i < second; ++i) { // Hz based on sample counts between closest matched pairs of rising edge voltages
        int seconds = index[i+1] - index[i];
        int sampleOfs = samplePos[index[i+1]] - samplePos[index[i]];
        double weight = (double)abs(seconds) / (abs(sampleVal[index[i+1]] - sampleVal[index[i]]) + 30);
        sumHz += (BufferSamples + (double)sampleOfs / seconds) * weight;        
        sumWeight += weight;
      }

      if (second)
        printf("%.6f Hz\r", sumHz / sumWeight);  // average

      break;
    }
    // NOTE: 1PPS can skip seconds if signal is weak
  }
  if (++second >= MaxSeconds) exit;
}


void startAudioIn() {
  SYSTEMTIME nearNTP_time; GetSystemTime(&nearNTP_time);
  double ntp = nearNTP_time.wMilliseconds / 1000.;
#ifdef USE_NTP
  extern double ntpTime();
  ntp = ntpTime();
#endif

  int ms = (int)(fmod(ntp, 1) * 1000);
  int sleep_ms = 2000 - ms - 100;  // pulse near start of second for fast search
  Sleep(sleep_ms);   // start near 1 second boundary

  startWaveIn();  // for consistent phase
  SYSTEMTIME wavInStartTime; GetSystemTime(&wavInStartTime);

  double stNtp = nearNTP_time.wSecond + nearNTP_time.wMilliseconds / 1000.;
  double stWis = wavInStartTime.wSecond + wavInStartTime.wMilliseconds / 1000.;
  bufferStartSeconds = fmod(ntp + stWis - stNtp, 60); // approx recording start time
}

int main() {
  setupAudioIn(AudDeviceName, &audioReadyCallback);
  startAudioIn();
  
  while (1) {
    switch(_getch()) {
       case 's' : // stop
        printf("\nstop\n");
        stopWaveIn();
        exit(0);
    }
  }

  return 0;
}

