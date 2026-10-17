// =============================================================================
//  ROOM-X — identidade visual (cores, tipografia, cor de acento animada)
//
//  Base neutra grafite/preto. Só os acentos mudam com o Target:
//    EARLY = coral/vermelho  ·  ALL = teal/verde-água  ·  LATE = azul eléctrico
// =============================================================================
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "BinaryData.h"
#include "../i18n/PiradexLanguage.h"

namespace roomx::ui
{
namespace col
{
    inline const juce::Colour bg        { 0xff0c0e12 };   // fundo da janela
    inline const juce::Colour panel     { 0xff14171d };   // painéis
    inline const juce::Colour panelHi   { 0xff1b1f27 };
    inline const juce::Colour well      { 0xff0f1116 };   // poços (espectro, grelha)
    inline const juce::Colour line      { 0xff262b35 };
    inline const juce::Colour lineHi    { 0xff343b48 };
    inline const juce::Colour text      { 0xffe6e9ef };
    inline const juce::Colour textDim   { 0xff8b93a3 };
    inline const juce::Colour textFaint { 0xff566070 };
    inline const juce::Colour silver    { 0xfff4f6fa };   // notas seleccionadas
    inline const juce::Colour danger    { 0xffff4d5e };
    inline const juce::Colour warn      { 0xffffa53d };

    inline const juce::Colour early     { 0xffff667a };
    inline const juce::Colour all       { 0xff40e0c0 };
    inline const juce::Colour late      { 0xff3da8ff };

    inline juce::Colour forTarget (int t)  { return t == 0 ? early : (t == 2 ? late : all); }
}

// Cor de acento partilhada, animada pelo editor (~220 ms)
struct Accent
{
    juce::Colour current = col::all;
    juce::Colour target  = col::all;

    bool step (float amount)
    {
        if (current == target) return false;
        auto mixed = current.interpolatedWith (target, amount);
        if (std::abs (mixed.getFloatRed() - target.getFloatRed()) + std::abs (mixed.getFloatGreen() - target.getFloatGreen())
            + std::abs (mixed.getFloatBlue() - target.getFloatBlue()) < 0.01f)
            mixed = target;
        current = mixed;
        return true;
    }
    juce::Colour dim (float a = 0.35f) const { return current.withAlpha (a); }
};

// Tipografia: Barlow (SIL OFL 1.1, embutida)
struct Fonts
{
    static juce::Typeface::Ptr regular()
    {
        static auto t = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowRegular_ttf, BinaryData::BarlowRegular_ttfSize);
        return t;
    }
    static juce::Typeface::Ptr medium()
    {
        static auto t = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowMedium_ttf, BinaryData::BarlowMedium_ttfSize);
        return t;
    }
    static juce::Typeface::Ptr semibold()
    {
        static auto t = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowSemiBold_ttf, BinaryData::BarlowSemiBold_ttfSize);
        return t;
    }
    static juce::Font text (float h)   { return juce::Font (juce::FontOptions (regular()).withHeight (h)); }
    static juce::Font label (float h)  { return juce::Font (juce::FontOptions (medium()).withHeight (h)); }
    static juce::Font title (float h)  { return juce::Font (juce::FontOptions (semibold()).withHeight (h)); }
    static juce::Font caps (float h, float tracking = 0.12f)
    {
        return juce::Font (juce::FontOptions (semibold()).withHeight (h)).withExtraKerningFactor (tracking);
    }
};

inline juce::String U (const char* utf8) { return juce::String::fromUTF8 (utf8); }
using piradex::i18n::tr;   // tr ("texto PT", "English text")
} // namespace roomx::ui
