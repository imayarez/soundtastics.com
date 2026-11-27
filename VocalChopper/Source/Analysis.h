#pragma once
#include "Common.h"

namespace vc
{
enum DetectMode { detAuto = 0, detVocal, detHit };

struct AnalysisResult
{
    bool isHit = false;
    std::vector<double> cuts; // segundos
    double A = 0, B = 1;      // region util (inicio / fin)
    double bpm = 120;
};

// Port directo de analyse() del prototipo.
AnalysisResult analyse (const juce::AudioBuffer<float>& buf, double sr, int mode, double sens);
}
