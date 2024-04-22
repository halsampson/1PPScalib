// 1PPScalib.cpp   calibrate SampleHz using 1PPS to FrontMic

#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <corecrt_share.h>

#include "waveIn.h"

#define AudDeviceName "FrontMic" 

// Note that typical mic input has ~4Hz high-pass filter (DC blocking)
//   recording is impulse response

double bufferStartSeconds;

const int CurveBucketDiv = 1; // 16384 / 32; // 32 points
int riseCurve[32768 / CurveBucketDiv];

void audioReadyCallback(int b) {
  static int settle;
  //if (settle++ < 16) return;

  const int EdgeThreshold = 18000; // auto adjust to < 90% of minimum high reading

  static bool once;
  static double edgeSample, lastEdgeSample;

  for (int s = 0; s < BufferSamples; ++s) {
    if (wavInBuf[b][s] > EdgeThreshold) { 
      if (!once) {
        once = true;
        printf("PC clock off %+.3f s\n", fmod(bufferStartSeconds + s / SampleHz + 0.5, 1) - 0.5); // assume < 500 ms
        // compare with http://nist.time.gov
      }

      printf("%d, %5d, %5d, %5d", s, wavInBuf[b][s-2], wavInBuf[b][s-1], wavInBuf[b][s]);

      static double samples;  
       
      // sample time interpolation
      // estimate time to reach EdgeThreshold / 2 assuming linear from wavInBuf[b][s-2] to wavInBuf[b][s] (vs. slow near top -> lower threshold)

      // Don't know when pulse rise starts or ends !
      // TODO: determine avg slope from many measurments of wavInBuf[b][s-1] -- plot CDF
      // -2 .. -1 rise is curved 819 + 87x + 0.593x^2 + -3.16E-03x^3
      // assume constant inter-pulse times to adjust mapping curve
      // 819..17155 asymptotic near high end

      // or slower ramp for multiple samples ?? -- add capacitance

      // simple linear:
      int s1 = wavInBuf[b][s - 1];
      static int minS1 = 657, maxS1 = 17805;
      if (s1 < minS1) minS1 = s1;
      if (s1 > maxS1) maxS1 = s1;

      edgeSample = s - (double)(riseCurve[s1 / CurveBucketDiv] - minS1) / (maxS1 - minS1);
      // [s -2] compensation?

      if (fabs(edgeSample - lastEdgeSample) > 1)  // audio discontinuous
        break;

      static int seconds = -4; // settling
      if (++seconds > 0) {
        double offset = edgeSample - lastEdgeSample;
        static double totalOffset;
        totalOffset += offset;
        double avgOffset = totalOffset / seconds;
        double deviation = offset - avgOffset;
        printf(", %+.4f, %+3.0f%%", avgOffset, 100 * deviation / avgOffset);

        samples += BufferSamples + offset;
        double Hz = samples / seconds;
        printf(", %.6f Hz", Hz);
      }
      break;
    }
    // NOTE: 1PPS can skip seconds if signal is weak
  }
  lastEdgeSample = edgeSample;
  printf("\n");
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
  for (int i=0; i < sizeof riseCurve / sizeof riseCurve[0]; ++i)
    riseCurve[i] = i * CurveBucketDiv;  // linear to start or asymptotic formula like 819 + 87x + 0.593x^2 + -3.16E-03x^3

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

