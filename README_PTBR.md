# TTT Camera FOV V0.5 — Teste de perspectiva (ULUS10537 / PPSSPP)

**Objetivo:** testar uma visão mais ampla preservando a posição original da câmera e os valores do viewport. É experimental: o offset de projeção `+0xC0` foi identificado no ELF, mas o efeito visual no PPSSPP ainda precisa ser confirmado.

## Instalação e compilação

1. **Desative** TTT_CAMERA_DIST03, SAFE04, SAFE041, outros PRXs de câmera e cheats de viewport/FPS.
2. Extraia o projeto completo para um **repositório próprio** no GitHub, preservando as pastas `.github`, `src`, `tests`, `TTT_CAMERA_FOV05` e os arquivos Makefile/exports.exp.
3. Em GitHub Actions, execute **TTT Camera FOV V05 (experimental)**. Não exige Python no ambiente PSPDEV.
4. Baixe o artefato `TTT-Camera-FOV-V05-Experimental` produzido pelo workflow.
5. Copie **a pasta inteira** `TTT_CAMERA_FOV05` do artefato para `PSP/PLUGINS/TTT_CAMERA_FOV05` no PPSSPP. Os arquivos devem estar lado a lado:

```
PSP/PLUGINS/TTT_CAMERA_FOV05/
  plugin.ini
  ttt_camera_fov05.prx
```

6. Ative o plugin nas configurações de plugin do PPSSPP e reinicie o jogo.

O `plugin.ini` usa exatamente o formato que você enviou:

```ini
[options]
version = 1
type = prx
filename = ttt_camera_fov05.prx

[games]
ULUS10537 = true
```

O workflow **confere esse nome automaticamente** e falha se o PRX não corresponder.

## Modos

O plugin começa **desligado**. Em batalha, pressione **L + R + Triângulo** e solte antes de cada toque:

- 0: original (fator 1,00)
- 1: campo visual levemente maior (coeficientes x0,92)
- 2: campo visual intermediário (coeficientes x0,84)
- 3: campo visual mais amplo (coeficientes x0,76)

**Importante:** esses números são fatores dos coeficientes da matriz, não graus ou aumento exato percentual do FOV. O motor pode usar várias matrizes; caso nenhum modo tenha efeito, o diagnóstico nos orientará a testar a matriz correta.

## O que é modificado

- Hook comprovadamente executado na batalha em `0x089AC4E0`, que chama a rotina original `0x089ABE54`.
- Modifica temporariamente os componentes de escala X/Y da matriz de perspectiva em `camera + 0xC0` (índices 0 e 5), chama a rotina original e **restaura** os componentes originais antes de retornar.
- Mantém a posição/rotação da câmera, viewport, lógica da batalha, FOV de outras câmeras fora dessa rotina e tempo de quadros **inalterados**.
- Outro hook apenas observa a câmera efetiva em `0x0885AD5C`.

## Teste solicitado

Compare com a câmera original no mesmo cenário: Goku parado, movimento lateral, combo e Kamehameha. Observe se os efeitos estão alinhados e se personagens não somem nos cantos. Não é garantido que os cortes dos especiais desapareçam; o game pode usar outra câmera ou outro pipeline de projeção.

Log: `PSP/PLUGINS/TTT_CAMERA_FOV05/diagnostico_fov_v05.txt`.

No log, `rebuild` deve subir durante a luta; `applied` deve subir nos modos 1–3 se os coeficientes forem válidos. `p00/p11` são os valores da matriz de projeção original; `out00/out11` são do resultado da reconstrução. Zero em `applied` significa que o plugin não alterou as matrizes, mesmo que tenha instalado os hooks.

**Segurança:** experimento para PPSSPP e ULUS10537 somente. Pode haver renderização incorreta/travamento. Se houver, desative o plugin e reinicie o PPSSPP. Não grave progresso durante o teste.

**Validação local:** estrutura, offsets e instruções do ELF e checagem `plugin.ini`; a compilação PSPDEV e o resultado visual ainda precisam do teste no seu GitHub/PPSSPP.
