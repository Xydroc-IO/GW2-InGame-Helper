# Security policy — GW2 In-Game Helper

## Supported versions

| Channel | Branch | Support |
|---------|--------|---------|
| Shipping | `master` | Current release on GitHub Releases |
| Beta | `GW2-InGame-Helper-Beta` | Experimental; may change without notice |

Only the latest published DLL on each channel is supported for security fixes.

## What this project is allowed to do

Normative boundaries: [`docs/COMPLIANCE.md`](docs/COMPLIANCE.md).

In short: Nexus APIs only, private CEF under the addon data folder, local IPC to our helper process, read-only official APIs / public killproof.me. **No** game memory R/W, Present hooks, or writes into `bin64/cef`.

## Reporting a vulnerability

Please **do not** open a public GitHub issue for exploitable vulnerabilities.

1. Email the maintainer via the address on the GitHub profile for [Xydroc-IO/GW2-InGame-Helper](https://github.com/Xydroc-IO/GW2-InGame-Helper), **or**
2. Use GitHub **Security Advisories** (private report) on that repository if available.

Include: affected version/stamp, environment (Windows / Proton), reproduction steps, and impact. Allow reasonable time for a fix before public disclosure.

## Secrets and keys

- ArenaNet API keys and tokens belong only in local `settings.ini` under the addon data folder — never commit them.
- Do not paste keys into issues, PR descriptions, or docs screenshots.
- CI must not require live API keys.

## Supply chain notes

- Private CEF zip is SHA-256 verified at install (`src/CefRuntime.h`).
- Elite Insights CLI zip is similarly verified when used.
- Prefer reviewing dependency / CEF major bumps carefully; treat helper/IPC changes as high risk.
