# PlaylistControl — Arquitetura

Este documento registra o que foi levantado sobre o ecossistema Playlist e a
arquitetura que resulta disso. Tudo o que está aqui foi confirmado em pelo menos
uma destas fontes, indicadas entre colchetes:

- **[M-PD]** Manual do Playlist Digital 5 (9ª edição, versão 5.0.9.01+)
- **[M-MK]** Manual do Maker 1.3.0.1
- **[M-CM]** Manual do Commercial Playlist
- **[M-PL]** Manual do Planner (web) e do Sync Service
- **[ARQ]** Inspeção dos arquivos reais de uma instalação (`C:\Playlist\pgm`)
- **[LOG]** Log de eventos do próprio Playlist (`pgm\Eventos\AAAA-MM-DD.log`)

Itens marcados como **PENDENTE** ainda não foram confirmados e não devem ser
tratados como verdade pelo código.

---

## 1. Princípio central

O operador não precisa abrir a pasta `pgm`, nem editar arquivo à mão, nem saber
qual programa gera cada arquivo. O PlaylistControl é a interface: ele encontra a
instalação, lê os arquivos, mostra a informação de forma estruturada, valida,
grava de forma segura e registra o que fez.

Consequências para a arquitetura:

1. Toda escrita passa por um único caminho (validação → backup → escrita
   atômica → reverificação → registro).
2. O código conhece a **origem** de cada arquivo (Planner, Commercial, Maker,
   Playlist, manual) e avisa quando uma alteração será sobrescrita.
3. Formatos são lidos e gravados **sem perda**: o que não é entendido é
   preservado byte a byte.
4. Nada é alterado apenas porque uma pasta chamada `Playlist` foi encontrada.

---

## 2. O ecossistema

```
            Planner (web) ──► Sync Service (.SyncService, localhost:5000)
                                   │ grava mapas dd-mm-aaaa.txt de hoje + 7 dias
                                   │ a cada 5 min e baixa as mídias
                                   ▼
 Commercial.exe ──exporta──► pgm\Mapas\*.txt ◄── edição manual (bloco de notas)
                                   │
 Maker ──► Playlist Server (serviço, porta 3033, SQL CE)
                 │ grava grades dd-mm-aaaa.txt
                 ▼
            pgm\Grades\*.txt ◄── edição manual
                                   │
 Config Manager ──► Folders.xml + Atalhos\*.lnk + Dados\LIGACAO.DBF
                                   │
 Ligacao.exe ("Registrar") ──► Dados\LIGACAO.DBF
                                   │
 Horário Eleitoral ──► Montagem\*.merge
                                   ▼
                          Playlist Digital (Playlist.exe)
            lê PLAYLIST.ini, CONFIG.XML, Operadores\*, Mapas, Grades, Dados
            grava Montagem, Comprove, Dados\COMPROVE.DBF, Indices\*.NTX, Eventos
```

### 2.1 Programas encontrados na máquina

| Programa | Versão | Local | Papel |
|---|---|---|---|
| Playlist Digital | 5.0.9.2 (exe 5.0.9.09/5.0.10.01) | `C:\Playlist\pgm` | Playout |
| Config Manager | 1.0.0.7 | `pgm\ConfigManager.exe` | Pastas/atalhos/comandos |
| Ligacao.exe | — | `pgm\Ligacao.exe` | Registro de códigos |
| SeparaComprove | — | `pgm\SeparaComprove.exe` | Divide/repara `COMPROVE.DBF` |
| Commercial | 2.96.0 | `C:\Commercial` | OPEC local, exporta mapas |
| Maker | 1.3.0.1 | `C:\Maker` | Programação musical |
| Playlist Server | 1.3.0.1 | serviço `PlaylistServer` | Banco do Maker, gera grades |
| Sync Service | 1.0.2.0 / 1.0.6 | `C:\Sync Service` | Entrega do Planner |
| Logger / Logger Manager | 2.0.x | — | Gravação e relatórios |

O Planner é uma aplicação web; na máquina existe apenas o Sync Service. Não há
um programa separado chamado "Mapas", "Grades" ou "Relógios": são arquivos de
texto lidos pelo Playlist Digital.

---

## 3. A pasta `pgm`

| Item | Formato | Quem escreve | Quem lê | Observação |
|---|---|---|---|---|
| `PLAYLIST.ini` | INI, CRLF, ASCII/CP1252 | manual | Playlist | Define como mapas, grades e relógios são localizados [M-PD] |
| `CONFIG.XML` | XML UTF-8, tabs, CRLF | Playlist (Ferramentas>Opções) | Playlist | Configurações gerais; reescrito pelo Playlist (ex.: `LastAuthStation_*` no startup) [ARQ] |
| `Folders.xml` | XML UTF-8 com BOM | Config Manager | Playlist, Maker | Pastas de trabalho; cópias rotativas em `Folders\Folders.xml.N` e `Folders.zip` [ARQ] |
| `Atalhos\*.lnk` | Shell link | Config Manager | Playlist | Um por pasta [ARQ][LOG] |
| `Mapas\*.txt` | TXT1 | Commercial, Sync Service, manual | Playlist | Blocos comerciais |
| `Grades\*.txt` | TXT1 | Playlist Server (Maker), manual | Playlist | Blocos musicais |
| `Mapas\Relogio.txt`, `Grades\Relogio.txt` | TXT1 | manual | Playlist (se configurado) | Relógio operacional |
| `Modelos\Modelo.txt` | TXT1 | manual | Maker (importação) | Modelo de programação |
| `Dados\LIGACAO.DBF` | dBase III (0x03), CP1252 | Ligacao.exe, Config Manager, Commercial | Playlist | Registro código→arquivo |
| `Dados\COMPROVE*.DBF` | dBase III | Playlist, SeparaComprove | Commercial, relatórios | Comprovação |
| `Dados\DATAS.DBF/.dbt` | dBase III + memo (0x83) | instalador | Painel Hoje | Datas comemorativas |
| `Indices\*.NTX` | Clipper NTX | Playlist | Playlist | Índices dos DBF |
| `Operadores\<nome>\Config.xml` | XML | Playlist | Playlist | Permissões por operador |
| `Operadores\<nome>\Layout*.xml` | XML | Playlist | Playlist | Painéis e cores |
| `Montagem\dd-mm-aaaa.TXT/.zip/.xml` | texto/zip | Playlist | Playlist | Montagem salva dos blocos |
| `Montagem\*.merge` | texto | Horário Eleitoral | Playlist | Inserções a mesclar |
| `Comprove\*.csv`, `OM*.txt` | CSV `;` / texto | Playlist | relatórios | Comprovação diária |
| `Eventos\*.log` | TSV CP1252 | Playlist, Config Manager | suporte | Log operacional |
| `PLAYLIST.RCV` | binário (UTF-16 com prefixos) | Playlist | Playlist | Recuperação; não tocar |
| `Textos\dd-mm-aaaa.rtf/txt` | RTF/TXT | Playlist/manual | Painel Texto do dia | [M-PD] |
| `Minisite\` | HTML | manual | Painel Mini Site | [M-PD] |
| `REMOTE.INI` (opcional) | INI | manual | Playlist (afiliadas) | [M-PD] |

Arquivos com senha em texto claro existem (`CONFIG.XML`: senhas de VLC,
câmera, FTP; `Operadores\*\Config.xml`). O PlaylistControl nunca os copia para
logs, e as fixtures de teste do repositório são sintéticas.

### 3.1 O Playlist monitora a pasta

O log mostra que o Playlist observa `MAPAS`, `GRADES`, `DADOS` e `MONTAGEM` e
reprocessa a programação ao detectar mudança [LOG]:

```
Monitorando a pasta C:\Playlist\pgm\MAPAS
Arquivo C:\Playlist\pgm\GRADES\27-09-2026.TXT modificado.
Atualizando programação devido alteração no arquivo C:\Playlist\pgm\DADOS\LIGACAO.DBF
Processamento de inserções: Lendo os mapas
```

Portanto:

- alterações em mapas, grades e `LIGACAO.DBF` valem **sem reiniciar**;
- uma escrita parcial é lida imediatamente → a escrita atômica é obrigatória;
- `CONFIG.XML` não aparece monitorado e é regravado pelo Playlist, então
  alterações nele exigem o Playlist fechado (ver §6.4).

---

## 4. Formatos

### 4.1 PLAYLIST.ini

INI simples, seções entre colchetes, `CHAVE=VALOR`, comentários com `;`
(inclusive seções inteiras comentadas, como `;[RDS]`). CRLF. Nem sempre termina
com quebra de linha [ARQ].

Seções conhecidas:

| Seção | Chaves | Fonte |
|---|---|---|
| `[BLOCO COMERCIAL]` | `FORMATO=AUTO\|TXT1`, `ARQUIVO=<padrão>` | [M-PD][M-CM][M-PL] |
| `[BLOCO MUSICAL]` | idem | [M-PD] |
| `[RELOGIO MUSICAL]`, `[RELOGIO COMERCIAL]` | `FORMATO=TXT1`, `ARQUIVO=` | [M-PD] |
| `[BEEP]` | `ARQUIVO=`, `HORARIO=0,15,30,45` | [M-PD] |
| `[AFILIADAS]` | `<ID>=<host>:<porta>` | [M-PD] |
| `[RDS]` | `ARQUIVO=`, `MENSAGEM=` (visto comentado) | [ARQ] — semântica **PENDENTE** |

Variáveis em `ARQUIVO`: `%d` dia, `%m` mês, `%Y` ano (4), `%y` ano (2), `%a`
dia da semana abreviado (`Seg Ter Qua Qui Sex Sáb Dom`), `%w` número do dia
[M-PD]. O manual se contradiz sobre `%w` (texto diz Dom=0; a tabela lista
1..7 com Domingo=7) — **PENDENTE**; o código mostra os dois candidatos.

Busca no modo `AUTO` [M-PD]:

- comercial: `Mapas\Mapadd-mm-aaaa`, `Mapas\Mapadd`, `Mapas\<Seg..Dom>`,
  `Mapas\<n>` e por fim `Mapas\Mapa.txt`;
- musical: `Grades\dd-mm-aaaa`, `Grades\Gradedd`, `Grades\<Seg..Dom>`,
  `Grades\Grade.txt` e por fim `Mapas\Grade.txt`.

Instalação real: `[BLOCO COMERCIAL] FORMATO=TXT1 ARQUIVO=MAPAS\%d-%m-%Y.TXT`
(padrão exigido pelo Sync Service e pelo Commercial com data completa) e
`[BLOCO MUSICAL] FORMATO=AUTO` [ARQ][M-PL][M-CM].

### 4.2 Formato TXT1 (mapas, grades, relógios, modelos)

Uma linha por bloco, em ordem crescente de horário [M-PD]:

```
HH:MM[ (PARÂMETROS)] [ITEM[, ITEM]...][, ]
```

- **Horário**: `HH:MM`. O manual mostra `6:15` sem zero à esquerda em um exemplo;
  aceitação pelo Playlist **PENDENTE** → o validador emite aviso, não erro.
- **Parâmetros** entre parênteses, separados por vírgula [M-PD][ARQ]:
  `ID=<nome>`, `FIXO`, `LOCAL`, `SAT`, `DUR=<duração>`, `LOCKED`, `DESCARTE`.
  `DUR` aparece como `DUR=3:00` / `DUR=13:00` (min:seg) [M-PD][ARQ] e como
  `DUR=300` nos mapas do Planner [ARQ]. O Planner usa blocos de 5 min, o que
  indica segundos — **PENDENTE**. O texto original é sempre preservado.
- **Itens** separados por vírgula (espaço opcional) [M-PD]:
  - código registrado ou código de pasta: `VH`, `55`, `MUS1`, `SERTA90`;
  - código com sufixo de refrão: `MUS1-R` [M-PD];
  - código com letra de rodízio do Commercial: `10-A` [M-CM] (forma exata
    **PENDENTE**);
  - arquivo entre aspas: `"Charlie Brown Jr. - Te Levar.MP3"` [ARQ] (Maker);
  - código + arquivo entre aspas: `"H6D26P73FUHF|rec_1767811356019.aac"` [ARQ]
    (Planner/Sync Service);
  - comando entre `<` `>`: `<IALOC>`, `<IANEWS>` [ARQ] — semântica **PENDENTE**;
  - nome sem aspas com espaços (`Charlie Brown Jr. - Céu Azul.mp3`) aparece
    em arquivo editado à mão [ARQ]; aceitação **PENDENTE**;
  - item vazio (`, ,`) aparece em arquivos reais [ARQ].
- Linhas geradas terminam com `", "` (Planner, Maker) ou sem vírgula
  (manual). Bloco vazio = só o horário (com ou sem espaço).
- Códigos: até 12 caracteres; zeros à esquerda ignorados; minúsculas viram
  maiúsculas [M-PD].
- Encoding: ASCII, CP1252 (grades do Maker com acentos) e UTF-8 (arquivos
  editados à mão, `Mapa Exemplo.txt`) coexistem [ARQ]. Qual o Playlist espera
  para caracteres não-ASCII: **PENDENTE**. O arquivo é regravado no mesmo
  encoding em que foi lido.

### 4.3 CONFIG.XML

Elemento raiz `<Config>`, um elemento por configuração, tabs, CRLF. Valor vazio
é gravado como quebra de linha + tabs (`<sFtpServer>\n\t</sFtpServer>`) [ARQ].
Algumas chaves têm sufixo do nome da máquina (`Saidas_DEV07`,
`LastAuthStation_DEV07`). O mapeamento chave → opção da tela
Ferramentas>Opções foi feito pelo manual [M-PD] e está em
`src/formats/configxml/ConfigSchema.cpp`. Booleanos: `0`/`1`. Há campos com
valores fora do padrão (`bCameraController_ShowTitle=257`,
`bCameraController_EnableVideoMix=-2013265919`) — preservados como estão.

Estratégia de escrita: **patch textual** no conteúdo do elemento, sem
reserializar o documento, para manter indentação, ordem e o formato de vazios.

### 4.4 Folders.xml (Config Manager)

`<Folders><Folders>N</Folders><Version>1.2</Version><Shared>...</Shared>
<Folder0>...</FolderN>`. Cada pasta: `ID`, `Title`, `Type`, `Target`,
`IconLocation`, `ShortcutArguments`, `ShortcutPathName`, `IconIndex`,
`Output`, `TotalFiles`, `DBFId` [ARQ].

`Type` / primeiro token de `ShortcutArguments` [ARQ][M-PD]:

| Tipo | Significado |
|---|---|
| `M` | Músicas |
| `$` | Comerciais |
| `V` | Vinhetas |
| `H` | Hora certa |
| `W` | Temperatura |
| `X` | Textos |
| `T` | Trilhas |
| `L` | Locuções |
| `P` | Pausa |
| `C` | Comando (`C UDP {PLAY}`, `C URL <url>`, `C COM4: P`, `C LPT1 n`, `C CameraOn`, `C Scene:...`) |
| vazio | Genérica (Institucional) |

`DBFId` é o código registrado da pasta; o mesmo código aparece em
`LIGACAO.DBF` com `TIPO=A` e `ARQUIVO=<Title>.lnk` [ARQ]. O log mostra que o
Config Manager atualiza os três lugares numa mesma operação [LOG].

### 4.5 LIGACAO.DBF

dBase III sem memo. Campos [ARQ]: `CODIGO C12, CTA C10, SEQMEDIA C1,
PROXIMO N2, ARQUIVO C250, SHORTNAME C12, DURACAO N8.2, TIPO C1, DATAREG D8,
HORAREG C5, DATAINI D8, HORAINI C5, DATAFIM D8, HORAFIM C5, TEXTO C150,
FLAGS C1`. `TIPO`: `A` atalho, `C` comercial (outros valores para música e
vinheta **PENDENTE**). `DATAINI..DATAFIM` = validade do registro [M-PD].

### 4.6 Índices NTX

Quatro índices Clipper NTX padrão [ARQ]:

| Arquivo | Expressão |
|---|---|
| `LIGA_COD.NTX` | `CODIGO` |
| `LIGA_ARQ.NTX` | `UPPER(ARQUIVO)` |
| `COMPROVE-C.NTX` | `CODIGO+DTOS(DATA)+BLOCO` |
| `COMPROVE-A.NTX` | `UPPER(ARQUIVO)+DESCEND(DTOS(DATA))+DESCEND(HORAFIM)` |

Observado: os `LIGA_*.NTX` foram recriados no startup do Playlist mesmo sem
alteração no DBF [ARQ][LOG]. Ver §9.

### 4.7 Montagem e merge

`Montagem\dd-mm-aaaa.TXT`: `HH:MM T, pos, "Pasta", "Arquivo"` com `T` = `M`
ou `C` [ARQ]. `*.merge` (Horário Eleitoral) usa o mesmo formato. O log registra
`Merge ... linha 1 inválida` para arquivos que referenciam a pasta `Eleicoes`
depois que ela foi renomeada para `Eleições` no Config Manager [LOG]. Hipótese:
o título da pasta precisa existir no `Folders.xml` — **PENDENTE** de teste
controlado, mas já é um diagnóstico útil.

---

## 5. Origem e ciclo de vida

`ecosystem::OriginDetector` classifica cada arquivo com um nível de confiança:

| Sinal | Origem provável |
|---|---|
| Mapa `dd-mm-aaaa.txt` com itens `"CÓDIGO\|arquivo"` e `(DUR=n)` | Planner / Sync Service |
| Mapa `dd-mm-aaaa.txt` só com códigos e `Emissora.xml` do Commercial apontando para esta pasta | Commercial |
| Grade `dd-mm-aaaa.txt` só com arquivos entre aspas, terminada em `", "` | Maker / Playlist Server |
| `Mapa.txt`, `Grade.txt`, `Relogio*.txt`, dia da semana | Manual |
| `*.merge` | Horário Eleitoral |

`ecosystem::LifecyclePolicy` transforma isso em regras de interface:

- **Planner**: o Sync Service regrava mapas de hoje + N dias (padrão 7) a cada
  5 min [M-PL]. Editar esses mapas localmente é permitido, mas o aviso é
  explícito: a alteração será perdida na próxima sincronização; o caminho
  correto é o Planner.
- **Commercial**: nova exportação sobrescreve o dia exportado [M-CM].
- **Maker**: o Playlist Server regrava a grade quando a programação do dia é
  alterada no Maker [M-MK].
- **Manual**: livre.

Se o arquivo mudou no disco depois de aberto no PlaylistControl, a gravação é
recusada e o conflito é mostrado (ver §6.3).

---

## 6. Segurança de escrita

### 6.1 Pipeline

```
editar modelo → serializar → validar (bloqueia se houver erro)
 → comparar com o snapshot de abertura (conflito?)
 → backup do original → gravar temporário no mesmo diretório
 → FlushFileBuffers → reler e reparsear o temporário
 → ReplaceFileW (fallback MoveFileExW REPLACE_EXISTING|WRITE_THROUGH)
 → novo snapshot → registro no histórico
```

A substituição ocorre no mesmo volume, então o Playlist vê o arquivo antigo ou o
novo, nunca um meio-termo.

### 6.2 Backup e histórico

`%LOCALAPPDATA%\PlaylistControl\history\` guarda uma cópia do estado anterior e
um manifesto JSON por operação: arquivo, horário, operação, operador do
Windows, hash SHA-256 antes/depois, resumo. A tela de Histórico mostra a
diferença e permite restaurar; a restauração passa pelo mesmo pipeline (e
também gera histórico). Retenção configurável.

### 6.3 Alterações externas

`storage::DirectoryWatcher` (ReadDirectoryChangesW — JUCE não tem observador de
diretório no Windows) observa `pgm`, `Mapas`, `Grades`, `Dados`, `Montagem`,
`Operadores`. Cada evento vira uma entrada no painel de atividades. Documentos
abertos comparam tamanho + data + hash com o snapshot; se mudaram, a tela avisa
e oferece recarregar ou comparar. A gravação nunca sobrescreve silenciosamente.

### 6.4 Playlist em execução

`install::ProcessMonitor` detecta `Playlist*.exe`, `ConfigManager.exe`,
`Ligacao.exe` e o estado dos serviços `PlaylistServer` e `.SyncService`.

| Arquivo | Playlist aberto |
|---|---|
| Mapas, grades, relógios | permitido (o Playlist relê) |
| `PLAYLIST.ini` | permitido; efeito após reiniciar o Playlist — **PENDENTE** se ele relê |
| `CONFIG.XML`, `Operadores\*` | bloqueado; o Playlist regrava esses arquivos |
| `Folders.xml`, `.lnk`, `LIGACAO.DBF` | somente leitura nesta versão |
| `Indices\*.NTX` | somente leitura |

### 6.5 Modo somente leitura

Liga por padrão quando a instalação ainda não foi confirmada pelo operador, e
pode ser ligado a qualquer momento. Faixa visível no topo da janela; todos os
comandos de gravação ficam desabilitados.

---

## 7. Módulos

```
src/
  core/          Result/Diagnostic, TimeOfDay, TextCodec (encoding + quebra de linha)
  storage/       FileSnapshot, SafeWriter, HistoryStore, DirectoryWatcher
  install/       InstallationLocator, InstallationValidator, ProcessMonitor
  formats/
    ini/         IniDocument (sem perda)
    playlistini/ PlaylistIni (esquema), FilePatternResolver
    schedule/    ScheduleDocument (TXT1 sem perda), BlockParams, ScheduleItem
    xmlpatch/    XmlPatchDocument (leitura via juce::XmlDocument, escrita por patch)
    configxml/   ConfigSchema, ConfigXml
    folders/     FoldersXml, FolderKind
    dbf/         DbfTable (leitura)
    ntx/         NtxHeader (leitura)
    montagem/    MontagemFile (leitura, também .merge)
  ecosystem/     OriginDetector, LifecyclePolicy, EcosystemScan (Commercial, Sync, Maker)
  catalog/       CodeCatalog (pastas + LIGACAO), resolução de itens
  validation/    ScheduleValidator, PlaylistIniValidator, ConfigValidator,
                 FoldersValidator, CrossValidator
  services/      Workspace (instalação carregada), DocumentSession, ActivityFeed
  logging/       Logger (JSON Lines)
  ui/            Theme/LookAndFeel, MainWindow, Sidebar, views/*, components/*
  app/           Main (JUCEApplication)
tests/           testes unitários (juce::UnitTest) e fixtures sintéticas
```

Dependências: `ui → services → (validation, catalog, ecosystem) → formats →
storage → core`. `formats` não conhece a interface; `ui` não lê arquivos.

---

## 8. Interface

JUCE puro. Identidade visual baseada no tema "Standard" do próprio Playlist
(`Operadores\Padrão\layout\Standard.xml`, cores COLORREF convertidas) [ARQ]:

| Uso | Cor |
|---|---|
| Barra lateral de bloco comercial | `#1A6B23`, texto `#D8F5DB` |
| Linhas alternadas comerciais | `#F6FFF6`, borda `#00F000` |
| Barra lateral de bloco musical | `#003C80`, texto `#E4F1FF` |
| Linhas alternadas musicais | `#EBF3FC` |
| Próxima inserção | `#F4F389` |
| Display "No ar" | fundo `#000000`, texto `#E40000` |
| Seleção | `#5CB142` (texto `#CCFFCC`) |
| Inserção com erro (Playlist) | `#C0C0C0` |

Navegação lateral: Painel, Mapas, Grades, Relógios, Playlist.ini,
Configurações (CONFIG.XML), Pastas e códigos, Diagnóstico, Histórico. Blocos
são desenhados como no Playlist: faixa vertical colorida à esquerda com o tipo
(Comercial/Musical), cabeçalho com data, horário, duração e parâmetros (F, SAT,
cadeado, DUR), e as inserções em linhas alternadas.

---

## 9. Recriação de índices (tese)

Objetivo solicitado: permitir que o PlaylistControl recrie os índices do
Playlist. Fica para o fim; esta seção reúne o que já se sabe.

A favor:

- Os índices são Clipper NTX, formato público e estável (cabeçalho de 1024
  bytes, páginas de 1024 bytes, chave com tamanho fixo) [ARQ].
- As expressões de chave estão gravadas no próprio cabeçalho e são simples
  (`CODIGO`, `UPPER(ARQUIVO)`, `DTOS`, `DESCEND`) [ARQ].
- O próprio Playlist recria `LIGA_*.NTX` ao iniciar [ARQ][LOG]; o caminho mais
  seguro pode ser simplesmente remover (com backup) os índices com o Playlist
  fechado e deixá-lo recriar — **PENDENTE** de teste em cópia.

O que falta:

- Saber o que o `SeparaComprove.exe` faz exatamente além do descrito no manual
  (divisão mensal de `COMPROVE.DBF` e descarte de registros corrompidos) [M-PD].
- Confirmar se o Playlist recria `COMPROVE-*.NTX` quando ausentes.
- Confirmar o comportamento de `DESCEND()` para campos caractere no NTX
  gravado pelo Playlist (complemento de bytes, como no Clipper).

Plano: implementar primeiro o leitor e verificador de NTX (compara o índice com
o DBF e aponta divergências), testar recriação somente em cópia da pasta com o
Playlist fechado, e só então decidir entre "remover e deixar o Playlist
recriar" e "gerar os NTX".
