#include "Core/UUID/UUID.h"

#include <random>

namespace Janus
{
namespace
{

[[nodiscard]] int HexValue(char value) noexcept
{
    if (value >= '0' && value <= '9')
    {
        return value - '0';
    }

    if (value >= 'a' && value <= 'f')
    {
        return value - 'a' + 10;
    }

    if (value >= 'A' && value <= 'F')
    {
        return value - 'A' + 10;
    }

    return -1;
}

} // namespace

UUID::UUID(Storage bytes) noexcept
    : m_Bytes(bytes)
{
}

UUID UUID::Random()
{
    static thread_local std::mt19937_64 generator([]
        {
            std::random_device device;
            std::seed_seq seed{
                device(), device(), device(), device(),
                device(), device(), device(), device()};
            return std::mt19937_64(seed);
        }());

    std::uniform_int_distribution<unsigned int> distribution(0, 255);

    Storage bytes{};
    for (auto& byte : bytes)
    {
        byte = static_cast<u8>(distribution(generator));
    }

    bytes[6] = static_cast<u8>((bytes[6] & 0x0Fu) | 0x40u);
    bytes[8] = static_cast<u8>((bytes[8] & 0x3Fu) | 0x80u);

    return UUID(bytes);
}

Result<UUID> UUID::Parse(std::string_view text)
{
    constexpr usize CanonicalLength = 36;
    if (text.size() != CanonicalLength
        || text[8] != '-'
        || text[13] != '-'
        || text[18] != '-'
        || text[23] != '-')
    {
        return Result<UUID>::Failure(
            ErrorCode::InvalidArgument,
            "UUID must use canonical 8-4-4-4-12 hexadecimal format.");
    }

    Storage bytes{};
    usize byteIndex = 0;

    for (usize index = 0; index < text.size();)
    {
        if (text[index] == '-')
        {
            ++index;
            continue;
        }

        if (index + 1 >= text.size() || byteIndex >= bytes.size())
        {
            return Result<UUID>::Failure(
                ErrorCode::InvalidArgument,
                "UUID contains an invalid hexadecimal sequence.");
        }

        const int high = HexValue(text[index]);
        const int low = HexValue(text[index + 1]);
        if (high < 0 || low < 0)
        {
            return Result<UUID>::Failure(
                ErrorCode::InvalidArgument,
                "UUID contains a non-hexadecimal character.");
        }

        bytes[byteIndex] =
            static_cast<u8>((static_cast<unsigned int>(high) << 4u)
                            | static_cast<unsigned int>(low));
        ++byteIndex;
        index += 2;
    }

    if (byteIndex != bytes.size())
    {
        return Result<UUID>::Failure(
            ErrorCode::InvalidArgument,
            "UUID contains an invalid number of hexadecimal digits.");
    }

    return Result<UUID>::Success(UUID(bytes));
}

bool UUID::IsValid() const noexcept
{
    for (const u8 byte : m_Bytes)
    {
        if (byte != 0)
        {
            return true;
        }
    }

    return false;
}

std::string UUID::ToString() const
{
    constexpr char Digits[] = "0123456789abcdef";

    std::string text;
    text.reserve(36);

    for (usize index = 0; index < m_Bytes.size(); ++index)
    {
        if (index == 4 || index == 6 || index == 8 || index == 10)
        {
            text.push_back('-');
        }

        const u8 byte = m_Bytes[index];
        text.push_back(Digits[(byte >> 4u) & 0x0Fu]);
        text.push_back(Digits[byte & 0x0Fu]);
    }

    return text;
}

const UUID::Storage& UUID::GetBytes() const noexcept
{
    return m_Bytes;
}

usize UUIDHash::operator()(const UUID& uuid) const noexcept
{
    constexpr u64 OffsetBasis = 14695981039346656037ull;
    constexpr u64 Prime = 1099511628211ull;

    u64 hash = OffsetBasis;
    for (const u8 byte : uuid.GetBytes())
    {
        hash ^= static_cast<u64>(byte);
        hash *= Prime;
    }

    return static_cast<usize>(hash);
}

} // namespace Janus
