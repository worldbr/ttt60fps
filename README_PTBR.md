# TTT CAMERA SAFE04 — teste de enquadramento (ULUS10537 / PPSSPP)

## O que este teste faz
- Preserva os dois hooks da V0.3 e suas chamadas originais.
- Não modifica viewport, FOV, FPS, modelos nem efeitos.
- Descobre a família das quatro câmeras regulares de batalha (passo 0x290).
- Ignora o deslocamento quando o jogo desenha com câmera de outra família; isso pode proteger cenas especiais.
- Em modo 4, aplica -40 unidades em condições estáveis, mas volta suavemente para -8 durante movimentos bruscos da câmera.
- **É experimental:** pode reduzir cortes de enquadramento, mas não conhece os limites de tela de cada lutador nem recalcúla o ponto de foco. Não é a correção definitiva de combos.

## Botão
L + R + Triângulo alterna: 0 original; 1 -15; 2 -25; 3 -40 (com proteção de câmera especial); 4 automático (-40/-8).
A versão V0.3 usava -40 como modo 3. O modo 4 é o mais importante para comparar.

## GitHub Actions
1. Coloque os arquivos deste projeto na raiz do repositório.
2. Actions > TTT Camera Safe Framing V04 > Run workflow.
3. Baixe o artefato; copie a pasta `TTT_CAMERA_SAFE04` para `PSP/PLUGINS` e habilite o plugin no PPSSPP.
4. Desative o PRX V0.3 e todos os cheats de viewport/60 FPS antes de testar.

## Teste
Compare modo 3 e modo 4 durante: luta parada; deslocamento com analógico para esquerda/direita; combo longo; início de especial; close antes de especial; retorno da câmera à luta. Sem save state automático.

## Log
`ms0:/PSP/PLUGINS/TTT_CAMERA_SAFE04/diagnostico_camera_v04.txt`
Confira: `normal=0x...` (câmera padrão encontrada), `special_skip` (câmera diferente detectada), `tracking_trigger` (movimento rápido detectado), `shift_bits` (último afastamento aplicado), `modified`.

## Limitações / recuperação
A V0.4 não calcula onde o Goku/Vegeta estão na tela. Os cortes nos cantos podem persistir. Pular o ajuste durante câmera cinematográfica depende de o jogo usar outro ponteiro; se usar a mesma câmera, não detecta a mudança. Por isso mantenha backup e reinicie o PPSSPP para restaurar os hooks.

Compilação PSPDEV e execução no jogo ainda não foram confirmadas.
