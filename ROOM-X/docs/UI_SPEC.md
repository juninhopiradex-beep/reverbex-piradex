# ROOM-X — Interface

Base lógica **1180 × 800** px, escalável 70–150 % com proporção fixa (a escala fica guardada na sessão). Abre a 90 % para caber em ecrãs de 1440 × 900.

| Zona | y | Conteúdo |
|---|---|---|
| Topo | 0–54 | Marca ROOM-X, Undo/Redo, preset (‹ › nome, banco, menu), Guardar, A ⧉ B, Afinação (diapasão), Info, Eco |
| Espaço | 60–90 | Categoria · nome do espaço · RT60 efectivo (Decay × Size), ‹ › e menu por categorias |
| Principal | 96–446 | Esquerda: Input + medidor, Pre-Delay (ou divisão sincronizada), Mix (+ cadeado), Decay, Size, Sync, Kill Dry · Centro: NoteScape + dial Tempero/Note Width · Direita: Target, Position, Density, Offset, Dual Mono, Output + medidor |
| Espectro | 454–552 | Botões de declive LC/HC, espectro wet, Tempero Range (pegas com nome da nota), pegas LC/HC na base |
| Note Selection | 558–594 | Note Lock, Manual / Sequence / MIDI |
| Painel inferior | 598–768 | Manual (teclas + escala + transpose) · Sequence (linhas, grelha, navegador, timing) · MIDI (actividade, teclado, Octave Repeat, Release) |
| Rodapé | 772–800 | Versão, estado da licença (BETA a laranja), Eco/afinação alterada, Bypass |

## Idioma (PT / EN)
- Seletor PT | EN em *Info › Idioma* e no canto do ecrã de senha. Troca imediata (todas as janelas abertas). Primeira abertura: idioma do sistema. Guardado em `Piradex.settings` (comum a todos os plugins Piradex).
- Todos os textos passam por `tr ("pt", "en")` (`Source/i18n/PiradexLanguage.h`). Os nomes dos parâmetros vistos pelo DAW não mudam.

## Cor
- Fundo `#0C0E12`, painéis `#14171D`, linhas `#262B35`, texto `#E6E9EF`, notas seleccionadas prata `#F4F6FA`.
- Acento por Target, transição ~200 ms (interpolação a 30 Hz): **EARLY `#FF667A`**, **ALL `#40E0C0`**, **LATE `#3DA8FF`**.
- O acento aplica-se a: arcos dos knobs, dial do Tempero, segmentos e contorno do NoteScape, espectro dentro do Tempero Range, pegas do range, LEDs das teclas, regiões activas da sequência, botões ligados. O resto fica neutro.

## NoteScape (original)
- 12 sectores (C no topo, sentido horário), 9 segmentos em arco por sector entre o dial e o anel das notas.
- Energia = mistura da energia real do wet por classe de altura (FFT 8192, dentro do Tempero Range) com o modelo do Tempero (realce das notas seleccionadas, alastramento para vizinhas com Note Width larga).
- Notas seleccionadas: arco prateado com entalhe + nome a negrito. Tempero negativo: sectores ocos e escuros ("buracos").
- Clicar/arrastar sobre as notas liga/desliga-as (modo Manual).

## Dial Tempero / Note Width
- Disco central: arrastar = Tempero (arco bipolar a partir do topo). Anel exterior: arrastar = Note Width (arco prateado simétrico em baixo).
- Roda do rato = Tempero; Shift + roda = Note Width; duplo clique repõe; Shift + arrastar = fino.

## Espectro
- Eixo log 20 Hz–20 kHz, marcas C1–C9 no topo e Hz em baixo, inclinação +4.5 dB/oct (ruído rosa fica plano).
- Fora do Tempero Range: cinzento e escurecido. Bandas das notas seleccionadas quando há Tempero.
- Pegas do range: arrastar cada uma; arrastar na faixa superior entre elas move as duas; duplo clique repõe.
- LC/HC: pegas na base (arrastar, Shift = fino, duplo clique repõe, arrastar entre elas desloca a banda); botões laterais alternam 12/36 dB/oct.

## Sequenciador
- Clique numa linha (à esquerda) abre o navegador de escalas/acordes para essa linha e torna-a a linha usada com o transporte parado.
- Grelha: clicar/arrastar em vazio cria; arrastar move (Alt duplica); bordas redimensionam; duplo clique / Delete apaga; clique direito: duplicar a seguir, apagar, limpar. Ctrl/Cmd + roda = zoom; roda = deslocar; navegador em baixo (arrastar janela ou bordas).
- Timing: Bars, Grid, Offset (arrastar na caixa), Transpose, Retrigger, Arm (seguir o DAW).

## Tipografia
Barlow (Regular, Medium, SemiBold — SIL OFL 1.1) embutida.
