#include "validation/Validators.h"
#include "formats/playlistini/FilePatternResolver.h"

namespace pc
{

namespace
{
Diagnostic make (Severity s, const juce::String& code, const juce::File& file, int line, const juce::String& message,
                 const juce::String& reason, const juce::String& fix, const juce::String& excerpt = {})
{
    Diagnostic d;
    d.severity = s;
    d.code = code;
    d.file = file;
    d.line = line;
    d.message = message;
    d.reason = reason;
    d.fix = fix;
    d.excerpt = excerpt.length() > 160 ? excerpt.substring (0, 157) + "..." : excerpt;
    return d;
}
} // namespace

DiagnosticList validateSchedule (const ScheduleDocument& doc, const juce::File& file, ScheduleKind kind,
                                 const ScheduleValidationContext& ctx)
{
    DiagnosticList out;
    std::optional<TimeOfDay> previous;
    const bool commercial = kind == ScheduleKind::commercial || kind == ScheduleKind::commercialClock;
    auto what = commercial ? juce::String ("comercial") : juce::String ("musical");

    for (int i = 0; i < (int) doc.lines().size(); ++i)
    {
        auto& l = doc.lines()[(size_t) i];
        int line = i + 1;

        if (l.kind == ScheduleLine::Kind::invalid)
        {
            out.add (make (Severity::error, "schedule.line", file, line, L"Linha sem horário de bloco válido.",
                           L"Cada linha deve começar com o horário do bloco no formato HH:MM (00:00 a 23:59). "
                           L"O Playlist não consegue montar um bloco a partir desta linha.",
                           L"Corrija o horário ou remova a linha.", l.raw));
            continue;
        }
        if (l.kind != ScheduleLine::Kind::block)
            continue;

        auto& b = l.block;
        if (! b.canonicalTime)
            out.add (make (Severity::warning, "schedule.time.format", file, line,
                           L"Horário " + b.timeText + L" sem zero à esquerda.",
                           L"Os arquivos gerados pelos programas Playlist usam sempre HH:MM; a leitura de H:MM não foi confirmada.",
                           "Use " + b.time.toString() + ".", l.raw));

        if (previous.has_value())
        {
            if (b.time == *previous)
                out.add (make (Severity::warning, "schedule.time.duplicate", file, line,
                               L"Horário " + b.time.toString() + " repetido.",
                               L"Dois blocos com o mesmo horário: o Playlist pode ignorar um deles.",
                               L"Junte os itens em uma única linha.", l.raw));
            else if (b.time < *previous)
                out.add (make (Severity::error, "schedule.time.order", file, line,
                               L"Horário " + b.time.toString() + " fora de ordem (vem depois de " + previous->toString() + ").",
                               L"O manual exige horários em ordem crescente, um bloco por linha.",
                               L"Mova a linha para a posição correta; o Playlist Control mantém a ordem ao inserir blocos.", l.raw));
        }
        previous = b.time;

        for (auto& p : b.params.items())
        {
            if (! BlockParams::isKnown (p.name))
                out.add (make (Severity::warning, "schedule.param.unknown", file, line,
                               L"Parâmetro desconhecido: " + p.toString() + ".",
                               L"Os parâmetros documentados são ID, DUR, FIXO, LOCAL, SAT, LOCKED e DESCARTE.",
                               L"Confira a digitação. O parâmetro foi mantido como está.", l.raw));
            else if (p.name.equalsIgnoreCase ("DUR") && ! parseDuration (p.value).valid)
                out.add (make (Severity::error, "schedule.param.dur", file, line,
                               L"Duração inválida: DUR=" + p.value + ".",
                               L"DUR deve ser minutos:segundos (ex.: DUR=3:00) ou um número de segundos (DUR=300, usado pelo Planner).",
                               "Corrija o valor de DUR.", l.raw));
            else if ((p.name.equalsIgnoreCase ("ID") || p.name.equalsIgnoreCase ("DUR")) && p.value.isEmpty())
                out.add (make (Severity::error, "schedule.param.empty", file, line,
                               L"Parâmetro " + p.name + " sem valor.", L"O parâmetro exige um valor após o '='.",
                               L"Informe o valor ou remova o parâmetro.", l.raw));
        }
        if (b.params.has ("FIXO") && ! commercial)
            out.add (make (Severity::info, "schedule.param.fixo", file, line,
                           "FIXO em bloco musical.", "O manual descreve FIXO apenas para blocos comerciais.",
                           L"Confirme se o parâmetro é necessário.", l.raw));
        if (b.params.has ("LOCAL") && b.params.has ("SAT"))
            out.add (make (Severity::error, "schedule.param.localsat", file, line,
                           L"O bloco é LOCAL e SAT ao mesmo tempo.", L"LOCAL e SAT são mutuamente exclusivos.",
                           "Mantenha apenas um dos dois.", l.raw));

        if (ctx.isClock && ! b.items.empty())
            out.add (make (Severity::warning, "schedule.clock.items", file, line,
                           L"Relógio com itens programados.",
                           L"Arquivos de relógio operacional contêm apenas horários e parâmetros.",
                           "Programe os itens no mapa ou na grade.", l.raw));

        for (auto& it : b.items)
        {
            if (it.kind == ItemKind::empty)
            {
                out.add (make (Severity::warning, "schedule.item.empty", file, line, L"Item vazio entre vírgulas.",
                               L"Pode indicar um código apagado por engano.", L"Remova a vírgula extra.", l.raw));
                continue;
            }
            if (it.kind == ItemKind::bareText)
                out.add (make (Severity::warning, "schedule.item.unquoted", file, line,
                               "Nome de arquivo sem aspas: " + it.raw + ".",
                               L"Nomes de arquivo nos mapas e grades gerados ficam entre aspas; sem aspas, espaços e vírgulas quebram a leitura.",
                               "Use o arquivo entre aspas.", l.raw));
            if (it.kind == ItemKind::code && it.baseCode().length() > 12)
                out.add (make (Severity::error, "schedule.item.code.length", file, line,
                               L"Código com mais de 12 caracteres: " + it.code + ".",
                               L"O registro aceita códigos de até 12 caracteres.", L"Use o código registrado.", l.raw));

            if (ctx.catalog == nullptr)
                continue;
            auto date = ctx.date.value_or (Date::today());
            auto r = ctx.catalog->resolve (it, date);
            switch (r.status)
            {
                case ItemStatus::unknownCode:
                    out.add (make (Severity::error, "schedule.item.unknown", file, line,
                                   L"Código " + it.code + L" não está registrado.",
                                   L"O Playlist mostra um X vermelho seguido do código e o item não vai ao ar no bloco " + what + ".",
                                   L"Registre o arquivo com esse código (Registrar / Ligacao.exe), registre a pasta no Config Manager "
                                   L"ou retire o código do " + (commercial ? juce::String ("mapa.") : juce::String ("arquivo.")), l.raw));
                    break;
                case ItemStatus::outOfValidity:
                    out.add (make (Severity::error, "schedule.item.validity", file, line,
                                   L"Registro do código " + it.code + " fora da validade em " + date.toString() + ".",
                                   L"Fora do período De/Até o código não é reconhecido e o item não vai ao ar.",
                                   L"Ajuste a validade do registro ou programe outro código.", l.raw));
                    break;
                case ItemStatus::fileMissing:
                    out.add (make (Severity::error, "schedule.item.file", file, line,
                                   L"Arquivo não encontrado nas pastas do Playlist: " + r.description + ".",
                                   "O Playlist mostra um X vermelho seguido do nome: o arquivo foi removido, renomeado "
                                   L"ou ainda não foi copiado para a pasta.",
                                   it.kind == ItemKind::codeAndFile
                                       ? juce::String (L"Aguarde o Sync Service baixar a mídia ou verifique o mapeamento de pastas dele.")
                                       : juce::String ("Coloque o arquivo na pasta correspondente ou corrija o nome."),
                                   l.raw));
                    break;
                case ItemStatus::ok:
                case ItemStatus::notChecked:
                    break;
            }
        }
    }
    return out;
}

DiagnosticList validatePlaylistIni (const PlaylistIni& ini, const juce::File& file, const juce::File& pgm,
                                    const Date& today, int daysAhead)
{
    DiagnosticList out;
    auto& doc = ini.document();

    for (auto kind : { ScheduleKind::commercial, ScheduleKind::musical, ScheduleKind::commercialClock, ScheduleKind::musicalClock })
    {
        auto s = ini.source (kind);
        auto section = sectionName (kind);
        int line = doc.lineOf (section, "FORMATO");
        if (line == 0)
            line = doc.lineOf (section);
        if (! s.sectionPresent)
            continue;

        if (s.format == ScheduleFormat::other)
            out.add (make (Severity::error, "ini.format", file, line, "FORMATO=" + s.formatRaw + " em [" + section + "].",
                           L"Os formatos documentados são AUTO e TXT1 (outros formatos, como DBF, não são tratados pelo Playlist Control).",
                           "Use AUTO ou TXT1."));
        if (s.format == ScheduleFormat::missing)
            out.add (make (Severity::warning, "ini.format.missing", file, line, "[" + section + "] sem FORMATO.",
                           L"Sem FORMATO o comportamento do Playlist não está documentado.", "Defina FORMATO=AUTO ou TXT1."));
        if (s.format == ScheduleFormat::txt1 && s.pattern.isEmpty())
            out.add (make (Severity::error, "ini.arquivo.missing", file, line, "[" + section + "] com FORMATO=TXT1 e sem ARQUIVO.",
                           "No formato TXT1 o Playlist precisa saber qual arquivo ler.", "Informe ARQUIVO=, por exemplo MAPAS\\%d-%m-%Y.TXT."));
        if ((kind == ScheduleKind::commercialClock || kind == ScheduleKind::musicalClock) && s.format == ScheduleFormat::automatic)
            out.add (make (Severity::warning, "ini.clock.auto", file, line, L"Relógio com FORMATO=AUTO.",
                           L"O manual documenta relógios apenas com FORMATO=TXT1 e ARQUIVO.", "Use FORMATO=TXT1."));

        for (auto& v : unknownPatternVariables (s.pattern))
            out.add (make (Severity::error, "ini.pattern.variable", file, doc.lineOf (section, "ARQUIVO"),
                           L"Variável " + v + " desconhecida em ARQUIVO=" + s.pattern + ".",
                           "O Playlist reconhece %d, %m, %Y, %y, %a e %w.", L"Corrija a variável."));

        if (s.format == ScheduleFormat::txt1 && s.pattern.isNotEmpty())
        {
            int missing = 0;
            juce::StringArray names;
            for (int d = 0; d <= daysAhead; ++d)
            {
                auto c = scheduleCandidates (s, today.addDays (d), pgm);
                if (! c.empty() && ! c.front().exists)
                {
                    ++missing;
                    if (names.size() < 3)
                        names.add (c.front().file.getFileName());
                }
            }
            if (missing > 0)
                out.add (make (kind == ScheduleKind::commercial || kind == ScheduleKind::musical ? Severity::warning : Severity::info,
                               "ini.files.missing", file, doc.lineOf (section, "ARQUIVO"),
                               juce::String (missing) + L" dia(s) entre hoje e os próximos " + juce::String (daysAhead)
                                   + " sem " + toDisplayString (kind).toLowerCase() + " (ex.: " + names.joinIntoString (", ") + ").",
                               L"Sem o arquivo do dia o Playlist não monta a programação automática desses blocos.",
                               L"Gere ou exporte o arquivo do dia, ou revise o padrão ARQUIVO."));
        }
    }

    for (auto& a : ini.affiliates())
    {
        auto host = a.address.upToLastOccurrenceOf (":", false, false);
        auto port = a.address.fromLastOccurrenceOf (":", false, false);
        if (! a.address.containsChar (':') || host.isEmpty() || ! port.containsOnly ("0123456789") || port.getIntValue() < 1
            || port.getIntValue() > 65535)
            out.add (make (Severity::error, "ini.affiliate", file, doc.lineOf ("AFILIADAS", a.id),
                           "Afiliada " + a.id + L" com endereço inválido: " + a.address + ".",
                           L"O formato é NOME=host:porta (ex.: CENTRO=192.168.0.3:3030).", L"Corrija o endereço."));
    }

    auto beep = ini.beep();
    if (beep.present)
    {
        if (! beep.minutesValid)
            out.add (make (Severity::error, "ini.beep.minutes", file, doc.lineOf ("BEEP", "HORARIO"),
                           L"HORARIO do [BEEP] inválido: " + beep.minutesRaw + ".",
                           L"HORARIO lista os minutos (0 a 59) separados por vírgula.", "Ex.: HORARIO=0,15,30,45."));
        if (beep.file.isNotEmpty() && ! (juce::File::isAbsolutePath (beep.file) ? juce::File (beep.file) : pgm.getChildFile (beep.file)).existsAsFile())
            out.add (make (Severity::warning, "ini.beep.file", file, doc.lineOf ("BEEP", "ARQUIVO"),
                           L"Arquivo do beep não encontrado: " + beep.file + ".",
                           L"Sem o arquivo o beep não toca.", "Coloque o arquivo na pasta pgm ou informe o caminho completo."));
    }
    return out;
}

std::optional<Diagnostic> validateConfigValue (const ConfigEntry& e, const juce::String& v, const juce::File& file)
{
    if (e.field == nullptr)
        return std::nullopt;
    auto& f = *e.field;
    auto label = juce::String::fromUTF8 (f.label);
    if (f.type == ConfigType::flag && v != "0" && v != "1")
        return make (Severity::error, "config.flag", file, e.line, label + ": valor " + v + L" inválido.",
                     L"Esta opção é marcada (1) ou desmarcada (0).", "Use 0 ou 1.");
    if (f.type == ConfigType::integer)
    {
        auto t = v.trim();
        auto digits = t.startsWithChar ('-') ? t.substring (1) : t;
        if (digits.isEmpty() || ! digits.containsOnly ("0123456789"))
            return make (Severity::error, "config.integer", file, e.line, label + ": \"" + v + L"\" não é um número inteiro.",
                         L"O Playlist espera um número nesta opção.", L"Informe apenas números.");
        auto n = t.getLargeIntValue();
        if (f.maxValue != 0 && (n < f.minValue || n > f.maxValue))
            return make (Severity::error, "config.range", file, e.line,
                         label + ": " + t + " fora do intervalo " + juce::String (f.minValue) + " a " + juce::String (f.maxValue) + ".",
                         juce::String::fromUTF8 (f.help), "Informe um valor dentro do intervalo.");
    }
    if (v.containsAnyOf ("\r\n"))
        return make (Severity::error, "config.newline", file, e.line, label + L": o valor contém quebra de linha.",
                     L"Valores do CONFIG.XML ficam em uma única linha.", "Remova a quebra de linha.");
    return std::nullopt;
}

DiagnosticList validateConfig (const ConfigXml& config, const juce::File& file)
{
    DiagnosticList out;
    for (auto& e : config.entries())
    {
        if (e.field == nullptr || ! e.editable() || e.value.isEmpty())
            continue;
        if (auto d = validateConfigValue (e, e.value, file))
        {
            d->severity = Severity::warning;
            d->message = "Valor atual fora do esperado. " + d->message;
            out.add (*d);
        }
    }
    return out;
}

DiagnosticList validateFolders (const FoldersXml& folders, const CodeCatalog& catalog, const juce::File& file,
                                const juce::File& pgm)
{
    DiagnosticList out;
    std::map<juce::String, juce::String> codes;
    for (auto& f : folders.folders())
    {
        if (f.kind != FolderKind::command && f.kind != FolderKind::pause && ! juce::File (f.target).isDirectory())
            out.add (make (Severity::error, "folders.target", file, f.line, "Pasta \"" + f.title + L"\" aponta para um diretório inexistente.",
                           L"O Playlist não encontra os arquivos dessa pasta e os itens programados com ela falham.",
                           L"Corrija o diretório no Config Manager ou recrie a pasta.", f.target));
        if (f.shortcutPath.isNotEmpty() && ! juce::File (f.shortcutPath).existsAsFile()
            && ! pgm.getChildFile ("Atalhos").getChildFile (f.title + ".lnk").existsAsFile())
            out.add (make (Severity::warning, "folders.shortcut", file, f.line, "Atalho da pasta \"" + f.title + L"\" não encontrado.",
                           L"O Config Manager mantém um atalho em Pgm\\Atalhos para cada pasta.",
                           "Abra e salve a pasta no Config Manager para recriar o atalho.", f.shortcutPath));
        if (f.code.length() > 12)
            out.add (make (Severity::error, "folders.code.length", file, f.line, L"Código da pasta \"" + f.title + "\" tem mais de 12 caracteres.",
                           L"Códigos registrados têm até 12 caracteres.", L"Use um código menor no campo Registrar."));
        if (f.code.isNotEmpty())
        {
            auto n = CodeCatalog::normalize (f.code);
            auto it = codes.find (n);
            if (it != codes.end())
                out.add (make (Severity::error, "folders.code.duplicate", file, f.line,
                               L"O código " + f.code + L" é usado pelas pastas \"" + it->second + "\" e \"" + f.title + "\".",
                               L"Atalhos precisam de código único; o mesmo código só pode se repetir entre músicas e comerciais.",
                               L"Altere o código de uma das pastas no Config Manager."));
            else
                codes[n] = f.title;

            bool registered = false;
            for (auto* r : catalog.registrationsFor (f.code))
                registered = registered || r->type == "A";
            if (catalog.registrations().size() > 0 && ! registered)
                out.add (make (Severity::warning, "folders.code.unregistered", file, f.line,
                               L"O código " + f.code + " da pasta \"" + f.title + L"\" não está registrado em LIGACAO.DBF.",
                               L"O Config Manager grava o código da pasta também no registro de códigos; sem ele o código pode não ser reconhecido nos mapas.",
                               "Salve a pasta novamente no Config Manager."));

            for (auto* r : catalog.registrationsFor (f.code))
                if (r->type != "A")
                    out.add (make (Severity::error, "folders.code.clash", file, f.line,
                                   L"O código " + f.code + " da pasta \"" + f.title + L"\" também está registrado para o arquivo " + r->file + ".",
                                   L"Vinhetas e atalhos devem ter código único, não utilizado por comerciais ou músicas.",
                                   L"Registre o arquivo com outro código ou altere o código da pasta."));
        }
    }

    for (auto& r : catalog.registrations())
    {
        if (r.deleted || r.type != "A")
            continue;
        if (folders.findByCode (r.code) == nullptr)
            out.add (make (Severity::warning, "ligacao.shortcut.orphan", pgm.getChildFile ("Dados/LIGACAO.DBF"), 0,
                           "Registro do atalho " + r.file + L" (código " + r.code + ") sem pasta correspondente no Folders.xml.",
                           "A pasta foi removida ou renomeada no Config Manager sem atualizar o registro.",
                           "Confira a pasta no Config Manager."));
    }
    if (folders.declaredCount() >= 0 && folders.declaredCount() != (int) folders.folders().size())
        out.add (make (Severity::warning, "folders.count", file, 3,
                       "Folders.xml declara " + juce::String (folders.declaredCount()) + L" pastas e contém "
                           + juce::String ((int) folders.folders().size()) + ".",
                       L"O contador deve refletir o número de pastas.", "Salve as pastas novamente no Config Manager."));
    return out;
}

DiagnosticList validateMerge (const MontagemFile& merge, const juce::File& file, const FoldersXml* folders)
{
    DiagnosticList out;
    for (auto& [line, text] : merge.invalidLines)
        out.add (make (Severity::error, "merge.line", file, line, "Linha de merge fora do formato.",
                       L"O formato é: HH:MM T, posição, \"Pasta\", \"Arquivo\".", "Gere o arquivo novamente.", text));
    if (folders == nullptr)
        return out;
    juce::StringArray reported;
    for (auto& e : merge.entries)
    {
        if (folders->findByTitle (e.folder) != nullptr || reported.contains (e.folder))
            continue;
        reported.add (e.folder);
        juce::String similar;
        for (auto& f : folders->folders())
            if (f.title.removeCharacters (" ").equalsIgnoreCase (e.folder.removeCharacters (" "))
                || juce::String (f.title).toLowerCase().replaceCharacters (juce::CharPointer_UTF8 ("\xc3\xa1\xc3\xa0\xc3\xa3\xc3\xa2\xc3\xa9\xc3\xaa\xc3\xad\xc3\xb3\xc3\xb4\xc3\xb5\xc3\xba\xc3\xa7"), "aaaaeeiooouc")
                       == e.folder.toLowerCase())
                similar = f.title;
        out.add (make (Severity::error, "merge.folder", file, e.line,
                       "O merge usa a pasta \"" + e.folder + L"\", que não existe no Config Manager.",
                       L"O Playlist recusa a linha (\"linha inválida\") e mantém as inserções de merge anteriores.",
                       similar.isNotEmpty() ? "A pasta parece ter sido renomeada para \"" + similar + "\". Gere o merge novamente "
                                              L"ou volte o título da pasta no Config Manager."
                                            : juce::String ("Cadastre a pasta no Config Manager ou gere o merge com a pasta correta.")));
    }
    return out;
}

} // namespace pc
