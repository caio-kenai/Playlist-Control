# PlaylistControl — Plano de implementação

Complementa o [ARCHITECTURE.md](ARCHITECTURE.md). Cada etapa termina com
compilação, testes, revisão e commit.

## Etapas

| # | Etapa | Entrega | Estado |
|---|---|---|---|
| 1 | Auditoria | manuais, programas instalados, `pgm`, logs | concluída |
| 2 | Documentação técnica | `ARCHITECTURE.md`, este plano | concluída |
| 3 | Projeto CMake + JUCE | JUCE 9 como submódulo, `playlistcontrol_core`, app, testes, script de build | |
| 4 | Core e storage | `TextCodec`, `FileSnapshot`, `SafeWriter`, `HistoryStore`, logger | |
| 5 | INI sem perda + `PlaylistIni` | leitura/escrita byte a byte, esquema, resolução de padrões `%d%m%Y%a%w`, busca AUTO | |
| 6 | TXT1 sem perda | `ScheduleDocument` (mapas, grades, relógios, modelos), parâmetros, itens | |
| 7 | XML por patch | `XmlPatchDocument`, `ConfigXml` + esquema, `FoldersXml` | |
| 8 | DBF/NTX/Montagem (leitura) | `DbfTable`, `NtxHeader`, `MontagemFile` | |
| 9 | Catálogo e validação | `CodeCatalog`, validadores, diagnósticos com local, causa e correção | |
| 10 | Ecossistema | `OriginDetector`, `LifecyclePolicy`, leitura de `Emissora.xml`, Sync Service, `SERVER.INI` | |
| 11 | Instalação | detecção (registro, `C:`/`D:`, Commercial), validação, confirmação, `ProcessMonitor` | |
| 12 | Serviços | `Workspace`, `DocumentSession` (snapshot/conflito), `DirectoryWatcher`, feed de atividades | |
| 13 | Interface | tema, janela principal, navegação, modo somente leitura | |
| 14 | Mapas/Grades/Relógios | visualização em blocos, edição de itens e parâmetros, dia/arquivo ativo | |
| 15 | Playlist.ini e CONFIG.XML | formulários estruturados, exigência de Playlist fechado onde necessário | |
| 16 | Pastas e códigos | Config Manager somente leitura: pastas, tipos, códigos, registros e validade | |
| 17 | Painel e diagnóstico | status, arquivos ativos, alertas, verificação cruzada | |
| 18 | Histórico | lista, diferença, restauração | |
| 19 | Testes reais | cópia da `pgm` + leitura da instalação real sem gravar | |
| 20 | README, Release, publicação | build Release, documentação final, push | |
| 21 | Índices (tese) | verificador NTX, experimento em cópia, conclusão | |

## Estratégia de testes

- Testes unitários em `juce::UnitTest`, executável `playlistcontrol_tests`
  (sai com código ≠ 0 em falha, integra com `ctest`).
- Fixtures sintéticas em `tests/fixtures`, reproduzindo as variações reais
  (CRLF/LF, sem quebra final, ASCII/CP1252/UTF-8/UTF-8 com BOM, itens com
  aspas, `CÓDIGO|arquivo`, `<TAG>`, itens vazios, parâmetros desconhecidos).
  Nenhum dado real (senhas, nomes de clientes) entra no repositório.
- Propriedade central testada em todo formato: **ler e gravar sem alterar
  nada produz os mesmos bytes**; alterar um campo muda somente aquele trecho.
- `SafeWriter`: arquivo em uso, falha de validação, conflito, restauração.
- Teste de leitura da instalação real opcional (`--installation <pgm>`),
  sempre somente leitura.
- Testes de interface ficam fora; as regras testáveis moram nos serviços.

## Riscos

| Risco | Mitigação |
|---|---|
| Gravar enquanto o Playlist lê | substituição atômica; o Playlist relê o arquivo inteiro |
| Sync/Commercial/Maker sobrescreverem edição | detecção de origem + aviso + histórico |
| Encoding errado corromper acentos | detecção e regravação no mesmo encoding; aviso ao introduzir caractere não representável |
| `CONFIG.XML` regravado pelo Playlist | edição só com Playlist fechado |
| Formatos não documentados | preservação do texto original; nada é normalizado sem pedido |
| Senhas em arquivos | nunca registradas em log; campos mascarados na interface |

## Pendências de investigação

1. Semântica de `DUR=300` (segundos?) versus `DUR=3:00`.
2. `%w`: Domingo = 0 ou 7.
3. Comandos `<IALOC>`, `<IANEWS>` nas grades.
4. Aceitação de horário sem zero à esquerda e de nome de arquivo sem aspas.
5. Encoding esperado pelo Playlist para mapas/grades com acentos.
6. Se o Playlist relê `PLAYLIST.ini` sem reiniciar.
7. Valores de `TIPO` em `LIGACAO.DBF` além de `A` e `C`.
8. Formato exato do rodízio com letra do Commercial.
9. Causa do `Merge ... linha 1 inválida` (hipótese: título de pasta).
10. Comportamento do `SeparaComprove.exe` e recriação de `COMPROVE-*.NTX`.
