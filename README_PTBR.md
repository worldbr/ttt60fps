# TTT — câmera mais afastada: Teste 01 (experimental)

**Jogo**: Dragon Ball Z Tenkaichi Tag Team **ULUS10537**, apenas PPSSPP. **Sem alteração na ISO.**

## Ideia

Este experimento **não** altera a escala do viewport (`0x08A784F4/F8`) e **não** altera os FPS.

O ELF mostra que a rotina `0x089ABEE0` produz a transformação da câmera e que `0x089ABE54` prepara suas matrizes. Existem duas chamadas `jal 0x089ABE54`, em `0x089AB02C` e `0x089AB734`. O projeto as intercepta preservando seus delay slots, desloca a translação da matriz `cam+0x40` (elementos `+0x70`, `+0x74`, `+0x78`) ao longo da terceira coluna (`cam+0x60`, `+0x64`, `+0x68`) e executa a rotina original **uma vez por chamada**.

**Atenção: o sentido de afastamento (+Z) e a persistência da posição ainda precisam ser validados no PPSSPP.** Dependendo do sistema interno de coordenadas, +Z pode aproximar em vez de afastar. O plugin inclui um modo com sinal invertido. O movimento é um deslocamento em **unidades do jogo**, não um percentual comprovado. Se o motor mantiver a posição modificada entre quadros, o afastamento pode se acumular; nesse caso desligue pelo atalho e reinicie o emulador. Evite saves durante o experimento.

## Instalação e compilação

1. Faça uma cópia de segurança e **desative todos os cheats de câmera** do seu amigo (`002784F4`/`002784F8` ou `202784F4`/`202784F8), os cheats de 60 FPS e os PRXs antigos de FPS/expansão. Reinicie o PPSSPP para restaurar a RAM.
2. Envie o conteúdo **desta pasta** para a raiz de um repositório GitHub. Execute **Actions > TTT - Camera afastada V01 experimental > Run workflow**. O arquivo gerado estará no artefato `TTT-Camera-Afastada-V01-Experimental`.
3. Copie `TTT_CAMERA_DIST01/ttt_camera_dist01.prx` e `plugin.ini` para `memstick/PSP/PLUGINS/TTT_CAMERA_DIST01`, conforme seu diretório real de memstick do PPSSPP, e ative o plugin.
4. Abra uma batalha comum. O plugin começa **DESATIVADO** por segurança; use **L + R + TRIÂNGULO** para alternar os modos.

| Modo | Deslocamento da câmera |
|---|---|
| 0 | original (sem alteração) |
| 1 | +80 unidades ao longo do eixo local Z (primeiro teste) |
| 2 | +160 unidades ao longo do eixo local Z |
| 3 | -80 unidades (teste de direção oposta) |

**Observação importante:** isto é um protótipo técnico para localizar o sentido correto da câmera. Ainda não é uma câmera de 120% nem um cheat final validado. Se o personagem sair da tela, ocorrer travamento ou a câmera se afastar sem parar, **pare o teste**, desligue o plugin e reinicie o PPSSPP.

## Log de diagnóstico

O PRX grava `PSP/PLUGINS/TTT_CAMERA_DIST01/diagnostico_camera.txt` a cada 5 segundos e quando você muda de modo.

- `HOOKS INSTALADOS 2/2`: chamadas reconhecidas, mas **não garante** câmera correta.
- `call_A/B`: número de vezes que cada rota de câmera é executada.
- `aplicado`: quantas vezes a translação foi alterada.
- `ignorado`: matriz com valores inválidos ou fora dos limites de segurança.
- `posbits`: números float originais antes do deslocamento, em hexadecimal IEEE-754.
- `len2bits`: norma ao quadrado do eixo Z; normalmente perto de `3F800000` (=1.0).

## O que testar

1. Compare o modo 0 com 1 usando Goku parado diante de um objeto fixo do cenário; observe se a câmera se **afasta** ou **aproxima**, e se ela continua apontada para Goku.
2. Mova o personagem e veja se a distância permanece estável, sem aumentar sozinha ao longo do tempo.
3. Carregue Kamehameha; observe energia, feixe e explosões. Teste também o modo 2 somente se modo 1 estiver estável.
4. Envie o **log e o resultado visual**. Com isso ajustaremos sinal/matriz e tentaremos um cheat ou patch mais estável.

## Validação / limitações

O ELF original fornecido possui SHA-256 `834605685873da14647b1dfe7bf332e326dd2a99a48840877da13d211ea18b1e`. O projeto verifica os dois JALs e seus delay slots antes de escrever. O script `tests/verificar_elf.py` valida essas instruções no ELF fornecido.

Código validado estaticamente, sem build real PSPDEV nem execução PPSSPP neste ambiente. **É possível que não modifique a câmera ou que provoque glitches**; o propósito é obter uma medida reproduzível em vez de alterar endereços arbitrários.
