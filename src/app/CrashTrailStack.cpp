#include "CrashTrailInternal.h"

#include <cstdio>
#include <cstring>

#include <windows.h>

using namespace CrashTrailDetail;

namespace CrashTrailDetail
{
static bool MemReadable(const void* p, size_t n)
{
	if (!p || n == 0)
		return false;
	MEMORY_BASIC_INFORMATION mbi{};
	const auto* bytes = static_cast<const BYTE*>(p);
	size_t left = n;
	while (left > 0)
	{
		if (VirtualQuery(bytes, &mbi, sizeof(mbi)) == 0)
			return false;
		if (mbi.State != MEM_COMMIT)
			return false;
		const DWORD prot = mbi.Protect & 0xffu;
		if (prot == PAGE_NOACCESS || prot == PAGE_EXECUTE ||
			(mbi.Protect & PAGE_GUARD))
			return false;
		const auto regionEnd = reinterpret_cast<const BYTE*>(mbi.BaseAddress) + mbi.RegionSize;
		const size_t chunk = static_cast<size_t>(regionEnd - bytes);
		const size_t step = chunk < left ? chunk : left;
		bytes += step;
		left -= step;
	}
	return true;
}

void WriteStack(FILE* f, EXCEPTION_POINTERS* ep)
{
	if (!f)
		return;

#if defined(_M_X64) || defined(__x86_64__)
	/* Primary: unwind from the faulting CONTEXT — CaptureStackBackTrace from a
	   vectored handler only shows WriteKnownModules / WriteCrashSnapshot. */
	if (ep && ep->ContextRecord)
	{
		CONTEXT ctx = *ep->ContextRecord;
		std::fprintf(f, "fault_stack\n");
		unsigned n = 0;
		DWORD64 prevRip = 0;
		for (; n < static_cast<unsigned>(kStackFrames); ++n)
		{
			if (ctx.Rip == 0 || ctx.Rip == prevRip)
				break;
			prevRip = ctx.Rip;
			char label[32];
			std::snprintf(label, sizeof(label), "  #%u", n);
			DescribeModuleAt(f, label, reinterpret_cast<const void*>(ctx.Rip));

			DWORD64 imageBase = 0;
			PRUNTIME_FUNCTION rf = RtlLookupFunctionEntry(ctx.Rip, &imageBase, nullptr);
			if (rf)
			{
				PVOID handlerData = nullptr;
				DWORD64 establisher = 0;
				RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, ctx.Rip, rf, &ctx,
					&handlerData, &establisher, nullptr);
				continue;
			}
			/* Leaf / no pdata: return address at [Rsp]. */
			if (!MemReadable(reinterpret_cast<const void*>(ctx.Rsp), sizeof(DWORD64)))
				break;
			const DWORD64 ret = *reinterpret_cast<const DWORD64*>(ctx.Rsp);
			if (ret == 0 || ret == ctx.Rip)
				break;
			ctx.Rip = ret;
			ctx.Rsp += sizeof(DWORD64);
		}
		if (n == 0)
			std::fprintf(f, "  (empty)\n");

		/* Raw return-address scan from fault RSP — helps when unwind stalls. */
		std::fprintf(f, "fault_stack_scan\n");
		unsigned scanned = 0;
		if (MemReadable(reinterpret_cast<const void*>(ep->ContextRecord->Rsp), sizeof(DWORD64)))
		{
			const auto* slot = reinterpret_cast<const DWORD64*>(ep->ContextRecord->Rsp);
			for (unsigned i = 0; i < 64 && scanned < 16; ++i)
			{
				if (!MemReadable(slot + i, sizeof(DWORD64)))
					break;
				const DWORD64 addr = slot[i];
				if (addr < 0x10000ull)
					continue;
				HMODULE mod = nullptr;
				if (!GetModuleHandleExW(
						GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
							GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
						reinterpret_cast<LPCWSTR>(addr),
						&mod) ||
					!mod)
					continue;
				char label[32];
				std::snprintf(label, sizeof(label), "  slot+%u", i);
				DescribeModuleAt(f, label, reinterpret_cast<const void*>(addr));
				++scanned;
			}
		}
		if (scanned == 0)
			std::fprintf(f, "  (none)\n");
	}
	else
	{
		std::fprintf(f, "fault_stack=(no context)\n");
	}
#else
	(void)ep;
	std::fprintf(f, "fault_stack=(unsupported arch)\n");
#endif

	void* frames[kStackFrames]{};
	const USHORT n = CaptureStackBackTrace(0, kStackFrames, frames, nullptr);
	std::fprintf(f, "handler_stack frames=%u\n", static_cast<unsigned>(n));
	for (USHORT i = 0; i < n; ++i)
	{
		char label[32];
		std::snprintf(label, sizeof(label), "  #%u", static_cast<unsigned>(i));
		DescribeModuleAt(f, label, frames[i]);
	}
}

}
