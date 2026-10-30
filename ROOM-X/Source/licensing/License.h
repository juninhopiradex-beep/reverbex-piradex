// =============================================================================
//  ROOM-X by Piradex — acesso por senha (offline), mesmo modelo da família
//  Piradex (ANALYSER, MIXSPACE, EQMotion...):
//
//   * MASTER: acesso completo, sem validade
//   * BETA  : acesso de tester, válido até uma data (mostrada no rodapé)
//   * "Lembrar este computador": token SHA-256 ligado ao ID do computador
//
//  O binário NÃO contém as senhas: só um "salt" aleatório e o hash
//  PBKDF2-HMAC-SHA256 (60 000 iterações) de cada uma. Novas senhas beta:
//  tools/generate_beta_password.py (cola a linha gerada em kKeys, em
//  Source/licensing/License.cpp, e faz push — o GitHub compila).
//
//  ILicenseProvider permite trocar no futuro por uma conta Beatfreak na
//  nuvem sem tocar no DSP nem na UI. Nenhuma verificação corre na thread
//  de áudio: o processador só lê um std::atomic<bool>.
// =============================================================================
#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>
#include <juce_cryptography/juce_cryptography.h>
#include <atomic>
#include <memory>

namespace roomx::lic
{
enum class Kind { None, Master, Beta };
enum class Result { Ok, Wrong, Expired, TooManyAttempts };

struct KeyEntry
{
    const char* id;
    Kind        kind;
    const char* saltHex;
    const char* hashHex;
    int expYear, expMonth, expDay;   // só BETA (válida até ao fim desse dia)
};

// ---------------------------------------------------------------------------
class ILicenseProvider
{
public:
    virtual ~ILicenseProvider() = default;
    virtual bool         isUnlocked() const = 0;
    virtual Kind         kind() const = 0;
    virtual juce::String statusText() const = 0;
    // Lento de propósito (PBKDF2) -> chamar FORA da thread de mensagens/áudio
    virtual int          matchPassword (const juce::String& password, bool& expired) const = 0;
    // Aplicar o resultado na thread de mensagens
    virtual Result       applyMatch (int index, bool expired, bool remember) = 0;
    virtual void         lock() = 0;
    virtual void         checkExpiry() = 0;
};

// ---------------------------------------------------------------------------
class LocalPasswordLicenseProvider final : public ILicenseProvider
{
public:
    LocalPasswordLicenseProvider();

    static bool inTrial() { return juce::Time::getCurrentTime() < juce::Time (2027, 0, 1, 0, 0, 0); } // aberto ate 31/12/2026
    bool         isUnlocked() const override { return unlocked.load() || inTrial(); }
    Kind         kind() const override       { return currentKind; }
    juce::String statusText() const override;
    int          matchPassword (const juce::String& password, bool& expired) const override;
    Result       applyMatch (int index, bool expired, bool remember) override;
    void         lock() override;
    void         checkExpiry() override;

    // Funções puras (testáveis)
    static juce::MemoryBlock pbkdf2 (const juce::String& password, const juce::MemoryBlock& salt, int iterations, int dkLen);
    static juce::MemoryBlock fromHex (const char* hex);
    static bool isExpired (const KeyEntry& k, juce::Time now = juce::Time::getCurrentTime());
    static int  numKeys();
    static const KeyEntry& key (int i);

    static constexpr int kIterations = 60000;

   #if defined (ROOMX_ALLOW_TEST_UNLOCK)   // só nas builds de teste, nunca no plugin distribuído
    void unlockForTests();
   #endif

private:
    void tryAutoUnlock();
    void setUnlocked (const KeyEntry& k);
    static juce::String tokenFor (const KeyEntry& k);

    std::unique_ptr<juce::PropertiesFile> props;
    std::atomic<bool> unlocked { false };
    Kind currentKind = Kind::None;
    const KeyEntry* currentKey = nullptr;
};

// ---------------------------------------------------------------------------
//  Ponto de acesso único (partilhado por todas as instâncias do plugin)
// ---------------------------------------------------------------------------
class License
{
public:
    static ILicenseProvider& get();

    // Limitação de tentativas (anti força-bruta): 5 erradas -> 30 s de espera
    static bool canAttempt (int& secondsLeft);
    static void registerFailure();
    static void registerSuccess();
};
} // namespace roomx::lic
