#include "formats/configxml/ConfigSchema.h"

namespace pc
{

namespace
{
using T = ConfigType;

// Labels and explanations follow the manual of the Playlist Digital 5
// (Menu de Configurações). Entries with confirmed = false have no explicit
// description in the manual; their meaning is inferred from the key name.
const std::vector<ConfigField> fields = {
    // Playlist Server
    { "sPlaylistServer", "Playlist Server", "Servidor", "Nome ou IP da máquina com o Playlist Server (banco do Maker e do Logger Manager).", T::text },
    { "nPlaylistServerPort", "Playlist Server", "Porta de dados", "Porta do Playlist Server. O padrão é 3033.", T::integer, 1, 65535 },
    { "sPlaylistServerUser", "Playlist Server", "Usuário", "Usuário de conexão com o Playlist Server.", T::text },
    { "sPlaylistServerPassword", "Playlist Server", "Senha", "Senha de conexão com o Playlist Server.", T::secret },

    // Avançado
    { "sNomeDaEmissora", "Avançado", "Título", "Nome da emissora.", T::text },
    { "sServidor", "Avançado", "Computador do AR", "Nome ou IP da máquina do AR, usado pelas estações da rede.", T::text },
    { "nPortaDados", "Avançado", "Porta de dados", "Porta entre o Playlist do AR e as estações da rede. O padrão é 3030.", T::integer, 1, 65535 },
    { "m_sProxyServerURL", "Avançado", "Servidor proxy", "servidor:porta do proxy da emissora, se houver.", T::text },
    { "webSocketPort", "Avançado", "Porta WebSocket", "Porta em que o Playlist abre o WebSocket (registrado no log ao iniciar).", T::integer, 1, 65535, "", false },
    { "waitBeforeRestart", "Avançado", "Espera antes de reiniciar", "Tempo de espera usado ao reiniciar o programa.", T::integer, 0, 3600, "s", false },

    // Station Office
    { "m_bStationOfficeSelecionado", "Station Office", "Enviar veiculação para o Station Office", "Envia as comprovações ao Station Office / Planner.", T::flag },
    { "m_sCSO", "Station Office", "Código Station Office (CSO)", "Código da emissora no Station Office. Deve ser usado somente na máquina do AR.", T::text },

    // Saídas (per machine)
    { "Saidas_*/Programacao", "Saídas", "Programação", "Placa que reproduz a programação no AR.", T::readOnly },
    { "Saidas_*/Escuta", "Saídas", "Pré-escuta", "Placa da pré-escuta.", T::readOnly },
    { "Saidas_*/Botoes", "Saídas", "QuickStart", "Placa dos botões QuickStart.", T::readOnly },
    { "Saidas_*/OpcaoTocar", "Saídas", "Opção Tocar", "Placa usada pela opção Tocar.", T::readOnly },
    { "Saidas_*/Comerciais", "Saídas", "Comerciais", "Placa dos comerciais.", T::readOnly },
    { "Saidas_*/Musicas", "Saídas", "Músicas", "Placa das músicas.", T::readOnly },
    { "Saidas_*/Vinhetas", "Saídas", "Vinhetas", "Placa das vinhetas.", T::readOnly },
    { "Saidas_*/AudioSource", "Entrada de áudio", "Entrada de áudio", "Placa usada como entrada de linha.", T::readOnly },
    { "Saidas_*/UrlSource", "Entrada de áudio", "URL da fonte de áudio", "Streaming reproduzido quando o Playlist está parado.", T::text },
    { "Saidas_*/PluginDSP", "Entrada de áudio", "Plugin DSP", "Plugin de processamento de áudio.", T::readOnly },

    // Entrada de áudio
    { "sSourceTypeSelected", "Entrada de áudio", "Tipo de fonte", "Audio (cabo físico ou virtual) ou URL (streaming).", T::text },
    { "FecharLinhaAoTocar", "Entrada de áudio", "Exibir entrada de linha enquanto parado", "Fecha a entrada de linha ao iniciar a programação e abre ao parar.", T::flag, 0, 0, "", false },
    { "nEsperaAoFecharLinha", "Entrada de áudio", "Tempo de espera antes de fechar a linha", "Segundos de espera antes de fechar a entrada de linha.", T::integer, 0, 3600, "s" },
    { "DTMFPlay", "Entrada de áudio", "Comando DTMF Play", "Tons que disparam PLAY. Vários comandos separados por ';'.", T::text },
    { "DTMFStop", "Entrada de áudio", "Comando DTMF Stop", "Tons que disparam STOP. Vários comandos separados por ';'.", T::text },
    { "DTMFLevel", "Entrada de áudio", "Nível DTMF", "Sensibilidade: alto demais ignora disparos, baixo demais causa falsos disparos.", T::integer, 0, 100000 },
    { "bGravarLogDTMF", "Entrada de áudio", "Gravar log de DTMF", "Registra os tons recebidos em dtmf.log.", T::flag, 0, 0, "", false },

    // Disparo remoto
    { "bAceitarDisparoEmBlocosLocais", "Disparo remoto", "Aceita comandos em blocos locais", "Aceita PLAY/STOP também em blocos que não são SAT.", T::flag },
    { "Disparo", "Disparo remoto", "Aceita comando remoto PLAY", "Aceita PLAY por porta paralela, porta de jogos ou IP.", T::flag, 0, 0, "", false },
    { "Botao2", "Disparo remoto", "Aceita comando remoto STOP", "Aceita STOP remoto.", T::flag, 0, 0, "", false },
    { "bBotao1Passa", "Disparo remoto", "Se exibindo, PLAY passa para o próximo áudio", "Um PLAY recebido durante a exibição avança para o próximo áudio.", T::flag, 0, 0, "", false },
    { "bBotao2NaoParaComercial", "Disparo remoto", "STOP não interrompe comercial", "Sem descrição no manual; o nome indica que o STOP é ignorado durante comerciais.", T::flag, 0, 0, "", false },

    // Afiliada de rede
    { "Afiliada", "Afiliada de rede", "Afiliada de rede", "Blocos SAT só começam por comando remoto ou manual.", T::flag },
    { "AfiliadaID", "Afiliada de rede", "ID da afiliada", "Identificador desta emissora na rede via IP.", T::text },
    { "DisparoComercial", "Afiliada de rede", "Se exibindo bloco local, iniciar próximo bloco SAT", "Ao receber PLAY durante um bloco local, passa ao próximo SAT.", T::flag, 0, 0, "", false },
    { "bIgnorarDisparoForaDoHorario", "Afiliada de rede", "Aceitar disparo somente no horário", "Ignora PLAY fora da tolerância em minutos abaixo.", T::flag },
    { "bPosicionarAutomaticamente", "Afiliada de rede", "Posicionar automaticamente os blocos", "Quando parado, posiciona os blocos conforme o relógio.", T::flag },
    { "nMinutosPosicionar", "Afiliada de rede", "Minutos para posicionar e aceitar disparo", "Tolerância para mais ou para menos, em minutos.", T::integer, 0, 1440, "min" },

    // XML para web
    { "sArquivoXML", "XML para web", "Arquivo com informação do item atual", "Caminho local do XML com o item no ar e os próximos.", T::text },
    { "sFtpServer", "XML para web", "Servidor FTP", "Servidor para envio do XML.", T::text },
    { "sFtpLogin", "XML para web", "Usuário FTP", "Usuário do FTP.", T::text },
    { "sFtpPassword", "XML para web", "Senha FTP", "Senha do FTP.", T::secret },
    { "sFtpFile", "XML para web", "Arquivo no servidor FTP", "Destino do XML no FTP.", T::text },
    { "bFtpPassive", "XML para web", "FTP passivo", "Modo de conexão do FTP.", T::flag },
    { "sXmlUdpAddress", "XML para web", "Enviar XML para UDP", "IP:porta que recebe o XML por UDP.", T::text },

    // RDS / RSS
    { "sRDSModel", "RDS", "Modelo", "Arquivo, Acádia (Biquad), Audemat, Audemat Enc. Silver ou Inovonics.", T::text },
    { "sRDSAddress", "RDS", "Endereço do encoder RDS", "IP:porta do encoder, ou nome do arquivo .txt quando o modelo é Arquivo.", T::text },
    { "sRDSDefaultText", "RDS", "Texto padrão", "Texto enviado quando não há música ou comercial com título.", T::text },
    { "bRDSComl", "RDS", "Enviar comerciais", "Envia o nome dos comerciais em execução.", T::flag },
    { "bRDSRemoveSpecialCharacters", "RDS", "Remover caracteres especiais", "Sem descrição no manual; remove acentos e símbolos do texto enviado.", T::flag, 0, 0, "", false },
    { "sRss_Addresses", "RSS", "Endereços de feed", "URLs separadas por ';'.", T::text },
    { "iRDS_IntervalInSeconds", "RSS", "Intervalo para envio ao RDS", "Tempo padrão de exibição de uma notícia.", T::integer, 1, 86400, "s" },
    { "iRDS_PSSize", "RSS", "Tamanho do campo PS", "Conforme o modelo do RDS.", T::integer, 1, 255 },
    { "iRDS_RTSize", "RSS", "Tamanho do campo RT", "Conforme o modelo do RDS.", T::integer, 1, 255 },
    { "bRDS_SendBoth", "RSS", "Enviar o mesmo para RT e PS", "Envia a mesma informação aos dois campos.", T::flag },

    // Streaming
    { "sCastModel", "Metadados para streaming", "Serviço", "Shoutcast V1, Shoutcast V2 ou IceCast V2.", T::text },
    { "sSHOUTcastServer", "Metadados para streaming", "Servidor", "Endereço do servidor de streaming.", T::text },
    { "sICEcastUser", "Metadados para streaming", "Usuário (IceCast V2)", "Usuário administrador.", T::text },
    { "sSHOUTcastPassword", "Metadados para streaming", "Senha", "Senha de administrador.", T::secret },
    { "sSHOUTcastId", "Metadados para streaming", "Id (IceCast V2)", "Mountpoint, ex.: /stream.", T::text },
    { "sSHOUTcastURL", "Metadados para streaming", "URL", "Site da emissora.", T::text },
    { "sURLToPostJSON", "Metadados para streaming", "URL para JSON", "Sem descrição no manual.", T::text, 0, 0, "", false },
    { "sTuneInStationId", "Metadados para streaming", "TuneIn - Station ID", "Sem descrição no manual.", T::text, 0, 0, "", false },
    { "sTuneInPartnerId", "Metadados para streaming", "TuneIn - Partner ID", "Sem descrição no manual.", T::text, 0, 0, "", false },
    { "sTuneInPartnerKey", "Metadados para streaming", "TuneIn - Partner Key", "Sem descrição no manual.", T::secret, 0, 0, "", false },

    // Camera Controller / VLC / AVRA
    { "sCameraController_software", "Camera Controller", "Software", "OBS ou vMix.", T::text },
    { "sCameraController_address", "Camera Controller", "Endereço", "IP:porta do software.", T::text },
    { "sCameraController_user", "Camera Controller", "Usuário", "Se aplicável.", T::text },
    { "sCameraController_pwd", "Camera Controller", "Senha", "Se aplicável.", T::secret },
    { "bCameraController_ShowTitle", "Camera Controller", "Sobreposição de título", "Valor gravado pelo Playlist (máscara de opções).", T::readOnly },
    { "bCameraController_EnableVideoMix", "Camera Controller", "Habilita a mixagem de vídeos", "Valor gravado pelo Playlist (máscara de opções).", T::readOnly },
    { "iCameraController_OverlayTime", "Camera Controller", "Tempo de sobreposição de título", "Entre 5000 e 10000 ms.", T::integer, 5000, 10000, "ms" },
    { "sVLCAddress", "VLC Controller", "Endereço", "IP:porta do VLC (padrão 8080).", T::text },
    { "sVLCPwd", "VLC Controller", "Senha", "Senha da interface HTTP Lua do VLC.", T::secret },
    { "sAvraAddress", "AVRA", "Servidor", "IP:porta do servidor AVRA.", T::text },
    { "bWebView2", "Diversos", "Usar WebView2", "Sem descrição no manual.", T::flag, 0, 0, "", false },

    // Diversos
    { "bTocarAoIniciar", "Diversos", "Tocar programação ao iniciar", "Começa a tocar assim que o programa abre.", T::flag },
    { "DicaDoDia", "Diversos", "Dica do dia", "Mostra dicas ao abrir.", T::flag },
    { "sPastaTrilhas", "Diversos", "Pasta de trilhas", "Trilhas de fundo sorteadas para textos ao vivo inseridos manualmente.", T::text },
    { "bSalvarMontagem", "Diversos", "Salvar montagem em txt", "Grava as montagens também em Pgm\\Montagem.", T::flag },
    { "bManterOrdemProgramacao", "Diversos", "Manter ordem da programação", "Ao exportar mapas, mantém a ordem que já estava no bloco.", T::flag },
    { "bIgnorarBloqueados", "Diversos", "Ignorar atualizações em blocos bloqueados", "Blocos bloqueados não recebem atualizações dos mapas.", T::flag },
    { "nPercComprovacao", "Diversos", "Percentual mínimo para comprovação", "Percentual executado para a inserção ser comprovada.", T::integer, 0, 100, "%" },
    { "bComprIgnoraMixagem", "Diversos", "Desconsiderar o tempo de mixagem na comprovação", "Mixagem não conta como tempo executado.", T::flag },
    { "bSalvarTempoDaMidiaNoComprove", "Diversos", "Salvar duração da mídia na comprovação", "Ignora os marcadores de início e fim na duração comprovada.", T::flag },
    { "nOffsetHTU", "Diversos", "Ajuste do leitor de temperatura", "Diferença em décimos de grau (1 grau = 10).", T::integer, -1000, 1000 },
    { "nDelayProcess", "Diversos", "Tempo entre atualizações", "Espera entre atualizações de LIGACAO.DBF, mapas e locuções, e antes de salvar.", T::integer, 0, 3600, "s" },
    { "bDisableMD5", "Diversos", "Desabilita MD5 no comprovante", "Não grava o MD5 do arquivo na comprovação.", T::flag },

    // Inserções
    { "MixComerciais", "Tempo de mixagem padrão", "Comerciais", "Mixagem padrão dos comerciais.", T::integer, 0, 60000, "ms" },
    { "MixMusicas", "Tempo de mixagem padrão", "Músicas", "Mixagem padrão das músicas.", T::integer, 0, 60000, "ms" },
    { "MixVinhetas", "Tempo de mixagem padrão", "Vinhetas", "Mixagem padrão das vinhetas.", T::integer, 0, 60000, "ms" },
    { "MixGeral", "Tempo de mixagem padrão", "Demais inserções", "Mixagem padrão das demais inserções.", T::integer, 0, 60000, "ms" },
    { "MixLocucoes", "Tempo de mixagem padrão", "Locuções", "Mixagem padrão das locuções.", T::integer, 0, 60000, "ms" },
    { "MixHCerta", "Tempo de mixagem padrão", "Hora certa / temperatura", "Mixagem padrão de hora certa e temperatura.", T::integer, 0, 60000, "ms" },
    { "MixRefrao", "Tempo de mixagem padrão", "Refrão", "Mixagem padrão do refrão.", T::integer, 0, 60000, "ms" },
    { "nFadeInRefrao", "Tempo de mixagem padrão", "Fade in do refrão", "Duração do fade no início do refrão.", T::integer, 0, 60000, "ms" },
    { "nFadeOutRefrao", "Tempo de mixagem padrão", "Fade out do refrão", "Duração do fade no fim do refrão.", T::integer, 0, 60000, "ms" },
    { "nTempoFade", "Tempo de mixagem padrão", "Fade nas passagens manuais", "Duração do fade ao adiantar para a próxima inserção.", T::integer, 0, 60000, "ms" },

    { "bFadeBotoes", "Fade automático", "Fade para QuickStart", "Abaixa a programação ao tocar um QuickStart.", T::flag },
    { "bFadeLocucao", "Fade automático", "Fade para locuções", "Abaixa a programação durante locuções gravadas.", T::flag },
    { "bFadeCarimbo", "Fade automático", "Fade para carimbos", "Abaixa a programação durante carimbos.", T::flag },
    { "bFadeTocar", "Fade automático", "Fade ao tocar da pasta", "Abaixa a programação ao tocar direto da pasta.", T::flag },
    { "nPercFadeLocucao", "Fade automático", "Percentual de fade", "Quanto maior, maior a redução de volume.", T::integer, 0, 100, "%", false },
    { "PercFadeProgr", "Fade automático", "Percentual de fade da programação", "Sem descrição separada no manual.", T::integer, 0, 100, "%", false },
    { "bFadeEntradaLinha", "Fade automático", "Aplicar fade na entrada de linha", "Abaixa a linha ao tocar QuickStart ou direto da pasta.", T::flag },
    { "nPercFadeEntradaLinha", "Fade automático", "Volume da entrada de áudio ao tocar", "Percentual de fade da linha.", T::integer, 0, 100, "%" },

    { "MusUsarPontoMix", "Músicas", "Usar marcadores de ponto de mixagem", "", T::flag },
    { "MusUsarMixIni", "Músicas", "Usar marcadores de mixagem do início", "", T::flag },
    { "MusFadeAuto", "Músicas", "Fade out automático", "", T::flag },
    { "MusPausaQdoNaoGravada", "Músicas", "Pausa em música programada mas não disponível", "Pausa para tocar a música de outra fonte.", T::flag },
    { "SenhaRemoveMusica", "Músicas", "Senha para remover música", "", T::secret },
    { "FormatoNomeMusica", "Músicas", "Padrão de nomenclatura dos arquivos", "'Artista - Música' ou 'Música - Artista'.", T::text },
    { "ComlUsarPontoMix", "Comerciais", "Usar marcadores de ponto de mixagem", "", T::flag },
    { "ComlUsarMixIni", "Comerciais", "Usar marcadores de mixagem do início", "", T::flag },
    { "SenhaRemoveComl", "Comerciais", "Senha para remover comercial", "", T::secret },
    { "VhUsarPontoMix", "Vinhetas", "Usar marcadores de ponto de mixagem", "", T::flag },
    { "VhUsarMixIni", "Vinhetas", "Usar marcadores de mixagem do início", "", T::flag },
    { "VhFadeAuto", "Vinhetas", "Fade out automático", "", T::flag },
    { "GenUsarPontoMix", "Inserções genéricas", "Usar marcadores de ponto de mixagem", "", T::flag },
    { "GenUsarMixIni", "Inserções genéricas", "Usar marcadores de mixagem do início", "", T::flag },
    { "GenFadeAuto", "Inserções genéricas", "Fade out automático", "", T::flag },
    { "bLocucaoAutomatica", "Locuções gravadas", "Inserir locução pré-gravada", "Insere DDMMHHMMa / DDMMHHMM no início e fim dos blocos musicais.", T::flag },
    { "bLocucaoAvancarSobreIntroducao", "Locuções gravadas", "Locução sobre introdução", "Mixa a locução com a introdução da música.", T::flag },
    { "bLocUsaPontoMix", "Locuções gravadas", "Usar marcadores de ponto de mixagem", "", T::flag },
    { "bLocUsaMixIni", "Locuções gravadas", "Usar marcadores de mixagem do início", "", T::flag },
    { "bTrilhaEmLocucao", "Locuções gravadas", "Trilha em locução", "Sem descrição no manual.", T::flag, 0, 0, "", false },
    { "bHCertaUsaPontoMix", "Hora certa / temperatura", "Usar marcadores de ponto de mixagem", "", T::flag },
    { "bHCertaUsaMixIni", "Hora certa / temperatura", "Usar marcadores de mixagem do início", "", T::flag },
};
} // namespace

const std::vector<ConfigField>& configSchema()
{
    return fields;
}

const ConfigField* findConfigField (const juce::String& key)
{
    for (auto& f : fields)
    {
        juce::String k (f.key);
        if (k == key)
            return &f;
        if (k.startsWith ("Saidas_*/") && key.startsWith ("Saidas_") && key.fromFirstOccurrenceOf ("/", false, false) == k.fromFirstOccurrenceOf ("/", false, false))
            return &f;
    }
    return nullptr;
}

juce::StringArray configGroups()
{
    juce::StringArray groups;
    for (auto& f : fields)
        groups.addIfNotAlreadyThere (juce::String::fromUTF8 (f.group));
    groups.add (L"Outras configurações");
    return groups;
}

} // namespace pc
