#pragma once

const double SampleHz = 191996.605;              // Audio sampling rate measured by long recording of 1PPS
// .653 78F
// .605 77F
// .633 76F
// 

const int BufferSamples = int(SampleHz + 0.5);
extern short wavInBuf[2][BufferSamples];

void setupAudioIn(const char* deviceName, void (*)(int b));
void startWaveIn();
bool waveInReady();
void stopWaveIn();
  // see also MM_WOM_DONE: https://learn.microsoft.com/en-us/windows/win32/multimedia/processing-the-mm-wom-done-message