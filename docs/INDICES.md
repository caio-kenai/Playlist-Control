# Índices do Playlist Digital — conferência e recriação

O Playlist Digital localiza códigos registrados e comprovações pelos índices da
pasta `pgm\Indices`. Quando um índice fica danificado ou desatualizado, o
procedimento do suporte é: **fechar o Playlist, apagar os arquivos da pasta
`Indices`, executar o `SeparaComprove.exe` e abrir o Playlist de novo**, que
grava índices novos ao iniciar.

O Playlist Control faz esse procedimento pela tela **Suporte > Índices**, com
cópia de segurança e conferência no final.

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

## Conferência

A tela **Índices** (e o **Diagnóstico**) percorre cada árvore por inteiro e a
compara com as chaves calculadas a partir de todos os registros da tabela. O
resultado de cada arquivo é *íntegro*, *não confere* (com a primeira diferença)
ou *ilegível*.

Validação feita com a instalação real, somente leitura
(`playlistcontrol_tests --category=installation --installation=C:\Playlist\pgm`):

| Índice | Conteúdo (chave → registro) | Reconstrução em memória |
|---|---|---|
| `LIGA_COD.NTX` (24 chaves, 1 página) | idêntico | idêntica byte a byte |
| `LIGA_ARQ.NTX` (24 chaves, 13 páginas) | idêntico | idêntica byte a byte |
| `COMPROVE-C.NTX` (30 chaves) | idêntico | mesmo conteúdo, outra disposição de páginas |
| `COMPROVE-A.NTX` (30 chaves) | idêntico | mesmo conteúdo, outra disposição de páginas |

## O que cada programa faz

**Playlist Digital.** Grava os quatro índices quando abre: na instalação
analisada, os quatro arquivos têm o horário de início do programa registrado em
`Eventos\2026-09-22.log` (16:49), sem alteração das tabelas nesse instante.
Durante a execução mantém os arquivos abertos e atualiza os `COMPROVE-*.NTX` a
cada comprovação.

**SeparaComprove.exe.** Executado sobre uma **cópia** da pasta (os arquivos
reais foram conferidos antes e depois, sem alteração):

- é uma janela de diálogo "SeparaComprove" com o campo "Nome do comprovante"
  (vazio = `COMPROVE`), os botões **OK** e **Cancela**, uma barra de progresso
  e uma linha de situação;
- trabalha na pasta onde está o executável (`Dados`, `Indices`);
- ao clicar em **OK**: copia `Dados\COMPROVE.DBF` para `COMPROVE.DBF.000`
  (ou o próximo número livre: `.001`, `.002`…, que ficam na pasta), regrava o `COMPROVE.DBF` — na cópia testada só a
  data de atualização do cabeçalho mudou — e fecha sozinho em menos de um
  segundo;
- **não** cria arquivos `.NTX`: os índices novos vêm do Playlist ao abrir;
- quando há comprovações de meses anteriores, separa-as em um arquivo por mês:
  a instalação real tem `Dados\COMPROVE 05-2026.DBF` a `COMPROVE 08-2026.DBF`,
  todos de 21/09/2026, e o `COMPROVE.DBF` ficou só com o mês corrente.

## Com o Playlist aberto?

**Não é viável.** Com o Playlist em execução:

- os arquivos `.NTX` estão abertos por ele e não podem ser apagados;
- mesmo que fossem substituídos, o Playlist continuaria usando as páginas que
  já leu e gravaria sobre a árvore nova (é o funcionamento do Clipper), o que
  corromperia o índice em vez de repará-lo;
- o SeparaComprove regrava o `COMPROVE.DBF`, que o Playlist também mantém
  aberto.

A alternativa adotada é **fechar, recriar e abrir de novo no menor tempo
possível**, automatizando cada passo. O tempo fora do ar é, em geral, o tempo de
o Playlist fechar e abrir: cópia, exclusão e SeparaComprove levam poucos
segundos.

## Como o Playlist Control recria

Disponível somente com **Permitir alterações** ativo. Antes de começar, a tela
mostra o que impede a operação e pede confirmação explícita.

1. **Conferir programas abertos.** Bloqueia se `Ligacao.exe`, `ConfigManager.exe`,
   o SeparaComprove ou outro programa da pasta `pgm` estiver aberto, ou se houver
   um Playlist cuja pasta não possa ser consultada (por exemplo, rodando como
   administrador): nesse caso ele nunca é fechado daqui. O Commercial aberto gera
   só um aviso.
2. **Fechar o Playlist Digital.** Envia `WM_CLOSE` às janelas do Playlist
   *desta* pasta `pgm`, como o operador faria ao fechar a janela. Nunca encerra o
   processo à força e não usa teclado ou mouse. A pergunta *"Deseja fechar o
   programa?"* é respondida com **Sim** pelo botão da própria janela (o
   operador já confirmou no Playlist Control); outras perguntas ficam para o
   operador. Sem fechar em 90 s, a pergunta ainda aberta é cancelada, o
   Playlist continua no ar e a operação para sem alterar nada.
3. **Guardar cópia.** Copia `Indices\*.NTX`, `Dados\COMPROVE.DBF` e
   `Dados\LIGACAO.DBF` para
   `%LOCALAPPDATA%\PlaylistControl\indices\<data_hora>\` e confere os tamanhos.
4. **Apagar os índices.** Remove os `.NTX`. Se algum não puder ser apagado, os
   já removidos são devolvidos e o Playlist é aberto de novo.
5. **Executar o SeparaComprove.** Inicia o programa na pasta `pgm`, localiza o
   diálogo dele e aciona o **OK** pelo próprio botão (`BM_CLICK`). Espera até 5
   minutos; mensagens que ele mostrar ficam para o operador responder. Se ainda
   estiver aberto no fim desse tempo, o Playlist **não** é aberto
   automaticamente.
6. **Abrir o Playlist Digital.** Mesmo executável que estava aberto, com a pasta
   `pgm` como pasta de trabalho.
7. **Conferir.** Espera os `.NTX` reaparecerem com tamanho estável e compara
   cada um com a tabela.

Tudo é registrado no log (`index.rebuild.*`) e na atividade do Painel.

**Tocar ao iniciar.** A tela mostra o valor de `bTocarAoIniciar` do
`CONFIG.XML`. Na instalação analisada ele está **desativado**: depois de
reaberto, o Playlist fica parado até alguém dar play.

## Teste do procedimento

O teste `--category=rebuild` executa as sete etapas numa instalação de
demonstração indicada explicitamente, nunca na instalação real:

```powershell
playlistcontrol_tests --category=rebuild --rebuild-demo=C:\PlaylistDemo\pgm
```

Na demonstração usada no desenvolvimento, um programa de teste com o nome
`Playlist.exe` fazia o papel do Playlist (abre uma janela, grava índices ao
iniciar e fecha com `WM_CLOSE`), com uma cópia do `SeparaComprove.exe`.
Resultado: Playlist fechado pela janela, cópia feita, índices apagados,
SeparaComprove concluído (`COMPROVE.DBF` regravado e `COMPROVE.DBF.000` criado),
Playlist reaberto e índices conferidos — incluindo a detecção de um índice
desatualizado de propósito.

## Pendências

- Confirmar com o Playlist real se ele pergunta algo ao fechar (a tela já
  espera a resposta do operador) e quanto tempo leva para abrir com uma base
  grande de comprovações.
- Estações da rede que abrem o Playlist a partir da mesma pasta `pgm` não são
  detectáveis daqui; a tela pede para fechá-las antes.
- Registros marcados como excluídos no DBF: a conferência os inclui, como faz o
  Clipper; a instalação analisada não tinha nenhum.
- Acentos fora de `ç`/`õ` em `UPPER()`: a tabela Windows-1252 é a hipótese
  coerente com os casos observados.
