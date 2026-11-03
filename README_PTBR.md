# TTT Camera FOV V0.6 — diagnostico de matrizes A/B

**ULUS10537 / PPSSPP somente.** Esta versao nao modifica o viewport nem afasta fisicamente a camera.

## Por que esta versao

Na V0.5, o codigo alterou coeficientes e o log registrou resultados diferentes, mas **nao houve mudanca visual**. O ELF mostra ao menos duas matrizes potenciais de projecao e duas matrizes combinadas. A V0.6 testa A (`camera+0x80`) e B (`camera+0xC0`) de forma independente e monitora a chamada de envio de matrizes em `0x089AC4F0`.

Isso **nao prova** que A/B representam o FOV principal. O objetivo e determinar qual caminho realmente modifica a imagem.

## Modos: L + R + Triangulo

- **0** = original (inicial)
- **1** = multiplica os coeficientes [0] e [5] da matriz A (`+0x80`) por **0,70**
- **2** = mesmos coeficientes da matriz B (`+0xC0`) por **0,70** (contraprova V0.5)
- **3** = multiplica ambas A e B por 0,70

Fator 0,70 e deliberadamente alto para revelar a mudanca; **nao e 30% exatos de FOV**. Nao teste em uma partida para salvar progresso.

## Compilacao GitHub Actions

1. Extraia todo o projeto para um repositorio GitHub proprio, preservando `.github/workflows`, `src`, `tests`, `Makefile`, `exports.exp` e `TTT_CAMERA_FOV06`.
2. Execute **TTT Camera FOV V06 (matrizes A/B)** no Actions.
3. Baixe o artefato produzido e instale a pasta `TTT_CAMERA_FOV06` completa em `PSP/PLUGINS/TTT_CAMERA_FOV06`.
4. Ative o plugin do PPSSPP, desative **todos os outros plugins de camera** e cheats de viewport e reinicie o jogo.

Arquivos corretos lado a lado:

```
PSP/PLUGINS/TTT_CAMERA_FOV06/
  plugin.ini
  ttt_camera_fov06.prx
```

`plugin.ini` sera validado ANTES e DEPOIS da compilacao, no formato do arquivo que voce enviou:

```ini
[options]
version = 1
type = prx
filename = ttt_camera_fov06.prx

[games]
ULUS10537 = true
```

## Protocolo de teste

Entre em uma batalha e compare a mesma cena com o Goku parado: original, modo 1, original, modo 2, original, modo 3. Deixe cada modo alguns segundos; observe **personagem, chao e efeitos**. Se houver uma distorcao anormal, desative o plugin e reinicie.

Depois, envie `PSP/PLUGINS/TTT_CAMERA_FOV06/diagnostico_fov_v06.txt` e diga qual modo mudou visualmente.

## Diagnostico

O log registra `rebuild`, `draw`, `upload` e os coeficientes de A/B/C e os resultados enviados. `upload` > 0 confirma que o hook de envio executou; `applied` > 0 confirma apenas uma alteracao numerica, **nao confirma efeito visual**.

A compilacao no PSPDEV e o efeito visual dependem do seu teste no PPSSPP. O teste de sintaxe local usa cabecalhos temporarios e nao substitui o PSPDEV.
