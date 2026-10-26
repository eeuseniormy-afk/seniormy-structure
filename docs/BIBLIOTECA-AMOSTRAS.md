# Biblioteca de amostras — SeniorMy Acoustic Guitars

© 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77 (seleção, edição e mapeamento).

Todas as gravações usadas são **CC0 1.0** (domínio público). Cada licença foi conferida na página/arquivo
original em 26/10/2026. A biblioteca processada (FLAC 24 bits, ~36 MB) fica versionada em `library/`
e é reproduzível com `python3 scripts/prepare_library.py <cache> library` (numpy, soundfile, libarchive-c).

| Instrumento | Fonte | Licença (onde foi verificada) | Versão / commit | Amostras |
|---|---|---|---|---|
| Violão Aço (e base do 12 Cordas) | Martin HD-28 — Jeff Learman, Discord SFZ GM Bank · https://github.com/sfzinstruments/Discord-SFZ-GM-Bank (`Discord GM/Melodic/026-Acoustic Guitar (steel)`) | CC0 — cabeçalho do SFZ: "License: Creative Commons CC0"; README do banco só aceita CC0/CC-BY | 7a9c478fe331f94f246d33332f0adedb25bbbe27 | 15 (terças menores, loops desenrolados) |
| Violão Nylon (e base do Flamenco) | Spanish Classical Guitar — roberto@zenvoid.org, FreePats · https://freepats.zenvoid.org/Guitar/acoustic-guitar.html | CC0 — página + cc0.txt | 2019-06-18 | 48 (cromático) |
| Ukulele | Ukulele tenor Flight Fireball — Mateusz Dąbrowski, FreePats · https://freepats.zenvoid.org/GuitarFamily/ukulele.html | CC0 — página + LICENSE.txt | 2026-08-11 | 13 + golpe no tampo |
| Banjo | SX 6-string banjo — itsclipping · https://github.com/sfzinstruments/ganjo | CC0 — LICENSE.md | ccff5cd5cd3b513873a48994c07724d9d3c39e1c | 47 (até 4 round robins) |
| Sitar | Dr. Narayan Bhagawan Raikar, Discord SFZ GM Bank (`105-Sitar`) | CC0 — readme.md e Sitar.sfz | 7a9c478fe331f94f246d33332f0adedb25bbbe27 | 26 |

## Instrumentos derivados e modelados (honestidade de origem)
- **Violão Flamenco — derivado** das amostras de nylon: realce de agudos (+5 dB), sustentação encurtada, ataque mais seco, golpes no tampo.
- **Violão 12 Cordas — derivado** das amostras de aço: cada ordem toca a corda principal + a corda de oitava (usando a amostra real uma oitava acima) ou uníssono, com desafinação e atraso entre as cordas do par.
- **Viola Caipira, Cavaquinho, Bandolim — modelados**: guia de onda digital (Karplus-Strong estendido) com excitação triangular na posição da palheta + ruído, perda dependente da frequência, afinação fracionária por passa-tudo (afinação medida: erro < 0,5 cent), ordens duplas (oitava/uníssono) e corpo modal de 6 ressonâncias por instrumento. **Não são gravações.**
- Ruídos de traste (chiado) são sintetizados.
- Sem a biblioteca instalada, os instrumentos gravados tocam pelo modelo físico genérico.

## Fontes descartadas (licença incompatível)
- Philharmonia Orchestra (guitarra, bandolim, banjo): proíbe disponibilizar "como amostras ou instrumento de sampler".
- FreePats FSS Steel-String Guitar e FlameStudios Kay 5-String Banjo: GPL (com exceção só para documentos) — não cabe num plugin fechado.
- Ovation Guitar e Terkelsen's Mandolin (sfzinstruments): sem licença declarada.
- Blue Jeans and Moonbeams (MIT): repositório do autor não está mais disponível.
- Freesound: os originais exigem login OAuth para baixar; nenhum pacote CC0 de bandolim/cavaquinho/viola com notas cromáticas foi encontrado.

## Processamento
Corte do silêncio inicial, verificação de afinação por autocorrelação (correções < 100 cents gravadas em `tune`),
desenrolar dos loops do Martin com decaimento exponencial, nivelamento parcial entre amostras vizinhas,
normalização por instrumento (pico −1 dBFS), fade final, mono, FLAC 24 bits.
