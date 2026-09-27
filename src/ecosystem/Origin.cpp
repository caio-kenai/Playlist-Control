#include "ecosystem/Origin.h"
#include "install/Installation.h"

namespace pc
{

juce::String toDisplayString (FileOrigin origin)
{
    switch (origin)
    {
        case FileOrigin::planner:    return "Planner (Sync Service)";
        case FileOrigin::commercial: return "Commercial";
        case FileOrigin::maker:      return "Maker (Playlist Server)";
        case FileOrigin::electoral:  return L"Horário Eleitoral";
        case FileOrigin::manual:     return "Manual";
        case FileOrigin::unknown:    return "Desconhecida";
    }
    return {};
}

std::optional<Date> dateFromFileName (const juce::String& fileName)
{
    auto name = fileName.upToLastOccurrenceOf (".", false, false);
    if (name.startsWithIgnoreCase ("Mapa"))
        name = name.substring (4);
    auto parts = juce::StringArray::fromTokens (name, "-", {});
    if (parts.size() != 3)
        return std::nullopt;
    for (auto& p : parts)
        if (p.isEmpty() || ! p.containsOnly ("0123456789"))
            return std::nullopt;
    Date d { parts[2].getIntValue(), parts[1].getIntValue(), parts[0].getIntValue() };
    if (d.year < 1990 || d.month < 1 || d.month > 12 || d.day < 1 || d.day > 31)
        return std::nullopt;
    return d;
}

OriginAssessment assessScheduleOrigin (const juce::File& file, const ScheduleDocument& doc, ScheduleKind kind,
                                       const OriginContext& ctx)
{
    OriginAssessment a;
    auto date = dateFromFileName (file.getFileName());
    auto* eco = ctx.ecosystem;

    int codeAndFile = 0, quoted = 0, codes = 0, numericDur = 0, blocks = 0;
    for (auto& l : doc.lines())
    {
        if (l.kind != ScheduleLine::Kind::block)
            continue;
        ++blocks;
        if (auto d = l.block.params.value ("DUR"); d.has_value() && parseDuration (*d).numeric)
            ++numericDur;
        for (auto& it : l.block.items)
        {
            codeAndFile += it.kind == ItemKind::codeAndFile ? 1 : 0;
            quoted += it.kind == ItemKind::quotedFile ? 1 : 0;
            codes += it.kind == ItemKind::code ? 1 : 0;
        }
    }

    const bool inMaps = file.getParentDirectory().getFileName().equalsIgnoreCase ("Mapas");
    const bool inGrades = file.getParentDirectory().getFileName().equalsIgnoreCase ("Grades");

    if (kind == ScheduleKind::commercial || inMaps)
    {
        if (eco != nullptr && eco->sync.found && eco->sync.writesMaps && date.has_value()
            && samePath (eco->sync.mapsFolder, file.getParentDirectory()))
        {
            a.origin = FileOrigin::planner;
            a.confirmed = true;
            a.reason = L"O Sync Service está configurado para gravar mapas nesta pasta.";
            if (eco->sync.managesDate (*date, ctx.today))
            {
                a.rewrittenAutomatically = true;
                a.overwriteWarning = "O Sync Service regrava este mapa a cada " + juce::String (eco->sync.intervalMinutes)
                                   + L" minutos (hoje e os próximos " + juce::String (eco->sync.daysAhead)
                                   + L" dias). Uma alteração feita aqui será perdida na próxima sincronização; "
                                     L"altere a programação no Planner.";
            }
            else
            {
                a.overwriteWarning = L"Este dia está fora do período sincronizado agora, mas o Sync Service volta a "
                                     L"regravar o mapa quando a data entrar no período.";
            }
            return a;
        }
        if (codeAndFile > 0 || (blocks > 0 && numericDur == blocks))
        {
            a.origin = FileOrigin::planner;
            a.reason = L"Itens no formato \"CÓDIGO|arquivo\" e DUR em segundos, como os mapas do Planner.";
            a.overwriteWarning = L"Mapas do Planner são regravados pelo Sync Service; confira no Planner.";
            return a;
        }
        if (eco != nullptr && eco->commercial.found && date.has_value()
            && samePath (eco->commercial.mapsFolder, file.getParentDirectory()))
        {
            a.origin = FileOrigin::commercial;
            a.reason = "O Commercial exporta mapas com data completa para esta pasta.";
            a.overwriteWarning = L"Uma nova exportação do Commercial para este dia substitui o arquivo.";
            return a;
        }
    }

    if (kind == ScheduleKind::musical || inGrades)
    {
        if (date.has_value() && quoted > 0 && codes == 0 && codeAndFile == 0)
        {
            a.origin = FileOrigin::maker;
            a.reason = L"Grade diária com nomes de arquivo entre aspas, como as gravadas pelo Playlist Server.";
            a.confirmed = false;
            if (eco != nullptr && eco->playlistServer.found)
                a.overwriteWarning = L"Qualquer alteração da programação deste dia no Maker regrava o arquivo.";
            return a;
        }
    }

    a.origin = FileOrigin::manual;
    a.reason = date.has_value() ? L"Arquivo diário sem sinais de geração automática."
                                : L"Arquivo fixo (padrão, dia da semana ou relógio), mantido manualmente.";
    return a;
}

PlaylistRuntime queryRuntime (const juce::File& pgm)
{
    PlaylistRuntime r;
    for (auto& p : runningProcesses())
    {
        auto name = p.exeName.toLowerCase();
        juce::File path (p.fullPath);
        bool inPgm = p.fullPath.isEmpty() || path.getParentDirectory() == pgm;
        if (isPlaylistExecutableName (p.exeName) && inPgm)
        {
            r.playlistRunning = true;
            r.playlistProcesses.add (p.exeName + " (PID " + juce::String (p.pid) + ")");
        }
        else if (name == "configmanager.exe")
            r.configManagerRunning = true;
        else if (name == "ligacao.exe")
            r.ligacaoRunning = true;
        else if (name == "commercial.exe")
            r.commercialRunning = true;
        else if (name == "maker.exe")
            r.makerRunning = true;
    }
    return r;
}

} // namespace pc
