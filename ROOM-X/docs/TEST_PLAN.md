# ROOM-X — Plano de testes

## Automáticos (correm no GitHub Actions)

### `tests/test_dsp.cpp` (sem JUCE)
- Banco modal: Tempero +100 realça a nota (A vs A# ≈ +12 dB), Tempero −100 escava (≈ −15 dB), Tempero Range deixa fora intacto.
- 29 espaços: RT60 medido vs alvo, pico, energia, correlação L/R (decorrelação).
- Stress: automação aleatória de todos os parâmetros a 44.1 / 96 / 192 kHz, blocos 16–4096 → sempre finito, sem explosão.
- CPU a 48 kHz: normal, Tempero com acorde, pior caso (12 notas, All, Dual), Eco.
- Teoria musical, sequenciador (loop, offset, retrigger, transpose), MIDI (CC64, octave repeat).

### `tests/test_plugin.cpp` (plugin real)
- Licença: vectores RFC do PBKDF2-HMAC-SHA256, senha errada, master e beta aceites (com `ROOMX_TEST_MASTER` / `ROOMX_TEST_BETA`), validade beta, chaves sem senha em claro, anti força-bruta.
- Sem licença: saída == entrada bit a bit.
- Com licença: cauda de reverb, bypass bit-transparente após a rampa, Kill Dry + Mix 0 = silêncio, MIDI + Octave Repeat + sustain.
- Estado: guardar/repor (parâmetros, sequência, afinação, bemóis), estado de versão futura com campos desconhecidos, sessão antiga sem sequência, Undo.
- Presets: ≥ 60 + 11, nomes únicos, todos carregam e soam sem NaN e sem ficarem "sujos", Note Lock, A/B, cópia B→A, leitura de `.roomx`.
- Taxas 44.1–192 kHz × blocos 16–4096 (incl. blocos menores do que o preparado), mono → estéreo.
- Capturas da interface (login, All, Early, Late com Tempero negativo, Sequence, MIDI) → artefacto `ROOM-X-capturas-UI`.

### Validação de formato
- macOS: `auval -v aumf RmX1 Prdx` em Apple Silicon e em Intel (Rosetta); pluginval nível 5 (VST3).
- Windows: pluginval nível 5 (VST3); verificação de que não depende do VC++ Redistributable.
- Linux (desenvolvimento): pluginval nível 8 passa com o plugin bloqueado e desbloqueado.

## Manuais (por DAW)
| DAW | Verificar |
|---|---|
| Logic Pro (AU) | carregar como efeito e como *MIDI-controlled Effect*; automação de Tempero/Target; bounce offline com Sequence |
| Ableton Live | VST3/AU, MIDI de outra pista, loop de 4 compassos com Sequence armado |
| Cubase / Nuendo | VST3, bypass do host, guardar/reabrir projecto |
| Studio One | escala da UI em ecrã HiDPI, presets do utilizador |
| Reaper | blocos irregulares, mudança de taxa de amostragem com o plugin aberto |
| FL Studio / Bitwig | estado ao duplicar a instância |

Para cada um: login master e beta, "lembrar este computador", plugin bloqueado numa sessão antiga (áudio intacto), troca de presets sem cliques, Eco ligado/desligado.

## Desempenho a medir no Mac do estúdio (MacBook Pro 2013, Catalina)
44.1/48/96 kHz × buffers 64/128/256 × Eco on/off × analisador on/off × pior caso (Density 100 %, Note Width larga, All, 12 notas).
