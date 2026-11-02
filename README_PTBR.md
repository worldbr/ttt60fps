# TTT 60 FPS — Teste 05 (ULUS10537 / PPSSPP)

Objetivo: recuperar efeitos sem voltar a duplicar a velocidade do processador de tarefas. **EXPERIMENTAL. Nao e patch definitivo de 60 FPS.**

## Instalar / compilar

1. Use uma copia de teste do jogo. Desative o Teste 03 e o Teste 04, e qualquer PRX de expansao para evitar conflitos de hooks.
2. Suba o conteudo deste ZIP num repositorio GitHub proprio, rode Actions > `TTT FPS Teste 05 - PRX experimental` > Run workflow, ou compile localmente com PSPDEV usando `make`.
3. Do artefato compilado, copie a pasta `TTT_FPS_TEST` para `PPSSPP/PSP/PLUGINS/TTT_FPS_TEST` (local da memory stick em sua instalacao). Habilite o plugin no PPSSPP se necessario.
4. Em `PSP/Cheats/ULUS10537.ini`, mantenha **apenas** o Teste 02 (global), e nao o NOP do Teste 04:

   ```ini
   _S ULUS10537
   _G Dragon Ball Z Tenkaichi Tag Team
   _C1 60 FPS GLOBAL TESTE 05
   _L 0x20278E0C 0x00000001
   ```

5. Reinicie completamente o jogo e inicie batalha normal. Compare efeitos, animacoes, movimentos, relogio e FPS.
6. Veja `PPSSPP/PSP/PLUGINS/TTT_FPS_TEST/diagnostico_60fps.txt` e compartilhe o log. `HOOK INSTALADO` confirma a interceptacao; `AMOSTRA` deve mostrar `hits` aproximadamente o dobro de `actual`.

## Correcao de compilacao V2 (erro `_exit`)

O projeto original chamava `snprintf()` da biblioteca C padrao, trazendo dependencias da `newlib` para um PRX construido com `-nostartfiles`. Isso causou o erro `libc.a(libc_a-abort.o): undefined reference to '_exit'` ao linkar no PSPDEV GCC 15.2.0. A V2 eliminou as chamadas a `snprintf()` e usa formatacao propria (texto, hex e decimal), sem `stdio.h`. O Makefile tambem usa `-ffreestanding -fno-builtin` para evitar dependencias implicitas da libc.

**Substitua os arquivos `src/main.c` e `Makefile` ou utilize este ZIP completo.** O workflow inclui `make clean` antes de compilar, para nao reutilizar objetos antigos.

Esta versao foi revisada estaticamente; o compilador PSPDEV nao esta instalado no ambiente de preparacao. A compilacao definitiva deve ser verificada no GitHub Actions.

## Explicacao tecnica

O patch anterior `_L 0x2007B9D4 0x00000000` apagava a chamada original `jal 0x08901E10` em RAM no endereco `0x0887B9D4`. Essa rotina percorre os callbacks de atualizacao de objetos; desativar por completo impede que parte dos efeitos seja criada/atualizada.

O PRX intercepta a chamada e a executa a cada **duas chamadas**. Mantem demais partes do jogo a 60 Hz. Portanto as animacoes e cronometro **ainda podem acelerar**, pois essas partes tambem podem atualizar fora desse walker. Nao produz quadros intermediarios interpolados por si so.

## Seguranca / limites

- Somente ULUS10537 e EBOOT compativel. O plugin verifica se o opcode original de `0x0887B9D4` e `0x0E240784` antes de alterar memoria. Se divergir, nao faz o patch.
- Instalar/testar em separado do PRX de expansao. Nao misture os arquivos dos plugins.
- Ao remover ou desativar o plugin, reinicie inteiramente PPSSPP / jogo; nao use save state com instrucao modificada.
- Compilacao nao foi executada neste ambiente (PSPDEV indisponivel). O codigo foi preparado para o SDK e precisa ser compilado e validado no PPSSPP.
