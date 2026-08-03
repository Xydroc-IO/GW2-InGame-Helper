# Guild Wars 2 Third-Party Addon Ecosystem: Multi-Frame Ranking, Must-Have Stacks, Repository-Derived Cognitive-Engineering Profiles, and Speculative CHC Proxies

| Field | Value |
|-------|-------|
| Document type | Technical report (academic style; not peer-reviewed) |
| Research date | 3 August 2026 |
| Scope | Public third-party overlays, loaders, meters, pathing tools, companion parsers used with Guild Wars 2 |
| Affiliation | Independent synthesis packaged with GW2 In-Game Helper documentation |
| Peer review | None |
| License of this report | Informational; cite primary sources individually |

**Keywords:** Guild Wars 2; third-party addons; ranking bias; age-normalized evaluation; cognitive-engineering profiles; CHC proxies; ArcDPS; Blish HUD; Raidcore Nexus; GW2 In-Game Helper.

---

## Abstract

Community “top addon” lists and single weighted composites that blend curated-list frequency, GitHub stars, and ecosystem centrality systematically disadvantage new entrants. List lag, star accumulation time, platform lock-in, and reviewer familiarity are structural biases. This report therefore **rejects a single composite score as a claim of overall quality or ranking fairness**. It presents three separate frames: (A) established maturity/adoption proxies; (B) category-relative capability comparison within shared jobs; (C) emerging tools (≤12 months) evaluated with age-aware engineering proxies. Must-have stacks are stratified by player archetype. Developer profiles are inferences from public repository artefacts only—not clinical psychology. A separate section presents **speculative, non-psychometric CHC-style ranges on an IQ-like scale**, lead by the explicit claim that **software repositories cannot measure IQ**. Under Frame A, **GW2 In-Game Helper is outside the established Top 10**. Under Frame B, it is a **capable peer in the in-game reference / browser / QoL-aggregator category**. Under Frame C, it ranks among the **most documentation-dense and feature-surface-rich new Nexus addons** in the candidate set, while remaining **optional / emerging** for must-have purposes.

---

## 1. Epistemic stance and ToS boundary

ArenaNet publishes a Third-Party Programs policy and does **not** provide official rankings or install counts. Use of third-party client modifications is unsupported; Support will not assist with issues caused by such tools; automation and unfair-advantage tools are prohibited. Tolerance of benign overlays in practice is not endorsement. All tools below are discussed as community artefacts at the account holder’s risk. Closed-source network/runtime hooks (ArcDPS, Unofficial Extras) carry additional supply-chain and policy surface.

This document is not affiliated with ArenaNet, NCSoft, deltaconnected, Raidcore, or Blish HUD maintainers except insofar as public artefacts are cited.

---

## 2. Why a single composite score is unfair to new addons

A prior draft used

\[
S = 0.25 L + 0.20 A + 0.25 E + 0.15 M + 0.15 U
\]

with \(L\) = curated-list frequency, \(A\) = star-scaled adoption proxy, \(E\) = ecosystem-platform role, \(M\) = maintenance recency, \(U\) = uniqueness. Under that index, ArcDPS scored ~98 and GW2 In-Game Helper ~32. That index is **informative about incumbency**, not engineering merit or player fit for recent tools.

### 2.1 Structural failure modes of list-frequency + raw stars

1. **List lag.** Consumer and creator lists (Dexerto, January 2025 update; Mukluk Labs; Convergence Corp) update slowly relative to greenfield Nexus modules. A tool created in July 2026 cannot appear on a January 2025 journalism list. Low \(L\) is chronological, not evaluative.
2. **Star accumulation time.** GitHub stars are a stock variable. Repos created 2017–2019 (GW2Radial, Addon Manager lineage, TacO-era attention) had years to accrue developer and streamer attention. Helper (created 2026-07-19, **1★** on the research date) is incomparable on raw \(A\).
3. **Ecosystem lock-in (\(E\)).** Platforms that other tools *require*—ArcDPS as meter/plugin host, Nexus as loader, Blish as module host—score high by definition. New feature addons cannot outrank platforms on \(E\) without becoming platforms: a category error if \(S\) is read as “quality.”
4. **Reviewer familiarity.** Guide authors recommend tools they already teach. New tools lack tutorial inventory and clip culture, depressing list and social signals irrespective of capability.
5. **Maintenance weight under-corrects.** Even with \(M = 100\), a 15% weight cannot offset near-zero \(L\) and \(A\). The composite mathematically buries new work while rewarding list inertia (e.g., Addon Manager’s high stars coexist with a last push of May 2024).
6. **Missing data asymmetry.** ArcDPS has no public GitHub repository; any star-based ranking either excludes the ecosystem’s most central meter or invents a proxy. Invented proxies then dominate \(A\), further entrenching incumbents.

**Conclusion:** Frame A below *keeps* maturity/adoption ranking but **labels it as such**. Frames B and C answer different questions. No single “Top 10 quality” list is asserted.

---

## 3. Data sources and metrics (research date 3 August 2026)

Candidates were drawn from Dexerto, Mukluk Labs, Convergence Corp, Hardstuck ArcDPS materials, Raidcore Nexus documentation, Blish HUD site materials, Linux community guides (2026), and GitHub project pages. The official forum compilation of external resources could not be retrieved (Cloudflare HTTP 403).

| Project | Repository / site | Stars | Created / first public | Last push (approx.) |
|---------|-------------------|------:|------------------------|---------------------|
| ArcDPS | deltaconnected.com/arcdps | — (no public GH) | long-standing | patch-driven (2026) |
| Blish HUD | blish-hud/Blish-HUD | 407 | 2019-01 | 2026-02-19 |
| Raidcore Nexus | RaidcoreGG/Nexus | 165 | 2021-11 | 2026-08-02 |
| GW2Radial | Friendly0Fire/GW2Radial | 369 | 2017-09 | 2025-11-16 |
| GW2 Addon Manager | gw2-addon-loader/GW2-Addon-Manager | 471 | 2019-08 | 2024-05-04 |
| GW2TacO | BoyC/GW2TacO | 255 | long-standing | 2025-12-01 |
| Elite Insights | baaron4/GW2-Elite-Insights-Parser | 153 | long-standing | 2026-08-03 |
| Burrito | AsherGlick/Burrito | 118 | — | 2025-10-13 |
| Unofficial Extras | Krappa322/arcdps_unofficial_extras_releases | ~68–69 | 2021-09 | 2026-07-19 |
| killproof.me plugin | knoxfighter/arcdps-killproof.me-plugin | 59 | 2020-12 | ~2026 |
| TaimiHUD | TaimiHUD/TaimiHUD | 49 | 2025-06-17 | 2026-07-27 |
| GW2Clarity | Friendly0Fire/GW2Clarity | 36 | 2022-04 | 2025-03-31 |
| Hoard & Seek | PieOrCake/hoard_and_seek | 5 | 2026-03-21 | 2026-07-06 |
| Alter Ego | PieOrCake/alter_ego | 3 | 2026-04-07 | 2026-07-01 |
| GW2 In-Game Helper | Xydroc-IO/GW2-InGame-Helper | 1 | 2026-07-19 | 2026-08-03 |

**Helper local engineering signals (workspace checkout, research date):** MIT-licensed Raidcore Nexus CEF browser / reference / QoL aggregator, version **2.2.1.0**; signature `HELP` / `0x48454C50`; private CEF 150 OSR (does not write game `bin64/cef`); ~45 975 lines across `src/` C++/headers; domain LOC approx. pathing 9 186, account 9 088, browse 6 432, logs 5 598, helper 4 813, browser 3 181; ≥19 markdown docs; WHITEPAPER alone ~4 481 words; README ~2 828 words; high commit density July–August 2026; 1 GitHub star.

**Boundary rules:** In-client hooks, overlays, and loaders are primary subjects. Elite Insights is included as a companion EVTC parser where Frame A/B require it. Pure websites, ReShade/Hook visual injectors, and GPU translation layers are out of Top-10 maturity tables unless noted as honorable mentions.

---

## 4. Frame A — Established maturity / adoption (not quality)

**Question answered:** Which tools currently dominate community recommendation and dependency graphs?

**Not answered:** Engineering quality of new tools; fitness for a given player; safety; “best addon.”

### Table A — Top 10 by maturity / adoption consensus

| Rank | Tool | Role | Adoption / list signals | Stars / proxy |
|-----:|------|------|-------------------------|---------------|
| 1 | ArcDPS | Combat meter, EVTC logs, plugin host | Near-universal in endgame guides | Consensus proxy (no GH) |
| 2 | Blish HUD | External overlay + module platform | Dexerto, Mukluk, Convergence | 407 |
| 3 | Raidcore Nexus | In-game loader / manager / API | Convergence; modern install guides | 165 |
| 4 | Elite Insights* | EVTC → HTML / dps.report companion | Implied by log workflow | 153 |
| 5 | GW2TacO | Marker/pathing format ancestor | Dexerto; Convergence | 255 |
| 6 | GW2 Addon Manager | Desktop installer (legacy) | Dexerto, Mukluk; **stale** (2024-05) | 471 |
| 7 | GW2Radial | Mount/utility radial | Mukluk; high stars | 369 |
| 8 | Unofficial Extras | ArcDPS extension data channel | Enables dependent plugins | ~68 |
| 9 | TaimiHUD | Nexus/ArcDPS pathing + timers | Convergence; Linux guides | 49 |
| 10 | Burrito | Native Linux tactical overlay | Convergence; Linux niche | 118 |

\*Companion tool, not an in-client overlay.

**Notes on Table A.** Addon Manager’s sixth place is partly **legacy adoption + list inertia**; maintenance is poor relative to Nexus. TacO’s uniqueness is declining as pathing migrates to Blish modules and TaimiHUD. Taimi’s ninth place already shows Frame A beginning to admit younger tools when list guides explicitly prefer them (Linux 2026).

### GW2 In-Game Helper under Frame A

Outside the Top 10. Not listed on Dexerto, Mukluk, Convergence, or Hardstuck as a pillar. Not featured as an ecosystem pillar on Raidcore’s Nexus marketing page in the materials reviewed. **1** GitHub star. Estimated band among named public addons: **far below incumbents on adoption stock metrics**. This is expected for a ≤1-month-old public repository and **must not be read as a quality verdict**.

---

## 5. Frame B — Category-relative capability peers

**Question answered:** Within a shared job, how do tools compare on *stated* capabilities and architectural fit?

Comparisons use public README/docs claims and observable architecture—not controlled user studies, not install counts.

### B1. Combat metrics and logging

| Tool | Capability notes | Relative placement |
|------|------------------|--------------------|
| ArcDPS | Live DPS/boons/CC; EVTC; plugin API | Category standard |
| Elite Insights | Offline/deep parse of EVTC | Required companion for analysis |
| Unofficial Extras | Squad/keybind/chat events for plugins | Infrastructure, not a meter |
| killproof.me plugin | Displays killproof.me credentials | Narrow specialist |

Helper participates only as a **consumer** (DPS Logs pad + optional Elite Insights CLI), not as a meter.

### B2. Overlay platforms and loaders

| Tool | Capability notes | Relative placement |
|------|------------------|--------------------|
| Nexus | Hot-load, library, API, chainload ArcDPS | Dominant *in-game* loader in 2026 practice |
| Blish HUD | Module repo, Gw2Sharp, MumbleLink, optional Arc pipe | Dominant *external* feature host |
| Addon Manager | Historical desktop installer | Declining maintenance |

### B3. Pathing / markers / timers

| Tool | Capability notes | Relative placement |
|------|------------------|--------------------|
| Blish Pathing (+ packs) | Mature module ecosystem; TacO-compatible packs | High adoption path |
| TaimiHUD | Rust; Nexus or ArcDPS; timers + commander markers; Linux-friendly | Strong modern Nexus-native option |
| GW2TacO | Format progenitor | Still relevant; uniqueness declining |
| Burrito | Linux-native overlay | Linux specialist |
| Helper Pathing | Curated packs + user `.taco`; compass; MumbleLink display-only | **Peer for Nexus users wanting pathing inside Helper**; does not displace Blish/Taimi as category leaders |

### B4. Input QoL (radial / clarity)

| Tool | Notes |
|------|-------|
| GW2Radial | Established radial; long star history |
| Nexus RadialMenus | Nexus-native alternative |
| GW2Clarity | Custom buff/cooldown grids; lower list frequency |

### B5. Account / inventory search (API)

| Tool | Notes |
|------|-------|
| Hoard & Seek | Multi-account search; cross-addon Nexus API |
| Blish Item Search modules | Established overlay path |
| Helper Account hub | Unlocks, inventory, wallet, vault, TP, crafting, progress via official API |

Category peers—not a single winner without user studies.

### B6. In-game reference / browser / QoL aggregator

| Tool | Architecture | Content surface | Relative placement |
|------|--------------|-----------------|--------------------|
| External desktop browser | Isolation; context-switch cost | Entire web | Default for many players |
| Blish modules (trackers, embeds, QoL) | Overlay modules; no full Chromium | Feature-specific | High adoption for discrete QoL |
| GW2 In-Game Helper | Nexus ImGui + **out-of-process CEF 150 OSR**; side pads | Curated catalog (wiki, builds, guides, Discord, cheat sheets) + Account/Events/Pathing/DPS Logs/Notes | **Category-capable peer**; unique among sampled tools for stock CEF-in-Nexus + wide catalog; adoption unproven |
| Older wiki-browser Nexus addons | Superseded per Helper README narrative | Narrower | Historical |

### Helper under Frame B

Within **B6**, Helper is a legitimate capability peer and, on public documentation, the most explicitly engineered **full Chromium-in-client** option in the Nexus sample. It is **not** the category leader by users. In B3/B5 it is a secondary option. In B1/B2 it is dependent, not competing. Honest peer set: in-game browser/wiki/build-site aggregators; Nexus QoL panels that surface web content; Blish modules that embed trackers; standalone sites opened externally.

---

## 6. Frame C — Emerging / new entrants (≤12 months)

**Inclusion:** Public creation or major public launch within approximately 12 months of the research date (approx. August 2025–August 2026): TaimiHUD (2025-06; borderline at ~13 months), Hoard & Seek (2026-03), Alter Ego (2026-04), GW2 In-Game Helper (2026-07), related PieOrCake stack.

**Age-aware proxies (ordinal; not a fake-precision composite):**

| Proxy | Meaning |
|-------|---------|
| Maintenance intensity | Commit/release density relative to age |
| Documentation density | Public words / architectural docs |
| Feature surface | Distinct user-facing subsystems |
| Integration depth | Nexus/ArcDPS API use; cross-addon APIs; compliance docs |
| Adoption stock | Stars/lists — reported but **down-weighted** for ranking within Frame C |

### Table C — Emerging tools (ordinal discussion)

| Tool | Age signal | Doc / eng. signals | Feature surface | Adoption stock | Frame C note |
|------|------------|--------------------|-----------------|----------------|--------------|
| TaimiHUD | ~13 mo (borderline) | Site + CONTRIBUTING; Rust toolchain | Pathing, timers, markers | 49★; list presence | Strongest *emerging pathing* adoption |
| Hoard & Seek | ~4.5 mo | API.md + integration docs | Account search + proxy API | 5★ | Strongest *cross-addon API* among new API tools |
| Alter Ego | ~4 mo | README; LLM disclosure | Characters/builds | 3★ | Depends on Hoard & Seek |
| GW2 In-Game Helper | ~2–3 wk public | WHITEPAPER, ARCHITECTURE, COMPLIANCE, KERNEL; very high doc corpus; ~46k LOC | Browser + Account + Pathing + DPS Logs + Events + Notes + cheat sheets | 1★; no list presence | Highest **doc density + feature surface** in sample; **unproven adoption** |

### Helper under Frame C

Among ≤12-month Nexus addons sampled, Helper shows **exceptional documentation and multi-subsystem surface** for its age, with explicit compliance boundaries (Nexus APIs only; no game-memory R/W; no Present hooks; no `bin64/cef` writes). Frame C does **not** license calling it a must-have. Correct label: **optional emerging reference shell**.

---

## 7. Where GW2 In-Game Helper stands (summary)

| Frame | Placement |
|-------|-----------|
| A Maturity/adoption Top 10 | **Not included**; far outside on stock metrics |
| B Category (B6 reference/browser) | **Capable peer**; Chromium-OSR differentiator; adoption unproven |
| C Emerging | **High engineering-doc / feature intensity**; optional |

**Strengths (public README / docs):** Single-DLL Nexus install; private CEF 150; curated site catalog; Account/DPS Logs/Events/Pathing/Notes pads; stated compliance posture; Windows + Linux via Wine/Proton claimed; active maintenance on research date.

**Limitations:** Adoption near-zero; absent from major curated lists; substantial overlap with external browsers and Blish/Taimi pathing; CEF first-run download cost; Defender false-positive notes for unsigned MinGW DLL; category honesty—a reference shell, not a foundational meter, loader, or format standard.

---

## 8. Must-have stacks by player archetype

“Must-have” means **commonly expected or strongly enabling** for that archetype in 2026 community practice—not marketing. Emerging tools are marked optional and are **not** elevated to must-have without evidence.

### New player
- Blish HUD *or* Nexus (+ optional TaimiHUD for markers)
- Skip ArcDPS until comfortable with combat fundamentals (reduces social pressure and setup complexity)
- Helper: **optional emerging** (in-game guides/wiki)

### Open-world / map completion / metas
- Blish Pathing / Event Table **or** Nexus + TaimiHUD
- Radial (GW2Radial or Nexus/Blish equivalent)
- Helper: **optional** (wiki/guides + pathing packs)

### Organized PvE (raids / fractals / strikes)
- ArcDPS (**expected** by many statics for logs)
- Elite Insights + dps.report workflow
- Unofficial Extras if using dependent plugins
- killproof.me plugin if groups require it
- Nexus for update hygiene; Blish optional for timers/pathing
- Helper: **optional** (DPS Logs UI / reference)—not a substitute for ArcDPS

### Linux (Proton / Wine)
- Nexus; TaimiHUD preferred for markers in recent Linux guides; Burrito as fallback; ArcDPS with extra friction
- Blish HUD: do not assume Windows parity
- Helper: **optional** if the Proton CEF path works for the user

### Minimalist
- Nexus alone, or nothing; ArcDPS only if required by groups
- Do not stack Blish + TacO + many Nexus modules without need
- Helper: **not** must-have

---

## 9. Repository-derived cognitive-engineering profiles

**Methodological caveat:** Profiles describe *observable engineering cognition*—problem framing, abstraction preference, documentation style, risk posture, collaboration pattern—as inferred from public artefacts (sites, READMEs, licenses, release notes, repository trees, shallow-clone shortlogs). They are **not** personality diagnoses, clinical assessments, or IQ claims. Uncertainty is high for closed-source authors. Style labels are artefact-based engineering metaphors only.

**Data gaps:** Unauthenticated GitHub API rate limits affected some collection; ArcDPS has no public source; Unofficial Extras implementation is closed; shallow clones undercount historical contributors.

### 9.1 deltaconnected — ArcDPS

- **Artefacts:** Closed distribution at deltaconnected.com/arcdps; DirectX proxy DLL (`d3d11.dll` / `dxgi.dll`); extension C API; EVTC docs; dense changelogs (sample through mid-2026).
- **Problem framing:** Accurate combat accounting and a stable ImGui-hosted extension bus under continuous client churn; limitations framed by what the server does not notify.
- **Abstraction:** Kernel / loader / metrics engine—not a UX aggregator.
- **Communication:** Terse, lowercase, imperative, non-marketing; hedged operational changelogs.
- **Risk posture:** Explicit unsupported / no-warranty warnings; closed binary; process injection via graphics proxy class of tool.
- **Collaboration:** Effectively solo public face; decentralized extension ecosystem.
- **Label:** *Systems minimalist / protocol realist*.
- **Uncertainty:** High—source unavailable; label reflects communication and ABI design.

### 9.2 dlamkins (Freesnöw) & Blish HUD core (+ agaertner, entrhopi)

- **Artefacts:** blish-hud/Blish-HUD (C# ~98%, MonoGame, `net472`); module template; blishhud.com; MIT; ~407★; multi-contributor org; satellites (`bhud-pkgs`, ArcDPS bridge, etc.).
- **Problem framing:** Separate-process, module-extensible overlay so authors need not reinvent windowing, API keys, input, packaging.
- **Abstraction:** Overlay platform + module runtime (external process).
- **Communication:** Polished user site + structured developer docs; Discord-centric support.
- **Collaboration:** Org-based; dlamkins ≈ platform owner; agaertner ≈ module-ecosystem specialist; entrhopi ≈ core product engineering—**do not collapse into one psychology**.
- **Label:** *Platform builder / ecosystem gardener*.

### 9.3 DeltaRaidcore — Raidcore Nexus

- **Artefacts:** RaidcoreGG/Nexus (C++ ~97%); `d3d11.dll` proxy; in-game manager; hot-load; Event PubSub; chainload; All Rights Reserved; CONTRIBUTING refuses external PRs; ~165★; push activity into August 2026.
- **Problem framing:** Host, load, update, and manage addons so developers focus on features.
- **Abstraction:** Loader + framework + addon library.
- **Communication:** Marketing-clear README; wiki; informal release commit voice atop formal license.
- **Risk posture:** Proxy DLL class; vendor claims of policy-aware design (claim, not independent audit).
- **Label:** *Proprietary platform steward*.

### 9.4 Friendly0Fire — GW2Radial, GW2Clarity

- **Artefacts:** C++/HLSL overlays on addon-loader stack; Radial ~369★; Clarity ~36★; FAQ with ArenaNet dialogue anecdotes; Clarity mode restrictions.
- **Problem framing:** Reduce mount/novelty selection friction; improve buff/skill readability.
- **Abstraction:** Focused feature addons, not hosts.
- **Communication:** Conversational Radial FAQ; terse Clarity README.
- **Risk posture:** Declines deeper game-function hooks for Action Camera automation (EULA / closed-source cheater rationale); Clarity unavailable in competitive modes.
- **Label:** *Interaction designer / constraint negotiator*.

### 9.5 TaimiHUD — arcnmx, kittywitch (+ Connicpu in Cargo authors)

- **Artefacts:** Rust workspace; Bevy ECS feature flags; dual `extension-arcdps` / `extension-nexus`; Nix; Fluent i18n; created 2025-06-17; ~49★; CONTRIBUTING LLM exclusion policy.
- **Problem framing:** Cross-host world-space guidance (pathing, timers, commander markers); bridge TacO/Blish-era formats into modern hosts.
- **Abstraction:** Feature addon with engine-like internals.
- **Communication:** Minimal GitHub README; external site; self-aware “User Guide: Hopefully someday?”
- **Collaboration:** Small core; shallow shortlog plurality arcnmx; kittywitch authorship incompletely evidenced by git shortlog alone (**uncertainty**).
- **Label:** *Systems-leaning feature team / format bridge*.

### 9.6 Krappa322 — Unofficial Extras (+ healing stats); knoxfighter on recent extras releases

- **Artefacts:** Closed extras binary + public `Definitions.h`; open `arcdps_healing_stats` (C++, gRPC stack); extras releases authored by knoxfighter in mid-2026 sample.
- **Problem framing:** Supply squad/keybind/chat events ArcDPS does not expose; fill healing-metrics gaps with local stats.
- **Abstraction:** Infrastructure plugin + feature plugin on Arc ABI.
- **Communication:** Disclaimer-heavy; separates “what it does” vs “what it provides for others.”
- **Risk posture:** Explicit network hooks and runtime modifications; closed source narrated as compliance with community-manager requests for similar readers.
- **Label:** *Extension infrastructure realist* (attribute recent extras publishing carefully to knoxfighter collaboration/handoff).

### 9.7 Xydroc-IO — GW2 In-Game Helper

- **Artefacts:** MIT C++ Nexus addon; out-of-process CEF 150 OSR; HLI5 IPC; WHITEPAPER, ARCHITECTURE, COMPLIANCE, KERNEL, ONBOARDING; version 2.2.1.0; created 2026-07-19; 1★; solo shortlog dominant.
- **Problem framing:** In-game access to guides, tools, official API account data, pathing, and log review without game CEF writes or game-memory reads.
- **Abstraction:** Hybrid—restricted browser kernel + feature pads on Nexus APIs.
- **Communication:** Exhaustive, formal, compliance-forward; documentation as control surface (opposite of Arc minimalism).
- **Risk posture:** Explicit Allowed/Forbidden tables; still inherits Nexus inject host risk; large Chromium attack surface mitigated by process-isolation claims.
- **Label:** *Compliance-forward UX aggregator / hybrid browser engineer*.
- **Uncertainty:** Medium—docs rich; adoption outcomes absent. Author affiliation with this packaging context is disclosed in §10.

### 9.8 Comparative matrix (ordinal within set)

| Developer/team | Platform vs feature | Openness | Ecosystem centrality | Doc intensity | Risk posture |
|----------------|---------------------|----------|----------------------|---------------|--------------|
| deltaconnected | Platform-kernel | Closed | Very high | Very low (public) | High (hooks) |
| Blish core | Platform | Open (MIT) | Very high | High | Medium (overlay) |
| DeltaRaidcore | Platform | Source-visible / ARR | Very high (loader) | Medium | Medium-high (proxy) |
| Friendly0Fire | Feature | Open | Medium-high | Medium | Medium |
| TaimiHUD | Feature (+ dual host) | Open | Rising | Low–medium | Medium |
| Krappa322 (+knoxfighter) | Infra feature | Closed extras / open healing | High (dependents) | Medium | High (runtime) |
| Xydroc-IO | Hybrid kernel+pads | Open (MIT) | Low (new) | Very high | Lower hooks; CEF/AV surface |

---

## 10. Speculative CHC-style ranges (non-psychometric; repositories cannot measure IQ)

### 10.1 Truth statement (read first)

**Software repositories, commit histories, documentation corpora, and star counts cannot measure intelligence quotient (IQ), general intelligence (\(g\)), or any clinical/cognitive construct.** No Wechsler, Stanford–Binet, WAIS, Woodcock–Johnson, or other standardized instrument was administered. No testing conditions, norms, age corrections, or reliability coefficients apply. The ranges below are **speculative engineering proxies mapped onto an IQ-like scale** (population mean 100, SD 15 *by analogy only*) so that readers who requested “IQ report–style constructs” have an explicitly labeled, **Very Low confidence** artefact. Wide intervals are intentional. False precision (e.g., “128”) is refused. Language such as “genius,” “gifted,” or clinical diagnosis is refused. **Do not cite these numbers as psychometrics.**

Constructs named follow common CHC / IQ-report vocabulary for reader orientation only:

| Construct | Usual meaning in IQ reports | Proxy used here (weak) |
|-----------|-----------------------------|-------------------------|
| **Gf** Fluid Reasoning | Novel problem solving | Architectural novelty, constraint juggling visible in design |
| **Gc** Crystallized Knowledge | Acquired domain knowledge | Longevity, API surface mastery, domain vocabulary in docs |
| **Gv** Visuospatial | Spatial / visual processing | Graphics/UI/overlay/spatial-pathing artefact complexity |
| **Gsm** Working Memory | Hold/manipulate information | Concurrent subsystem / IPC / state-machine complexity signals |
| **Gs** Processing Speed | Timed clerical/perceptual speed | **Generally N/A** — commit velocity ≠ Gs |
| **Gq** Quantitative | Quantitative reasoning | Metrics, parsing, statistics, numeric protocol work |
| **Full-scale heuristic** | Composite IQ analogue | Midpoint-of-ranges intuition only; **not** FSIQ |

### 10.2 Estimated ranges by developer / team

All intervals are speculative. Confidence: **Very Low** for every cell.

#### deltaconnected (ArcDPS)

| Construct | Speculative range | Proxy rationale (weak) |
|-----------|------------------:|------------------------|
| Gf | 118–145 | Sustained novel constraint solving under opaque client changes; extension ABI design |
| Gc | 125–148 | Multi-year combat-protocol and EVTC domain depth |
| Gv | 112–138 | DXGI/D3D proxy + ImGui-hosted UI surfaces |
| Gsm | 115–140 | Concurrent combat accounting, logging, extension bus |
| Gs | N/A | Not inferable from patch-aligned release speed |
| Gq | 120–145 | DPS/boon/CC accounting and log schemas |
| Full-scale heuristic | 118–142 | Wide; closed source inflates uncertainty |

#### dlamkins & Blish HUD core (+ agaertner, entrhopi as collaborators—not separately scored)

| Construct | Speculative range | Proxy rationale (weak) |
|-----------|------------------:|------------------------|
| Gf | 115–140 | Module platform design; cross-cutting overlay services |
| Gc | 118–142 | Long-running .NET/MonoGame GW2 overlay domain |
| Gv | 112–136 | Overlay layout, module UX, content pipeline |
| Gsm | 112–136 | Multi-module runtime coordination |
| Gs | N/A | Not inferable |
| Gq | 108–130 | Present but less central than Arc metrics work |
| Full-scale heuristic | 114–136 | Org product; individual differentiation limited |

*agaertner / entrhopi:* insufficient isolated public psychometrics proxies; treat as contributors within the Blish engineering culture, not separately ranged.

#### DeltaRaidcore (Nexus)

| Construct | Speculative range | Proxy rationale (weak) |
|-----------|------------------:|------------------------|
| Gf | 118–143 | Loader/hot-load/API consolidation under DX proxy constraints |
| Gc | 115–138 | Multi-year addon-host domain knowledge |
| Gv | 110–135 | In-game ImGui manager / UX shell |
| Gsm | 115–140 | Host, loader, update, pub/sub concurrency |
| Gs | N/A | Not inferable |
| Gq | 110–132 | Secondary to systems design in public artefacts |
| Full-scale heuristic | 115–138 | Source-visible but governance closed |

#### Friendly0Fire (GW2Radial / GW2Clarity)

| Construct | Speculative range | Proxy rationale (weak) |
|-----------|------------------:|------------------------|
| Gf | 112–136 | Input-state machines; conditional radial queuing |
| Gc | 112–135 | Long Radial maintenance; ArenaNet-constraint lore |
| Gv | 118–142 | Radial geometry, HLSL, Clarity grid/atlas work |
| Gsm | 108–132 | Stateful input UX; less platform-scale concurrency |
| Gs | N/A | Not inferable |
| Gq | 105–128 | Light relative to meters/parsers |
| Full-scale heuristic | 110–134 | Feature-focused artefact set |

#### TaimiHUD team (arcnmx primary commit signal; kittywitch in authors)

| Construct | Speculative range | Proxy rationale (weak) |
|-----------|------------------:|------------------------|
| Gf | 118–142 | Rust ECS pathing engine; dual-host abstraction |
| Gc | 110–134 | Pathing-format bridging; younger project than Arc/Blish |
| Gv | 120–145 | World-space markers, trails, spatial overlays |
| Gsm | 115–138 | Engine-like subsystem fan-out |
| Gs | N/A | Not inferable |
| Gq | 110–132 | Timers/numerics present; not primary public signal |
| Full-scale heuristic | 115–138 | Team aggregate; roles incompletely separable |

#### Krappa322 (+ knoxfighter on extras releases)

| Construct | Speculative range | Proxy rationale (weak) |
|-----------|------------------:|------------------------|
| Gf | 115–140 | Hook/integration design under policy constraints |
| Gc | 115–138 | Arc extension ABI + network-event domain |
| Gv | 105–128 | Secondary (ImGui surfaces); less spatial than pathing tools |
| Gsm | 115–140 | Live share / plugin bus complexity (healing + extras) |
| Gs | N/A | Not inferable |
| Gq | 118–142 | Healing stats, EVTC enrichment, protobuf/gRPC stacks |
| Full-scale heuristic | 114–138 | Closed extras block code inspection |

#### Xydroc-IO (GW2 In-Game Helper)

| Construct | Speculative range | Proxy rationale (weak) |
|-----------|------------------:|------------------------|
| Gf | 115–140 | CEF OSR + IPC + multi-pad integration under Nexus constraints |
| Gc | 108–132 | Strong written GW2/API/compliance corpus; short calendar tenure |
| Gv | 112–136 | ImGui pads + browser compositing; less world-space than Taimi |
| Gsm | 115–140 | Multi-process browser kernel + many feature domains |
| Gs | N/A | High commit density ≠ Gs |
| Gq | 108–130 | Log/account numerics secondary to integration work |
| Full-scale heuristic | 112–136 | Doc richness can inflate perceived Gc; adoption unproven |

### 10.3 Interpretation rules

1. Overlapping intervals mean **no ranking by IQ is supported**.
2. Higher Gv for pathing/radial authors vs higher Gq for meter/extras authors is a **proxy story**, not a test result.
3. Helper’s ranges do **not** place it “above” incumbents; Frame A adoption and Frame B category leadership remain independent conclusions.
4. If any range is republished, it must retain the **Very Low confidence** and **non-psychometric** labels.

---

## 11. Threats to validity

1. No install counts; stars ≠ users; ArcDPS cannot be star-ranked.
2. List corpus incomplete (official forum compilation blocked).
3. Frame B lacks controlled usability trials.
4. Frame C proxies favor projects that invest in markdown—may correlate with author style more than player value.
5. CHC/IQ-like section has **no psychometric validity**; included only because requested, with maximal caveats.
6. Author of Helper is affiliated with this report’s packaging context; Frame A/B/C separations, explicit non-must-have labeling, and honest Frame A exclusion are used to reduce advocacy bias. Readers should still treat Helper self-metrics cautiously.
7. Shallow clones and API rate limits undercount contribution graphs.

---

## 12. Reproducibility

Re-query listed URLs and GitHub metadata on a chosen research date; rebuild Tables A–C with updated stars/`pushed_at`; keep frames separate; do not collapse into a single \(S\) if the research question is quality or newcomer fairness. Cite as: *Multi-frame GW2 addon ecosystem report, 3 August 2026*, not as an industry standard. Speculative CHC ranges must be reproduced only with §10.1 intact.

---

## References (selected)

1. ArenaNet Help Center — Policy: Third-Party Programs.  
2. deltaconnected — arcdps distribution and API README.  
3. blish-hud/Blish-HUD; blishhud.com.  
4. RaidcoreGG/Nexus; raidcore.gg/gw2/nexus.  
5. Dexerto — Best Guild Wars 2 add-ons (2025 update).  
6. Mukluk Labs — GW2 Add-Ons.  
7. Convergence Corp — Addons.  
8. Hardstuck — ArcDPS guide; related Snow Crows logging practice.  
9. BoyC/GW2TacO; Friendly0Fire/GW2Radial & GW2Clarity; TaimiHUD/TaimiHUD; Krappa322 unofficial extras releases & healing stats; knoxfighter killproof plugin; baaron4 Elite Insights; AsherGlick/Burrito; PieOrCake/hoard_and_seek & alter_ego; Xydroc-IO/GW2-InGame-Helper.  
10. Linux community addon practice notes (Bazzite / Universal Blue discourse, January 2026 update sample).

---

*End of report.*
