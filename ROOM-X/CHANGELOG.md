# Changelog — ROOM-X by Piradex

## 2.0.1 — não publicado
### Áudio
- Calibração do decaimento: o RT60 produzido passa a coincidir com o mostrado (mediana medido/alvo 0,80 → 1,03; 28 de 29 espaços entre 0,85 e 1,15). O Broken Space, "gated" por desenho, fica fora deste critério.
- Offset: oscilador por rotação complexa em vez de `sin`/`cos` por amostra; saída numericamente idêntica.
- Novo botão "Só banco" (*Solo Modal Bank*): ouve só a ressonância do banco modal, sem o sinal seco nem o reverb. Não altera presets nem sessões antigas (parâmetro novo, desligado por omissão).
- Presets: +203 de fábrica (29 espaços × 7 atmosferas: Cinema, Nocturne, Shimmer, Shadow, Velvet, Metal, Mirror). Total: 263 de fábrica + 11 tutoriais.
- Interface: o asterisco de preset alterado passa a "(alterado)"; "Escolher escala…" em destaque na cor do alvo; rodapé com texto maior; unidades com espaço (100 %, 1.0 x). Kill Dry movido para junto do MIX; "Escolher escala…" com destaque pelo sistema de cores do look-and-feel.
- Interface: Dual Mono passa para o fundo da coluna do INPUT; legendas "BLOQ." nos cadeados de mistura e de notas; o RT60 mostrado é rotulado como "alvo" (decaimento nominal, não o medido).
- Sinal de teste embutido (Desligado / Impulso / Ruído rosa), no rodapé, sem precisar de DAW.
- RT60 medido ao vivo ("Medir RT60"): dispara um impulso e calcula o decaimento real da resposta (Schroeder, −5 a −25 dB). Validado: no espaço Aurora Hall mede 2,59 s contra 2,40 s de alvo.
- Igualar A/B ("Match A/B"): o slot B recebe o trim de saída que o aproxima do A, pela energia da resposta medida em cada slot. É uma aproximação: meça cada slot primeiro.
- Pesquisa de presets: "Pesquisar preset…" no menu, por nome ou banco.
- Teste: Undo de carregamento de preset verificado; o Undo espera que as alterações do APVTS assentem (são assíncronas).
- Undo/Redo corrigido na origem: o APVTS só passa os parâmetros para a árvore de estado num Timer (10 Hz, até 500 ms parado), e o UndoManager só regista o que lá está. Um Undo logo a seguir a uma edição não a via, e dois gestos rápidos fundiam-se num passo. Agora a passagem é forçada antes de cada transação, de cada Undo/Redo e no fim de cada carregamento de preset, e a 30 Hz pelo editor. Testes estritos, sem esperas (50 repetições); com o flush desligado, os mesmos testes falham.
- Teclado e leitor de ecrã: Tab percorre os controlos (knobs, faders, botões, selectores, pads de notas); setas mudam valores, Espaço/Enter acciona, Esc devolve o foco ao DAW. Os cliques do rato não roubam o foco, para a barra de espaço continuar a controlar o transporte do DAW. Todos os controlos têm título para o leitor de ecrã; os selectores e os pads de notas lêem o valor actual ("C, E, G"). Anel de foco na cor do alvo.
- Medição: o custo de CPU do Offset (oscilador por rotação) não mudou de forma mensurável face a 2.0.0 (≈0,18 s por 60 s de áudio, antes e depois). A alteração mantém-se pela precisão e pela ausência de `sin`/`cos` por amostra, não por poupança de CPU.
- Acessibilidade: cor de texto secundário (`textFaint`) ajustada para contraste WCAG AA (≥ 4,5:1) sobre o fundo e os painéis.
- Testes: contagem de presets exata (falha se mudar); teste de Undo espera pela condição em vez de tempo fixo.

### Repositório
- Removidos da raiz o modelo antigo de projeto JUCE ("ReverbEx": `CMakeLists.txt`, `Source/`) e o workflow `build.yml`, que compilava esse modelo a cada push para `main`. O pipeline ativo é `.github/workflows/roomx.yml`.

### Documentação e testes
- Tabela de CPU atualizada com medições reproduzíveis; calibração do RT60 documentada.
- Teste de RT60 mais exigente (0,85–1,15 do alvo, exceto Broken Space).

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
