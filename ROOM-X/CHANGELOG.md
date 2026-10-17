# Changelog — ROOM-X by Piradex

## 2.0.0 — 2026-10-17
- Interface em **Português e Inglês**: seletor PT | EN em *Info › Idioma* e no ecrã de senha; troca imediata, sem reabrir o plugin. Na primeira abertura segue o idioma do sistema. A escolha é comum a todos os plugins Piradex (`Piradex.settings`). Os nomes de automação no DAW não mudam — sessões antigas abrem iguais.
- Formato **AAX** (Pro Tools) para macOS e Windows.
- Corrigido: a caixa do nome do preset encolhia a cada redesenho do topo.

## 1.0.0 (beta) — 2026-10-17
- Formato AAX (Pro Tools) para macOS e Windows; assinatura PACE automática quando houver credenciais nos segredos do repositório.
- Controlo Temper passa a chamar-se Tempero.

## 1.0.0 (beta) — 2026-10-15
Primeira versão completa.

### Áudio
- Motor híbrido original: reflexões iniciais multi-tap determinísticas, FDN de 16 linhas com matriz de rotações de Givens (estável para qualquer Density), banco modal de 121 ressoadores TPT-SVF (D#0–D#10).
- Tempero (−100…+100), Note Width (1–99 %), Target Early/All/Late (bancos separados para reflexões e cauda), Tempero Range.
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
- NoteScape radial segmentado (energia real do wet por classe de altura + modelo do Tempero), dial central Tempero/Note Width, espectro wet com Tempero Range e filtros arrastáveis.
- Presets: 60 de fábrica + 11 tutoriais, presets do utilizador (.roomx), importar, A/B com cópia nos dois sentidos, Undo/Redo, Note Lock, Mix Lock.

### Licença
- Ecrã de login (MASTER / BETA com validade), "Lembrar este computador" com token ligado à máquina, PBKDF2-HMAC-SHA256 60 000 iterações fora da thread de áudio, anti força-bruta. Plugin bloqueado = áudio bit-transparente. `ILicenseProvider` para futura conta Beatfreak.

### Build
- GitHub Actions: macOS Universal 2 (AU + VST3, 10.13+, auval Intel e Apple Silicon, pluginval, .pkg/.dmg) e Windows x64 (VST3, pluginval, Inno Setup, .zip); testes DSP e de integração com capturas da interface.
