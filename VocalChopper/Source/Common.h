#pragma once
// Vocal Chopper · Soundtastics
// Cosas compartidas por todo el plugin: modulos de JUCE, constantes y utilidades pequenas.

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <vector>
#include <memory>
#include <atomic>
#include <cmath>
#include <array>

namespace vc
{
constexpr double kPi = 3.14159265358979323846;

template <typename T> inline T clampv (T x, T a, T b) { return x < a ? a : (x > b ? b : x); }
inline double modp (double a, double b) { double r = std::fmod (a, b); return r < 0 ? r + b : r; }
inline int modi (int a, int b) { int r = a % b; return r < 0 ? r + b : r; }

// Igual que hash() del prototipo (entero sin signo de 32 bits)
inline uint32_t hash32 (uint32_t n)
{
    n = (n ^ 61u ^ (n >> 16)) * 9u;
    n ^= n >> 4;
    n *= 0x27d4eb2du;
    n ^= n >> 15;
    return n;
}

// Igual que rng() del prototipo (mulberry32)
struct Rng
{
    uint32_t s;
    explicit Rng (uint32_t seed) : s (seed) {}
    double operator()()
    {
        s += 0x6D2B79F5u;
        uint32_t t = (s ^ (s >> 15)) * (1u | s);
        t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
        return (double) (t ^ (t >> 14)) / 4294967296.0;
    }
};

inline juce::String noteName (int midi)
{
    static const char* NN[] = { "C","C#","D","D#","E","F","F#","G","G#","A","A#","B" };
    return juce::String (NN[modi (midi, 12)]) + juce::String ((int) std::floor (midi / 12.0) - 2);
}

// ---------- tablas del prototipo
struct PresetInfo { const char* name; const char* info; };
inline const std::array<PresetInfo, 6>& presets()
{
    static const std::array<PresetInfo, 6> p {{
        { "STRAIGHT", "Reproduce los chops tal cual" },
        { "STUTTER",  "Rolls 1/16 y 1/32 al final de cada compas" },
        { "BUILD-UP", "Subida 1/8 > 1/16 > 1/32 > 1/64" },
        { "TRIPLETS", "Rolls en tresillos 1/8T y 1/16T" },
        { "GLITCH",   "Micro-rolls aleatorios" },
        { "CHOP SEQ", "Secuencia los chops en corcheas" } }};
    return p;
}
enum Preset { pStraight = 0, pStutter, pBuildUp, pTriplets, pGlitch, pChopSeq };

inline const std::array<const char*, 4>& dirNames()
{
    static const std::array<const char*, 4> d {{ "FORWARD", "REVERSE", "PING-PONG", "RANDOM" }};
    return d;
}
enum Dir { dForward = 0, dReverse, dPingPong, dRandom };

struct Sub { const char* name; double v; };
inline const std::array<Sub, 8>& subs()
{
    static const std::array<Sub, 8> s {{
        { "1/4", 1.0 / 4 }, { "1/8", 1.0 / 8 }, { "1/8T", 1.0 / 12 }, { "1/16", 1.0 / 16 },
        { "1/16T", 1.0 / 24 }, { "1/32", 1.0 / 32 }, { "1/32T", 1.0 / 48 }, { "1/64", 1.0 / 64 } }};
    return s;
}

// Mapeos de mandos (identicos al HTML)
inline double spd (double v) { return std::pow (2.0, -(v - 0.5) * 2.0); }
inline int subIdx (double size) { return clampv ((int) std::floor (size * 8.0), 0, 7); }
inline double gateFrac (double size) { return size < 0.45 ? 1.0 : 1.0 - (size - 0.45) / 0.55 * 0.85; }
inline int globalSemis (double pitch) { return (int) std::lround ((pitch - 0.5) * 24.0); }
inline double inDb (double v) { return v < 0.5 ? (v - 0.5) * 48.0 : (v - 0.5) * 24.0; }

// Colores Soundtastics
namespace col
{
    const juce::Colour acc   { 0xffffd23f };
    const juce::Colour cy    { 0xff7fe6ff };
    const juce::Colour mut   { 0xa8ffffff };
    const juce::Colour navy  { 0xff16226a };
    const juce::Colour ink   { 0xff14205e };
    const juce::Colour sel   { 0xff2448e6 };
    const juce::Colour red   { 0xffff5a6e };
}
} // namespace vc
