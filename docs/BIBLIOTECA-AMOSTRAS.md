# Biblioteca orquestral do SeniorMy Structure — fontes e licenças

© 2026 W. Carolino — SeniorMy — CNPJ 69.610.819/0001-77 (seleção, edição, processamento e mapeamento).

## Escolha

| Candidata | Licença | Situação |
|---|---|---|
| **VSCO 2 Community Edition 1.1.0** (Versilian Studios) | **CC0 1.0** — domínio público, uso comercial e redistribuição livres | **Usada** |
| Sonatina Symphonic Orchestra | Creative Commons Sampling Plus 1.0 (licença aposentada pela Creative Commons, proíbe uso em publicidade) e parte das amostras vem da Philharmonia, com termos incompatíveis — status legal incerto (https://github.com/peastman/sso, http://freepats.zenvoid.org/other-sample-libraries.html) | Descartada |
| Philharmonia Orchestra sound samples | Termos do site: uso livre em músicas, mas "must not be sold or made available 'as is' (i.e. as samples **or as a sampler instrument**)" (https://philharmonia.co.uk/resources/sound-samples) | Descartada — proíbe exatamente este uso (instrumento sampler) |

Verificação da licença do VSCO 2 CE (25/10/2026): arquivo `LICENSE` do repositório
https://github.com/sgossner/VSCO-2-CE = texto integral da CC0 1.0 Universal; o `Readme.txt` diz
"You are permitted to use these samples for ANY purpose" e **pede** (sem obrigar) que as amostras não
sejam vendidas isoladamente e que se dê crédito a Versilian Studios / Sam Gossner e Ivy Audio / Simon
Dalzell com link para http://vis.versilstudios.net/vsco-community.html. O SeniorMy Structure atende os dois
pedidos: as amostras só são distribuídas dentro do instrumento (os Termos de Uso proíbem extraí-las e
revendê-las) e o crédito aparece na tela **Sobre**, em THIRD-PARTY-NOTICES.txt e em LICENSE-SOURCES.txt.

## Origem fixa

- Repositório: `sgossner/VSCO-2-CE`, commit `440300901dfe9275fd84e0b7763af1f8443ae62e` (04/08/2020, versão 1.1.0).
- Gravação: Sam Gossner (Versilian Studios) e Simon Dalzell (Ivy Audio). Corte das amostras: Elan Hickler (Soundemote).
- O script `scripts/prepare_library.py` baixa só as pastas abaixo (668 arquivos WAV, ~670 MB) direto de
  `raw.githubusercontent.com` nesse commit, e grava em `library/LICENSE-SOURCES.txt` a URL de origem e a
  licença de **cada** arquivo.

## Pastas usadas → instrumentos do plugin

| Seção no plugin | Articulação | Pasta no VSCO 2 CE |
|---|---|---|
| Violinos I | Sustain | Strings/Violin Section/susVib |
| Violinos I | Spiccato | Strings/Violin Section/Spic |
| Violinos I | Pizzicato | Strings/Violin Section/Pizz |
| Violinos I | Trêmolo | Strings/Violin Section/Trem |
| Violinos I | Legato (linha superior) | Strings/Solo Violin/Arco Vib (violino solo) |
| Violinos II | Sustain/Spiccato/Pizz/Trêmolo | mesmas gravações dos Violinos I, outra "estante": pan, −5 cents, RR deslocado, início deslocado |
| Violas | Sustain / Spiccato / Pizz / Trêmolo | Strings/Viola Section/susvib · spic · pizz · trem |
| Violoncelos | Sustain / Spiccato / Pizz / Trêmolo | Strings/Cello Section/susvib · spic · pizzT · trem |
| Contrabaixos | Sustain / Spiccato / Pizz / Trêmolo | Strings/Solo Contrabass/SusVib · Spic · Pizz · Trem (contrabaixo **solo**) |
| Trompas (tutti) | Sustain / Staccato | Brass/F Horn/sus · stac |

Ensemble/tutti = combinação das seções acima pelo arranjador (não há gravação de orquestra completa).

## Resultado do preparo (execução de 25/10/2026 no sandbox)

23 instrumentos, 799 zonas, 668 arquivos FLAC 24 bits, **339 MB** em disco (~410 MB de RAM em 16 bits).

| seção | articulação | zonas | camadas | RR | notas-raiz (MIDI) | Violinos II derivado |
|---|---|---|---|---|---|---|
| vln1 | sustain | 22 | 2 | 1 | 55–86 | não |
| vln1 | spiccato | 44 | 2 | 2 | 55–86 | não |
| vln1 | pizzicato | 44 | 2 | 2 | 55–86 | não |
| vln1 | tremolo | 21 | 2 | 1 | 55–86 | não |
| vln1 | legato | 30 | 2 | 1 | 55–96 | não |
| vla | sustain | 26 | 2 | 1 | 48–86 | não |
| vla | spiccato | 48 | 2 | 2 | 48–86 | não |
| vla | pizzicato | 46 | 2 | 2 | 48–86 | não |
| vla | tremolo | 24 | 2 | 1 | 48–86 | não |
| vc | sustain | 27 | 2 | 2 | 36–77 | não |
| vc | spiccato | 52 | 2 | 2 | 36–77 | não |
| vc | pizzicato | 52 | 2 | 2 | 36–77 | não |
| vc | tremolo | 25 | 2 | 1 | 36–77 | não |
| cb | sustain | 26 | 2 | 1 | 30–59 | não |
| cb | spiccato | 42 | 2 | 2 | 28–59 | não |
| cb | pizzicato | 40 | 2 | 2 | 28–59 | não |
| cb | tremolo | 16 | 2 | 1 | 28–59 | não |
| horns | sustain | 29 | 4 | 1 | 33–77 | não |
| horns | spiccato | 54 | 3 | 2 | 33–77 | não |
| vln2 | sustain | 22 | 2 | 1 | 55–86 | sim |
| vln2 | spiccato | 44 | 2 | 2 | 55–86 | sim |
| vln2 | pizzicato | 44 | 2 | 2 | 55–86 | sim |
| vln2 | tremolo | 21 | 2 | 1 | 55–86 | sim |

## Processamento (SeniorMy)

1. Nota, camada (v1/v2/v3 ou p/f) e round robin lidos do nome do arquivo; **oitava conferida medindo a
   altura real** (o VSCO usa dó central = C3 em quase tudo, mas C4 no violino solo).
2. Corte do silêncio inicial (−48 dB do pico, 4 ms de pré-ataque), fade-in de 1 ms.
3. Sustains/trêmolos limitados a 7,5 s com ponto de loop (fim 0,15 s antes do final; início entre 35–60%
   com RMS mais parecido) — o motor faz crossfade de potência constante de 120 ms no loop.
   Spiccato até 1,5 s, pizzicato até 2,5 s.
4. Volume: notas vizinhas da mesma camada aproximadas da mediana (±4 dB × 0,7) sem apagar a diferença
   natural entre camadas p/f; camada forte de cada instrumento normalizada para −20 dBFS RMS.
5. Balanço orquestral e pan (assentos americanos) em `library.json`; o usuário ajusta no mixer.
