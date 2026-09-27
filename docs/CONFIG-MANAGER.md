# Config Manager — como o Playlist Control configura as pastas

A tela **Configuração > Config Manager** faz o mesmo que o `ConfigManager.exe`
(versão 1.0.0.7): cria, altera e remove as pastas do Playlist Digital, com o
mesmo layout — barra **Salvar / Adicionar / Excluir**, pastas em ícones
agrupadas em *Gerais*, *Comandos*, *Vinhetas* e *Músicas*, e o quadro
**Propriedades da pasta selecionada** (Título, Tipo, Diretório, Registrar,
ícone e, para comandos, Linha de comando).

Tudo o que está descrito aqui foi observado executando uma **cópia** do Config
Manager numa pasta separada, com o `Folders.xml` apontando para a cópia, e
comparando os arquivos antes e depois de cada operação. A instalação real foi
conferida por hash antes e depois: nenhum arquivo mudou.

## Onde fica a configuração

Ao salvar, o Config Manager grava, numa mesma operação:

| Arquivo | O que muda |
|---|---|
| `Folders.xml` | as pastas (`<FolderN>`), o total (`<Folders>N</Folders>`) |
| `Atalhos\<Título>.lnk` | um atalho do Windows por pasta: destino = diretório (ou a pasta `pgm` para Pausa e Comando), argumentos = letra do tipo ou a linha de comando, ícone = arquivo e índice, descrição = título |
| `Dados\LIGACAO.DBF` | um registro por pasta com código: `TIPO=A`, `CODIGO`, `ARQUIVO=<Título>.lnk`, `DATAREG`, `HORAREG` |
| `Indices\LIGA_COD.NTX`, `LIGA_ARQ.NTX` | índices do `LIGACAO.DBF` |

No fim ele mostra *"Os atalhos foram salvos com sucesso, reinicie o Playlist
Digital!"* — as pastas só passam a valer quando o Playlist é aberto de novo.

Detalhes observados:

- **Pasta nova**: acrescentada no fim do `Folders.xml` com o próximo `ID`;
  o resto do arquivo fica byte a byte igual. No DBF, um registro novo no fim;
  a data do cabeçalho não é alterada.
- **Renomear ou trocar o código**: o mesmo registro do DBF é atualizado
  (código, arquivo, data e hora); o atalho antigo é apagado e o novo criado.
- **Excluir**: a pasta sai do `Folders.xml` e o atalho é apagado, mas o
  registro do código **fica** no `LIGACAO.DBF`. Se uma pasta com o mesmo título
  for criada depois, o registro é reaproveitado.
- **Atalhos**: o Config Manager regrava todos a cada salvamento; o conteúdo
  (destino, argumentos, ícone, descrição) é o mesmo.
- **Índices**: os que ele grava conferem com a tabela, mas a disposição das
  páginas não é a mesma da carga ordenada — as duas são árvores válidas, e o
  Playlist regrava os `LIGA_*` ao abrir.

## Tipos

| Tipo (lista do Config Manager) | Letra | Grupo | Ícone padrão (`Icones5.dll`) | Diretório |
|---|---|---|---|---|
| Músicas | `M` | Músicas | 331 | sim |
| Comerciais | `$` | Gerais | 303 | sim |
| Vinhetas | `V` | Vinhetas | 356 | sim |
| Locuções | `L` | Gerais | 4 | sim |
| Textos | `X` | Gerais | 6 | sim |
| Trilhas | `T` | Gerais | 359 | sim |
| Hora Certa | `H` | Comandos | 86 | sim |
| Pausa | `P` | Comandos | 393 | não (pasta `pgm`) |
| Comando | `C` | Comandos | 6 | não (pasta `pgm`) |
| Aleatórias | `A` | Comandos | 6 | sim |
| Sequenciais | `S` | Comandos | 6 | sim |
| Temperatura | `W` | Comandos | 215 | sim |
| Outras | (vazio) | Gerais | 6 | sim |

- **Nova pasta**: o título é o nome do diretório escolhido (ou "Pausa" /
  "Comando"); o código são as três primeiras letras do título em maiúsculas e,
  se já existir, mais três caracteres aleatórios (ex.: `DESMVY`). Pausa e
  Comando começam sem código.
- Para Pausa e Comando o tipo e o diretório ficam desabilitados.
- Para Temperatura o Config Manager grava `TEMP` nos argumentos de uma pasta
  nova, enquanto a pasta existente da instalação usa `W`. O Playlist Control
  grava `W`, como a pasta que funciona.
- O Config Manager não mostra ícones de arquivos `.ico` (o log registra
  *"Erro ao carregar ícone (Commercial): Intervalo solicitado ultrapassa o fim
  da matriz"*); o Playlist Control mostra e avisa.

## Como o Playlist Control grava

1. **Validação**: título obrigatório, sem `\ / : * ? " < > |` e único; código
   com até 12 caracteres, sem espaços, vírgulas, parênteses, aspas ou `| < >`,
   único entre as pastas e não registrado para outro arquivo (ex.: um
   comercial); diretório existente; linha de comando para Comando; caracteres
   representáveis em Windows-1252 (o DBF).
2. **Programas abertos**: Config Manager, Ligacao e SeparaComprove bloqueiam a
   operação. Se o Playlist Digital estiver aberto, ele é fechado pela própria
   janela — a pergunta *"Deseja fechar o programa?"* é respondida com **Sim**
   pelo botão dela, porque o operador já confirmou no Playlist Control.
3. **Cópia** de `Folders.xml`, `LIGACAO.DBF`, `LIGA_*.NTX` e de todos os
   atalhos em `%LOCALAPPDATA%\PlaylistControl\pastas\<data_hora>\`.
4. **Gravação**: `Folders.xml`, `LIGACAO.DBF` e os índices pelo caminho seguro
   (conferência, temporário, substituição atômica, Histórico); depois os
   atalhos. Se algo falhar, todos os arquivos da cópia são devolvidos.
5. **Reabertura** do Playlist, se ele estava aberto.

O `Folders.xml` é regravado preservando o texto: pastas não alteradas ficam
iguais, valores alterados são trocados no lugar, pastas novas vão para o fim e
removidas saem com a renumeração dos elementos. Com a instalação real, regravar
sem alterações produz o mesmo arquivo byte a byte
(`playlistcontrol_tests --category=installation`).

## Testes

- `--category=folders`: formato do `Folders.xml`, edição do DBF, sugestão de
  código, plano de alterações (registro, índices, atalhos), conflitos,
  atalhos gravados e relidos, gravação completa com cópia.
- `--category=rebuild --rebuild-demo=<pgm>`: numa instalação de demonstração,
  salva uma pasta nova com um programa de teste no lugar do Playlist que
  pergunta *"Deseja fechar o programa?"* ao fechar; a pergunta é respondida, as
  pastas gravadas e o programa aberto de novo.

## Pendências

- A pergunta de fechamento do Playlist real não foi vista na tela: se ela for
  um diálogo de tarefa (sem botões legíveis), o Playlist Control responde com o
  botão *Sim* pelo identificador padrão. Outras perguntas ficam para o
  operador; sem resposta em 90 s, são canceladas e o Playlist continua aberto.
- O menu **Dicas** do Config Manager não foi reproduzido.
