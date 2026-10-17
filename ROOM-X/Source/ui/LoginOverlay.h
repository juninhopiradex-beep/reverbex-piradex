// =============================================================================
//  ROOM-X — ecrã de login (bloqueia a UI até haver autorização)
//  A verificação PBKDF2 corre numa thread à parte; o resultado é aplicado na
//  thread de mensagens. Enquanto bloqueado, o áudio passa intacto.
// =============================================================================
#pragma once

#include "Context.h"

namespace roomx::ui
{
class LoginOverlay : public juce::Component, private juce::Timer, private piradex::i18n::Listener
{
public:
    explicit LoginOverlay (Accent& a);
    ~LoginOverlay() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void mouseDown (const juce::MouseEvent&) override {}   // engole cliques

    std::function<void()> onUnlocked;

private:
    Accent& accent;
    juce::TextEditor password;
    juce::ToggleButton remember;
    juce::TextButton enter;
    LanguageSwitch language { &accent };
    juce::Label message;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
    bool busy = false;

    juce::Rectangle<int> card() const;
    void submit();
    void timerCallback() override;
    void languageChanged() override;
};
} // namespace roomx::ui
