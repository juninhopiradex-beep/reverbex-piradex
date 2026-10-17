#include "License.h"
#include "../i18n/PiradexLanguage.h"
#include <cstring>
#include <iterator>

namespace roomx::lic
{
// ---------------------------------------------------------------------------
//  Chaves aceites (só salt + hash; as senhas nunca aparecem no código).
//  Senhas próprias do ROOM-X (diferentes das dos outros plugins Piradex).
// ---------------------------------------------------------------------------
static const KeyEntry kKeys[] =
{
    { "master", Kind::Master,
      "a0f2747d92c737cbe4f98a4fd5c8108c",
      "7a33847c3fdd7acc6067d25eeff7f5633962a48611fc49273448bcde4abbe0d2", 0, 0, 0 },

    { "beta-1", Kind::Beta,
      "9633a8c62968c2fa5663687e00379063",
      "7d5422d5dd9075e6ad4a387f1d7fdbd97f3f187185739d8fd846c323f7295bf3", 2027, 1, 31 },

    // promoção pública (loja, posts): válida até 01/11/2026
    { "beta-promo", Kind::Beta,
      "d6c8264268f860a8829e89038d4f7a82",
      "8d8828ddd10f3209756597ff6a91ae4ad9218481888e5a071a66d1acb44d5ad9", 2026, 11, 1 },
};

int LocalPasswordLicenseProvider::numKeys() { return (int) std::size (kKeys); }
const KeyEntry& LocalPasswordLicenseProvider::key (int i) { return kKeys[juce::jlimit (0, numKeys() - 1, i)]; }

// ---------------------------------------------------------------------------
static juce::MemoryBlock sha256 (const void* data, size_t size)
{
    return juce::SHA256 (data, size).getRawData();
}

static juce::MemoryBlock hmacSha256 (const juce::MemoryBlock& key, const juce::MemoryBlock& msg)
{
    constexpr size_t B = 64;
    juce::MemoryBlock k = key.getSize() > B ? sha256 (key.getData(), key.getSize()) : key;
    k.ensureSize (B, true);
    uint8_t ipad[B], opad[B];
    for (size_t i = 0; i < B; ++i)
    {
        ipad[i] = (uint8_t) (static_cast<const uint8_t*> (k.getData())[i] ^ 0x36);
        opad[i] = (uint8_t) (static_cast<const uint8_t*> (k.getData())[i] ^ 0x5c);
    }
    juce::MemoryBlock inner (ipad, B);
    inner.append (msg.getData(), msg.getSize());
    const auto ih = sha256 (inner.getData(), inner.getSize());
    juce::MemoryBlock outer (opad, B);
    outer.append (ih.getData(), ih.getSize());
    return sha256 (outer.getData(), outer.getSize());
}

juce::MemoryBlock LocalPasswordLicenseProvider::pbkdf2 (const juce::String& password, const juce::MemoryBlock& salt, int iterations, int dkLen)
{
    const auto pw = password.toRawUTF8();
    const juce::MemoryBlock key (pw, strlen (pw));
    juce::MemoryBlock out;
    for (uint32_t block = 1; (int) out.getSize() < dkLen; ++block)
    {
        juce::MemoryBlock s = salt;
        const uint8_t be[4] { (uint8_t) (block >> 24), (uint8_t) (block >> 16), (uint8_t) (block >> 8), (uint8_t) block };
        s.append (be, 4);
        auto u = hmacSha256 (key, s);
        juce::MemoryBlock t = u;
        for (int i = 1; i < iterations; ++i)
        {
            u = hmacSha256 (key, u);
            auto* tp = static_cast<uint8_t*> (t.getData());
            auto* up = static_cast<const uint8_t*> (u.getData());
            for (size_t j = 0; j < t.getSize(); ++j) tp[j] ^= up[j];
        }
        out.append (t.getData(), t.getSize());
    }
    out.setSize ((size_t) dkLen);
    return out;
}

juce::MemoryBlock LocalPasswordLicenseProvider::fromHex (const char* hex)
{
    juce::MemoryBlock mb;
    mb.loadFromHexString (hex);
    return mb;
}

bool LocalPasswordLicenseProvider::isExpired (const KeyEntry& k, juce::Time now)
{
    if (k.kind != Kind::Beta) return false;
    const juce::Time end (k.expYear, k.expMonth - 1, k.expDay, 23, 59, 59, 0, true);
    return now > end;
}

static bool constantTimeEquals (const juce::MemoryBlock& a, const juce::MemoryBlock& b)
{
    if (a.getSize() != b.getSize()) return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < a.getSize(); ++i)
        diff |= (uint8_t) (static_cast<const uint8_t*> (a.getData())[i] ^ static_cast<const uint8_t*> (b.getData())[i]);
    return diff == 0;
}

// ---------------------------------------------------------------------------
LocalPasswordLicenseProvider::LocalPasswordLicenseProvider()
{
    juce::PropertiesFile::Options o;
    o.applicationName     = "ROOM-X by Piradex";
    o.folderName          = "Piradex";
    o.filenameSuffix      = ".settings";
    o.osxLibrarySubFolder = "Application Support";
    o.storageFormat       = juce::PropertiesFile::storeAsXML;
    props = std::make_unique<juce::PropertiesFile> (o);
    tryAutoUnlock();
}

juce::String LocalPasswordLicenseProvider::tokenFor (const KeyEntry& k)
{
    // ligado ao computador e à chave; não guarda a senha
    const juce::String s = juce::SystemStats::getUniqueDeviceID() + "|" + k.id + "|" + k.hashHex + "|ROOM-X";
    return juce::SHA256 (s.toUTF8()).toHexString();
}

void LocalPasswordLicenseProvider::setUnlocked (const KeyEntry& k)
{
    currentKey  = &k;
    currentKind = k.kind;
    unlocked.store (true);
}

void LocalPasswordLicenseProvider::tryAutoUnlock()
{
    const auto id = props->getValue ("key");
    const auto token = props->getValue ("token");
    if (id.isEmpty() || token.isEmpty()) return;
    for (auto& k : kKeys)
        if (id == k.id && token == tokenFor (k) && ! isExpired (k))
        {
            setUnlocked (k);
            return;
        }
}

int LocalPasswordLicenseProvider::matchPassword (const juce::String& password, bool& expired) const
{
    const auto pw = password.trim();
    expired = false;
    for (int i = 0; i < numKeys(); ++i)
    {
        const auto& k = kKeys[i];
        const auto expected = fromHex (k.hashHex);
        const auto got = pbkdf2 (pw, fromHex (k.saltHex), kIterations, (int) expected.getSize());
        if (! constantTimeEquals (got, expected)) continue;
        if (isExpired (k)) { expired = true; continue; }
        return i;
    }
    return -1;
}

Result LocalPasswordLicenseProvider::applyMatch (int index, bool expired, bool remember)
{
    if (index < 0 || index >= numKeys())
        return expired ? Result::Expired : Result::Wrong;

    const auto& k = kKeys[index];
    setUnlocked (k);
    if (remember)
    {
        props->setValue ("key", juce::String (k.id));
        props->setValue ("token", tokenFor (k));
    }
    else
    {
        props->removeValue ("key");
        props->removeValue ("token");
    }
    props->saveIfNeeded();
    return Result::Ok;
}

void LocalPasswordLicenseProvider::lock()
{
    unlocked.store (false);
    currentKind = Kind::None;
    currentKey = nullptr;
    props->removeValue ("key");
    props->removeValue ("token");
    props->saveIfNeeded();
}

#if defined (ROOMX_ALLOW_TEST_UNLOCK)
void LocalPasswordLicenseProvider::unlockForTests() { setUnlocked (kKeys[0]); }
#endif

void LocalPasswordLicenseProvider::checkExpiry()
{
    if (unlocked.load() && currentKey != nullptr && isExpired (*currentKey))
        lock();
}

juce::String LocalPasswordLicenseProvider::statusText() const
{
    if (! unlocked.load() || currentKey == nullptr) return {};
    if (currentKind == Kind::Master) return piradex::i18n::tr ("Licença completa", "Full license");
    return piradex::i18n::tr ("BETA · válida até ", "BETA · valid until ")
         + juce::String (currentKey->expDay).paddedLeft ('0', 2) + "/"
         + juce::String (currentKey->expMonth).paddedLeft ('0', 2) + "/" + juce::String (currentKey->expYear);
}

// ---------------------------------------------------------------------------
ILicenseProvider& License::get()
{
    static LocalPasswordLicenseProvider instance;
    return instance;
}

namespace
{
    std::atomic<int>   failures { 0 };
    std::atomic<juce::int64> lockedUntilMs { 0 };
}

bool License::canAttempt (int& secondsLeft)
{
    const auto now = juce::Time::currentTimeMillis();
    const auto until = lockedUntilMs.load();
    secondsLeft = until > now ? (int) ((until - now + 999) / 1000) : 0;
    return secondsLeft == 0;
}

void License::registerFailure()
{
    if (++failures >= 5)
    {
        failures = 0;
        lockedUntilMs = juce::Time::currentTimeMillis() + 30000;
    }
}

void License::registerSuccess() { failures = 0; }
} // namespace roomx::lic
