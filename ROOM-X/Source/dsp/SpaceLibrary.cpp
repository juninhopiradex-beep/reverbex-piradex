#include "SpaceLibrary.h"

namespace roomx::dsp
{
namespace
{
using C = SpaceCategory;

//  name, cat, description,
//  rt60, hfRatio, dampHz, lfRatio, lfHz,
//  erLen, erStart, erTaps, erGain, erShape, erSlope, erDampHz, erDiff, erWidth,
//  lateMin, lateMax, lateGain, diffusion, modDepth, modRate, dispersion, lateOnset, width, seed
const SpaceDef kSpaces[kNumSpaces] =
{
    // ------------------------------------------------------------------ Rooms
    { "Studio A",       C::Rooms, "Sala de captação equilibrada, madeira e absorção controlada",
      0.55f, 0.62f, 5200.f, 1.05f, 220.f,
      38.f, 2.2f, 22, 0.80f, 0.85f, 1.4f, 9000.f, 0.55f, 0.70f,
      9.f, 38.f, 0.70f, 0.62f, 0.25f, 0.55f, 0.f, 4.f, 0.85f, 1101 },
    { "Velvet Room",    C::Rooms, "Sala suave e escura, ideal para vozes íntimas",
      0.85f, 0.45f, 3600.f, 1.10f, 180.f,
      46.f, 3.0f, 20, 0.62f, 0.95f, 1.8f, 5200.f, 0.68f, 0.75f,
      11.f, 46.f, 0.78f, 0.70f, 0.35f, 0.42f, 0.f, 6.f, 0.90f, 1102 },
    { "Tight Chamber",  C::Rooms, "Câmara pequena e densa, ataque curto",
      0.32f, 0.70f, 6800.f, 0.95f, 250.f,
      22.f, 1.2f, 24, 0.95f, 0.75f, 1.2f, 11000.f, 0.62f, 0.55f,
      6.f, 24.f, 0.60f, 0.70f, 0.18f, 0.70f, 0.f, 2.f, 0.70f, 1103 },
    { "Wood Room",      C::Rooms, "Sala de madeira quente, ligeiramente ressonante",
      0.70f, 0.55f, 4300.f, 1.20f, 260.f,
      42.f, 2.6f, 18, 0.85f, 0.90f, 1.3f, 7000.f, 0.45f, 0.65f,
      10.f, 42.f, 0.72f, 0.55f, 0.22f, 0.50f, 0.f, 5.f, 0.80f, 1104 },
    { "Wide Booth",     C::Rooms, "Cabine larga e aberta, estéreo amplo",
      0.48f, 0.75f, 7600.f, 0.90f, 200.f,
      34.f, 3.5f, 26, 0.75f, 0.80f, 1.5f, 12000.f, 0.50f, 1.00f,
      8.f, 34.f, 0.65f, 0.60f, 0.30f, 0.62f, 0.f, 3.f, 1.00f, 1105 },
    { "Concrete Loft",  C::Rooms, "Loft de betão, reflexões duras e brilhantes",
      1.25f, 0.88f, 9000.f, 0.95f, 160.f,
      70.f, 4.5f, 26, 0.90f, 0.70f, 1.0f, 14000.f, 0.30f, 0.80f,
      14.f, 66.f, 0.80f, 0.48f, 0.25f, 0.38f, 0.f, 8.f, 0.85f, 1106 },

    // ------------------------------------------------------------------ Halls
    { "Aurora Hall",    C::Halls, "Sala de concerto luminosa com cauda longa e aberta",
      2.40f, 0.70f, 6200.f, 1.15f, 200.f,
      85.f, 9.0f, 24, 0.60f, 0.85f, 1.2f, 9500.f, 0.60f, 0.85f,
      28.f, 105.f, 0.92f, 0.74f, 0.55f, 0.32f, 0.f, 14.f, 0.95f, 2101 },
    { "Grand Arc",      C::Halls, "Arco monumental, grave generoso",
      3.20f, 0.58f, 4800.f, 1.30f, 170.f,
      95.f, 12.f, 22, 0.55f, 0.90f, 1.1f, 7800.f, 0.62f, 0.80f,
      32.f, 120.f, 0.95f, 0.76f, 0.60f, 0.27f, 0.f, 18.f, 1.00f, 2102 },
    { "Opera Void",     C::Halls, "Teatro lírico vazio, médios cheios",
      2.00f, 0.64f, 5400.f, 1.05f, 220.f,
      78.f, 8.0f, 24, 0.66f, 0.88f, 1.3f, 8600.f, 0.58f, 0.75f,
      26.f, 98.f, 0.90f, 0.72f, 0.45f, 0.36f, 0.f, 12.f, 0.90f, 2103 },
    { "Nordic Hall",    C::Halls, "Sala nórdica fria e cristalina",
      2.70f, 0.92f, 10500.f, 0.90f, 180.f,
      80.f, 10.f, 26, 0.58f, 0.80f, 1.1f, 15000.f, 0.55f, 0.90f,
      30.f, 110.f, 0.90f, 0.78f, 0.40f, 0.30f, 0.f, 15.f, 0.95f, 2104 },
    { "Celestial Stage",C::Halls, "Palco aberto com cauda brilhante e modulada",
      4.20f, 0.80f, 8200.f, 1.05f, 190.f,
      90.f, 14.f, 22, 0.50f, 0.85f, 1.0f, 12000.f, 0.65f, 0.95f,
      34.f, 125.f, 0.96f, 0.80f, 0.90f, 0.22f, 0.f, 22.f, 1.00f, 2105 },
    { "Deep Auditorium",C::Halls, "Auditório grande e escuro, muito profundo",
      5.50f, 0.50f, 3900.f, 1.25f, 160.f,
      110.f, 16.f, 20, 0.48f, 0.92f, 1.0f, 6400.f, 0.66f, 0.85f,
      38.f, 138.f, 0.98f, 0.80f, 0.70f, 0.18f, 0.f, 26.f, 1.00f, 2106 },

    // ------------------------------------------------------------- Mechanical
    { "Silver Plate",   C::Mechanical, "Placa metálica brilhante, ataque imediato",
      2.10f, 0.85f, 9800.f, 0.85f, 240.f,
      12.f, 0.4f, 16, 0.35f, 0.60f, 2.0f, 15000.f, 0.75f, 0.90f,
      7.f, 52.f, 1.00f, 0.82f, 0.30f, 0.80f, 0.15f, 0.f, 1.00f, 3101 },
    { "Dark Plate",     C::Mechanical, "Placa escura e espessa para vozes e caixas",
      2.60f, 0.48f, 4200.f, 1.00f, 220.f,
      12.f, 0.4f, 16, 0.30f, 0.60f, 2.0f, 7000.f, 0.75f, 0.85f,
      8.f, 58.f, 1.00f, 0.82f, 0.32f, 0.70f, 0.12f, 0.f, 1.00f, 3102 },
    { "Copper Plate",   C::Mechanical, "Placa de cobre de médios quentes",
      1.70f, 0.66f, 6200.f, 0.95f, 260.f,
      10.f, 0.3f, 14, 0.32f, 0.60f, 2.0f, 10000.f, 0.75f, 0.80f,
      6.f, 46.f, 1.00f, 0.80f, 0.26f, 0.90f, 0.20f, 0.f, 0.95f, 3103 },
    { "Spring Matrix",  C::Mechanical, "Tanque de molas duplo com 'boing' dispersivo",
      2.30f, 0.55f, 4600.f, 0.80f, 300.f,
      18.f, 1.0f, 10, 0.25f, 0.70f, 1.5f, 6500.f, 0.30f, 0.60f,
      24.f, 44.f, 0.95f, 0.35f, 0.15f, 1.10f, 0.95f, 0.f, 0.75f, 3104 },
    { "Vintage Chamber",C::Mechanical, "Câmara de eco antiga em azulejo",
      1.60f, 0.60f, 5000.f, 1.00f, 240.f,
      52.f, 5.0f, 20, 0.65f, 0.85f, 1.2f, 7200.f, 0.55f, 0.70f,
      18.f, 70.f, 0.88f, 0.66f, 0.20f, 0.45f, 0.08f, 6.f, 0.80f, 3105 },

    // ---------------------------------------------------------------- Unusual
    { "Glass Tunnel",   C::Unusual, "Túnel de vidro com ecos rápidos e brilhantes",
      2.20f, 0.95f, 12000.f, 0.85f, 260.f,
      140.f, 6.0f, 28, 0.70f, 1.20f, 0.6f, 16000.f, 0.20f, 0.60f,
      20.f, 44.f, 0.85f, 0.35f, 0.15f, 0.50f, 0.35f, 10.f, 0.70f, 4101 },
    { "Neon Cave",      C::Unusual, "Gruta húmida, densa e escura",
      3.80f, 0.52f, 4200.f, 1.35f, 150.f,
      120.f, 11.f, 28, 0.62f, 0.80f, 0.9f, 6500.f, 0.62f, 0.85f,
      30.f, 140.f, 0.94f, 0.78f, 0.55f, 0.26f, 0.f, 20.f, 1.00f, 4102 },
    { "Hollow Steel",   C::Unusual, "Contentor de aço oco, metálico e afinado",
      1.90f, 0.85f, 8200.f, 0.80f, 300.f,
      48.f, 2.5f, 18, 0.80f, 0.70f, 1.0f, 11000.f, 0.25f, 0.55f,
      12.f, 26.f, 0.85f, 0.25f, 0.08f, 0.60f, 0.55f, 4.f, 0.70f, 4103 },
    { "Frozen Corridor",C::Unusual, "Corredor longo e gelado, eco palpitante",
      2.60f, 0.88f, 9500.f, 0.90f, 220.f,
      180.f, 14.f, 30, 0.65f, 1.35f, 0.5f, 13000.f, 0.15f, 0.90f,
      40.f, 90.f, 0.82f, 0.40f, 0.25f, 0.40f, 0.10f, 30.f, 0.90f, 4104 },
    { "Air Capsule",    C::Unusual, "Cápsula pequena de ar, muito curta e fechada",
      0.22f, 0.80f, 9000.f, 0.85f, 260.f,
      14.f, 0.8f, 20, 0.95f, 0.70f, 1.2f, 12000.f, 0.70f, 0.40f,
      3.f, 14.f, 0.55f, 0.78f, 0.10f, 0.90f, 0.f, 1.f, 0.60f, 4105 },
    { "Liquid Room",    C::Unusual, "Sala líquida com modulação lenta e ondulante",
      2.00f, 0.60f, 5200.f, 1.10f, 200.f,
      60.f, 6.0f, 22, 0.60f, 0.85f, 1.1f, 8000.f, 0.60f, 0.85f,
      18.f, 80.f, 0.92f, 0.75f, 2.80f, 0.15f, 0.f, 10.f, 1.00f, 4106 },

    // ------------------------------------------------------------- Special FX
    { "Infinite Bloom", C::SpecialFX, "Cauda que floresce lentamente e quase não acaba",
      12.0f, 0.70f, 6500.f, 1.00f, 200.f,
      420.f, 30.f, 32, 0.45f, 1.00f, -1.6f, 9000.f, 0.70f, 1.00f,
      40.f, 150.f, 0.96f, 0.80f, 1.20f, 0.18f, 0.f, 160.f, 1.00f, 5101 },
    { "Harmonic Dust",  C::SpecialFX, "Poeira esparsa e cintilante — ideal para Tempero alto",
      4.00f, 0.95f, 14000.f, 0.70f, 300.f,
      260.f, 8.f, 32, 0.60f, 1.40f, 0.4f, 16000.f, 0.05f, 1.00f,
      22.f, 34.f, 0.80f, 0.10f, 0.05f, 0.60f, 0.40f, 20.f, 1.00f, 5102 },
    { "Orbital Choir",  C::SpecialFX, "Cauda coral, larga e intensamente modulada",
      6.50f, 0.75f, 7000.f, 1.00f, 200.f,
      140.f, 18.f, 26, 0.45f, 0.90f, 0.8f, 9000.f, 0.65f, 1.00f,
      36.f, 130.f, 0.96f, 0.80f, 4.50f, 0.33f, 0.f, 40.f, 1.00f, 5103 },
    { "Prism Tail",     C::SpecialFX, "Reflexões invertidas que crescem até à cauda",
      3.40f, 0.85f, 9000.f, 0.95f, 220.f,
      360.f, 20.f, 32, 0.70f, 1.00f, -2.2f, 13000.f, 0.40f, 1.00f,
      30.f, 110.f, 0.70f, 0.70f, 0.60f, 0.25f, 0.20f, 280.f, 1.00f, 5104 },
    { "Broken Space",   C::SpecialFX, "Espaço 'gated' cortado e granulado",
      0.45f, 0.80f, 8000.f, 1.00f, 220.f,
      240.f, 3.f, 32, 0.85f, 1.00f, 0.0f, 11000.f, 0.25f, 0.95f,
      10.f, 30.f, 0.35f, 0.40f, 0.20f, 1.40f, 0.f, 4.f, 0.90f, 5105 },
    { "Zero Gravity",   C::SpecialFX, "Suspensão quase infinita, escura e flutuante",
      22.0f, 0.55f, 4600.f, 1.10f, 180.f,
      200.f, 24.f, 28, 0.40f, 0.90f, 0.6f, 7000.f, 0.75f, 1.00f,
      42.f, 160.f, 0.96f, 0.82f, 2.00f, 0.10f, 0.f, 60.f, 1.00f, 5106 },
};
} // namespace

const SpaceDef& space (int index) noexcept
{
    if (index < 0) index = 0;
    if (index >= kNumSpaces) index = kNumSpaces - 1;
    return kSpaces[index];
}

const char* categoryName (SpaceCategory c) noexcept
{
    switch (c)
    {
        case SpaceCategory::Rooms:      return "Rooms";
        case SpaceCategory::Halls:      return "Halls";
        case SpaceCategory::Mechanical: return "Mechanical";
        case SpaceCategory::Unusual:    return "Unusual";
        case SpaceCategory::SpecialFX:  return "Special FX";
    }
    return "";
}
} // namespace roomx::dsp
