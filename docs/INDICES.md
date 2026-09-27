# Recriação dos índices do Playlist Digital — análise

Pergunta: o PlaylistControl consegue recriar os índices do Playlist Digital
(`pgm\Indices\*.NTX`) com segurança?

**Conclusão: sim, é viável.** Os índices foram decodificados e reconstruídos a
partir das tabelas; para os índices do registro de códigos, o resultado é
idêntico byte a byte ao arquivo que o próprio Playlist grava. A operação ainda
**não** está disponível na interface: faltam as decisões descritas no fim.

## Os índices

| Arquivo | Tabela | Expressão da chave | Tamanho da chave |
|---|---|---|---|
| `LIGA_COD.NTX` | `Dados\LIGACAO.DBF` | `CODIGO` | 12 |
| `LIGA_ARQ.NTX` | `Dados\LIGACAO.DBF` | `UPPER(ARQUIVO)` | 250 |
| `COMPROVE-C.NTX` | `Dados\COMPROVE.DBF` | `CODIGO+DTOS(DATA)+BLOCO` | 25 |
| `COMPROVE-A.NTX` | `Dados\COMPROVE.DBF` | `UPPER(ARQUIVO)+DESCEND(DTOS(DATA))+DESCEND(HORAFIM)` | 116 |

Formato Clipper NTX:

- cabeçalho de 1024 bytes: assinatura (6), versão, página raiz, próxima página
  livre, tamanho do item (chave + 8), tamanho da chave, casas decimais, máximo
  de itens por página, meia página, expressão da chave (texto) e indicador de
  chave única;
- páginas de 1024 bytes: quantidade de itens, tabela com `máximo + 1`
  deslocamentos e os itens — página filha (4 bytes), número do registro no DBF
  (4 bytes, a partir de 1) e a chave;
- chaves comparadas byte a byte; `UPPER()` usa as maiúsculas do Windows-1252
  (`ç` → `Ç`, `õ` → `Õ`); `DTOS()` é a data `AAAAMMDD` como gravada no DBF;
  `DESCEND()` troca cada byte por `256 - byte`.

## Experimento

Feito somente com leitura: os arquivos da instalação foram lidos com
compartilhamento total, e a reconstrução aconteceu em memória
(`playlistcontrol_tests --category=installation --installation=C:\Playlist\pgm`).

1. Cada índice real foi percorrido por inteiro (`readNtx`) e comparado com as
   chaves calculadas a partir de todos os registros da tabela
   (`verifyNtx`).
2. Cada índice foi reconstruído com o algoritmo de carga ordenada
   (`buildNtx`) e comparado byte a byte com o original.

| Índice | Conteúdo (chave → registro) | Reconstrução byte a byte |
|---|---|---|
| `LIGA_COD.NTX` (24 chaves, 1 página) | idêntico | **idêntica** (0 bytes diferentes) |
| `LIGA_ARQ.NTX` (24 chaves, 13 páginas) | idêntico | **idêntica** (0 bytes diferentes) |
| `COMPROVE-C.NTX` (30 chaves) | idêntico | layout diferente |
| `COMPROVE-A.NTX` (30 chaves) | idêntico | layout diferente |

Os índices de comprovação têm o mesmo conteúdo, mas outra disposição de
páginas: o Playlist os atualiza a cada comprovação, inserindo uma chave por vez,
enquanto a reconstrução monta a árvore de uma vez. As duas formas são árvores
válidas com as mesmas chaves.

O construtor também é testado com árvores de 1 a 200 chaves e páginas de duas
chaves (o caso de `LIGA_ARQ.NTX`), relendo o resultado e comparando com as
chaves esperadas.

## O que o Playlist faz

- Os `LIGA_*.NTX` são recriados quando o Playlist abre: no dia 22/09/2026 os
  dois arquivos foram regravados às 16:49:31, instante do início do programa
  registrado no `Eventos\2026-09-22.log`, sem alteração do `LIGACAO.DBF`
  desde 16:46:50.
- Os `COMPROVE-*.NTX` são atualizados junto com o `COMPROVE.DBF` a cada
  comprovação.
- O `SeparaComprove.exe`, segundo o manual, divide o `COMPROVE.DBF` em um
  arquivo por mês e descarta registros corrompidos. O programa em si não foi
  executado nem desmontado: o comportamento exato (inclusive se ele recria os
  `COMPROVE-*.NTX`) continua **pendente**.

## Como a recriação seria feita

1. Exigir o Playlist Digital, o Config Manager, o Ligacao.exe e o Commercial
   fechados nesta máquina — e avisar que estações da rede podem estar com o
   Playlist aberto pelo compartilhamento, o que não é detectável daqui.
2. Ler e validar o DBF; recusar se estiver truncado ou com registros
   ilegíveis (o reparo do `COMPROVE.DBF` é tarefa do SeparaComprove).
3. Guardar a pasta `Indices` inteira no Histórico.
4. Montar cada índice em memória, relê-lo e conferir com a tabela.
5. Gravar cada arquivo pelo mesmo caminho seguro das demais alterações
   (temporário, substituição atômica, releitura).
6. Conferir de novo com `verifyNtx` e registrar no histórico e no log.

## O que já está no PlaylistControl

- `formats/ntx/NtxIndex`: leitura da árvore, avaliação das expressões,
  verificação contra a tabela e construção.
- O **Diagnóstico** compara todos os índices com suas tabelas e aponta índice
  danificado ou desatualizado.

## Decisões pendentes

- Liberar a recriação somente para `LIGA_*.NTX` (reprodução exata comprovada)
  ou também para `COMPROVE-*.NTX` (mesmo conteúdo, disposição diferente).
- Como tratar estações da rede que abrem o Playlist pelo compartilhamento.
- Registros marcados como excluídos no DBF: a verificação os inclui, como faz o
  Clipper; a instalação analisada não tinha nenhum, então esse caso não foi
  confirmado contra um índice real.
- Acentos fora de `ç`/`õ` em `UPPER()`: a tabela Windows-1252 é a hipótese
  coerente com os casos observados, sem confirmação para os demais caracteres.
