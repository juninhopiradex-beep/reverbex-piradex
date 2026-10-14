# ROOM-X — Arquitectura DSP

Todo o DSP é original (Piradex / Beatfreak Developer). Não usa código, dados, IRs ou tecnologia de conversão modal de terceiros. O núcleo (`Source/dsp`, `Source/music`) não depende de JUCE e é testado em `tests/test_dsp.cpp`.

## Fluxo do sinal (por núcleo estéreo)

```
entrada ─► Input ─► Pre-Delay ─┬─► Early Reflections ─► Offset (SSB) ─► Banco Modal "Early" ─┐
                               │                                                              ├─► Position ─► Wet Low/High Cut ─► Mix (potência constante) ─► Output
                               └─► atraso de início ─► FDN tardio ─► Offset (SSB) ─► Banco Modal "Late" ─┘
```

- **Latência: 0 amostras** (nenhum modo usa convolução, look-ahead ou oversampling).
- **Dual Mono**: dois núcleos independentes (L e R), cada um alimentado em mono; em modo ligado o núcleo único é estéreo verdadeiro.
- Mudanças estruturais (espaço, Dual Mono, Eco) fazem um fade de 15 ms do wet → sem cliques.
- Segurança: se aparecer NaN/Inf o núcleo é reiniciado em silêncio (nunca devia acontecer; os testes de stress verificam).

## Early Reflections (`EarlyReflections.*`)
- Rede multi-tap esparsa: até 32 taps por canal, gerados de forma **determinística** a partir da *seed* de cada espaço (xorshift32 → mesmos valores em qualquer máquina e em bounce offline).
- Distribuição temporal com expoente (`erShape`) e declive de ganho (`erSlope`; negativo = reflexões que crescem, "swell").
- Decorrelação L/R por troca de canal de origem e sinais aleatórios; amortecimento de 1 pólo; 2 difusores passa-tudo.
- **Size** escala os tempos com leitura fraccionária Hermite e suavização de 220 ms (sem saltos de afinação).
- **Density** desliga taps numa ordem aleatória mas fixa, com normalização de energia.

## Cauda tardia (`FDNReverb.*`)
- FDN de **16 linhas** (8 em modo Eco). Matriz de realimentação = andares de rotações de Givens em *butterfly* — ortogonal para qualquer ângulo θ, logo **sempre estável**.
  - θ = 45° → mistura total (Hadamard rodado): cauda densa e lisa.
  - θ → 0° → linhas quase independentes: modos esparsos, metálicos, "chime". **Density** controla θ.
- Absorção por linha com duas prateleiras de 1.ª ordem monótonas (graves/agudos) calculadas a partir do RT60 desejado (Decay × Size × RT60 do espaço).
- Modulação por oscilador em quadratura por linha (sem tabelas, sem `sin` por amostra), passa-tudo dispersivo opcional (molas/metal), difusão de entrada.

## Camada modal musical (`ModalBank.*`)
Banco paralelo de **121 ressoadores TPT-SVF**, um por semitom de D#0 a D#10 (≈19 Hz–20 kHz a A4 = 440 Hz), aplicado separadamente à saída das reflexões e da cauda (é assim que o **Target** funciona: EARLY tempera só o banco Early, LATE só o banco Late, ALL ambos).

Para cada amostra:

```
y = low + high + midGain · mid + resGain · Σ g_k · BP_k(x)
```

- `low`/`high` = espectro fora do **Temper Range** (Butterworth de 4.ª ordem), intocado.
- `mid = x − low − high` → reconstrução perfeita com Temper = 0 (o banco é contornado bit a bit).
- **Temper > 0**: atenua o resto (`midGain = (1−t)^1.5`) e soma os ressoadores das notas seleccionadas com compensação de nível pela largura de banda total seleccionada.
- **Temper < 0**: subtrai os ressoadores das notas seleccionadas (entalhes musicais) mantendo o resto.
- **Note Width** → largura de banda de 0.03 a 1.6 semitons (curva de potência), consistente em todas as oitavas.
- No caminho Early os ressoadores têm largura mínima de 3 Hz para não "tocarem" para sempre.
- Só os ressoadores activos são calculados (processados aos pares, 4 recursões independentes por iteração → vectorizável). Ganhos por nota com ataque 15 ms / release configurável (MIDI) → mudanças de acorde sem cliques.

## Offset
Desvio de frequência por banda lateral única (transformador de Hilbert por dois ramos de passa-tudo + modulação em quadratura), −500…+500 Hz, aplicado aos dois caminhos. Fase calculada em `double` e partilhada entre canais.

## Filtros wet, Position, Mix
- Low/High Cut 20 Hz–20 kHz, 12 dB/oct (SVF) ou 36 dB/oct (Butterworth de 6.ª ordem, 3 SVF).
- **Position** −99 (Early) … +99 (Late): equilíbrio perceptivo entre reflexões e cauda.
- **Mix** potência constante (cos/sin); **Kill Dry** remove o seco; **Bypass** com fade de 12 ms para o sinal original.

## Regras de tempo real
- Sem alocação, locks bloqueantes, ficheiros, rede ou UI em `processBlock` (buffers pré-alocados em `prepareToPlay`; o host pode mandar blocos maiores, que são processados em pedaços).
- Sequência partilhada por `SpinLock` em modo *try-lock* do lado do áudio (nunca bloqueia).
- Medidores e análise via `std::atomic` e `AbstractFifo`. FFT, NoteScape e espectro correm na thread da UI.
- `ScopedNoDenormals` + `flushDenormal` nos estados recursivos.

## Espaços (29)
Receitas originais em `SpaceLibrary.cpp` (RT60, rácios graves/agudos, janela e forma das reflexões, comprimentos das linhas, difusão, modulação, dispersão, largura estéreo, seed). Categorias: Rooms (6), Halls (6), Mechanical (5), Unusual (6), Special FX (6).

## Medições (testes, 48 kHz, 1 núcleo)
| Caso | CPU |
|---|---|
| Normal (Temper 0) | ~5 % |
| Temper 60, acorde maior | ~7.5 % |
| Pior caso: 12 notas, All, Dual Mono | ~22 % |
| Pior caso em Eco | ~18 % |

Os valores vêm do servidor de testes Linux; num Mac moderno são inferiores.
