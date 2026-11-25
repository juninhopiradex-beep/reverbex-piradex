#include "LoginOverlay.h"
#include "../licensing/License.h"

namespace roomx::ui
{
LoginOverlay::LoginOverlay (Accent& a) : accent (a), screen ("ROOM-X", col::all, ROOMX_VERSION_STRING)
{
    screen.onSubmit = [this] (const juce::String& pw, bool rem) { submit (pw, rem); };
    addAndMakeVisible (screen);
}

LoginOverlay::~LoginOverlay()
{
    alive->store (false);
    stopTimer();
}

void LoginOverlay::resized() { screen.setBounds (getLocalBounds()); }

void LoginOverlay::visibilityChanged()
{
    if (isVisible() && isShowing()) screen.focusField();
}

void LoginOverlay::timerCallback()
{
    int secs = 0;
    if (! lic::License::canAttempt (secs))
        screen.setStatus (tr ("Demasiadas tentativas. Tente de novo dentro de ", "Too many attempts. Try again in ") + juce::String (secs) + " s.", true);
    else
    {
        stopTimer();
        screen.setStatus ({}, false);
        screen.setBusy (false);
    }
}

void LoginOverlay::submit (const juce::String& pw, bool rem)
{
    if (busy) return;
    int secs = 0;
    if (! lic::License::canAttempt (secs)) { startTimer (500); timerCallback(); return; }
    busy = true;
    screen.setBusy (true);
    auto flag = alive;
    juce::Component::SafePointer<LoginOverlay> safe (this);

    // PBKDF2 (60 000 iteracoes por chave) fora da thread de mensagens
    juce::Thread::launch ([pw, rem, flag, safe]
    {
        bool expired = false;
        const int idx = lic::License::get().matchPassword (pw, expired);
        piradex::pcc::Check online;                                       // chave de licenca: pergunta ao PCC
        if (idx < 0 && ! expired) online = lic::License::get().checkOnline (pw);
        juce::MessageManager::callAsync ([idx, expired, rem, flag, safe, online]
        {
            if (! flag->load() || safe == nullptr) return;
            auto& self = *safe;
            self.busy = false;
            self.screen.setBusy (false);
            const auto r = online.outcome != piradex::pcc::Outcome::Disabled ? lic::License::get().applyOnline (online, rem)
                                                                             : lic::License::get().applyMatch (idx, expired, rem);
            if (r == lic::Result::Ok)
            {
                lic::License::registerSuccess();
                self.screen.clearField();
                self.screen.setStatus ({}, false);
                if (self.onUnlocked) self.onUnlocked();
                return;
            }
            lic::License::registerFailure();
            self.screen.setStatus (r == lic::Result::Online ? lic::License::get().onlineText()
                                   : r == lic::Result::Expired ? tr ("Esta senha BETA j\xc3\xa1 expirou.", "This BETA password has expired.")
                                                               : tr ("Senha incorrecta.", "Incorrect password."), true);
            int s = 0;
            if (! lic::License::canAttempt (s)) { self.screen.setBusy (true); self.startTimer (500); }
        });
    });
}
} // namespace roomx::ui
