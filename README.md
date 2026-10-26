# SeniorMy Structure 1.0.1

Coberturas automáticas de cordas, violinos e orquestra **dentro da harmonia da música**: o plugin escuta a
trilha de piano (ou de outro instrumento) pela entrada de áudio/sidechain e/ou recebe MIDI, detecta acordes e
tonalidade em tempo real e gera pads sustentados, linha superior legato, ostinatos em spiccato, pizzicatos,
trêmolos e tutti com condução de vozes suave.

© 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.

## Formatos e plataformas
- macOS: **AU + VST3 + app**, binário universal (Intel x86_64 + Apple Silicon arm64), **macOS 10.13+**
  (CI com Xcode 16.4, `CMAKE_OSX_DEPLOYMENT_TARGET=10.13`). Instalador `.pkg` único.
- Windows: **VST3 + app**, x64 (Windows 10+). Instalador `.exe` (Inno Setup) e `.zip` portátil.
- Compilação: GitHub Actions, **manual** (`workflow_dispatch`) — `.github/workflows/build-structure.yml`;
  ao final publica uma Release `v<versão>` com os instaladores e `SHA256SUMS.txt`.

## Recursos
| Área | O que faz |
|---|---|
| Análise | Cromagrama em tempo real (FFT 8192/16384, branqueamento espectral, supressão de harmônicos), croma do baixo, modelos de acordes (tríades, sus, 7ªs, inversões), histerese ajustável, tonalidade por perfis Krumhansl–Kessler. MIDI de entrada tem prioridade e é exato. |
| Harmonia | Tonalidade auto/manual, escala (maior, menores natural/harmônica/melódica, dórico, frígio, lídio, mixolídio, lócrio), extensões (tríade → 11ª/13ª pela escala), inversões (auto, fundamental, 1ª, 2ª, seguir o baixo), densidade, registro, abertura, humanização, sensibilidade, travar acorde, respeitar escala. |
| Arranjo | Até 8 faixas: Pad, Linha legato, Padrão (grade de 6 vozes × até 32 passos com velocity), Tutti. Condução de vozes por busca de custo (movimento mínimo, sem quintas/oitavas paralelas, terça garantida, espaçamento). Notas comuns ficam ligadas. Sincroniza com a DAW (ou relógio interno). |
| Presets | 7 de fábrica (Cinematic Pad, Ballad Strings, Pop Ostinato, Pizzicato Light, Full Orchestra, Tremolo Tension, Warm Low Strings); salvar / substituir / excluir presets do usuário (`Documentos/SeniorMy Structure/Presets/*.smstructure`). |
| Editor | Lista de faixas, propriedades (tipo, articulação, seções, oitava, volume, grade, duração), piano roll de padrões, rolo das linhas geradas ao vivo por seção. |
| Importar | WAV/AIFF/FLAC/MP3/MIDI arrastados ou por botão → andamento, acordes por tempo, tonalidade e ritmo de 16 passos; tocar a cobertura sobre a progressão, criar ostinato com o ritmo do loop, exportar a cobertura em MIDI (arrastar para a DAW ou salvar). |
| Som | Sampler com camadas de velocity em crossfade contínuo (dinâmica), round robin, loop com crossfade, release, Violinos II decorrelacionados, pan de assentos americanos, mixer por seção, reverb de sala FDN 8 linhas. Piano interno modal leve, tocável por MIDI e teclado na tela, com opção de tocar a cobertura. |
| Jurídico | Termos de Uso PT-BR/EN com aceite obrigatório na 1ª abertura (bloqueia e silencia até aceitar) e no instalador; tela Sobre com créditos e licenças. |

## Biblioteca de amostras
VSCO 2 Community Edition (Versilian Studios), **CC0 1.0**. Gerada no CI por `scripts/prepare_library.py`
(baixa só as pastas usadas num commit fixo, confere oitavas, corta, cria loops, iguala volumes e grava FLAC
24 bits). ~339 MB em disco, ~410 MB de RAM. Detalhes: `docs/BIBLIOTECA-AMOSTRAS.md`.
Sem biblioteca instalada o plugin usa cordas sintetizadas de reserva.

Pasta procurada: macOS `/Library/Application Support/SeniorMy/Structure Library`; Windows
`C:\Users\Public\Documents\SeniorMy\Structure Library`; ou a variável `SENIORMY_STRUCTURE_LIBRARY`.

## Compilar localmente
```bash
python3 -m pip install numpy soundfile
python3 scripts/prepare_library.py samples-cache library
cmake -B build -G Xcode -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13 "-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64"   # macOS
cmake --build build --config Release --target SeniorMyStructure_All
cmake --build build --config Release --target core_test library_test   # testes
bash scripts/mac/package.sh                                             # .pkg
```
Linux (só verificação): `cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release && ninja -C build-linux SeniorMyStructure_Standalone core_test library_test`.

Capturas de tela automáticas: `SENIORMY_SNAPSHOT_DIR=/pasta` ao abrir o app.

## Estrutura
- `Source/core/` — núcleo C++17 sem JUCE: `Harmony.h` (escalas, acordes, tonalidade), `Analysis.h` (FFT,
  cromagrama, rastreador), `Arranger.h` (condução de vozes e linhas), `Sampler.h`, `Dsp.h` (reverb, piano),
  `Engine.h` (motor em tempo real), `LoopAnalysis.h`, `Presets.h`, `SynthBank.h`.
- `Source/` — plugin JUCE: processador, editor e páginas, aparência, Termos, biblioteca, presets, importação.
- `tests/` — `core_test.cpp` (núcleo) e `library_test.cpp` (biblioteca real, importação, exportação MIDI, presets).
- `scripts/` — biblioteca, instaladores macOS/Windows. `tools/` — ícone e envio ao GitHub.
- Envio: `python3 tools/push_github.py` (padrão: repo `eeuseniormy-afk/seniormy-structure`, branch `main`).
