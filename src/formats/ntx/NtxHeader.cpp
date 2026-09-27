#include "formats/ntx/NtxHeader.h"

namespace pc
{

NtxHeader NtxHeader::parse (const juce::MemoryBlock& bytes)
{
    NtxHeader h;
    auto* p = static_cast<const juce::uint8*> (bytes.getData());
    h.fileSize = (juce::int64) bytes.getSize();
    if (bytes.getSize() < 1024)
    {
        h.problem = L"Arquivo menor que o cabeçalho de 1024 bytes.";
        return h;
    }

    h.signature = juce::ByteOrder::littleEndianShort (p);
    h.version = juce::ByteOrder::littleEndianShort (p + 2);
    h.rootPage = juce::ByteOrder::littleEndianInt (p + 4);
    h.nextFreePage = juce::ByteOrder::littleEndianInt (p + 8);
    h.itemSize = juce::ByteOrder::littleEndianShort (p + 12);
    h.keySize = juce::ByteOrder::littleEndianShort (p + 14);
    h.keyDecimals = juce::ByteOrder::littleEndianShort (p + 16);
    h.maxItems = juce::ByteOrder::littleEndianShort (p + 18);
    h.halfPage = juce::ByteOrder::littleEndianShort (p + 20);
    h.keyExpression = juce::String (reinterpret_cast<const char*> (p + 22), (size_t) strnlen (reinterpret_cast<const char*> (p + 22), 256));
    h.unique = p[278] != 0;
    h.pages = (int) ((bytes.getSize() - 1024) / 1024);

    if (h.signature != 6 && h.signature != 7)
        h.problem = "Assinatura NTX inesperada (" + juce::String (h.signature) + ").";
    else if (bytes.getSize() % 1024 != 0)
        h.problem = L"Tamanho do arquivo não é múltiplo de 1024 bytes.";
    else if (h.keySize <= 0 || h.itemSize != h.keySize + 8)
        h.problem = "Tamanho de chave inconsistente.";
    else if (h.rootPage % 1024 != 0 || h.rootPage >= (juce::uint32) bytes.getSize())
        h.problem = L"Página raiz fora do arquivo.";
    else if (h.keyExpression.isEmpty())
        h.problem = L"Expressão de chave vazia.";
    else
        h.valid = true;
    return h;
}

} // namespace pc
