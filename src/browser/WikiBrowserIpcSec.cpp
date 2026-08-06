#include "WikiBrowserShared.h"

#include <cstring>
#include <vector>

#include <windows.h>
#include <aclapi.h>

namespace WikiBrowserDetail
{
	namespace
	{
		/* RAII: SECURITY_ATTRIBUTES whose DACL grants GENERIC_ALL only to the
		   current process user. Same-user foreign processes no longer get the
		   default "Everyone / world" Local\ object ACL. Helper is same-user so
		   OpenFileMapping / OpenEvent keep working. Falls back to nullptr SA
		   (legacy default) if ACL APIs fail — required for some Wine builds. */
		struct UserOnlySa
		{
			SECURITY_ATTRIBUTES sa{};
			PSECURITY_DESCRIPTOR psd = nullptr;
			PACL pacl = nullptr;
			std::vector<uint8_t> tokenInfo;
			bool ready = false;

			UserOnlySa()
			{
				HANDLE token = nullptr;
				if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
					return;

				DWORD need = 0;
				GetTokenInformation(token, TokenUser, nullptr, 0, &need);
				if (need == 0)
				{
					CloseHandle(token);
					return;
				}
				tokenInfo.resize(need);
				if (!GetTokenInformation(token, TokenUser, tokenInfo.data(), need, &need))
				{
					CloseHandle(token);
					tokenInfo.clear();
					return;
				}
				CloseHandle(token);

				auto* user = reinterpret_cast<TOKEN_USER*>(tokenInfo.data());
				if (!user || !user->User.Sid || !IsValidSid(user->User.Sid))
					return;

				EXPLICIT_ACCESSW ea{};
				ea.grfAccessPermissions = GENERIC_ALL;
				ea.grfAccessMode = SET_ACCESS;
				ea.grfInheritance = NO_INHERITANCE;
				ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
				ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
				ea.Trustee.ptstrName = reinterpret_cast<LPWSTR>(user->User.Sid);

				if (SetEntriesInAclW(1, &ea, nullptr, &pacl) != ERROR_SUCCESS || !pacl)
				{
					pacl = nullptr;
					return;
				}

				psd = static_cast<PSECURITY_DESCRIPTOR>(
					LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH));
				if (!psd)
				{
					LocalFree(pacl);
					pacl = nullptr;
					return;
				}
				if (!InitializeSecurityDescriptor(psd, SECURITY_DESCRIPTOR_REVISION) ||
					!SetSecurityDescriptorDacl(psd, TRUE, pacl, FALSE))
				{
					LocalFree(psd);
					LocalFree(pacl);
					psd = nullptr;
					pacl = nullptr;
					return;
				}

				sa.nLength = sizeof(sa);
				sa.lpSecurityDescriptor = psd;
				sa.bInheritHandle = FALSE;
				ready = true;
			}

			~UserOnlySa()
			{
				if (pacl)
					LocalFree(pacl);
				if (psd)
					LocalFree(psd);
			}

			UserOnlySa(const UserOnlySa&) = delete;
			UserOnlySa& operator=(const UserOnlySa&) = delete;

			SECURITY_ATTRIBUTES* Ptr() { return ready ? &sa : nullptr; }
		};
	}

	SECURITY_ATTRIBUTES* UserOnlyIpcSecurityAttributes()
	{
		/* One process-wide SA — IPC objects are created once per session. */
		static UserOnlySa sSa;
		return sSa.Ptr();
	}
}
