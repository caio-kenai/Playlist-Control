#include "formats/operators/OperatorProfile.h"

namespace pc
{

Permission parsePermission (const juce::String& value)
{
    auto v = value.trim();
    if (v == "-1") return Permission::inherit;
    if (v == "1")  return Permission::yes;
    if (v == "0")  return Permission::no;
    return Permission::other;
}

const PermissionValue* OperatorProfile::find (const juce::String& group, const juce::String& key) const
{
    for (auto& p : permissions)
        if (p.group == group && p.key == key)
            return &p;
    return nullptr;
}

std::optional<OperatorProfile> parseOperatorProfile (const juce::MemoryBlock& bytes, const juce::String& folderName,
                                                     juce::String& error)
{
    auto doc = XmlPatchDocument::parse (bytes, error);
    if (! doc.has_value())
        return std::nullopt;
    auto root = doc->root();
    if (doc->node (root).name != "Operador")
    {
        error = "O elemento raiz não é <Operador>.";
        return std::nullopt;
    }

    auto text = [&] (int parent, const char* name) {
        auto c = doc->childNamed (parent, name);
        return c >= 0 ? doc->value (c) : juce::String();
    };

    OperatorProfile p;
    p.folderName = folderName;
    p.id = text (root, "Id");
    p.isTemplate = text (root, "TipoOperador") == "1";
    p.removed = text (root, "OperadorRemovido") == "1";

    auto access = doc->childNamed (root, "Acesso");
    if (access >= 0)
    {
        p.name = text (access, "Nome");
        auto admin = text (access, "Admin");
        p.admin = admin == "1" || admin == "-1";
        p.hasPassword = text (access, "Senha").isNotEmpty();
    }
    if (p.name.isEmpty())
        p.name = p.id.isNotEmpty() ? p.id : folderName;

    for (auto* group : { "Geral", "Edit", "Blocos", "BlocoComercial", "BlocoMusical", "InsCom", "InsMus", "InsVH",
                         "InsGen", "InsPause", "Paineis" })
    {
        auto g = doc->childNamed (root, group);
        if (g < 0)
            continue;
        for (auto c : doc->node (g).children)
        {
            PermissionValue v;
            v.group = group;
            v.key = doc->node (c).name;
            v.raw = doc->value (c);
            v.permission = parsePermission (v.raw);
            p.permissions.push_back (v);
        }
    }

    auto folders = doc->childNamed (root, "Pastas");
    if (folders >= 0)
    {
        for (auto c : doc->node (folders).children)
        {
            auto value = doc->value (c);
            (value == "0" ? p.hiddenFolders : p.visibleFolders).add (doc->node (c).name);
        }
    }
    return p;
}

juce::String permissionLabel (const juce::String& group, const juce::String& key)
{
    static const std::map<juce::String, const char*> labels = {
        { "Geral/CanEdit", "Personaliza fontes e cores" },
        { "Geral/bDarkMode", "Modo dark" },
        { "Geral/bClockAmPm", "Relógio AM/PM" },
        { "Geral/bPassarComBarraDeEspaco", "Barra de espaços passa para a próxima inserção" },
        { "Geral/bLockPanes", "Travar painéis" },
        { "Geral/bLockBreaks", "Bloqueia/desbloqueia blocos" },
        { "Geral/InsAdd", "Adiciona inserções" },
        { "Geral/InsDel", "Remove inserções" },
        { "Geral/InsMove", "Move inserções" },
        { "Geral/SalvaMontagem", "Salva edição de bloco" },
        { "Geral/PstView", "Visualiza as pastas" },
        { "Geral/PstPlay", "Toca inserções diretamente das pastas" },
        { "Geral/MrcEdit", "Edita os arquivos de áudio (marcadores)" },
        { "Geral/bEditaTagID3", "Edita informações de áudio" },
        { "Geral/bUseUppercase", "Converter nomes de arquivos para maiúsculas" },
        { "Geral/bUsarCarimboHoraCerta", "Usar carimbos de hora certa e temperatura" },
        { "Geral/PnBtn", "Usar QuickStart" },
        { "Geral/bQuickstartViaRede", "Usar QuickStart via rede" },
        { "Geral/bQuickstartViaRedePlay", "Executar QuickStart via rede" },
        { "Geral/bQuickstartViaRedeReport", "Comprovar QuickStart via rede" },
        { "Geral/BtnCria", "Cria QuickStart" },
        { "Geral/bPainelBotoesIndividual", "Painéis QuickStart individuais" },
        { "Edit/OneClickAdd", "Inserir com um só clique" },
        { "Edit/OneClickOpen", "Abrir pasta com um só clique" },
        { "Edit/InserirAntes", "Inserir antes do item selecionado" },
        { "Blocos/BlPausar", "Sempre pausar ao final de um bloco" },
        { "Blocos/bPararBlocosVazios", "Parar em blocos vazios" },
        { "Blocos/PlayAuto", "Se parado, exibir o próximo bloco no horário" },
        { "Blocos/IgnorarPausas", "Ignorar todas as trilhas e pausas" },
        { "BlocoComercial/BcAdd", "Adiciona inserções" },
        { "BlocoComercial/BcAceitaMusica", "Aceita músicas" },
        { "BlocoComercial/BcDel", "Remove inserções" },
        { "BlocoComercial/BcMove", "Move inserções" },
        { "BlocoComercial/DescartarComerciais", "Descartar inserções" },
        { "BlocoComercial/RemoveBlocoComercial", "Remove blocos" },
        { "BlocoMusical/BmAdd", "Adiciona inserções" },
        { "BlocoMusical/BmAceitaComl", "Aceita comerciais" },
        { "BlocoMusical/BmDel", "Remove inserções" },
        { "BlocoMusical/BmMove", "Move inserções" },
        { "BlocoMusical/DescartarMusicas", "Descartar inserções" },
        { "BlocoMusical/RemoveBlocoMusical", "Remove blocos" },
        { "InsPause/AddPause", "Adicionar pausas" },
        { "InsPause/MaxPause", "Tempo máximo da pausa (s)" },
    };
    auto it = labels.find (group + "/" + key);
    if (it != labels.end())
        return juce::String (juce::CharPointer_UTF8 (it->second));

    // Same five permissions for each insertion type.
    auto suffix = [&] (const char* add, const char* del, const char* move) -> juce::String {
        if (key == add) return "Adiciona";
        if (key == del) return "Remove";
        if (key == move) return "Move";
        if (key.contains ("MoveEntreBlocos")) return "Move entre blocos";
        if (key.contains ("Avanca")) return juce::String (juce::CharPointer_UTF8 ("Avança para o próximo áudio"));
        return key;
    };
    if (group == "InsCom") return suffix ("IcAdd", "IcDel", "IcMove");
    if (group == "InsMus") return suffix ("ImAdd", "ImDel", "ImMove");
    if (group == "InsVH")  return suffix ("VhAdd", "VhDel", "VhMove");
    if (group == "InsGen") return suffix ("IgAdd", "IgDel", "IgMove");
    return key;
}

} // namespace pc
