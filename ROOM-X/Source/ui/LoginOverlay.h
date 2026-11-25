// =============================================================================
//  ROOM-X — ecra de login (1.0.2: ecra unico Piradex do kit partilhado)
//  A verificacao PBKDF2 corre numa thread a parte; o resultado e aplicado na
//  thread de mensagens. Enquanto bloqueado, o audio passa intacto.
// =============================================================================
#pragma once

#include "Context.h"
#include "../i18n/PiradexKitUI.h"

namespace roomx::ui
{
class LoginOverlay : public juce::Component, private juce::Timer
{
public:
    explicit LoginOverlay (Accent& a);
    ~LoginOverlay() override;
    void resized() override;
    void visibilityChanged() override;
    void mouseDown (const juce::MouseEvent&) override {}   // engole cliques

    std::function<void()> onUnlocked;

private:
    Accent& accent;
    piradex::kit::LoginScreen screen;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    bool busy = false;

    void submit (const juce::String& pw, bool rem);
    void timerCallback() override;
};
} // namespace roomx::ui
