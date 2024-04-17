// 1PPScalib.cpp   calibrate SampleHz using 1PPS to FrontMic

#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <corecrt_share.h>

#include "waveIn.h"

#define AudDeviceName "FrontMic" 

double bufferStartSeconds;

void audioReadyCallback(int b) {
  static int settle;
  //if (settle++ < 16) return;

  for (int s = 0; s < BufferSamples; ++s) {
    if (wavInBuf[b][s] > 12000) { // pulse edge threshold
      printf("%d: %5d %5d %5d ", s, wavInBuf[b][s-2], wavInBuf[b][s-1], wavInBuf[b][s]);

      static double samples, lastEdge;  
       
      // edge midpoint level sample time interpolation
      double edgeSample = s - (double)(wavInBuf[b][s-1] - wavInBuf[b][s-2]) / (wavInBuf[b][s] - wavInBuf[b][s-2]);
      printf("%.2f %+.4f ", edgeSample, edgeSample - lastEdge);

      static int seconds = -4; // settling
      if (seconds > 0) {
        double Hz = (samples + edgeSample) / seconds;
        printf("%.6f Hz", Hz);
      } else samples = -edgeSample;
      samples += BufferSamples;
      lastEdge = edgeSample;
      ++seconds;
      printf("\n");
      break;
    }
  }

  bufferStartSeconds += BufferSamples / SampleHz; // next buffer start time
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

