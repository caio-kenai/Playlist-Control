"""Regenerates tests/fixtures/installation: a small, invented Playlist
installation with the same file formats as a real one (no real data).

    python tools/make_fixtures.py

Folders.xml uses %PLAYLIST_ROOT% where a real file has C:\\Playlist; the
tests replace it with the temporary copy's path.
"""

import os
import shutil
import struct

ROOT = os.path.join(os.path.dirname(__file__), "..", "tests", "fixtures", "installation")
PGM = os.path.join(ROOT, "pgm")


def write(rel, data, root=PGM):
    path = os.path.join(root, rel)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)


def crlf(lines, final_newline=True):
    text = "\r\n".join(lines)
    return text + ("\r\n" if final_newline else "")


def dbf(fields, records, version=0x03, date=(126, 9, 21)):
    """dBase III table. fields: (name, type, length, decimals)."""
    header_len = 32 + 32 * len(fields) + 1
    record_len = 1 + sum(f[2] for f in fields)
    out = bytearray()
    out += struct.pack("<BBBBIHH", version, date[0], date[1], date[2], len(records), header_len, record_len)
    out += b"\0" * 20
    for name, ftype, length, dec in fields:
        out += name.encode("ascii").ljust(11, b"\0")
        out += ftype.encode("ascii")
        out += b"\0" * 4
        out += bytes([length, dec])
        out += b"\0" * 14
    out += b"\x0d"
    for deleted, values in records:
        out += b"*" if deleted else b" "
        for (name, ftype, length, dec), value in zip(fields, values):
            raw = str(value).encode("cp1252")
            if ftype == "N":
                raw = raw.rjust(length)
            else:
                raw = raw.ljust(length)
            out += raw[:length]
    out += b"\x1a"
    return bytes(out)


def ntx(expression, key_size, version=0, pages=1, keys=None):
    """Clipper NTX. With 'keys' (sorted (key bytes, record) pairs that fit in
    one page) the root page holds them; otherwise the page is empty."""
    header = bytearray(1024)
    item = key_size + 8
    max_items = (1024 - 4) // (item + 2) - 1
    struct.pack_into("<HHIIHHHHH", header, 0, 6, version, 1024, 0, item, key_size, 0, max_items, max_items // 2)
    header[22:22 + len(expression)] = expression.encode("ascii")
    body = bytearray(1024 * pages)
    keys = keys or []
    assert len(keys) <= max_items
    base = 2 + (max_items + 1) * 2
    struct.pack_into("<H", body, 0, len(keys))
    for i in range(max_items + 1):
        struct.pack_into("<H", body, 2 + 2 * i, base + i * item)
    for i, (key, rec) in enumerate(keys):
        o = base + i * item
        struct.pack_into("<II", body, o, 0, rec)
        body[o + 8:o + 8 + key_size] = key.ljust(key_size, b" ")[:key_size]
    return bytes(header) + bytes(body)


def upper1252(data):
    out = bytearray()
    for c in data:
        if 0x61 <= c <= 0x7A or (0xE0 <= c <= 0xFE and c != 0xF7):
            c -= 0x20
        out.append(c)
    return bytes(out)


def descend(data):
    return bytes((256 - c) & 0xFF for c in data)


def main():
    if os.path.isdir(ROOT):
        shutil.rmtree(ROOT)

    # Program and configuration -------------------------------------------
    write("Playlist.exe", b"")
    write("ConfigManager.exe", b"")
    write("PLAYLIST.ini", crlf([
        "[BLOCO COMERCIAL]",
        "FORMATO=TXT1",
        "ARQUIVO=MAPAS\\%d-%m-%Y.TXT",
        "",
        "[BLOCO MUSICAL]",
        "FORMATO=AUTO",
        "",
        ";[RDS]",
        ";ARQUIVO=RDS\\RDS.txt",
        "",
        "[AFILIADAS]",
        "CENTRO=192.168.0.10:3030",
    ], final_newline=False).encode("ascii"))

    config = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        "<Config>",
        "\t<LastAuthStation_ESTUDIO>1790106572</LastAuthStation_ESTUDIO>",
        "\t<waitBeforeRestart>30</waitBeforeRestart>",
        "\t<Saidas_ESTUDIO>",
        "\t\t<GUID_Programacao>{00000000-0000-0000-0000-000000000001}</GUID_Programacao>",
        "\t\t<Programacao>Alto-falantes (Placa Principal)</Programacao>",
        "\t\t<Escuta>Fones (Placa Principal)</Escuta>",
        "\t\t<AudioSource>Entrada de linha (Placa Principal)</AudioSource>",
        "\t\t<UrlSource>https://stream.exemplo.local/radio</UrlSource>",
        "\t\t<PluginDSP>",
        "\t\t</PluginDSP>",
        "\t</Saidas_ESTUDIO>",
        "\t<webSocketPort>9002</webSocketPort>",
        "\t<sPlaylistServer>localhost</sPlaylistServer>",
        "\t<nPlaylistServerPort>3033</nPlaylistServerPort>",
        "\t<sPlaylistServerPassword>",
        "\t</sPlaylistServerPassword>",
        "\t<sNomeDaEmissora>Radio Exemplo</sNomeDaEmissora>",
        "\t<sServidor>ESTUDIO</sServidor>",
        "\t<nPortaDados>3030</nPortaDados>",
        "\t<m_bStationOfficeSelecionado>1</m_bStationOfficeSelecionado>",
        "\t<m_sCSO>000000</m_sCSO>",
        "\t<Afiliada>1</Afiliada>",
        "\t<AfiliadaID>EXEMPLO</AfiliadaID>",
        "\t<bPosicionarAutomaticamente>0</bPosicionarAutomaticamente>",
        "\t<nMinutosPosicionar>9</nMinutosPosicionar>",
        "\t<sFtpServer>",
        "\t</sFtpServer>",
        "\t<sVLCAddress>localhost:8080</sVLCAddress>",
        "\t<sVLCPwd>senha-de-teste</sVLCPwd>",
        "\t<bCameraController_EnableVideoMix>-2013265919</bCameraController_EnableVideoMix>",
        "\t<nPercComprovacao>50</nPercComprovacao>",
        "\t<MixMusicas>1750</MixMusicas>",
        "\t<bTocarAoIniciar>0</bTocarAoIniciar>",
        "\t<sRDSDefaultText>Radio &amp; Cia</sRDSDefaultText>",
        "\t<novaChaveDesconhecida>abc</novaChaveDesconhecida>",
        "</Config>",
    ]
    write("CONFIG.XML", crlf(config).encode("utf-8"))

    # Folders (Config Manager) ----------------------------------------------
    folders = [
        ("Musicas", "M", "%PLAYLIST_ROOT%\\Musicas", "MUSIC", 4),
        ("Comerciais", "$", "%PLAYLIST_ROOT%\\Comerciais", "COM", 1),
        ("Vinhetas", "V", "%PLAYLIST_ROOT%\\Vinhetas", "VH", 6),
        ("Hora Certa", "H", "%PLAYLIST_ROOT%\\Hora Certa", "HC", 5),
        ("Pausa", "P", "%PLAYLIST_ROOT%\\pgm", "PAUSA", 7),
        ("Streaming", "C", "%PLAYLIST_ROOT%\\pgm", "STREAM", 11),
        ("Elei\u00e7\u00f5es", "$", "%PLAYLIST_ROOT%\\Elei\u00e7\u00f5es", "ELEI", 24),
        ("Acervo Antigo", "M", "%PLAYLIST_ROOT%\\Acervo Antigo", "ANTIGO", 25),
    ]
    xml = ['<?xml version="1.0" encoding="utf-8"?>', "<Folders>",
           "  <Folders>%d</Folders>" % len(folders), "  <Version>1.2</Version>",
           "  <Shared>", "    <Folders>1</Folders>", "    <Server>ESTUDIO</Server>",
           "    <Folder0>", "      <Name>Playlist</Name>", "      <Path>%PLAYLIST_ROOT%</Path>", "    </Folder0>",
           "  </Shared>"]
    for i, (title, ftype, target, code, fid) in enumerate(folders):
        args = ftype
        if title == "Streaming":
            args = "C URL https://stream.exemplo.local/radio"
        xml += [
            "  <Folder%d>" % i,
            "    <ID>%d</ID>" % fid,
            "    <Title>%s</Title>" % title,
            "    <Type>%s</Type>" % ftype,
            "    <Target>%s</Target>" % target,
            "    <IconLocation>%%PLAYLIST_ROOT%%\\pgm\\Icones\\Icones5.dll</IconLocation>",
            "    <ShortcutArguments>%s</ShortcutArguments>" % args,
            "    <ShortcutPathName>%%PLAYLIST_ROOT%%\\pgm\\Atalhos\\%s.lnk</ShortcutPathName>" % title,
            "    <IconIndex>0</IconIndex>",
            "    <Output>-1</Output>",
            "    <TotalFiles>0</TotalFiles>",
            "    <DBFId>%s</DBFId>" % code,
            "  </Folder%d>" % i,
        ]
    xml.append("</Folders>")
    write("Folders.xml", b"\xef\xbb\xbf" + crlf(xml).encode("utf-8"))
    for title, *_ in folders:
        if title != "Acervo Antigo":
            write("Atalhos/%s.lnk" % title, b"")

    # Media ----------------------------------------------------------------
    for rel in ["Musicas/Banda Azul - Cora\u00e7\u00e3o.mp3", "Musicas/Trio Sol - Estrada.mp3",
                "Comerciais/Padaria Pao Quente.mp3", "Comerciais/Loja Centro.mp3",
                "Comerciais/Oficina.mp3", "Comerciais/Oficina B.mp3", "Comerciais/spot_padaria.aac",
                "Vinhetas/VH Passagem.mp3", "Hora Certa/1200.mp3",
                "Elei\u00e7\u00f5es/Candidato 10.mp3"]:
        write(rel, b"\0", root=ROOT)

    # LIGACAO.DBF ------------------------------------------------------------
    lig_fields = [("CODIGO", "C", 12, 0), ("CTA", "C", 10, 0), ("SEQMEDIA", "C", 1, 0), ("PROXIMO", "N", 2, 0),
                  ("ARQUIVO", "C", 250, 0), ("SHORTNAME", "C", 12, 0), ("DURACAO", "N", 8, 2), ("TIPO", "C", 1, 0),
                  ("DATAREG", "D", 8, 0), ("HORAREG", "C", 5, 0), ("DATAINI", "D", 8, 0), ("HORAINI", "C", 5, 0),
                  ("DATAFIM", "D", 8, 0), ("HORAFIM", "C", 5, 0), ("TEXTO", "C", 150, 0), ("FLAGS", "C", 10, 0)]

    def reg(code, file, tipo, ini="", fim=""):
        return (False, [code, "", "", "", file, "", "", tipo, "20260901", "10:00", ini, "", fim, "", "", ""])

    records = [
        reg("MUSIC", "Musicas.lnk", "A"), reg("COM", "Comerciais.lnk", "A"), reg("VH", "Vinhetas.lnk", "A"),
        reg("HC", "Hora Certa.lnk", "A"), reg("PAUSA", "Pausa.lnk", "A"), reg("STREAM", "Streaming.lnk", "A"),
        reg("ELEI", "Elei\u00e7\u00f5es.lnk", "A"), reg("ANTIGO", "Acervo Antigo.lnk", "A"),
        reg("55", "Padaria Pao Quente.mp3", "C"),
        reg("23", "Loja Centro.mp3", "C", "20260101", "20260131"),
        reg("62", "Farmacia.mp3", "C"),
        reg("12", "Oficina.mp3", "C"), reg("12", "Oficina B.mp3", "C"),
        (True, ["77", "", "", "", "Removido.mp3", "", "", "C", "20260901", "10:00", "", "", "", "", "", ""]),
    ]
    write("Dados/LIGACAO.DBF", dbf(lig_fields, records))

    comp_fields = [("CODIGO", "C", 12, 0), ("DATA", "D", 8, 0), ("BLOCO", "C", 5, 0), ("TIPOBLOCO", "C", 1, 0),
                   ("DATAVEIC", "D", 8, 0), ("HORAINI", "C", 8, 0), ("HORAFIM", "C", 8, 0), ("DURACAO", "C", 8, 0),
                   ("PASTA", "C", 50, 0), ("ARQUIVO", "C", 100, 0), ("OPERADOR", "C", 15, 0)]
    write("Dados/COMPROVE.DBF", dbf(comp_fields, [
        (False, ["55", "20260930", "10:00", "C", "20260930", "10:00:05", "10:00:35", "00:00:30", "Comerciais", "Padaria Pao Quente", "Ana"]),
        (False, ["", "20260930", "10:02", "M", "20260930", "10:02:00", "10:05:10", "00:03:10", "Musicas", "Trio Sol - Estrada", "Ana"]),
    ]))

    # Indexes consistent with the tables, except LIGA_ARQ.NTX, left stale on
    # purpose (as after a registration made while the index was not updated).
    def pad(v, n):
        return str(v).encode("cp1252").ljust(n)[:n]
    lig = [(i + 1, v) for i, (deleted, v) in enumerate(records)]
    cod = sorted((pad(v[0], 12), r) for r, v in lig)
    write("Indices/LIGA_COD.NTX", ntx("CODIGO", 12, keys=cod))
    write("Indices/LIGA_ARQ.NTX", ntx("UPPER(ARQUIVO)", 250, version=1, pages=2))
    comp = [(1, ["55", "20260930", "10:00", "Padaria Pao Quente", "10:00:35"]),
            (2, ["", "20260930", "10:02", "Trio Sol - Estrada", "10:05:10"])]
    keys_c = sorted((pad(c[0], 12) + c[1].encode() + pad(c[2], 5), r) for r, c in comp)
    keys_a = sorted((upper1252(pad(c[3], 100)) + descend(c[1].encode()) + descend(pad(c[4], 8)), r) for r, c in comp)
    write("Indices/COMPROVE-C.NTX", ntx("CODIGO+DTOS(DATA)+BLOCO", 25, keys=keys_c))
    write("Indices/COMPROVE-A.NTX", ntx("UPPER(ARQUIVO)+DESCEND(DTOS(DATA))+DESCEND(HORAFIM)", 116, version=2, keys=keys_a))

    # Maps and grades -------------------------------------------------------
    planner = []
    for h in range(24):
        for m in (0, 20, 40):
            t = "%02d:%02d" % (h, m)
            if 12 <= h < 13:
                planner.append(t + " (DUR=300, ID=Programa Meio-dia) ")
            elif h == 10 and m == 0:
                planner.append(t + ' (DUR=300) "A1B2C3D4E5F6|spot_padaria.aac", "Q1W2E3R4T5Y6|nao_baixado.mp3", ')
            else:
                planner.append(t + " (DUR=300) ")
    write("Mapas/01-10-2026.txt", crlf(planner).encode("ascii"))

    write("Mapas/02-10-2026.txt", crlf([
        "06:00 VH, 55, 23, VH, 62, 12, HC",
        "06:30 VH, 99, 55",
        "06:15 VH, 12",
        "07:00 VH, 55, MUSIC, COM",
        "07:30",
    ]).encode("ascii"))
    write("Mapas/Mapa.txt", crlf(["%02d:%02d " % (h, m) for h in range(24) for m in (0, 30)]).encode("ascii"))
    write("Mapas/Relogio.txt", crlf(["06:00 (FIXO)", "06:30 (SAT)", "07:00 (DUR=3:00)", "07:30 (ID=OUVINTE)"]).encode("ascii"))

    grade = []
    for h in range(24):
        for m in (2, 32):
            grade.append("%02d:%02d \"Banda Azul - Cora\u00e7\u00e3o.mp3\", \"Trio Sol - Estrada.mp3\", " % (h, m))
    grade[5] = '02:32 "Banda Azul - Cora\u00e7\u00e3o.mp3", "Musica Inexistente.mp3", '
    write("Grades/01-10-2026.txt", crlf(grade).encode("cp1252"))
    write("Grades/Relogio.txt", crlf(["%02d:02 " % h for h in range(24)]).encode("ascii"))

    # Montagem / merge --------------------------------------------------------
    write("Montagem/01-10-2026 - POLITICO.merge", crlf([
        '13:20 C, 0, "Eleicoes", "Candidato 10.mp3"',
        '14:00 C, 2, "Eleicoes", "Candidato 10.mp3"',
    ]).encode("ascii"))
    write("Montagem/30-09-2026.TXT", crlf([
        '10:00 C, 0, "Comerciais", "Padaria Pao Quente.mp3"',
        '10:02 M, 0, "Musicas", "Trio Sol - Estrada.mp3"',
        '10:02 M, -1, "Musicas", "Banda Azul - Cora\u00e7\u00e3o.mp3"',
    ]).encode("cp1252"))

    # Operators ---------------------------------------------------------------
    def operator(op_id, tipo, admin, password, values):
        lines = ['<?xml version="1.0" encoding="UTF-8"?>', "<Operador>",
                 "\t<TipoOperador>%d</TipoOperador>" % tipo, "\t<Id>%s</Id>" % op_id,
                 "\t<OperadorRemovido>0</OperadorRemovido>", "\t<Acesso>", "\t\t<Nome>%s</Nome>" % op_id]
        lines += ["\t\t<Senha>%s</Senha>" % password] if password else ["\t\t<Senha>", "\t\t</Senha>"]
        lines += ["\t\t<Admin>%d</Admin>" % admin, "\t</Acesso>", "\t<Geral>"]
        lines += ["\t\t<%s>%s</%s>" % (k, v, k) for k, v in values]
        lines += ["\t</Geral>", "\t<Pastas>", "\t\t<Musicas>1</Musicas>", "\t\t<Comerciais>0</Comerciais>",
                  "\t</Pastas>", "</Operador>"]
        return crlf(lines).encode("utf-8")

    write("Operadores/Padr\u00e3o/Config.xml", operator("Padr\u00e3o", 1, 0, None,
                                                        [("InsAdd", "1"), ("InsDel", "1"), ("bLockPanes", "0")]))
    write("Operadores/Ana/Config.xml", operator("Ana", 0, 1, "x",
                                              [("InsAdd", "-1"), ("InsDel", "0"), ("bLockPanes", "-1")]))

    # Event log -----------------------------------------------------------------
    write("Eventos/2026-09-30.log", "\r\n".join([
        "2026-09-30 10:00:00.000\t(1A2B)\tESTUDIO        \tAna            \tCarregando C:\\Playlist\\pgm\\Playlist.exe Vers\u00e3o 5.0.9.09",
        "2026-09-30 10:00:01.000\t(1A2B)\tESTUDIO        \tAna            \tMonitorando a pasta C:\\Playlist\\pgm\\MAPAS",
        "2026-09-30 10:00:02.000\t(1A2B)\tESTUDIO        \tAna            \tMerge C:\\Playlist\\pgm\\MONTAGEM\\01-10-2026 - POLITICO.merge: linha 1 inv\u00e1lida. As inser\u00e7\u00f5es de merge anteriores foram mantidas.",
        "",
    ]).encode("cp1252"))

    print("fixtures written to", os.path.abspath(ROOT))


if __name__ == "__main__":
    main()
