# SeniorMy Acoustic Guitars

Instrumento virtual (AU, VST3 e Standalone; macOS universal 10.13+ e Windows x64) com violões de aço,
nylon, flamenco e 12 cordas, viola caipira, cavaquinho, bandolim, ukulele, banjo e sitar.

- Motor de batidas e dedilhados com atraso por corda, humanização, abafamento, golpes e ordens duplas.
- Digitações idiomáticas automáticas para qualquer afinação (cebolão, D G B D, G C E A reentrante, Sol aberto…), capotraste e posição.
- Campo harmônico, tom/escala, progressões de até 16 acordes, reconhecimento de acorde na mão esquerda, modo Teclado.
- Editor de padrões (grade com força por passo, subdivisão, swing, humanização), padrões e presets do usuário.
- Importação de loops (áudio/MIDI → acordes, tom, BPM) e exportação MIDI.
- Mixer de 3 partes (nível, pan, timbre, corpo, largura, reverb, ruídos) + master.

Compilação: `cmake -B build && cmake --build build --config Release` (JUCE 8.0.16 via FetchContent).
Testes: `core_test` (núcleo, sem JUCE) e `library_test library` (biblioteca, presets, MIDI).
CI: `.github/workflows/build-acoustic-guitars.yml` (instaladores + Release `acoustic-guitars-v<versão>`).
Amostras e licenças: `docs/BIBLIOTECA-AMOSTRAS.md` e `Resources/legal/THIRD-PARTY-NOTICES.txt`.

© 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77. Todos os direitos reservados.
