# Changelog — ROOM-X by Piradex

## 1.0.0 (beta) — 2026-10-15
Primeira versão completa.

### Áudio
- Motor híbrido original: reflexões iniciais multi-tap determinísticas, FDN de 16 linhas com matriz de rotações de Givens (estável para qualquer Density), banco modal de 121 ressoadores TPT-SVF (D#0–D#10).
- Temper (−100…+100), Note Width (1–99 %), Target Early/All/Late (bancos separados para reflexões e cauda), Temper Range.
- Pre-Delay livre (0–2 s) ou sincronizado; Mix potência constante; Decay 0.01–30x; Size 50–200 %; Position; Density; Offset (SSB, ±500 Hz); Input/Output; Kill Dry; Dual Mono; Bypass com rampa linear e cópia exacta da entrada.
- Low/High Cut do wet (12/36 dB/oct). Latência 0. Sem alocação/locks no processBlock.
- 29 espaços originais em 5 categorias. Modo Eco (FDN de 8 linhas + analisador mais lento).

### Música
- Manual (12 notas, pintar por arrasto, transpose, limpar/todas/inverter), navegador de 12 intervalos, 25 acordes, 24 escalas e 13 modos.
- Sequenciador harmónico: 8 linhas, 1–64 compassos, grelha com tercinas, session offset, transpose, retrigger, seguir o DAW, regiões sem sobreposição (criar/mover/duplicar/redimensionar), zoom e navegador; reprodução determinística em loop e bounce.
- MIDI: todos os canais, sustain CC64, Octave Repeat, release ajustável.
- Afinação global: A4 400–500 Hz, ±50 cents, sustenidos/bemóis.

### Interface
- Base grafite 1180×800 escalável (70–150 %) com acento animado por Target (coral / teal / azul eléctrico).
- NoteScape radial segmentado (energia real do wet por classe de altura + modelo do Temper), dial central Temper/Note Width, espectro wet com Temper Range e filtros arrastáveis.
- Presets: 60 de fábrica + 11 tutoriais, presets do utilizador (.roomx), importar, A/B com cópia nos dois sentidos, Undo/Redo, Note Lock, Mix Lock.

### Licença
- Ecrã de login (MASTER / BETA com validade), "Lembrar este computador" com token ligado à máquina, PBKDF2-HMAC-SHA256 60 000 iterações fora da thread de áudio, anti força-bruta. Plugin bloqueado = áudio bit-transparente. `ILicenseProvider` para futura conta Beatfreak.

### Build
- GitHub Actions: macOS Universal 2 (AU + VST3, 10.13+, auval Intel e Apple Silicon, pluginval, .pkg/.dmg) e Windows x64 (VST3, pluginval, Inno Setup, .zip); testes DSP e de integração com capturas da interface.
