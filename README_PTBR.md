# TTT Camera Safe V04.1 — reparo da V04 (ULUS10537 / PPSSPP)

## Por que a V04 não funcionou?
O log V04 mostrou `modified=0` em TODOS os modos. O filtro que exigia um agrupamento fixo de quatro câmeras (`in_normal_group`) estava rígido demais. A V03, sem esse filtro, havia afastado a câmera corretamente.

## O que mudou
- **Modo 3 (-40)**: restaura exatamente o deslocamento da V03, sem depender do filtro de câmera da V04. É o teste de controle e deve afastar a câmera como antes.
- Modos 1 (-15) e 2 (-25): afastamentos mais leves, com o mesmo método da V03.
- **Modo 4 (-40 automático)**: filtro muito mais flexível; permite múltiplas câmeras da batalha, distingue de forma experimental câmeras de cena especial por faixa/ponteiro e reduz o deslocamento para -8 quando detecta deslocamento/rotação brusca. **É experimental e pode falhar em alguns closes**.
- Novos diagnósticos registram por que o modo 4 não modificou algo (`auto_skipped`, `script_skip`, `far_skip`, `not_ready`).
- Hooks, delay slots e funções originais preservados; sem viewport/FOV/FPS.

## Instalação
1. Desative/remova V03, V04 e outros plugins de câmera; desligue cheats de câmera e de 60 FPS.
2. Copie o conteúdo do ZIP completo para a raiz do repositório (substituindo src/main.c, Makefile e exports.exp).
3. No GitHub Actions, execute `TTT Camera Safe V041 (corrigida)`.
4. Baixe o artifact e instale a pasta `TTT_CAMERA_SAFE041` na pasta PSP/PLUGINS do PPSSPP. Habilite no PPSSPP.
5. Reinicie o jogo e entre em uma batalha; plugin começa desligado.

## Controles
L + R + Triângulo alterna: 0 (original), 1 (-15), 2 (-25), 3 (-40 idêntico à V03), 4 (automático).

## Teste recomendado
1. Em batalha normal, ative **modo 3** por 10–15 segundos. Confirme se volta a afastar como a V03.
2. Passe ao **modo 4** e teste movimentos laterais, combos e closes antes de golpes especiais. Compare os cortes de personagem.
3. Envie `PSP/PLUGINS/TTT_CAMERA_SAFE041/diagnostico_camera_v041.txt`.

## Observações
- Esta versão não calcula o posicionamento dos dois lutadores na tela. Portanto, NÃO garante correção de enquadramento em combos; a solução definitiva exigirá encontrar/alterar o alvo de câmera.
- Durante o modo 3, cenas especiais podem continuar com os problemas da V03. Modo 4 tenta reduzir a interferência, mas pode falhar em cutscenes porque a detecção é heurística.
- Caso ocorram travamentos ou câmera incorreta, desative o PRX e reinicie o PPSSPP; não salve durante testes.
- Compilação PSPDEV e teste real no jogo ainda não foram realizados aqui.
