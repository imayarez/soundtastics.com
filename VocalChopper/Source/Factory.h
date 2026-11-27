#pragma once
#include "Common.h"

namespace vc
{
// Libreria de fabrica. Igual que en el prototipo, de momento son sonidos sintetizados
// de prueba; cuando tengais las grabaciones reales se sustituyen aqui.
struct PatchSettings
{
    int preset = 0;
    int dir = 0;
    double size = 0.4;
    double artifacts = 0.0;
    double blur = 0.15;
};

struct GeneratedAudio
{
    juce::AudioBuffer<float> buf; // estereo
    double sr = 44100.0;
    std::vector<double> cuts;
    double bpm = 0;               // solo loops de bateria
};

struct PatchInfo
{
    const char* bank;
    const char* name;
    const char* info;
    bool isHit;
    PatchSettings set;
};

const std::vector<PatchInfo>& patchList();
GeneratedAudio generatePatch (int index);
}
