#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "licensing/License.h"

using namespace roomx;
namespace P = roomx::params;

static const juce::Identifier kStateVersion { "stateVersion" };
static const juce::Identifier kUseFlats     { "useFlats" };
static constexpr int kCurrentStateVersion = 1;

RoomXProcessor::RoomXProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, &undoManager, "ROOMX", P::createLayout())
{
    auto f = [this] (const char* id) { auto* p = apvts.getRawParameterValue (id); jassert (p != nullptr); return p; };
    raw.input = f (P::input);         raw.output = f (P::output);       raw.mix = f (P::mix);
    raw.killDry = f (P::killDry);     raw.soloBank = f (P::soloBank);     raw.testSignal = f (P::testSignal);     raw.preDelay = f (P::preDelay);   raw.preDelaySync = f (P::preDelaySync);
    raw.preDelayDiv = f (P::preDelayDiv); raw.decay = f (P::decay);     raw.size = f (P::size);
    raw.position = f (P::position);   raw.density = f (P::density);     raw.offset = f (P::offset);
    raw.temper = f (P::temper);       raw.noteWidth = f (P::noteWidth); raw.target = f (P::target);
    raw.rangeLow = f (P::rangeLow);   raw.rangeHigh = f (P::rangeHigh); raw.lowCut = f (P::lowCut);
    raw.lowCutSlope = f (P::lowCutSlope); raw.highCut = f (P::highCut); raw.highCutSlope = f (P::highCutSlope);
    raw.space = f (P::space);         raw.dualMono = f (P::dualMono);   raw.bypass = f (P::bypass);
    raw.eco = f (P::eco);             raw.noteMode = f (P::noteMode);   raw.notes = f (P::notes);
    raw.midiOctave = f (P::midiOctave); raw.midiRelease = f (P::midiRelease);
    raw.seqBars = f (P::seqBars);     raw.seqTranspose = f (P::seqTranspose); raw.seqArmed = f (P::seqArmed);
    raw.seqOffset = f (P::seqOffset); raw.tuneRef = f (P::tuneRef);     raw.tuneCents = f (P::tuneCents);

    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (P::bypass));

    apvts.state.setProperty (kStateVersion, kCurrentStateVersion, nullptr);
    sequence.tree();                         // cria a sequência por defeito
    presets = std::make_unique<state::PresetManager> (apvts, sequence);
    undoManager.clearUndoHistory();

    apvts.state.addListener (this);
    publishSequence();
    ui.licensed = lic::License::get().isUnlocked();
}

RoomXProcessor::~RoomXProcessor()
{
    alive->store (false);
    apvts.state.removeListener (this);
    cancelPendingUpdate();
}

// =============================================================================
void RoomXProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    preparedBlock = juce::jmax (64, samplesPerBlock);
    dryBuf.setSize (2, preparedBlock, false, true, false);
    wetBuf.setSize (2, preparedBlock, false, true, false);
    rawBuf.setSize (2, preparedBlock, false, true, false);
    engine.prepare (sampleRate, preparedBlock);
    measBuf.assign ((size_t) (12.0 * sampleRate), 0.0f);   // alocação fora do áudio
    measState.store (0);
    measIdx = measFrom = 0;
    testSample = 0;
    cacheValid = false;
    energy0.store (std::numeric_limits<float>::quiet_NaN());
    energy1.store (std::numeric_limits<float>::quiet_NaN());
    abTrimDb.store (0.0f);

    for (auto* s : { &inGainSm, &outGainSm, &dryGainSm, &wetGainSm })
        s->prepare (sampleRate, 25.0);
    bypassStep = (float) (1.0 / (0.015 * sampleRate));     // rampa linear de 15 ms
    inGainSm.reset (dsp::dbToGain (raw.input->load()));
    outGainSm.reset (dsp::dbToGain (raw.output->load()));
    dryGainSm.reset (1.0f);
    wetGainSm.reset (0.0f);
    bypassMix = raw.bypass->load() > 0.5f ? 1.0f : 0.0f;

    midiState.reset();
    seqEngine.reset();
    analysisFifo.reset();
}

void RoomXProcessor::releaseResources()
{
    engine.reset();
}

bool RoomXProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in  = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    if (in  != juce::AudioChannelSet::mono() && in  != juce::AudioChannelSet::stereo()) return false;
    return in.size() <= out.size();
}

double RoomXProcessor::getTailLengthSeconds() const
{
    const float rt = dsp::ReverbEngine::effectiveRt60 ((int) raw.space->load(), raw.decay->load(), raw.size->load() / 100.0f);
    return juce::jmin (60.0, (double) rt * 1.2 + raw.preDelay->load() * 0.001);
}

music::Tuning RoomXProcessor::currentTuning() const
{
    music::Tuning t;
    t.referenceHz = raw.tuneRef->load();
    t.fineCents   = raw.tuneCents->load();
    return t;
}

bool RoomXProcessor::useFlats() const          { return (bool) apvts.state.getProperty (kUseFlats, false); }
void RoomXProcessor::setUseFlats (bool b)      { apvts.state.setProperty (kUseFlats, b, nullptr); }

// =============================================================================
//  Sequência: thread de mensagens -> áudio
// =============================================================================
void RoomXProcessor::valueTreePropertyChanged (juce::ValueTree& t, const juce::Identifier&)
{
    if (t.hasType (state::ids::sequence) || t.hasType (state::ids::row) || t.hasType (state::ids::region))
        triggerAsyncUpdate();
}

void RoomXProcessor::handleAsyncUpdate()
{
    publishSequence();
}

void RoomXProcessor::publishSequence()
{
    const auto d = sequence.compile();
    {
        const juce::SpinLock::ScopedLockType sl (seqLock);
        seqShared = d;
    }
    ++seqVersion;
}

// =============================================================================
// ---- ruído rosa (Paul Kellett, filtro de 7 polos)
float RoomXProcessor::nextPink() noexcept
{
    testSeed = testSeed * 1664525u + 1013904223u;
    const float w = (float) (int32_t) testSeed * (1.0f / 2147483648.0f);
    const float out = pink[0] + pink[1] + pink[2] + pink[3] + pink[4] + pink[5] + pink[6] + w * 0.5362f;
    pink[0] = 0.99886f * pink[0] + w * 0.0555179f;
    pink[1] = 0.99332f * pink[1] + w * 0.0750759f;
    pink[2] = 0.96900f * pink[2] + w * 0.1538520f;
    pink[3] = 0.86650f * pink[3] + w * 0.3104856f;
    pink[4] = 0.55000f * pink[4] + w * 0.5329522f;
    pink[5] = -0.7616f * pink[5] - w * 0.0168980f;
    pink[6] = w * 0.115926f;
    return out * 0.11f;
}

// ---- medição do RT60 ao vivo
void RoomXProcessor::armRt60Measurement()
{
    // o impulso de teste exige o modo "Impulso"
    if (auto* p = apvts.getParameter (roomx::params::testSignal))
        p->setValueNotifyingHost (p->convertTo0to1 (1.0f));
    measIdx = 0;
    measFrom = 0;
    measSlot = presets ? presets->activeSlot() : 0;   // a energia medida fica associada a este slot
    cacheValid = false;
    measState.store (1);
}

double RoomXProcessor::rt60Result() const
{
    if (measState.load() != 3) return -1.0;
    if (cacheValid) return cachedRt60;

    // integral inversa de Schroeder da energia da resposta
    const int n = (int) measBuf.size();
    std::vector<double> edc ((size_t) n + 1, 0.0);
    for (int i = n - 1; i >= 0; --i)
        edc[(size_t) i] = edc[(size_t) i + 1] + (double) measBuf[(size_t) i] * (double) measBuf[(size_t) i];
    double result = -1.0;
    if (edc[0] > 0.0)
    {
        // ajuste linear entre -5 e -25 dB; RT60 = -60 dB / declive
        int i5 = -1, i25 = -1;
        for (int i = 0; i < n; ++i)
        {
            const double db = 10.0 * std::log10 (edc[(size_t) i] / edc[0] + 1.0e-30);
            if (i5 < 0 && db <= -5.0)  i5 = i;
            if (i5 >= 0 && db <= -25.0) { i25 = i; break; }
        }
        if (i5 >= 0 && i25 - i5 >= 8)
        {
            const double fs = currentRate.load();
            double sx = 0, sy = 0, sxx = 0, sxy = 0;
            const int m = i25 - i5 + 1;
            for (int i = i5; i <= i25; ++i)
            {
                const double t = i / fs;
                const double y = 10.0 * std::log10 (edc[(size_t) i] / edc[0] + 1.0e-30);
                sx += t; sy += y; sxx += t * t; sxy += t * y;
            }
            const double slope = (m * sxy - sx * sy) / (m * sxx - sx * sx);
            if (slope < -1.0e-6)
                result = -60.0 / slope;
        }
    }
    // energia total da resposta, por slot (base da igualação A/B)
    double sumSq = 0.0;
    for (float v : measBuf) sumSq += (double) v * (double) v;
    (measSlot == 0 ? energy0 : energy1).store ((float) (10.0 * std::log10 (sumSq + 1.0e-30)));

    cachedRt60 = result;
    cacheValid = true;
    return result;
}

void RoomXProcessor::applyAbMatch() noexcept
{
    float trim = 0.0f;
    if (abMatchOn.load() && presets != nullptr && presets->activeSlot() == 1)
    {
        const float e0 = energy0.load(), e1 = energy1.load();
        if (std::isfinite (e0) && std::isfinite (e1))
            trim = juce::jlimit (-12.0f, 12.0f, e0 - e1);   // B sobe ou desce até ao nível de A
    }
    abTrimDb.store (trim);
}

void RoomXProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);

    // ---- sem licença: totalmente transparente (sessões antigas nunca ficam mudas)
    const bool licensed = lic::License::get().isUnlocked();
    ui.licensed = licensed;
    if (! licensed)
    {
        if (numIn == 1 && numOut > 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
        midi.clear();
        return;
    }

    // ---- MIDI (todas as entradas/canais)
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn())                       { midiState.noteOn (m.getNoteNumber()); ++ui.midiActivity; }
        else if (m.isNoteOff())                 { midiState.noteOff (m.getNoteNumber()); ++ui.midiActivity; }
        else if (m.isSustainPedalOn())          midiState.sustain (true);
        else if (m.isSustainPedalOff())         midiState.sustain (false);
        else if (m.isAllNotesOff() || m.isAllSoundOff()) midiState.allNotesOff();
    }
    midi.clear();

    // ---- transporte
    double ppq = 0.0, bpm = 120.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
        }
    bpm = juce::jlimit (20.0, 999.0, bpm);
    ui.bpm = (float) bpm;
    if (playing && ! wasPlaying) retrigPpq = 0.0;    // determinístico em cada Play/bounce
    wasPlaying = playing;

    // ---- sequência (cópia sem bloquear)
    const int v = seqVersion.load();
    if (v != seqLocalVersion)
    {
        const juce::SpinLock::ScopedTryLockType tl (seqLock);
        if (tl.isLocked()) { seqLocal = seqShared; seqLocalVersion = v; }
    }

    music::SequenceTransport tr;
    tr.hostPlaying = playing;
    tr.ppq = ppq;
    tr.bars = (int) raw.seqBars->load();
    tr.offsetBeats = raw.seqOffset->load();
    tr.armed = raw.seqArmed->load() > 0.5f;
    tr.transpose = (int) raw.seqTranspose->load();
    if (retrigRequest.exchange (false))
        retrigPpq = playing ? ppq - tr.offsetBeats : 0.0;
    tr.retrigPpq = retrigPpq;

    // ---- notas seleccionadas
    const auto mode = (P::NoteMode) (int) raw.noteMode->load();
    music::NoteSet noteSet;
    float releaseMs = 45.0f;
    if (mode == P::NoteMode::Manual)
    {
        noteSet = music::NoteSet::fromPitchClasses ((uint16_t) (int) raw.notes->load());
        ui.seqPosition = -1.0f;
    }
    else if (mode == P::NoteMode::Sequence)
    {
        const auto st = seqEngine.evaluate (seqLocal, tr);
        noteSet = music::NoteSet::fromPitchClasses (st.mask);
        ui.seqPosition = (float) st.positionBeats;
        ui.seqActiveRow = st.activeRow;
    }
    else
    {
        noteSet = midiState.notes (raw.midiOctave->load() > 0.5f);
        releaseMs = raw.midiRelease->load();
        ui.seqPosition = -1.0f;
    }
    for (int i = 0; i < 3; ++i) ui.notes[i] = noteSet.w[i];

    // ---- parâmetros do motor
    dsp::EngineParams ep;
    float pre = raw.preDelay->load();
    if (raw.preDelaySync->load() > 0.5f)
    {
        const auto& divs = P::preDelayDivisions();
        const int di = juce::jlimit (0, divs.size() - 1, (int) raw.preDelayDiv->load());
        pre = (float) juce::jmin (2000.0, divs[di].beats * 60000.0 / bpm);
    }
    ep.preDelayMs = pre;
    ep.decay      = raw.decay->load();
    ep.size       = raw.size->load() / 100.0f;
    ep.position   = raw.position->load();
    ep.density    = raw.density->load() / 100.0f;
    ep.offsetHz   = raw.offset->load();
    ep.spaceIndex = (int) raw.space->load();
    ep.dualMono   = raw.dualMono->load() > 0.5f;
    ep.eco        = raw.eco->load() > 0.5f;
    ep.lowCutHz   = raw.lowCut->load();
    ep.lowCut36   = (int) raw.lowCutSlope->load();
    ep.highCutHz  = raw.highCut->load();
    ep.highCut36  = (int) raw.highCutSlope->load();
    ep.target     = (dsp::Target) juce::jlimit (0, 2, (int) raw.target->load());
    ep.temper.temper    = raw.temper->load() / 100.0f;
    ep.temper.noteWidth = raw.noteWidth->load() / 100.0f;
    ep.temper.rangeLow  = juce::jmin (raw.rangeLow->load(), raw.rangeHigh->load());
    ep.temper.rangeHigh = juce::jmax (raw.rangeLow->load(), raw.rangeHigh->load());
    ep.temper.notes     = noteSet;
    ep.temper.tuning    = currentTuning();
    ep.temper.releaseMs = releaseMs;
    ep.temper.eco       = ep.eco;
    ep.temper.soloBank  = raw.soloBank->load() > 0.5f;

    // ---- ganhos
    const float mixAmt = juce::jlimit (0.0f, 1.0f, raw.mix->load() / 100.0f);
    const bool kill = raw.killDry->load() > 0.5f;
    inGainSm.setTarget (dsp::dbToGain (raw.input->load()));
    outGainSm.setTarget (dsp::dbToGain (raw.output->load() + abTrimDb.load()));
    dryGainSm.setTarget (kill ? 0.0f : std::cos (mixAmt * juce::MathConstants<float>::halfPi));
    wetGainSm.setTarget (std::sin (mixAmt * juce::MathConstants<float>::halfPi));
    bypassTarget = raw.bypass->load() > 0.5f ? 1.0f : 0.0f;

    // ---- processamento em pedaços do tamanho preparado
    const bool monoIn = numIn == 1;
    for (int start = 0; start < n; start += preparedBlock)
        processChunk (buffer, start, juce::jmin (preparedBlock, n - start), ep, monoIn);
}

void RoomXProcessor::processChunk (juce::AudioBuffer<float>& buffer, int start, int n,
                                   const dsp::EngineParams& ep, bool monoIn)
{
    const int numOut = buffer.getNumChannels();
    float* outL = buffer.getWritePointer (0, start);
    float* outR = numOut > 1 ? buffer.getWritePointer (1, start) : nullptr;
    const float* inL = outL;
    const float* inR = monoIn || outR == nullptr ? outL : outR;

    float* dL = dryBuf.getWritePointer (0);
    float* dR = dryBuf.getWritePointer (1);
    float* wL = wetBuf.getWritePointer (0);
    float* wR = wetBuf.getWritePointer (1);
    float* rL = rawBuf.getWritePointer (0);
    float* rR = rawBuf.getWritePointer (1);

    float inPk[2] = { 0, 0 }, outPk[2] = { 0, 0 };
    for (int i = 0; i < n; ++i)
    {
        rL[i] = inL[i]; rR[i] = inR[i];
        const float g = inGainSm.next();
        dL[i] = inL[i] * g; dR[i] = inR[i] * g;
        inPk[0] = juce::jmax (inPk[0], std::abs (dL[i]));
        inPk[1] = juce::jmax (inPk[1], std::abs (dR[i]));
    }

    // ---- sinal de teste: substitui a entrada por impulsos (periódicos ou o da medição) ou ruído rosa
    const int testMode = (int) raw.testSignal->load();
    int impulseAt = -1;
    if (testMode == 0)
        testSample = 0;
    else
    {
        const int64_t period = (int64_t) (2.0 * currentRate.load());
        // durante a medição (armado/a gravar) só há um impulso: os periódicos
        // entrariam na resposta e o decaimento nunca desceria
        const int ms = measState.load();
        const bool periodicOk = (ms == 0 || ms == 3);
        for (int i = 0; i < n; ++i)
        {
            float x = 0.0f;
            if (testMode == 1)
            {
                if ((periodicOk && testSample % period == 0) || (ms == 1 && impulseAt < 0))
                {
                    x = 1.0f;
                    if (impulseAt < 0) impulseAt = i;
                }
            }
            else
                x = 0.25f * nextPink();
            dL[i] = dR[i] = x;
            ++testSample;
        }
    }

    // bypass totalmente activo: cópia exacta da entrada (o motor descansa)
    if (bypassMix >= 1.0f && bypassTarget >= 1.0f)
    {
        if (! engineIdle) { engine.reset(); engineIdle = true; }
        for (int i = 0; i < n; ++i)
        {
            outPk[0] = juce::jmax (outPk[0], std::abs (rL[i]));
            outPk[1] = juce::jmax (outPk[1], std::abs (rR[i]));
            if (outR != nullptr) { outL[i] = rL[i]; outR[i] = rR[i]; }
            else                 { outL[i] = 0.5f * (rL[i] + rR[i]); }
            dryGainSm.next(); wetGainSm.next(); outGainSm.next();
        }
        for (int c = 0; c < 2; ++c)
        {
            if (inPk[c]  > ui.inPeak[c].load())  ui.inPeak[c]  = inPk[c];
            if (outPk[c] > ui.outPeak[c].load()) ui.outPeak[c] = outPk[c];
        }
        return;
    }
    engineIdle = false;
    engine.process (dL, dR, wL, wR, n, ep);

    // ---- medição do RT60: grava a resposta mono (pós-motor) a partir do impulso
    {
        int st = measState.load();
        if (st == 1 && impulseAt >= 0)
        {
            measIdx = 0;
            measFrom = impulseAt;
            st = 2;
            measState.store (2);
        }
        if (st == 2)
        {
            const int cap = (int) measBuf.size();
            for (int i = measFrom; i < n && measIdx < cap; ++i)
                measBuf[(size_t) measIdx++] = 0.5f * (wL[i] + wR[i]);
            measFrom = 0;
            if (measIdx >= cap)
                measState.store (3);
        }
    }

    // análise (wet mono, pós-filtros)
    {
        int s1, z1, s2, z2;
        analysisFifo.prepareToWrite (n, s1, z1, s2, z2);
        for (int i = 0; i < z1; ++i) analysisBuffer[(size_t) (s1 + i)] = 0.5f * (wL[i] + wR[i]);
        for (int i = 0; i < z2; ++i) analysisBuffer[(size_t) (s2 + i)] = 0.5f * (wL[z1 + i] + wR[z1 + i]);
        analysisFifo.finishedWrite (z1 + z2);
    }

    for (int i = 0; i < n; ++i)
    {
        const float dg = dryGainSm.next(), wg = wetGainSm.next(), og = outGainSm.next();
        bypassMix = bypassTarget > bypassMix ? juce::jmin (bypassTarget, bypassMix + bypassStep)
                                             : juce::jmax (bypassTarget, bypassMix - bypassStep);
        const float b = bypassMix;
        float l = (dL[i] * dg + wL[i] * wg) * og;
        float r = (dR[i] * dg + wR[i] * wg) * og;
        l = b * rL[i] + (1.0f - b) * l;
        r = b * rR[i] + (1.0f - b) * r;
        outPk[0] = juce::jmax (outPk[0], std::abs (l));
        outPk[1] = juce::jmax (outPk[1], std::abs (r));
        if (outR != nullptr) { outL[i] = l; outR[i] = r; }
        else                 { outL[i] = 0.5f * (l + r); }
    }

    for (int c = 0; c < 2; ++c)
    {
        if (inPk[c]  > ui.inPeak[c].load())  ui.inPeak[c]  = inPk[c];
        if (outPk[c] > ui.outPeak[c].load()) ui.outPeak[c] = outPk[c];
    }
}

// =============================================================================
//  Estado
// =============================================================================
void RoomXProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty (kStateVersion, kCurrentStateVersion, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void RoomXProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;

    auto tree = juce::ValueTree::fromXml (*xml);
    // ---- migração de versões futuras/antigas: ignora campos desconhecidos,
    //      completa o que falta (a APVTS repõe valores por defeito)
    const int version = (int) tree.getProperty (kStateVersion, 1);
    juce::ignoreUnused (version);
    if (! tree.getChildWithName (state::ids::sequence).isValid())
        tree.appendChild (state::SequenceModel::toTree (music::SequenceData::makeDefault()), nullptr);
    tree.setProperty (kStateVersion, kCurrentStateVersion, nullptr);

    apvts.replaceState (tree);

    auto finish = [flag = alive, this]
    {
        if (! flag->load()) return;
        presets->restoreNameFromState();
        publishSequence();
    };
    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr
        && juce::MessageManager::getInstance()->isThisTheMessageThread())
        finish();
    else
    {
        publishSequence();
        juce::MessageManager::callAsync (finish);
    }
}

juce::AudioProcessorEditor* RoomXProcessor::createEditor()
{
    return new RoomXEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RoomXProcessor();
}
