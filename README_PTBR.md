# TTT 60 FPS — Teste 07: diagnóstico, sem cortar atualizações

**Jogo:** Dragon Ball Z: Tenkaichi Tag Team (`ULUS10537`) no **PPSSPP**.

**Objetivo:** medir, durante combate normal, a frequência de cinco pontos do pipeline. Todos os hooks chamam **o código original em TODAS as invocações**. O PRX não ignora atualizações nem tenta corrigir velocidade ou efeitos nesta versão. Ele não implementa interpolação.

## O que o Teste 07 faz

Instala temporariamente wrappers em instruções MIPS `jal` originais, preservando os *delay slots* e chamando os destinos originais. Soma contadores em memória, sem escrever logs na thread do jogo. Uma thread auxiliar salva uma amostra aproximadamente a cada cinco segundos. **Como todo hook, ainda altera temporariamente algumas instruções em RAM; pode afetar desempenho ou compatibilidade.**

| Contador | Endereço do JAL | Destino original | O que mede |
|---|---|---|---|
| `update_group` | `0x0885AC9C` | `0x0887B9AC` | Quantas vezes começa o bloco de atualização de tarefas |
| `task_update` | `0x0887B9D4` | `0x08901E10` | Quantas vezes o walker de update é chamado (não conta objetos individuais) |
| `draw_group` | `0x0885ACAC` | `0x0887BA50` | Quantas vezes começa a preparação do desenho de tarefas |
| `task_draw` | `0x0887BB0C` | `0x08902134` | Quantas vezes o walker de desenho é chamado (pode acontecer várias vezes por quadro) |
| `frame_composer` | `0x08804728` | `0x08837B68` | Quantas vezes o loop principal chama o compositor de quadro |

O plugin também registra `step`, a variável de VBlank `0x08A78E0C` (normalmente `1` com o cheat, ou `2` em batalha original).

## Como compilar

1. Envie o conteúdo **desta pasta** para a raiz de um repositório GitHub (ou substitua o conteúdo do repositório usado no Teste 05).
2. Vá em **Actions → TTT FPS - Teste 07 Diagnostico → Run workflow**.
3. No artefato `TTT-FPS-Teste07-Diagnostico`, procure `TTT_FPS_TEST07/ttt_fps_teste07.prx` e `plugin.ini`.
4. Se preferir compilar localmente: `make clean && make` com PSPDEV configurado.

**Status:** código-fonte verificado sintaticamente com compilador C local e comparado estaticamente com o ELF de referência. O build PSPDEV/PPSSPP **ainda precisa ser executado por você**.

## Instalação / isolamento

1. Faça backup do jogo e da pasta de plugins. **Desative/remova os PRXs dos Testes 05 e 06**, o TTT Expansion PRX e cheats que modifiquem as chamadas `0x0885AC9C`, `0x0885ACAC`, `0x0887B9D4` ou `0x0887BB0C`. Reinicie completamente o PPSSPP, sem restaurar um savestate criado com esses patches.
2. Copie a pasta `TTT_FPS_TEST07` do artefato para `memstick/PSP/PLUGINS/TTT_FPS_TEST07` (caminho da memória virtual do PPSSPP).
3. Confirme que o plugin está ativo nas configurações de plugins do PPSSPP.
4. **Primeiro experimento (30 FPS, linha de base):** **desative o cheat de 60 FPS**, reinicie o jogo, entre numa batalha e aguarde 15-20 segundos, com Goku fazendo golpes e carregando Kamehameha.
5. **Segundo experimento (60 FPS, sem correção):** reinicie o jogo e ative **somente** este cheat (não use outros NOPs):

```ini
_S ULUS10537
_G Dragon Ball Z Tenkaichi Tag Team
_C1 TTT - 60 FPS APENAS PARA DIAGNOSTICO
_L 0x20278E0C 0x00000001
```

6. Entre em batalha novamente, aguarde 15-20 segundos e execute golpes com efeitos. O combate continuará **acelerado** nesta etapa; é esperado.
7. Leia o arquivo `memstick/PSP/PLUGINS/TTT_FPS_TEST07/diagnostico_teste07.txt`. **Me envie o log completo** e descreva FPS, velocidade e efeitos.

## Como interpretar

- `HOOKS INSTALADOS 5/5` confirma que o PRX aceitou as instruções originais (não garante ausência de bug).
- Cada contador mostra `total(+delta/5s)`; o **delta** indica chamadas desde a amostra anterior.
- `frame_composer`: deve ajudar a comparar 30 versus 60 apresentações por segundo; não mede FPS efetivos na tela ou GPU.
- `task_update` e `task_draw` contam chamadas a seus walkers, não número de personagens ou efeitos.
- Se alguma instrução não corresponder ao ELF de referência, o PRX **não instala nenhum hook**. Um log `ERRO opcode ...` pode significar EBOOT diferente ou cheat/plugin conflitante.
- As amostras abrangem menus ou carregamentos, não só combate; compare apenas janelas em que você passou os cinco segundos inteiros em luta.

## Segurança e reversão

**Este PRX é diagnóstico:** não restaura movimento normal, não corrige efeitos e não deve ser misturado com PRXs antigos. Desative o PRX e o cheat e **reinicie inteiramente o emulador** para desfazer os patches voláteis. Não salve progresso de jogo durante experimentos com comportamento acelerado.

ELF referência: `00000010-0000000A-EBOOT.OLD(1).ELF` (SHA-256 `834605685873da14647b1dfe7bf332e326dd2a99a48840877da13d211ea18b1e`). Apenas essa versão foi checada estaticamente. Rodar em outra região/versão pode falhar, e o plugin deve abortar caso o opcode difira.
