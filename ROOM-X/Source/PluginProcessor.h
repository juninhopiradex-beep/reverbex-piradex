// =============================================================================
//  ROOM-X by Piradex — processador
//  Regras de tempo real: nada de alocação, locks bloqueantes, ficheiros,
//  rede ou chamadas à UI dentro de processBlock.
// =============================================================================
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "dsp/ReverbEngine.h"
#include "music/MidiNoteState.h"
#include "music/SequenceEngine.h"
#include "state/Parameters.h"
#include "state/PresetManager.h"
#include "state/SequenceModel.h"

class RoomXProcessor final : public juce::AudioProcessor,
                             private juce::ValueTree::Listener,
                             private juce::AsyncUpdater
{
public:
    RoomXProcessor();
    ~RoomXProcessor() override;

    // ---- AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    // ---- medição do RT60 ao vivo (chamadas da mensagem)
    void armRt60Measurement();           // dispara um impulso de teste e grava a resposta
    int    rt60State() const noexcept { return measState.load(); }   // 0 parado, 1 armado, 2 a gravar, 3 pronto
    double rt60Result() const;           // segundos (-1 se não houver decaimento válido)

    // ---- igualar nível A/B: o slot B ganha o trim que o aproxima do A (energia da resposta medida)
    void  setAbMatch (bool on) noexcept { abMatchOn.store (on); }
    bool  abMatch() const noexcept      { return abMatchOn.load(); }
    float abTrim() const noexcept       { return abTrimDb.load(); }
    void  applyAbMatch() noexcept;       // mensagem: atualiza o trim conforme o slot activo

    // ---- Undo/Redo (mensagem). O APVTS só passa os valores dos parâmetros para a
    // árvore de estado num Timer (10 Hz, até 500 ms quando parado), e o UndoManager só
    // regista o que já está na árvore. Sem forçar essa passagem, as edições caem na
    // transação seguinte ou o Undo corre antes de elas existirem. copyState() força-a.
    void flushParamsToState()                                  { (void) apvts.copyState(); }
    void beginUndoTransaction (const juce::String& name = {}) { flushParamsToState(); undoManager.beginNewTransaction (name); }
    bool undoAction()                                          { flushParamsToState(); return undoManager.undo(); }
    bool redoAction()                                          { flushParamsToState(); return undoManager.redo(); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override    { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    // ---- acesso para a UI
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;
    roomx::state::SequenceModel sequence { apvts };
    std::unique_ptr<roomx::state::PresetManager> presets;

    struct UiInfo
    {
        std::atomic<float> inPeak[2] {}, outPeak[2] {};
        std::atomic<uint64_t> notes[3] {};          // notas efectivamente "tempered"
        std::atomic<int>   midiActivity { 0 };
        std::atomic<float> seqPosition { -1.0f };   // em tempos; -1 = parado
        std::atomic<int>   seqActiveRow { 0 };
        std::atomic<float> bpm { 120.0f };
        std::atomic<bool>  licensed { false };
    } ui;

    // FIFO de análise (sinal wet mono) — lido pela UI
    juce::AbstractFifo analysisFifo { 1 << 15 };
    std::vector<float> analysisBuffer = std::vector<float> (1 << 15, 0.0f);
    double getCurrentSampleRate() const noexcept { return currentRate.load(); }

    void requestRetrigger() noexcept { retrigRequest.store (true); }
    roomx::music::Tuning currentTuning() const;
    bool useFlats() const;
    void setUseFlats (bool);

private:
    juce::AudioParameterBool* bypassParam = nullptr;
    roomx::dsp::ReverbEngine engine;

    // ponteiros para os valores crus dos parâmetros
    struct Raw
    {
        std::atomic<float> *input, *output, *mix, *killDry, *soloBank, *testSignal, *preDelay, *preDelaySync, *preDelayDiv,
                           *decay, *size, *position, *density, *offset, *temper, *noteWidth, *target,
                           *rangeLow, *rangeHigh, *lowCut, *lowCutSlope, *highCut, *highCutSlope,
                           *space, *dualMono, *bypass, *eco, *noteMode, *notes, *midiOctave, *midiRelease,
                           *seqBars, *seqTranspose, *seqArmed, *seqOffset, *tuneRef, *tuneCents;
    } raw {};

    // sequência: troca sem bloqueio entre a thread de mensagens e a de áudio
    juce::SpinLock seqLock;
    roomx::music::SequenceData seqShared;
    std::atomic<int> seqVersion { 0 };
    roomx::music::SequenceData seqLocal;
    int seqLocalVersion = -1;
    roomx::music::SequenceEngine seqEngine;
    double retrigPpq = 0.0;
    bool wasPlaying = false;
    std::atomic<bool> retrigRequest { false };

    roomx::music::MidiNoteState midiState;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    // buffers pré-alocados
    juce::AudioBuffer<float> dryBuf, wetBuf, rawBuf;
    int preparedBlock = 0;
    std::atomic<double> currentRate { 48000.0 };

    roomx::dsp::Smoother inGainSm, outGainSm, dryGainSm, wetGainSm;
    float bypassMix = 0.0f, bypassTarget = 0.0f, bypassStep = 0.001f;
    bool  engineIdle = false;

    // sinal de teste e medição do RT60 ao vivo
    std::atomic<int> measState { 0 };
    std::vector<float> measBuf;          // ~12 s de resposta, pré-alocado
    int measIdx = 0, measFrom = 0;
    int64_t testSample = 0;
    uint32_t testSeed = 0x2545F491u;
    float pink[7] {};
    float nextPink() noexcept;
    mutable double cachedRt60 = -1.0;
    mutable bool   cacheValid = false;
    int measSlot = 0;
    mutable std::atomic<float> energy0 { std::numeric_limits<float>::quiet_NaN() };   // energia (dB) medida no slot A
    mutable std::atomic<float> energy1 { std::numeric_limits<float>::quiet_NaN() };   // energia (dB) medida no slot B
    std::atomic<bool>  abMatchOn { false };
    std::atomic<float> abTrimDb { 0.0f };

    // ValueTree::Listener / AsyncUpdater (sequência -> áudio)
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override        { triggerAsyncUpdate(); }
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { triggerAsyncUpdate(); }
    void valueTreeRedirected (juce::ValueTree&) override                         { triggerAsyncUpdate(); }
    void handleAsyncUpdate() override;
    void publishSequence();

    void processChunk (juce::AudioBuffer<float>& buffer, int start, int n,
                       const roomx::dsp::EngineParams& ep, bool monoIn);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomXProcessor)
};
