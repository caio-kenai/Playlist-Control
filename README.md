<p align="center">
  <img src="assets/logo.png" alt="PlaylistControl" width="320">
</p>

# PlaylistControl

Central de configuração e operação do **Playlist Digital** para Windows.

O PlaylistControl reúne em uma única janela o que hoje exige abrir a pasta
`C:\Playlist\pgm`, o bloco de notas e várias ferramentas: mapas comerciais,
grades musicais, relógios operacionais, o `PLAYLIST.ini`, as opções do
`CONFIG.XML`, pastas e códigos registrados, operadores e diagnóstico.
O operador não precisa conhecer a estrutura interna dos arquivos: o programa
lê, mostra de forma estruturada, valida, grava com segurança e registra o que
foi alterado.

Escrito em C++20 com [JUCE](https://juce.com) e CMake.

![Painel](docs/screenshots/01-painel.png)

## Funcionalidades

**Programação**

- **Mapas, grades e relógios** desenhados como no painel de programação do
  Playlist: faixa lateral Comercial/Musical, cabeçalho com horário e
  parâmetros (ID, DUR, FIXO, SAT, LOCAL, bloqueado, descarte) e cada item com
  o que ele toca de fato.
- Cada código é resolvido contra as pastas do Config Manager e os registros do
  `LIGACAO.DBF`, considerando a validade do registro na data do arquivo e o
  rodízio. Itens que o Playlist mostraria com X vermelho aparecem com X
  vermelho e o motivo.
- Edição de blocos e itens (código, arquivo, `CÓDIGO|arquivo`, comando),
  criação e remoção de blocos, criação de arquivos novos (vazios, com os
  horários de outro arquivo ou copiados).
- **Origem de cada arquivo**: Planner (Sync Service), Commercial, Maker
  (Playlist Server), Horário Eleitoral ou manual. Mapas que o Sync Service
  regrava a cada sincronização são sinalizados antes de qualquer edição.

**Configuração**

- **Leitura de mapas (`PLAYLIST.ini`)**: formato AUTO/TXT1 e padrão de
  arquivo de cada fonte, com a prévia dos próximos 7 dias mostrando qual
  arquivo o Playlist vai procurar e se ele existe; afiliadas e beep. Seções que
  o programa não conhece são mantidas.
- **Opções do Playlist (`CONFIG.XML`)**: as opções de Ferramentas > Opções
  agrupadas como no Playlist, com a explicação do manual. A edição só é
  liberada com o Playlist fechado, porque ele regrava esse arquivo.
- **Pastas e códigos**: pastas do Config Manager, comandos, códigos
  registrados, validade e onde cada arquivo foi encontrado (consulta).
- **Operadores**: permissões de cada operador, com o valor efetivo quando a
  permissão segue o padrão (consulta).

**Suporte**

- **Diagnóstico** de toda a instalação. Cada problema informa o que está
  errado, onde (arquivo e linha), por que isso importa para o Playlist e como
  resolver.
- **Histórico**: toda gravação guarda a versão anterior, com data, operação,
  usuário e a diferença linha a linha; qualquer versão pode ser restaurada.
- **Alterações externas**: a pasta `pgm` é observada; quando outro programa
  muda um arquivo aberto, ele é recarregado ou, se houver edição pendente, o
  conflito é mostrado e nada é sobrescrito.
- **Modo somente leitura**, ativo por padrão até o operador confirmar a
  instalação e permitir alterações.

| Mapas | Leitura de mapas |
|---|---|
| ![Mapas](docs/screenshots/02-mapas.png) | ![Leitura de mapas](docs/screenshots/05-leitura-de-mapas.png) |
| **Opções do Playlist** | **Diagnóstico** |
| ![Opções](docs/screenshots/06-opcoes.png) | ![Diagnóstico](docs/screenshots/09-diagnostico.png) |

As imagens usam a instalação fictícia de `tests/fixtures/installation`.

## Segurança na gravação

Nenhum arquivo do Playlist é alterado sem passar por este caminho:

1. validação do conteúdo novo (erros de estrutura impedem a gravação);
2. comparação com o arquivo no disco — se outro programa o alterou depois de
   aberto, a gravação é recusada;
3. cópia de segurança da versão atual no Histórico;
4. gravação em arquivo temporário na mesma pasta, descarga no disco e
   releitura;
5. substituição atômica (`ReplaceFileW`) — o Playlist vê o arquivo antigo ou o
   novo, nunca um meio-termo;
6. releitura do resultado e registro no histórico e no log.

Os arquivos são regravados sem perda: linhas não alteradas, comentários,
ordem, codificação (ASCII, Windows-1252, UTF-8 com ou sem BOM), quebras de
linha e ausência de quebra final são preservados. Isso foi verificado lendo e
regravando em memória os 201 mapas, grades e modelos, o `PLAYLIST.ini` e todos
os XML de uma instalação real: todos voltaram idênticos byte a byte.

## Requisitos

- Windows 10 ou 11 (x64).
- Para compilar: Visual Studio 2022 (ou Build Tools) com a carga de trabalho
  C++ — ela já inclui CMake e Ninja; Git.

## Compilação

```powershell
git clone --recursive https://github.com/caio-kenai/PlaylistControl.git
cd PlaylistControl

# Release com Ninja + testes
powershell -ExecutionPolicy Bypass -File scripts\build.ps1

# Debug
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Debug

# Solução do Visual Studio (build-vs\PlaylistControl.sln)
powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -VisualStudio
```

O CMake é a fonte da configuração; a solução do Visual Studio é gerada por
ele. Sem o script:

```powershell
cmake -G "Visual Studio 17 2022" -A x64 -S . -B build-vs
cmake --build build-vs --config Release
```

Os executáveis ficam em `<pasta de build>\bin`: `PlaylistControl.exe` e
`playlistcontrol_tests.exe`. O JUCE 9.0.2 vem como submódulo em
`external/JUCE`; o script o baixa se estiver ausente.

## Uso

1. Abra o `PlaylistControl.exe`. Ele procura a instalação pelo registro do
   Playlist Digital, pela configuração do Commercial e do Sync Service e por
   `X:\Playlist\pgm` em cada unidade. A pasta só é aceita se tiver o executável
   do Playlist e seus arquivos de configuração.
2. Confirme a instalação em **Instalação** (ou escolha outra pasta).
3. O programa começa em **somente leitura**. Para editar, use **Permitir
   alterações** no topo.
4. Atalhos: `Ctrl+S` salva, `F5` recarrega, `Ctrl+1` a `Ctrl+9` trocam de tela.

Onde o PlaylistControl guarda seus dados:

| O quê | Onde |
|---|---|
| Configurações | `%APPDATA%\PlaylistControl\PlaylistControl.settings` |
| Histórico (cópias de segurança) | `%LOCALAPPDATA%\PlaylistControl\history` |
| Log (JSON por linha, um arquivo por dia) | `%LOCALAPPDATA%\PlaylistControl\logs` |

O log não contém conteúdo de arquivos nem valores de senha.

## Arquivos do Playlist que o programa entende

| Arquivo | Leitura | Gravação |
|---|---|---|
| `Mapas\*.txt`, `Grades\*.txt`, relógios, modelos | sim | sim (o Playlist relê sozinho) |
| `PLAYLIST.ini` | sim | sim |
| `CONFIG.XML` | sim | sim, com o Playlist fechado |
| `Folders.xml`, `Atalhos\*.lnk` (Config Manager) | sim | não |
| `Dados\LIGACAO.DBF` (registros) | sim | não |
| `Operadores\*\Config.xml` | sim | não |
| `Indices\*.NTX` | cabeçalho | não |
| `Montagem\*.merge`, montagens | sim | não |

O levantamento completo de formatos, origens e comportamento está em
[ARCHITECTURE.md](ARCHITECTURE.md); o plano e as pendências, em
[IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md).

## Estrutura

```
src/
  core/         diagnósticos, codificação de texto sem perda, horários, diff
  storage/      leitura compartilhada, gravação segura, histórico
  formats/      INI, TXT1 (mapas/grades/relógios), XML por patch, DBF, NTX, montagem
  ecosystem/    Commercial, Sync Service, Playlist Server; origem dos arquivos
  install/      detecção e validação da instalação
  catalog/      pastas + registros: resolução de códigos
  validation/   verificações com causa e correção
  services/     workspace, sessões de arquivo, observador de pasta
  ui/           interface JUCE
tests/          testes unitários e instalação fictícia
tools/          geração das fixtures e do ícone
```

## Testes

```powershell
build-release\bin\playlistcontrol_tests.exe
```

Os testes cobrem leitura e regravação byte a byte de cada formato, edição,
validação, gravação segura (conflito, arquivo em uso, falha de verificação,
somente leitura), histórico, catálogo e detecção de origem, usando a
instalação fictícia gerada por `tools/make_fixtures.py`.

Verificação somente leitura contra uma instalação real (nada é gravado):

```powershell
build-release\bin\playlistcontrol_tests.exe --category=installation --installation=C:\Playlist\pgm
```

## Solução de problemas

- **"Pasta não reconhecida"**: a pasta escolhida precisa ser a `pgm`, com o
  `Playlist.exe` e ao menos um entre `PLAYLIST.ini`, `CONFIG.XML` e
  `Folders.xml`.
- **"O arquivo está em uso por outro programa"**: outro programa segura o
  arquivo sem permitir a substituição; tente de novo em alguns segundos. Nada
  foi alterado.
- **"O arquivo foi alterado por outro programa"**: recarregue e refaça a
  alteração; compare no Histórico se precisar.
- **Opções do Playlist bloqueadas**: feche o Playlist Digital.
- **Muitos "arquivo não encontrado"**: confira no Config Manager se as pastas
  apontam para os diretórios certos; em **Pastas e códigos** aparece onde cada
  arquivo foi encontrado.

## Limitações conhecidas

- Pastas, códigos registrados e operadores são somente consulta; continuam
  sendo alterados no Config Manager, no Registrar e no Playlist.
- Pontos do formato ainda não confirmados (por exemplo, `DUR=300` em segundos
  e `%w` com domingo 0 ou 7) estão listados em
  [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md) e são tratados de forma
  conservadora: o texto original é sempre preservado.

## Licença

[GNU AGPL-3.0](LICENSE).
