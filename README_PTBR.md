# TTT Camera Distance — V0.3 experimental (PPSSPP / ULUS10537)

## Por que a V0.3?
- A V0.1 interceptou 2 calls que não eram executados na batalha.
- A V0.2 confirmou o ponteiro global da câmera e 30 chamadas/segundo no desenho (0x0885AD5C).
- Análise estática do ELF confirmou um terceiro callsite para reconstrução das matrizes: `0x089AC4E0`, com `JAL 0x089ABE54`, precedido por `0x089AC4D4 -> 0x089ABEE0` que prepara a transformação original.
- A V0.3 intercepta **ambas** as chamadas (`0x089AC4E0` para alteração experimental, `0x0885AD5C` para medição), preservando o JAL delay slot e chamando cada função original uma vez.

## Instalação
1. Desative PRXs de câmera antigos e quaisquer cheats de viewport/FOV e cheats de 60 FPS. Reinicie completamente PPSSPP.
2. Coloque os arquivos na raiz do seu repositório de GitHub e execute `Actions -> TTT Camera Distance V03 (experimental)`.
3. Copie o diretório compilado `TTT_CAMERA_DIST03` para `PSP/PLUGINS/` no PPSSPP e habilite o plugin para ULUS10537.
4. Inicie a batalha normalmente; o plugin começa no modo 0 (desligado).
5. Aperte **L + R + Triângulo** (uma vez por mudança de modo):
   - Modo 0: original, sem alteração.
   - Modo 1: +40 unidades no eixo Z local da câmera.
   - Modo 2: +80 unidades no eixo Z local.
   - Modo 3: -40 unidades no eixo Z local (teste da direção oposta).
6. Movimente Goku, use Kamehameha e observe alinhamento de personagem/efeitos, distância e movimentação.
7. Envie `PSP/PLUGINS/TTT_CAMERA_DIST03/diagnostico_camera_v03.txt` e conte o que mudou em cada modo.

## Segurança e limites
- **Experimental.** O novo hook de reconstrução foi confirmado estaticamente, mas **ainda não foi confirmado que esse callsite executa durante a batalha**. O log mostra contadores `rebuild`, `modified` e `draw` para separar os casos.
- Em modos 1-3, a alteração ocorre **depois da preparação original da matriz e antes de reconstruir matrizes derivadas**, evitando alterar diretamente o viewport.
- O eixo Z local pode apontar para frente ou para trás. Se os modos positivos aproximarem, experimente modo 3.
- Ainda é possível que efeitos, câmera especiais ou colisões não acompanhem o afastamento corretamente.
- Se aparecer tremor, efeito deslocado, travamento ou câmera desalinhada, desligue o plugin e reinicie PPSSPP. Não salve o jogo com câmera modificada.
- Não usar simultaneamente com V0.1, V0.2 ou plugin de expansão. Não altera ISO, FPS ou arquivos do jogo.
- A compilação PSPDEV e a execução no PPSSPP **ainda dependem do teste do usuário**.

## Interpretação do log
- `draw=+150` por 5s: visualização da câmera ativa continua monitorada.
- `rebuild=+N`: reconstrução executada; `rebuild=0` indica que a rotina escolhida não é ativa nesse cenário.
- `modified=+N` em modo >0: escreveu na matriz antes do rebuild.
- `rejected`: ponteiro ou matriz falhou nas verificações de segurança.
- `rebuild_cam`, `draw_cam`, `active`: endereços para avaliar se modificamos a mesma câmera que a batalha usa.
- `old_xyz_bits`: posição antes do deslocamento, em floats IEEE754.

## Referência do ELF
SHA-256: `834605685873da14647b1dfe7bf332e326dd2a99a48840877da13d211ea18b1e`.
Sites e opcodes validados nesse arquivo, não garantidos para outros executáveis ou regiões.
