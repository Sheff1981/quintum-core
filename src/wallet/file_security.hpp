#pragma once

#include <filesystem>

namespace quintum::wallet {

[[nodiscard]] bool restrict_file_permissions(
    const std::filesystem::path& path
) noexcept;

} // namespace quintum::wallet
