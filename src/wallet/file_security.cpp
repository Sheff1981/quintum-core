#include "wallet/file_security.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <aclapi.h>

#include <vector>
#else
#include <sys/stat.h>
#endif

namespace quintum::wallet {

bool restrict_file_permissions(
    const std::filesystem::path& path) noexcept
{
#ifdef _WIN32
    HANDLE token{nullptr};

    if (!OpenProcessToken(
            GetCurrentProcess(),
            TOKEN_QUERY,
            &token)) {
        return false;
    }

    DWORD required{0U};

    (void)GetTokenInformation(
        token,
        TokenUser,
        nullptr,
        0U,
        &required
    );

    if (required == 0U ||
        GetLastError() !=
            ERROR_INSUFFICIENT_BUFFER) {
        CloseHandle(token);
        return false;
    }

    std::vector<unsigned char> buffer;

    try {
        buffer.resize(required);
    } catch (...) {
        CloseHandle(token);
        return false;
    }

    if (!GetTokenInformation(
            token,
            TokenUser,
            buffer.data(),
            required,
            &required)) {
        CloseHandle(token);
        return false;
    }

    const auto* token_user =
        reinterpret_cast<const TOKEN_USER*>(
            buffer.data()
        );

    EXPLICIT_ACCESSW access{};
    access.grfAccessPermissions =
        FILE_ALL_ACCESS;
    access.grfAccessMode = SET_ACCESS;
    access.grfInheritance =
        NO_INHERITANCE;
    access.Trustee.TrusteeForm =
        TRUSTEE_IS_SID;
    access.Trustee.TrusteeType =
        TRUSTEE_IS_USER;
    access.Trustee.ptstrName =
        static_cast<LPWSTR>(
            token_user->User.Sid
        );

    PACL acl{nullptr};

    const DWORD acl_result =
        SetEntriesInAclW(
            1U,
            &access,
            nullptr,
            &acl
        );

    if (acl_result != ERROR_SUCCESS ||
        acl == nullptr) {
        CloseHandle(token);

        if (acl != nullptr) {
            LocalFree(acl);
        }

        return false;
    }

    std::wstring writable_path =
        path.native();

    const DWORD security_result =
        SetNamedSecurityInfoW(
            writable_path.data(),
            SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION |
                PROTECTED_DACL_SECURITY_INFORMATION,
            nullptr,
            nullptr,
            acl,
            nullptr
        );

    LocalFree(acl);
    CloseHandle(token);

    return security_result ==
           ERROR_SUCCESS;
#else
    return ::chmod(
               path.c_str(),
               S_IRUSR | S_IWUSR) == 0;
#endif
}

} // namespace quintum::wallet
