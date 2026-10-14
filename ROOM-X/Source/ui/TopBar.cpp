#include "TopBar.h"
#include "../PluginProcessor.h"
#include "../dsp/ReverbEngine.h"

namespace roomx::ui
{
namespace P = roomx::params;

// Marca ROOM-X: "ROOM" + X geométrico (dois traços cruzados, um no acento)
void drawRoomXLogo (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour accent, float h, bool byline)
{
    const auto f = Fonts::title (h);
    const juce::String room ("ROOM");
    const float rw = juce::GlyphArrangement::getStringWidth (f.withExtraKerningFactor (0.06f), room);
    const float xw = h * 0.62f;
    const float y = area.getCentreY() - h * 0.5f + (byline ? -h * 0.18f : 0.0f);
    float x = area.getX();
    g.setColour (col::text);
    g.setFont (f.withExtraKerningFactor (0.06f));
    g.drawText (room, juce::Rectangle<float> (x, y, rw + 4.0f, h), juce::Justification::centredLeft);
    x += rw + h * 0.10f;
    // hífen curto
    g.fillRect (x, y + h * 0.52f, h * 0.20f, h * 0.085f);
    x += h * 0.30f;
    // X: traço prateado + traço de acento
    const float t = h * 0.13f, top = y + h * 0.17f, bot = y + h * 0.85f;
    g.setColour (col::silver);
    g.drawLine (x, top, x + xw, bot, t);
    g.setColour (accent);
    g.drawLine (x + xw, top, x, bot, t);
    g.setColour (accent.withAlpha (0.25f));
    g.drawLine (x + xw, top, x, bot, t * 2.6f);

    if (byline)
    {
        g.setColour (col::textDim);
        g.setFont (Fonts::caps (h * 0.34f, 0.22f));
        g.drawText ("BY PIRADEX", juce::Rectangle<float> (area.getX() + 1.0f, y + h * 0.98f, rw + xw + h, h * 0.4f), juce::Justification::centredLeft);
    }
}

// =============================================================================
TopBar::TopBar (Context& c, state::PresetManager& pm) : ctx (c), presets (pm)
{
    for (auto* b : { &undo, &redo, &prev, &next, &save, &info, &eco, &tuning, &copyAB })
    {
        b->accent = &c.accent;
        addAndMakeVisible (b);
    }
    eco.setClickingTogglesState (true);
    ecoAtt = std::make_unique<APVTS::ButtonAttachment> (c.apvts, P::eco, eco);
    eco.onColour = juce::Colour (0xff7bd88f);

    undo.onClick = [this] { ctx.undo().undo(); };
    redo.onClick = [this] { ctx.undo().redo(); };
    prev.onClick = [this] { presets.loadPrevious(); repaint(); };
    next.onClick = [this] { presets.loadNext(); repaint(); };
    save.onClick = [this] { showSaveMenu(); };
    info.onClick = [this] { if (onInfo) onInfo(); };
    tuning.onClick = [this] { if (onTuning) onTuning(); };

    for (auto* b : { &slotA, &slotB })
    {
        b->setClickingTogglesState (false);
        b->getProperties().set ("fontSize", 15.0);
        b->getProperties().set ("corner", 14.0);
        addAndMakeVisible (b);
    }
    slotA.onClick = [this] { presets.selectSlot (0); repaint(); };
    slotB.onClick = [this] { presets.selectSlot (1); repaint(); };
    copyAB.onClick = [this] { if (presets.activeSlot() == 0) presets.copyAtoB(); else presets.copyBtoA(); };
    copyAB.drawBackground = false;
}

void TopBar::resized()
{
    auto r = getLocalBounds().reduced (16, 9);
    logoArea = r.removeFromLeft (190).toFloat();
    r.removeFromLeft (10);
    undo.setBounds (r.removeFromLeft (34)); r.removeFromLeft (6);
    redo.setBounds (r.removeFromLeft (34)); r.removeFromLeft (18);

    auto right = r.removeFromRight (200);
    eco.setBounds (right.removeFromRight (34)); right.removeFromRight (6);
    info.setBounds (right.removeFromRight (34)); right.removeFromRight (6);
    tuning.setBounds (right.removeFromRight (34));
    r.removeFromRight (14);
    auto ab = r.removeFromRight (124);
    slotA.setBounds (ab.removeFromLeft (44));
    copyAB.setBounds (ab.removeFromLeft (36).reduced (2));
    slotB.setBounds (ab.removeFromLeft (44));
    r.removeFromRight (16);

    save.setBounds (r.removeFromRight (34)); r.removeFromRight (10);
    presetBox = r.toFloat();
    auto pb = presetBox.toNearestInt();
    prev.setBounds (pb.removeFromLeft (34).reduced (3));
    next.setBounds (pb.removeFromLeft (34).reduced (3));
    prev.drawBackground = next.drawBackground = false;
}

void TopBar::tick()
{
    undo.setEnabled (ctx.undo().canUndo());
    redo.setEnabled (ctx.undo().canRedo());
    slotA.setToggleState (presets.activeSlot() == 0, juce::dontSendNotification);
    slotB.setToggleState (presets.activeSlot() == 1, juce::dontSendNotification);
    copyAB.setTooltip (presets.activeSlot() == 0 ? U ("Copiar A para B") : U ("Copiar B para A"));
    tuning.showDot = std::abs (ParamRef (ctx.apvts, P::tuneRef).get() - 440.0f) > 0.05f
                  || std::abs (ParamRef (ctx.apvts, P::tuneCents).get()) > 0.05f;
    const bool d = presets.isDirty();
    if (d != dirty || shownName != presets.currentName()) { dirty = d; shownName = presets.currentName(); repaint(); }
    tuning.repaint();
}

void TopBar::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff15181e), 0, 0, juce::Colour (0xff0e1014), 0, (float) getHeight(), false));
    g.fillAll();
    g.setColour (col::line);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    drawRoomXLogo (g, logoArea, ctx.accent.current, 24.0f, true);

    // caixa de preset
    g.setColour (col::well);
    g.fillRoundedRectangle (presetBox, 17.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (presetBox.reduced (0.5f), 17.0f, 1.0f);
    auto t = presetBox.withTrimmedLeft (78.0f).withTrimmedRight (34.0f);
    g.setColour (col::line);
    g.fillRect (presetBox.getX() + 70.0f, presetBox.getY() + 8.0f, 1.0f, presetBox.getHeight() - 16.0f);

    const auto& list = presets.entries();
    const int idx = presets.currentIndex();
    juce::String bank = juce::isPositiveAndBelow (idx, list.size()) ? list[idx].bank : juce::String();
    g.setColour (col::text);
    g.setFont (Fonts::label (15.5f));
    const auto name = presets.currentName() + (dirty ? " *" : "");
    g.drawText (name, t, juce::Justification::centredLeft);
    if (bank.isNotEmpty())
    {
        g.setColour (col::textFaint);
        g.setFont (Fonts::text (13.0f));
        g.drawText (bank, t, juce::Justification::centredRight);
    }
    IconButton::drawIcon (g, Icon::Menu, presetBox.removeFromRight (34.0f).reduced (8.0f), col::textDim);
}

void TopBar::mouseDown (const juce::MouseEvent& e)
{
    if (presetBox.withTrimmedLeft (70.0f).contains (e.position)) showPresetMenu();
}

void TopBar::showPresetMenu()
{
    const auto& list = presets.entries();
    juce::PopupMenu root;
    juce::StringArray banks;
    for (auto& e : list) banks.addIfNotAlreadyThere (e.bank);
    for (auto& b : banks)
    {
        juce::PopupMenu sub;
        for (int i = 0; i < list.size(); ++i)
            if (list[i].bank == b)
                sub.addItem (i + 1, list[i].name, true, i == presets.currentIndex());
        root.addSubMenu (b, sub);
    }
    root.addSeparator();
    root.addItem (10001, U ("Guardar como…"));
    root.addItem (10002, U ("Importar preset…"));
    root.addItem (10003, U ("Abrir pasta de presets"));
    root.addItem (10004, "Actualizar lista");
    root.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (presetBox.toNearestInt())).withMinimumWidth ((int) presetBox.getWidth()),
                        [this] (int r)
    {
        if (r <= 0) return;
        if (r <= 10000) { presets.load (r - 1); repaint(); return; }
        if (r == 10001) saveAs();
        if (r == 10002)
        {
            auto chooser = std::make_shared<juce::FileChooser> ("Importar preset ROOM-X", juce::File(), "*.roomx");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser] (const juce::FileChooser& fc)
            {
                if (fc.getResult().existsAsFile()) { presets.importFile (fc.getResult()); repaint(); }
            });
        }
        if (r == 10003) { auto f = state::PresetManager::userFolder(); f.createDirectory(); f.startAsProcess(); }
        if (r == 10004) { presets.refresh(); repaint(); }
    });
}

void TopBar::showSaveMenu()
{
    const int idx = presets.currentIndex();
    const bool canOverwrite = juce::isPositiveAndBelow (idx, presets.entries().size()) && ! presets.entries()[idx].factory;
    juce::PopupMenu m;
    m.addItem (1, "Guardar", canOverwrite);
    m.addItem (2, U ("Guardar como…"));
    m.addItem (3, U ("Importar preset…"));
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (save), [this] (int r)
    {
        if (r == 1) { presets.saveCurrent(); repaint(); }
        if (r == 2) saveAs();
        if (r == 3)
        {
            auto chooser = std::make_shared<juce::FileChooser> ("Importar preset ROOM-X", juce::File(), "*.roomx");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this, chooser] (const juce::FileChooser& fc)
            {
                if (fc.getResult().existsAsFile()) { presets.importFile (fc.getResult()); repaint(); }
            });
        }
    });
}

void TopBar::saveAs()
{
    auto* w = new juce::AlertWindow (U ("Guardar preset"), U ("Nome do preset:"), juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", presets.currentName());
    w->addButton ("Guardar", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancelar", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<TopBar> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int r)
    {
        if (r == 1 && safe != nullptr)
        {
            safe->presets.saveUser (w->getTextEditorContents ("name"));
            safe->repaint();
        }
    }), true);
}

// =============================================================================
//  SpaceSelector
// =============================================================================
SpaceSelector::SpaceSelector (Context& c)
    : ctx (c), space (c.apvts, P::space), decay (c.apvts, P::decay), size (c.apvts, P::size)
{
    for (auto* b : { &prev, &next }) { b->accent = &c.accent; b->drawBackground = false; addAndMakeVisible (b); }
    prev.onClick = [this] { space.setOnce ((float) (((int) space.get() + dsp::kNumSpaces - 1) % dsp::kNumSpaces)); };
    next.onClick = [this] { space.setOnce ((float) (((int) space.get() + 1) % dsp::kNumSpaces)); };
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void SpaceSelector::resized()
{
    box = getLocalBounds().toFloat().withSizeKeepingCentre (380.0f, (float) getHeight() - 4.0f);
    prev.setBounds (box.toNearestInt().removeFromLeft (30).reduced (2));
    next.setBounds (box.toNearestInt().removeFromRight (30).reduced (2));
}

void SpaceSelector::paint (juce::Graphics& g)
{
    const int idx = juce::jlimit (0, dsp::kNumSpaces - 1, (int) space.get());
    const auto& s = dsp::space (idx);
    g.setColour (hot ? col::panelHi : col::panel);
    g.fillRoundedRectangle (box, box.getHeight() * 0.5f);
    g.setColour (col::line);
    g.drawRoundedRectangle (box.reduced (0.5f), box.getHeight() * 0.5f, 1.0f);

    auto t = box.reduced (34.0f, 0.0f);
    g.setColour (col::textFaint);
    g.setFont (Fonts::caps (11.0f, 0.12f));
    const juce::String cat = U (dsp::categoryName (s.category)).toUpperCase();
    g.drawText (cat, t.removeFromLeft (92.0f), juce::Justification::centredLeft);
    const float rt = dsp::ReverbEngine::effectiveRt60 (idx, decay.get(), size.get() / 100.0f);
    g.setColour (col::textDim);
    g.setFont (Fonts::label (14.0f));
    g.drawText (juce::String (rt, rt < 10.0f ? 2 : 1) + " s", t.removeFromRight (64.0f), juce::Justification::centredRight);
    g.setColour (col::text);
    g.setFont (Fonts::label (16.0f));
    g.drawText (s.name, t, juce::Justification::centred);
}

void SpaceSelector::mouseDown (const juce::MouseEvent&)
{
    juce::PopupMenu root;
    const int cur = (int) space.get();
    for (int c = 0; c < 5; ++c)
    {
        root.addSectionHeader (U (dsp::categoryName ((dsp::SpaceCategory) c)));
        for (int i = 0; i < dsp::kNumSpaces; ++i)
            if ((int) dsp::space (i).category == c)
            {
                juce::PopupMenu::Item it (dsp::space (i).name);
                it.itemID = i + 1;
                it.isTicked = i == cur;
                root.addItem (it);
            }
    }
    root.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (box.toNearestInt())).withMinimumWidth ((int) box.getWidth()),
                        [this] (int r) { if (r > 0) space.setOnce ((float) (r - 1)); });
}
} // namespace roomx::ui
