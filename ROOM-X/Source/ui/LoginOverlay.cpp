#include "LoginOverlay.h"
#include "TopBar.h"
#include "../licensing/License.h"

namespace roomx::ui
{
LoginOverlay::LoginOverlay (Accent& a) : accent (a)
{
    password.setPasswordCharacter ((juce::juce_wchar) 0x2022);
    password.setFont (Fonts::label (19.0f));
    password.setJustification (juce::Justification::centred);
    password.setColour (juce::TextEditor::outlineColourId, col::line);
    password.setColour (juce::TextEditor::focusedOutlineColourId, col::all);
    password.setColour (juce::TextEditor::backgroundColourId, col::well);
    password.onReturnKey = [this] { submit(); };
    addAndMakeVisible (password);

    remember.setToggleState (true, juce::dontSendNotification);
    addAndMakeVisible (remember);

    enter.getProperties().set ("fontSize", 15.0);
    enter.setToggleState (true, juce::dontSendNotification);
    enter.onClick = [this] { submit(); };
    addAndMakeVisible (enter);

    message.setJustificationType (juce::Justification::centred);
    message.setFont (Fonts::text (14.0f));
    addAndMakeVisible (message);
    addAndMakeVisible (language);
    piradex::i18n::addListener (this);
    languageChanged();
}

void LoginOverlay::languageChanged()
{
    password.setTextToShowWhenEmpty (tr ("Senha", "Password"), col::textFaint);
    remember.setButtonText (tr ("Lembrar este computador", "Remember this computer"));
    enter.setButtonText (tr ("Entrar", "Sign in"));
    message.setText ({}, juce::dontSendNotification);
    password.repaint();
    repaint();
}

LoginOverlay::~LoginOverlay()
{
    piradex::i18n::removeListener (this);
    alive->store (false);
    stopTimer();
}

juce::Rectangle<int> LoginOverlay::card() const { return getLocalBounds().withSizeKeepingCentre (460, 360); }

void LoginOverlay::paint (juce::Graphics& g)
{
    g.fillAll (col::bg.withAlpha (0.94f));
    const auto c = card().toFloat();
    juce::DropShadow (juce::Colours::black.withAlpha (0.7f), 40, { 0, 10 }).drawForRectangle (g, c.toNearestInt());
    g.setColour (col::panel);
    g.fillRoundedRectangle (c, 14.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (c.reduced (0.5f), 14.0f, 1.0f);

    // faixa tricolor (Early / All / Late)
    auto strip = c.withHeight (3.0f).reduced (14.0f, 0.0f).translated (0, 1.0f);
    const float w3 = strip.getWidth() / 3.0f;
    g.setColour (col::early); g.fillRect (strip.removeFromLeft (w3));
    g.setColour (col::all);   g.fillRect (strip.removeFromLeft (w3));
    g.setColour (col::late);  g.fillRect (strip);

    auto t = c.reduced (28.0f, 26.0f);
    auto logo = t.removeFromTop (64.0f);
    drawRoomXLogo (g, logo.withSizeKeepingCentre (190.0f, 64.0f), accent.current, 34.0f, true);
    t.removeFromTop (8.0f);
    g.setColour (col::textDim);
    g.setFont (Fonts::text (14.5f));
    g.drawText (tr ("Introduza a sua senha para começar", "Enter your password to get started"), t.removeFromTop (24.0f), juce::Justification::centred);
}

void LoginOverlay::resized()
{
    auto c = card().reduced (48, 0);
    c.removeFromTop (138);
    password.setBounds (c.removeFromTop (42));
    c.removeFromTop (14);
    remember.setBounds (c.removeFromTop (26).withSizeKeepingCentre (220, 26));
    c.removeFromTop (14);
    enter.setBounds (c.removeFromTop (40).withSizeKeepingCentre (170, 40));
    c.removeFromTop (8);
    message.setBounds (c.removeFromTop (40));
    language.setBounds (card().removeFromBottom (44).removeFromRight (96).reduced (14, 10));
}

void LoginOverlay::visibilityChanged()
{
    if (isVisible() && isShowing())
        password.grabKeyboardFocus();
}

void LoginOverlay::timerCallback()
{
    int secs = 0;
    if (! lic::License::canAttempt (secs))
        message.setText (tr ("Demasiadas tentativas. Tente de novo dentro de ", "Too many attempts. Try again in ") + juce::String (secs) + " s.", juce::dontSendNotification);
    else
    {
        stopTimer();
        message.setText ({}, juce::dontSendNotification);
        enter.setEnabled (true);
    }
}

void LoginOverlay::submit()
{
    if (busy || password.isEmpty()) return;
    int secs = 0;
    if (! lic::License::canAttempt (secs)) { startTimer (500); timerCallback(); return; }

    busy = true;
    enter.setEnabled (false);
    message.setColour (juce::Label::textColourId, col::textDim);
    message.setText (tr ("A verificar…", "Checking…"), juce::dontSendNotification);

    const auto pw = password.getText();
    const bool rem = remember.getToggleState();
    auto flag = alive;
    juce::Component::SafePointer<LoginOverlay> safe (this);

    // PBKDF2 (60 000 iterações por chave) fora da thread de mensagens
    juce::Thread::launch ([pw, rem, flag, safe]
    {
        bool expired = false;
        const int idx = lic::License::get().matchPassword (pw, expired);
        juce::MessageManager::callAsync ([idx, expired, rem, flag, safe]
        {
            if (! flag->load() || safe == nullptr) return;
            auto& self = *safe;
            self.busy = false;
            self.enter.setEnabled (true);
            const auto r = lic::License::get().applyMatch (idx, expired, rem);
            if (r == lic::Result::Ok)
            {
                lic::License::registerSuccess();
                self.password.clear();
                self.message.setText ({}, juce::dontSendNotification);
                if (self.onUnlocked) self.onUnlocked();
                return;
            }
            lic::License::registerFailure();
            self.password.selectAll();
            self.message.setColour (juce::Label::textColourId, col::danger);
            self.message.setText (r == lic::Result::Expired ? tr ("Esta senha BETA já expirou.", "This BETA password has expired.") : tr ("Senha incorrecta.", "Incorrect password."),
                                  juce::dontSendNotification);
            int s = 0;
            if (! lic::License::canAttempt (s)) { self.enter.setEnabled (false); self.startTimer (500); }
        });
    });
}
} // namespace roomx::ui
