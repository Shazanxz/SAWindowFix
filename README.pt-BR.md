# SAWindowFix

[English](README.md) | **Português**

Um mod ASI para **GTA San Andreas** (e **SA:MP**) que roda o jogo em **janela sem bordas** ocupando o monitor inteiro. O Alt-Tab é instantâneo, o jogo continua rodando em segundo plano e nada é minimizado ou perdido quando você troca de janela.

## Recursos

- **Janela sem bordas**: parece tela cheia, mas se comporta como janela. Alt-Tab sem tela preta e sem minimizar.
- **Continua rodando em segundo plano**: sem pausa, sem menu de pausa ao voltar, e o áudio continua tocando.
- **Renderiza na resolução do menu**: a imagem é esticada até o monitor, então baixar a resolução no menu do jogo dá mais FPS em placas de vídeo fracas.
- **Visão 4:3 original**: mantém o campo de visão da tela cheia original, em vez do zoom do modo janela.
- **Mouse correto** no menu do jogo e nos diálogos e no chat do SA:MP, em qualquer resolução.
- **Vsync desligado**.

## Compatibilidade

| Jogo | Status |
|---|---|
| GTA San Andreas 1.0 US | Testado, funcionando |
| Outras versões (1.01, 2.0, Steam, ...) | Não suportado, nada é alterado |

| Junto com | Status |
|---|---|
| SA:MP, SAMPFUNCS, MoonLoader, CLEO | Testado, funcionando |
| Outros mods de janela / sem pausa | Não compatível (veja abaixo) |

É preciso ter um **ASI Loader** instalado (o CLEO e o SAMPFUNCS já incluem um).

Antes de mudar qualquer coisa, o SAWindowFix confere todos os pontos que ele modifica. Se outro mod de janela, sem bordas ou sem pausa já tiver alterado algum deles, o SAWindowFix **não muda nada** e registra no log qual mod está em conflito. Mantenha só um dos dois.

## Instalação

1. Baixe o `SAWindowFix.asi` em [Releases](../../releases).
2. Copie para a pasta do GTA San Andreas, ao lado do `gta_sa.exe`.
3. Abra o jogo. Ele já abre em janela sem bordas.

O `SAWindowFix.ini` é criado automaticamente na primeira execução.

## Configuração

```ini
[SAWindowFix]
; Write SAWindowFix.log next to the .asi (0 = off, 1 = on)
Log=0
```

- **Log**: `1` gera o `SAWindowFix.log` ao lado do `.asi`. Ative ao relatar um problema e anexe o arquivo.

O log registra:
- a versão do mod e o ambiente (caminho do jogo, versão do Windows, monitor)
- cada patch aplicado
- os resets do device, com a causa
- o FPS a cada 10 segundos
- detalhes de crash (módulo + offset, registradores, pilha)

## Dicas

- **Mais FPS**: escolha uma resolução menor no menu do jogo (ex.: `1280x720`). O jogo desenha menos pixels e a imagem é esticada até o monitor.
- **Travadinha depois de Win+L, UAC ou Ctrl+Alt+Del**: é o Direct3D 9 recuperando o device. É normal e não tem como evitar.

## Como funciona

O mod modifica o `gta_sa.exe` na memória ao carregar:

- **Janela**: força os parâmetros de apresentação do D3D9 para modo janela logo antes de o device ser criado, transforma a janela em um popup sem bordas e hookeia `IDirect3DDevice9::Reset`/`Present` para ela continuar assim depois de um reset do device ou do menu ESC.
- **Sem pausa**: os tratamentos de perda de foco no procedimento da janela são neutralizados, então o `ForegroundApp` nunca é zerado e o loop principal continua rodando.
- **Imagem esticada**: as checagens de tamanho da área cliente do RenderWare são puladas, então o backbuffer mantém a resolução do menu enquanto a janela cobre o monitor. As mensagens do mouse são reescaladas no import `PeekMessageA` do jogo, antes de qualquer procedimento de janela (incluindo o do SA:MP) recebê-las.

Cada ponto modificado é comparado com os bytes originais antes de qualquer escrita. As chamadas são redirecionadas em vez de apagadas com NOP, para que outros mods que hookeiam as mesmas chamadas continuem funcionando.

## Compilando

Requer Visual Studio 2019+ (carga de trabalho "Desenvolvimento para desktop com C++") e CMake 3.20+.

```sh
cmake -S . -B build -A Win32
cmake --build build --config Release
```

Saída: `build/Release/SAWindowFix.asi`. Adicione `-DGTA_SA_DIR="C:/Jogos/GTA San Andreas"` para copiar para o jogo a cada build.

A versão é gerada pelo git: cada commit aumenta o último número (`1.0.1`, `1.0.2`, ...), uma tag `vX.Y.Z` define uma nova base, e builds com alterações não commitadas recebem `-dev`.

## Licença

Licenciado sob a [PolyForm Noncommercial License 1.0.0](LICENSE).

- **Permitido**: usar, estudar, modificar, fazer fork e compartilhar o mod e o código, para qualquer fim não comercial.
- **Proibido**: vender ou usar para qualquer fim comercial.
- **Obrigatório**: quem compartilhar o mod ou um código derivado dele precisa incluir a licença e manter a linha de crédito:
  `Required Notice: Copyright (c) 2026 Shazanxz (https://github.com/Shazanxz)`

Sugestões, issues e pull requests são bem-vindos.

*O texto em [LICENSE](LICENSE) (em inglês) é o que vale juridicamente; este resumo é só para facilitar.*

## Créditos

- [**Shazanxz**](https://github.com/Shazanxz) - desenvolvimento e manutenção
- [**mrxenginner**](https://github.com/mrxenginner) - criação da primeira versão e melhorias
- [**ThirteenAG**](https://github.com/ThirteenAG) - inspiração, pelo [WindowedMode Plugin](https://github.com/ThirteenAG/III.VC.SA.WindowedMode)
