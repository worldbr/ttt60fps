# TTT Camera Diagnostico V02 — ULUS10537 / PPSSPP

## Por que criamos a V02?
O log da V01 comprovou que os 2 callsites escolhidos nunca foram atingidos (call_A=call_B=0); por isso nenhuma alteracao aconteceu. Nesta V02, medimos um JAL diretamente dentro do desenho da batalha:

- `0x0885AD5C`: `jal 0x088068FC` (word `0x0E201A3F`)
- `0x0885AD60`: `move a0,s0` (word `0x02002025`), preservado como delay slot.
- `s0` vem de `*(uint32_t *)0x08A7E25C` (ponteiro ativo da camera).

**A V02 nao afasta a camera.** E um teste exclusivamente de monitoramento para confirmar que o codigo executa durante a batalha, qual estrutura e usada e quais sao as transformacoes. Nao mexe na renderizacao, viewport, FPS nem faz NOP em funcoes.

## Instalar
1. Desative TTT_CAMERA_DIST01 (V01), cheats de camera e PRXs experimentais.
2. Coloque os arquivos do ZIP na raiz do repositorio GitHub e rode `Actions > TTT - Camera Diagnostico V02 > Run workflow`.
3. Baixe o artifact. Copie a pasta `TTT_CAMERA_DIAG02` gerada para `PSP/PLUGINS/` do PPSSPP.
4. Confirme que o plugin esta habilitado e reinicie o emulador.
5. Entre numa batalha por 15 a 20 segundos e mova a camera. Volte ao menu e envie o arquivo `PSP/PLUGINS/TTT_CAMERA_DIAG02/diagnostico_camera_v02.txt`.

## Como interpretar
- `hits` proximo de 150 por 5 s em combate 30 FPS: novo callsite confirmado.
- `hits=0`: caminho ainda nao e atingido ou hook precisa ser revisado; nada sera alterado.
- `arg` e `global` mostram o ponteiro recebido na chamada e o ponteiro global da camera ativa.
- `mat40_*` mostra os bits IEEE754 das componentes da matriz de camera, SEM escrever nelas.

A verificacao local analisa o ELF original exato, SHA256 `834605685873da14647b1dfe7bf332e326dd2a99a48840877da13d211ea18b1e`. Compilacao no PSPDEV e funcionamento no PPSSPP aguardam teste.
