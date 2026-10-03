#include "wallet/mnemonic.hpp"

#include "crypto/random.hpp"
#include "crypto/sha256.hpp"
#include "wallet/bip39_english.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace quintum::wallet {
namespace {

constexpr std::size_t kMnemonicWords{24U};
constexpr std::size_t kBitsPerWord{11U};
constexpr std::size_t kEntropyBytes{32U};
constexpr std::size_t kChecksumBytes{1U};
constexpr std::size_t kPackedBytes{
    kEntropyBytes + kChecksumBytes
};

bool ascii_space(char ch) noexcept
{
    return ch == ' ' ||
           ch == '\t' ||
           ch == '\r' ||
           ch == '\n';
}

std::vector<std::string_view> split_words(
    std::string_view phrase)
{
    std::vector<std::string_view> words;
    words.reserve(kMnemonicWords);

    std::size_t offset{0U};

    while (offset < phrase.size()) {
        while (offset < phrase.size() &&
               ascii_space(phrase[offset])) {
            ++offset;
        }

        if (offset == phrase.size()) {
            break;
        }

        const std::size_t start = offset;

        while (offset < phrase.size() &&
               !ascii_space(phrase[offset])) {
            ++offset;
        }

        words.push_back(
            phrase.substr(
                start,
                offset - start
            )
        );
    }

    return words;
}

std::optional<std::uint16_t> word_index(
    std::string_view word) noexcept
{
    const auto it = std::lower_bound(
        detail::kBip39EnglishWords.begin(),
        detail::kBip39EnglishWords.end(),
        word
    );

    if (it == detail::kBip39EnglishWords.end() ||
        *it != word) {
        return std::nullopt;
    }

    return static_cast<std::uint16_t>(
        std::distance(
            detail::kBip39EnglishWords.begin(),
            it
        )
    );
}

std::uint16_t extract_index(
    const std::array<Byte, kPackedBytes>& packed,
    std::size_t word) noexcept
{
    std::uint16_t index{0U};
    const std::size_t first_bit =
        word * kBitsPerWord;

    for (std::size_t bit = 0U;
         bit < kBitsPerWord;
         ++bit) {
        const std::size_t bit_index =
            first_bit + bit;
        const std::size_t byte_index =
            bit_index / 8U;
        const std::size_t bit_in_byte =
            7U - (bit_index % 8U);

        index = static_cast<std::uint16_t>(
            (index << 1U) |
            ((packed[byte_index] >>
              bit_in_byte) & 1U)
        );
    }

    return index;
}

void append_index_bits(
    std::array<Byte, kPackedBytes>& packed,
    std::size_t word,
    std::uint16_t index) noexcept
{
    const std::size_t first_bit =
        word * kBitsPerWord;

    for (std::size_t bit = 0U;
         bit < kBitsPerWord;
         ++bit) {
        const std::size_t bit_index =
            first_bit + bit;
        const std::size_t byte_index =
            bit_index / 8U;
        const std::size_t bit_in_byte =
            7U - (bit_index % 8U);
        const std::uint16_t source_shift =
            static_cast<std::uint16_t>(
                10U - bit
            );
        const Byte value =
            static_cast<Byte>(
                (index >> source_shift) &
                1U
            );

        packed[byte_index] |=
            static_cast<Byte>(
                value << bit_in_byte
            );
    }
}

} // namespace

MnemonicEncodeResult encode_recovery_mnemonic(
    const RecoverySeed& seed)
{
    MnemonicEncodeResult out;

    std::array<Byte, kPackedBytes> packed{};
    std::copy(
        seed.begin(),
        seed.end(),
        packed.begin()
    );

    const auto checksum =
        crypto::sha256(
            std::span<const Byte>{
                seed.data(),
                seed.size()
            }
        );

    packed.back() = checksum.front();

    for (std::size_t word = 0U;
         word < kMnemonicWords;
         ++word) {
        const std::uint16_t index =
            extract_index(
                packed,
                word
            );

        if (!out.words.empty()) {
            out.words.push_back(' ');
        }

        out.words.append(
            detail::kBip39EnglishWords[index]
        );
    }

    crypto::secure_erase(packed);
    return out;
}

MnemonicDecodeResult decode_recovery_mnemonic(
    std::string_view phrase)
{
    MnemonicDecodeResult out;

    const auto words =
        split_words(phrase);

    if (words.size() != kMnemonicWords) {
        out.error =
            MnemonicError::wrong_word_count;
        return out;
    }

    std::array<Byte, kPackedBytes> packed{};

    for (std::size_t word = 0U;
         word < words.size();
         ++word) {
        const auto index =
            word_index(words[word]);

        if (!index) {
            crypto::secure_erase(packed);
            out.error =
                MnemonicError::unknown_word;
            return out;
        }

        append_index_bits(
            packed,
            word,
            *index
        );
    }

    std::copy_n(
        packed.begin(),
        static_cast<std::ptrdiff_t>(
            out.seed.size()
        ),
        out.seed.begin()
    );

    const auto checksum =
        crypto::sha256(
            std::span<const Byte>{
                out.seed.data(),
                out.seed.size()
            }
        );

    if (packed.back() !=
        checksum.front()) {
        crypto::secure_erase(packed);
        crypto::secure_erase(out.seed);
        out.error =
            MnemonicError::invalid_checksum;
        return out;
    }

    crypto::secure_erase(packed);
    return out;
}

} // namespace quintum::wallet
