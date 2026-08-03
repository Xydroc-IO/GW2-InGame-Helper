#!/usr/bin/env python3
"""Generate GW2 Addon Ecosystem Academic Report PDF (multi-frame ranking)."""

from __future__ import annotations

import os
from pathlib import Path

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY, TA_LEFT
from reportlab.lib.pagesizes import letter
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import inch
from reportlab.platypus import (
    KeepTogether,
    ListFlowable,
    ListItem,
    PageBreak,
    Paragraph,
    SimpleDocTemplate,
    Spacer,
    Table,
    TableStyle,
)

OUT = Path(__file__).resolve().parents[1] / "GW2_Addon_Ecosystem_Academic_Report_2026-08.pdf"
MD_OUT = Path(__file__).resolve().parent / "GW2_Addon_Ecosystem_Academic_Report_2026-08.md"

PAGE_W, PAGE_H = letter
MARGIN = 0.7 * inch


def styles():
    base = getSampleStyleSheet()
    s = {
        "title": ParagraphStyle(
            "T",
            parent=base["Title"],
            fontSize=16,
            leading=20,
            spaceAfter=8,
            alignment=TA_CENTER,
        ),
        "subtitle": ParagraphStyle(
            "ST",
            parent=base["Normal"],
            fontSize=9,
            leading=12,
            alignment=TA_CENTER,
            textColor=colors.HexColor("#333333"),
            spaceAfter=14,
        ),
        "h1": ParagraphStyle(
            "H1",
            parent=base["Heading1"],
            fontSize=13,
            leading=16,
            spaceBefore=14,
            spaceAfter=6,
            textColor=colors.HexColor("#1a1a1a"),
        ),
        "h2": ParagraphStyle(
            "H2",
            parent=base["Heading2"],
            fontSize=11,
            leading=14,
            spaceBefore=10,
            spaceAfter=4,
            textColor=colors.HexColor("#222222"),
        ),
        "h3": ParagraphStyle(
            "H3",
            parent=base["Heading3"],
            fontSize=10,
            leading=13,
            spaceBefore=8,
            spaceAfter=3,
        ),
        "body": ParagraphStyle(
            "B",
            parent=base["Normal"],
            fontSize=8.5,
            leading=11.5,
            alignment=TA_JUSTIFY,
            spaceAfter=5,
        ),
        "caveat": ParagraphStyle(
            "C",
            parent=base["Normal"],
            fontSize=8,
            leading=10.5,
            alignment=TA_JUSTIFY,
            textColor=colors.HexColor("#4a0000"),
            borderPadding=4,
            spaceAfter=8,
            spaceBefore=4,
        ),
        "small": ParagraphStyle(
            "SM",
            parent=base["Normal"],
            fontSize=7.5,
            leading=9.5,
            alignment=TA_LEFT,
            spaceAfter=3,
        ),
        "cell": ParagraphStyle(
            "CELL",
            parent=base["Normal"],
            fontSize=7,
            leading=9,
        ),
        "footer": ParagraphStyle(
            "F",
            parent=base["Normal"],
            fontSize=7,
            textColor=colors.grey,
            alignment=TA_CENTER,
        ),
    }
    return s


def P(text: str, style):
    return Paragraph(text.replace("\n", "<br/>"), style)


def make_table(headers, rows, col_widths, sty):
    data = [[P(h, sty["cell"]) for h in headers]]
    for row in rows:
        data.append([P(str(c), sty["cell"]) for c in row])
    t = Table(data, colWidths=col_widths, repeatRows=1)
    t.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#2c3e50")),
                ("TEXTCOLOR", (0, 0), (-1, 0), colors.white),
                ("FONTNAME", (0, 0), (-1, 0), "Helvetica-Bold"),
                ("BACKGROUND", (0, 1), (-1, -1), colors.HexColor("#f8f9fa")),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.HexColor("#f8f9fa"), colors.white]),
                ("GRID", (0, 0), (-1, -1), 0.4, colors.HexColor("#cccccc")),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("LEFTPADDING", (0, 0), (-1, -1), 3),
                ("RIGHTPADDING", (0, 0), (-1, -1), 3),
                ("TOPPADDING", (0, 0), (-1, -1), 3),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
            ]
        )
    )
    return t


def iq_table(sty, rows):
    headers = ["Construct", "Speculative proxy range", "Artifact basis (1 line)", "Confidence"]
    # usable width ~ 7.1 in
    return make_table(headers, rows, [1.35 * inch, 1.35 * inch, 3.5 * inch, 0.85 * inch], sty)


def add_footer(canvas, doc):
    canvas.saveState()
    canvas.setFont("Helvetica", 7)
    canvas.setFillColor(colors.grey)
    canvas.drawString(MARGIN, 0.4 * inch, "GW2 Addon Ecosystem Report — 3 Aug 2026 — Multi-frame; IQ proxies non-psychometric")
    canvas.drawRightString(PAGE_W - MARGIN, 0.4 * inch, f"Page {doc.page}")
    canvas.restoreState()


def build():
    sty = styles()
    story = []

    # ----- Title -----
    story.append(P("Guild Wars 2 Third-Party Addon Ecosystem", sty["title"]))
    story.append(
        P(
            "Multi-Frame Ranking, Must-Have Stacks, Repository-Derived Cognitive-Engineering Profiles,<br/>"
            "and Speculative Non-Psychometric Ability Proxies",
            sty["subtitle"],
        )
    )
    story.append(
        P(
            "<b>Document type:</b> Technical report (academic style; not peer-reviewed) &nbsp;|&nbsp; "
            "<b>Research date:</b> 3 August 2026 &nbsp;|&nbsp; "
            "<b>Keywords:</b> Guild Wars 2; third-party addons; ranking bias; CHC scaffolds; "
            "ArcDPS; Blish HUD; Nexus; GW2 In-Game Helper",
            sty["small"],
        )
    )

    # ----- Abstract -----
    story.append(P("Abstract", sty["h1"]))
    story.append(
        P(
            "Community “top addon” lists and single weighted composites that blend curated-list frequency, "
            "GitHub stars, and ecosystem centrality systematically disadvantage new entrants. List lag, star "
            "accumulation time, platform lock-in, and reviewer familiarity are structural biases. This report "
            "<b>rejects a single composite score as a claim of overall quality</b>. It presents three frames: "
            "(A) established maturity/adoption proxies; (B) category-relative capability comparison; "
            "(C) emerging tools (≤12 months) with age-aware engineering proxies. Must-have stacks are stratified "
            "by player archetype. Developer cognitive-engineering profiles are inferences from public repository "
            "artefacts only—not clinical psychology. A separate section reports <b>speculative</b> CHC-style "
            "ability proxy ranges on an IQ-like scale; these are <b>not</b> psychometric scores. Under Frame A, "
            "<b>GW2 In-Game Helper is outside the established Top 10</b>. Under Frame B it is a capable peer in "
            "the in-game reference/browser/QoL-aggregator category. Under Frame C it shows high documentation "
            "and feature-surface intensity for its age while remaining <b>optional / emerging</b> for must-have purposes.",
            sty["body"],
        )
    )

    # ----- §1 Epistemic -----
    story.append(P("1. Epistemic stance and ToS boundary", sty["h1"]))
    story.append(
        P(
            "ArenaNet publishes a Third-Party Programs policy and does not provide official rankings or install "
            "counts. Third-party client modifications are unsupported; Support will not assist with issues caused "
            "by such tools; automation and unfair-advantage tools are prohibited. Tolerance of benign overlays in "
            "practice is not endorsement. Closed-source network/runtime hooks (ArcDPS, Unofficial Extras) carry "
            "additional supply-chain and policy surface. All discussion is descriptive of community artefacts.",
            sty["body"],
        )
    )

    # ----- §2 Critique -----
    story.append(P("2. Why a single composite score is unfair to new addons", sty["h1"]))
    story.append(
        P(
            "A prior draft used <i>S</i> = 0.25<i>L</i> + 0.20<i>A</i> + 0.25<i>E</i> + 0.15<i>M</i> + 0.15<i>U</i> "
            "(list frequency, star-scaled adoption, ecosystem role, maintenance, uniqueness). That index is "
            "informative about <b>incumbency</b>, not engineering merit or player fit for recent tools.",
            sty["body"],
        )
    )
    story.append(
        ListFlowable(
            [
                ListItem(
                    P(
                        "<b>List lag.</b> Dexerto (Jan 2025), Mukluk, and Convergence update slowly. A tool created "
                        "July 2026 cannot appear on a January 2025 list. Low <i>L</i> is chronological.",
                        sty["body"],
                    )
                ),
                ListItem(
                    P(
                        "<b>Star accumulation time.</b> Stars are a stock variable. Repos from 2017–2019 had years "
                        "to accrue attention. Helper (created 2026-07-19; 1★ on research date) is incomparable on raw <i>A</i>.",
                        sty["body"],
                    )
                ),
                ListItem(
                    P(
                        "<b>Ecosystem lock-in (<i>E</i>).</b> Platforms others require (ArcDPS, Nexus, Blish) score "
                        "high by definition. New feature addons cannot outrank platforms on <i>E</i> without becoming platforms.",
                        sty["body"],
                    )
                ),
                ListItem(
                    P(
                        "<b>Reviewer familiarity.</b> Guide authors recommend tools they already teach; new tools lack tutorial inventory.",
                        sty["body"],
                    )
                ),
                ListItem(
                    P(
                        "<b>Maintenance under-corrects.</b> Even <i>M</i>=100 at 15% weight cannot offset near-zero <i>L</i> and <i>A</i>.",
                        sty["body"],
                    )
                ),
            ],
            bulletType="1",
            start="1",
        )
    )
    story.append(
        P(
            "<b>Conclusion:</b> Frame A keeps maturity/adoption ranking but labels it as such. Frames B and C "
            "answer different questions. No single “Top 10 quality” list is asserted.",
            sty["body"],
        )
    )

    # ----- §3 Data -----
    story.append(P("3. Data sources and metrics (3 August 2026)", sty["h1"]))
    story.append(
        P(
            "Sources: Dexerto; Mukluk Labs; Convergence Corp; Hardstuck ArcDPS guide; Raidcore Nexus docs; "
            "Blish HUD site; GitHub project pages; deltaconnected.com/arcdps. Official forum compilation blocked (Cloudflare).",
            sty["body"],
        )
    )
    story.append(
        make_table(
            ["Project", "Repo / site", "Stars", "Created / public", "Last push (approx.)"],
            [
                ["ArcDPS", "deltaconnected.com/arcdps", "— (no public GH)", "long-standing", "patch-driven"],
                ["Blish HUD", "blish-hud/Blish-HUD", "407", "2019-01", "2026-02-19"],
                ["Raidcore Nexus", "RaidcoreGG/Nexus", "165", "2021-11", "2026-08-02"],
                ["GW2Radial", "Friendly0Fire/GW2Radial", "369", "2017-09", "2025-11-16"],
                ["GW2 Addon Manager", "gw2-addon-loader/…", "471", "2019-08", "2024-05-04"],
                ["GW2TacO", "BoyC/GW2TacO", "255", "long-standing", "2025-12-01"],
                ["Elite Insights", "baaron4/…", "153", "long-standing", "2026-08-03"],
                ["Burrito", "AsherGlick/Burrito", "118", "—", "2025-10-13"],
                ["Unofficial Extras", "Krappa322/…_releases", "~68", "2021-09", "2026-07-19"],
                ["killproof plugin", "knoxfighter/…", "59", "2020-12", "2026-07-25"],
                ["TaimiHUD", "TaimiHUD/TaimiHUD", "49", "2025-06-17", "2026-07-27"],
                ["GW2Clarity", "Friendly0Fire/GW2Clarity", "36", "2022-04", "2025-03-31"],
                ["Hoard & Seek", "PieOrCake/hoard_and_seek", "5", "2026-03-21", "2026-07-06"],
                ["Alter Ego", "PieOrCake/alter_ego", "3", "2026-04-07", "2026-07-01"],
                ["GW2 In-Game Helper", "Xydroc-IO/GW2-InGame-Helper", "1", "2026-07-19", "2026-08-03"],
            ],
            [1.35 * inch, 1.85 * inch, 0.85 * inch, 1.2 * inch, 1.35 * inch],
            sty,
        )
    )
    story.append(Spacer(1, 6))
    story.append(
        P(
            "<b>Helper local engineering signals:</b> ~45 975 lines in <font face='Courier'>src/</font>; "
            "domain LOC approx. pathing 9 186, account 9 088, browse 6 432, logs 5 598, helper 4 813, browser 3 181; "
            "≥19 markdown docs; WHITEPAPER ~4 481 words; README ~2 828 words; high Jul–Aug 2026 commit density; "
            "MIT; Nexus signature HELP; private CEF 150 OSR architecture.",
            sty["small"],
        )
    )

    # ----- Frame A -----
    story.append(P("4. Frame A — Established ecosystem tools (maturity / adoption proxy)", sty["h1"]))
    story.append(
        P(
            "<b>Question answered:</b> Which tools currently dominate community recommendation and dependency graphs? "
            "<b>Not answered:</b> engineering quality of new tools; fitness for a given player; safety.",
            sty["body"],
        )
    )
    story.append(P("Table A — Top 10 by maturity/adoption consensus", sty["h2"]))
    story.append(
        make_table(
            ["Rank", "Tool", "Role", "Adoption / list signals", "Stars / proxy"],
            [
                ["1", "ArcDPS", "Combat meter, EVTC, plugin host", "Near-universal in endgame guides", "Consensus proxy"],
                ["2", "Blish HUD", "External overlay + modules", "Dexerto, Mukluk, Convergence", "407"],
                ["3", "Raidcore Nexus", "In-game loader / API", "Convergence; modern install guides", "165"],
                ["4", "Elite Insights*", "EVTC parser companion", "Implied by log workflow", "153"],
                ["5", "GW2TacO", "Marker format ancestor", "Dexerto; Convergence", "255"],
                ["6", "GW2 Addon Manager", "Desktop installer (legacy)", "Dexerto, Mukluk; stale 2024-05", "471"],
                ["7", "GW2Radial", "Mount/utility radial", "Mukluk; high stars", "369"],
                ["8", "Unofficial Extras", "ArcDPS extension channel", "Enables dependent plugins", "~68"],
                ["9", "TaimiHUD", "Pathing + timers (Nexus/Arc)", "Convergence; Linux guides", "49"],
                ["10", "Burrito", "Linux tactical overlay", "Convergence; Linux niche", "118"],
            ],
            [0.45 * inch, 1.25 * inch, 1.55 * inch, 2.4 * inch, 1.2 * inch],
            sty,
        )
    )
    story.append(
        P(
            "*Companion tool, not an in-client overlay. <b>GW2 In-Game Helper under Frame A:</b> Outside Top 10. "
            "Not listed on Dexerto/Mukluk/Convergence/Hardstuck as a pillar. 1 GitHub star. Expected for a "
            "≤1-month-old public repo—not a quality verdict.",
            sty["body"],
        )
    )

    # ----- Frame B -----
    story.append(P("5. Frame B — Category-relative capability ranking", sty["h1"]))
    story.append(
        P(
            "Comparisons use public README/docs claims and observable architecture—not controlled user studies.",
            sty["body"],
        )
    )
    story.append(P("5.1 Combat metrics &amp; logging", sty["h2"]))
    story.append(
        make_table(
            ["Tool", "Capability notes", "Relative placement"],
            [
                ["ArcDPS", "Live DPS/boons/CC; EVTC; plugin API", "Category standard"],
                ["Elite Insights", "Offline/deep parse of EVTC", "Required companion for analysis"],
                ["Unofficial Extras", "Squad/keybind/chat events for plugins", "Infrastructure, not a meter"],
                ["killproof plugin", "Displays killproof.me credentials", "Narrow specialist"],
                ["Helper", "Consumer only (DPS Logs + optional EI CLI)", "Dependent, not competing"],
            ],
            [1.4 * inch, 3.4 * inch, 2.0 * inch],
            sty,
        )
    )
    story.append(P("5.2 Overlay platforms &amp; loaders", sty["h2"]))
    story.append(
        make_table(
            ["Tool", "Notes", "Placement"],
            [
                ["Nexus", "Hot-load, library, API, chainload ArcDPS", "Dominant in-game loader 2026"],
                ["Blish HUD", "Module repo; Gw2Sharp; MumbleLink; optional Arc pipe", "Dominant external feature host"],
                ["Addon Manager", "Historical desktop installer", "Declining maintenance"],
            ],
            [1.4 * inch, 3.6 * inch, 1.8 * inch],
            sty,
        )
    )
    story.append(P("5.3 Pathing / markers / timers", sty["h2"]))
    story.append(
        make_table(
            ["Tool", "Notes", "Placement"],
            [
                ["Blish Pathing", "Mature modules; TacO-compatible packs", "High adoption path"],
                ["TaimiHUD", "Rust; Nexus or ArcDPS; Linux-friendly", "Strong modern Nexus-native option"],
                ["GW2TacO", "Format progenitor", "Still relevant; uniqueness declining"],
                ["Burrito", "Linux-native overlay", "Linux specialist"],
                ["Helper Pathing", "Tekkit/Lady/Hero + user .taco; compass", "Peer for Nexus users; not category leader"],
            ],
            [1.4 * inch, 3.4 * inch, 2.0 * inch],
            sty,
        )
    )
    story.append(P("5.4 In-game reference / browser / QoL aggregator (Helper’s primary category)", sty["h2"]))
    story.append(
        make_table(
            ["Tool", "Architecture", "Relative placement"],
            [
                ["Desktop browser", "Isolation; context switch", "Default for many players"],
                ["Blish modules", "Feature-specific overlays; no full Chromium", "High adoption for discrete QoL"],
                [
                    "GW2 In-Game Helper",
                    "Nexus ImGui + out-of-process CEF 150 OSR; pads",
                    "Category-capable peer; Chromium-in-Nexus differentiator; adoption unproven",
                ],
                ["Older wiki browsers", "Superseded per Helper README", "Historical"],
            ],
            [1.5 * inch, 2.8 * inch, 2.5 * inch],
            sty,
        )
    )
    story.append(
        P(
            "<b>Helper under Frame B:</b> Within the reference/browser category, Helper is a legitimate capability "
            "peer and, on public documentation, the most explicitly engineered full Chromium-in-client option in "
            "the Nexus sample. It is not the category leader by users. Elsewhere it is secondary or dependent.",
            sty["body"],
        )
    )

    # ----- Frame C -----
    story.append(P("6. Frame C — Emerging / new entrants (≤12 months)", sty["h1"]))
    story.append(
        P(
            "Inclusion approx. Aug 2025–Aug 2026: TaimiHUD (2025-06), Hoard &amp; Seek (2026-03), Alter Ego (2026-04), "
            "GW2 In-Game Helper (2026-07). Age-aware proxies: maintenance intensity, documentation density, feature "
            "surface, integration depth. Adoption stock is reported but down-weighted within this frame.",
            sty["body"],
        )
    )
    story.append(
        make_table(
            ["Tool", "Age signal", "Doc / eng. signals", "Feature surface", "Adoption", "Frame C note"],
            [
                [
                    "TaimiHUD",
                    "~13 mo (border)",
                    "Site + CONTRIBUTING; Rust",
                    "Pathing, timers, markers",
                    "49★; lists",
                    "Strongest emerging pathing adoption",
                ],
                [
                    "Hoard & Seek",
                    "~4.5 mo",
                    "API.md + INTEGRATION_PROMPT",
                    "Search + cross-addon API",
                    "5★",
                    "Strongest new API-bus pattern",
                ],
                [
                    "Alter Ego",
                    "~4 mo",
                    "README; LLM disclosure",
                    "Characters/builds",
                    "3★",
                    "Depends on H&S",
                ],
                [
                    "In-Game Helper",
                    "~2–3 wk public",
                    "WHITEPAPER/ARCH/COMPLIANCE; ~46k LOC",
                    "Browser + Account + Pathing + Logs + …",
                    "1★; no lists",
                    "Highest doc/feature intensity; unproven adoption",
                ],
            ],
            [1.05 * inch, 0.95 * inch, 1.55 * inch, 1.35 * inch, 0.85 * inch, 1.35 * inch],
            sty,
        )
    )
    story.append(
        P(
            "<b>Helper under Frame C:</b> Exceptional documentation and multi-subsystem surface for its age, with "
            "explicit compliance boundaries. Does <b>not</b> license calling it a must-have. Label: "
            "<b>optional emerging reference shell</b>.",
            sty["body"],
        )
    )

    # ----- Placement summary -----
    story.append(P("7. Where GW2 In-Game Helper stands (all frames)", sty["h1"]))
    story.append(
        make_table(
            ["Frame", "Placement"],
            [
                ["A Maturity/adoption Top 10", "Not included; far outside"],
                ["B Category (reference/browser)", "Capable peer; Chromium-OSR differentiator; adoption unproven"],
                ["C Emerging", "High engineering-doc/feature intensity; optional"],
            ],
            [2.4 * inch, 4.5 * inch],
            sty,
        )
    )

    # ----- Must-have -----
    story.append(P("8. Must-have stacks by player archetype", sty["h1"]))
    story.append(
        P(
            "“Must-have” means commonly expected or strongly enabling in 2026 community practice—not marketing. "
            "Emerging tools are marked optional.",
            sty["body"],
        )
    )
    story.append(
        make_table(
            ["Archetype", "Core stack", "Helper"],
            [
                [
                    "New player",
                    "Blish or Nexus (+ optional TaimiHUD); skip ArcDPS early",
                    "Optional emerging (guides/wiki)",
                ],
                [
                    "Open-world / metas",
                    "Blish Pathing/Events or Nexus+TaimiHUD; radial",
                    "Optional (wiki + pathing packs)",
                ],
                [
                    "Organized PvE",
                    "ArcDPS; Elite Insights; Extras if needed; killproof if required; Nexus",
                    "Optional (DPS Logs UI / reference)",
                ],
                [
                    "Linux (Proton)",
                    "Nexus; TaimiHUD preferred; Burrito fallback; ArcDPS with friction",
                    "Optional if CEF path works",
                ],
                [
                    "Minimalist",
                    "Nexus alone or nothing; ArcDPS only if groups require",
                    "Not must-have",
                ],
            ],
            [1.3 * inch, 3.6 * inch, 2.2 * inch],
            sty,
        )
    )

    story.append(PageBreak())

    # ----- Profiles -----
    story.append(P("9. Repository-derived cognitive-engineering profiles", sty["h1"]))
    story.append(
        P(
            "<b>Methodological caveat:</b> Profiles describe observable engineering cognition—problem framing, "
            "abstraction preference, documentation style, risk posture—as inferred from public artefacts. They are "
            "<b>not</b> personality diagnoses, IQ claims, or clinical assessments. Uncertainty is high for closed-source authors.",
            sty["caveat"],
        )
    )

    profiles = [
        (
            "9.1 deltaconnected — ArcDPS",
            "Label: <b>systems minimalist / protocol realist</b> (inferred).",
            [
                "<b>Artefacts:</b> Closed distribution site; minimal HTML; DirectX proxy DLL; combat metrics + logging + plugin surface.",
                "<b>Problem framing:</b> Reliable combat telemetry under game updates with minimal user ceremony.",
                "<b>Abstraction:</b> Kernel/hook layer; other tools orbit it.",
                "<b>Communication:</b> Extremely terse; all-caps risk warnings; install as file replace.",
                "<b>Risk posture:</b> Runtime modification; unsupported; user assumes responsibility.",
                "<b>Collaboration:</b> Solo public face; plugin ecosystem external.",
                "<b>Uncertainty:</b> High—source unavailable.",
            ],
        ),
        (
            "9.2 dlamkins (Freesnöw) &amp; Blish HUD core (+ agaertner, entrhopi et al.)",
            "Label: <b>platform builder / ecosystem gardener</b> (inferred).",
            [
                "<b>Artefacts:</b> MIT C# overlay (~98% C#); module template; docs site; Discord support; ~407★; multi-contributor history.",
                "<b>Problem framing:</b> Extensible overlay so authors ship modules without reimplementing input/API/settings.",
                "<b>Abstraction:</b> Platform/module host (external process).",
                "<b>Communication:</b> User-friendly site + developer docs; setup-video culture.",
                "<b>Risk posture:</b> Overlay model; optional ArcDPS bridge plugin.",
                "<b>Collaboration:</b> Org with multiple top contributors (agaertner, entrhopi among historical leads).",
            ],
        ),
        (
            "9.3 DeltaRaidcore — Raidcore Nexus",
            "Label: <b>proprietary platform steward</b> (inferred).",
            [
                "<b>Artefacts:</b> C++ proxy loader; hot-load; addon library; Event PubSub; chainload; All Rights Reserved; ~165★; active 2026.",
                "<b>Problem framing:</b> Collapse fragmented DLL management into in-game discover/install/update + developer API.",
                "<b>Abstraction:</b> Loader + framework + store.",
                "<b>Communication:</b> Marketing-clear README; wiki; Discord/Patreon.",
                "<b>Risk posture:</b> Proxy d3d11.dll; project messaging emphasizes no botting / auditability claims.",
                "<b>Collaboration:</b> Small core; external PRs not the public contribution model; thanks cite Sognus et al. for guidance.",
            ],
        ),
        (
            "9.4 Friendly0Fire — GW2Radial, GW2Clarity",
            "Label: <b>interaction designer / constraint negotiator</b> (inferred).",
            [
                "<b>Artefacts:</b> C++ overlays; Radial ~369★ since 2017; Clarity grids (2022); CI; Discord; ArenaNet-boundary narratives in Radial lore.",
                "<b>Problem framing:</b> Reduce keybind overload (radial); improve buff/skill clarity (grids).",
                "<b>Abstraction:</b> Focused feature addons, not full platforms.",
                "<b>Communication:</b> Practical READMEs; credit-heavy.",
                "<b>Risk posture:</b> Loader-era install; Clarity notes competitive-mode unavailability.",
            ],
        ),
        (
            "9.5 TaimiHUD — arcnmx, kittywitch (Kat Inskip)",
            "Label: <b>systems-leaning feature team</b> (inferred).",
            [
                "<b>Artefacts:</b> Rust-dominant; Nexus or ArcDPS hosts; IRC + Discord; sparse README (“User Guide: Hopefully someday?”); CONTRIBUTING reportedly restricts LLM-generated contributions.",
                "<b>Problem framing:</b> Cross-platform pathing/timers/markers without Windows-only overlay assumptions.",
                "<b>Abstraction:</b> Performance-oriented native addon on modern hosts.",
                "<b>Communication:</b> External site for install/FAQ; contributor guide.",
            ],
        ),
        (
            "9.6 Krappa322 — Unofficial Extras (+ knoxfighter on related Arc ecosystem)",
            "Label: <b>extension infrastructure realist</b> (inferred).",
            [
                "<b>Artefacts:</b> Release repo; extras closed-source by community-manager request for runtime readers; Definitions.h for integrators; disclaimers; knoxfighter’s OSS (e.g. Boon Table, killproof plugin, arcdps-extension) forms adjacent healing/plugin surface.",
                "<b>Problem framing:</b> Supply missing squad/keybind/chat signals to the plugin economy.",
                "<b>Abstraction:</b> Extension bus over ArcDPS hooks.",
                "<b>Communication:</b> Precise capability lists; legal/risk clarity.",
                "<b>Risk posture:</b> Explicit runtime modification warnings.",
            ],
        ),
        (
            "9.7 Xydroc-IO (Xydroc) — GW2 In-Game Helper",
            "Label: <b>compliance-forward UX aggregator / hybrid browser engineer</b> (inferred).",
            [
                "<b>Artefacts:</b> MIT C++ Nexus addon; out-of-process CEF 150 OSR; HLI5 IPC; academic WHITEPAPER + ARCHITECTURE + COMPLIANCE + KERNEL; CONTRIBUTING ownership zones; CI; ~46k LOC multi-domain; created 2026-07-19; 1★ research date.",
                "<b>Problem framing:</b> Contemporary web + account/pathing/log pads inside GW2 without Present hooks or game-memory reads; Proton as first-class constraint.",
                "<b>Abstraction:</b> Hybrid—restricted browser kernel + feature pads.",
                "<b>Communication:</b> Exhaustive, formal, compliance-forward; stamp/version discipline.",
                "<b>Risk posture:</b> Explicit forbidden list; AV false-positive docs; Wine sandbox trade-offs stated.",
                "<b>Collaboration:</b> Solo-dominant commit history; process docs anticipate takeover.",
                "<b>Uncertainty:</b> Medium on engineering intent (docs rich); high on user-outcome value (adoption absent).",
            ],
        ),
    ]

    for title, label, bullets in profiles:
        story.append(P(title, sty["h2"]))
        story.append(P(label, sty["body"]))
        for b in bullets:
            story.append(P("• " + b, sty["small"]))

    story.append(P("9.8 Comparative matrix (ordinal)", sty["h2"]))
    story.append(
        make_table(
            ["Developer/team", "Platform vs feature", "Openness", "Ecosystem centrality", "Doc intensity", "Risk posture"],
            [
                ["deltaconnected", "Platform-kernel", "Closed", "Very high", "Very low", "High (hooks)"],
                ["Blish core", "Platform", "Open (MIT)", "Very high", "High (site)", "Medium (overlay)"],
                ["DeltaRaidcore", "Platform", "ARR / reserved", "Very high (loader)", "Medium", "Medium-high (proxy)"],
                ["Friendly0Fire", "Feature", "Open", "Medium-high", "Medium", "Medium"],
                ["TaimiHUD", "Feature (+dual host)", "Open", "Rising", "Low–medium", "Medium"],
                ["Krappa322", "Infra feature", "Closed (policy)", "High (deps)", "Medium", "High (runtime)"],
                ["Xydroc-IO", "Hybrid kernel+pads", "Open (MIT)", "Low (new)", "Very high", "Lower hooks; CEF/AV surface"],
            ],
            [1.15 * inch, 1.2 * inch, 1.0 * inch, 1.25 * inch, 1.0 * inch, 1.4 * inch],
            sty,
        )
    )

    story.append(PageBreak())

    # ----- IQ section -----
    story.append(P("10. Speculative non-psychometric ability proxies (CHC scaffold)", sty["h1"]))
    story.append(
        P(
            "<b>CRITICAL CAVEAT — READ BEFORE ANY NUMERIC TABLE.</b> Real Fluid Reasoning (Gf), Crystallized "
            "Intelligence (Gc), Visual-Spatial Processing (Gv), Short-Term Working Memory (Gsm), Processing Speed "
            "(Gs), Quantitative Knowledge (Gq), and full-scale IQ require <b>standardized psychometric testing</b> "
            "(e.g., Wechsler Adult Intelligence Scale indexes; Cattell–Horn–Carroll broad abilities). "
            "<b>Public GitHub repositories and addon binaries cannot measure IQ.</b> Numeric ranges below are "
            "<b>non-psychometric, speculative, artefact-inferred heuristics</b> mapped onto an IQ-like scale "
            "(population mean 100, SD 15) solely as a reporting scaffold requested for this document. They are "
            "<b>not diagnoses, not clinical assessments, not validated scores, and not suitable for employment, "
            "educational, or clinical decisions</b>. Prefer wide ranges; never treat midpoints as point estimates. "
            "Confidence is <b>Very Low</b> (or lower) for all cells. Processing Speed (Gs) is generally "
            "<b>not inferable</b> from commit velocity or release cadence. Closed-source authors receive wider "
            "ranges and still lower evidentiary weight.",
            sty["caveat"],
        )
    )

    story.append(P("10.1 What typically appears in IQ / ability reports (educational primer)", sty["h2"]))
    story.append(
        P(
            "Contemporary ability testing often reports: (1) <b>Full-Scale / General Ability</b> composites; "
            "(2) <b>Verbal / Crystallized</b> indexes (vocabulary, information, similarities—Gc-aligned); "
            "(3) <b>Perceptual / Fluid / Visuospatial</b> indexes (matrix reasoning, block design—Gf/Gv-aligned); "
            "(4) <b>Working Memory</b> (digit span, sequencing—Gsm); (5) <b>Processing Speed</b> (symbol search, "
            "coding—Gs); sometimes (6) <b>Quantitative</b> knowledge (Gq). CHC theory organizes these as broad "
            "abilities under a general factor <i>g</i>. This report uses those names as labels only.",
            sty["body"],
        )
    )

    # IQ tables per developer
    iq_blocks = [
        (
            "10.2 deltaconnected (ArcDPS)",
            [
                ["Gf Fluid reasoning", "120–145", "Long-lived combat/DX protocol under patch churn (opaque source)", "Very Low"],
                ["Gc Crystallized", "110–135", "Domain lexicon of combat metrics/plugins; terse public docs", "Very Low"],
                ["Gv Visuospatial", "110–135", "Overlay HUD / DirectX presentation path (inferred)", "Very Low"],
                ["Gsm Working memory", "115–140", "Concurrent hook + metrics + plugin host complexity (inferred)", "Very Low"],
                ["Gs Processing speed", "N/A (80–140)", "Not inferable from patch cadence; placeholder only", "—"],
                ["Gq Quantitative", "115–140", "DPS/boon/CC statistical surfaces", "Very Low"],
                ["Full-scale heuristic", "115–140", "Wide blend; closed source forbids tighter bound", "Very Low"],
            ],
        ),
        (
            "10.3 dlamkins / Blish HUD core",
            [
                ["Gf Fluid reasoning", "115–135", "Module platform architecture; API layering", "Very Low"],
                ["Gc Crystallized", "115–140", "Extensive public docs/site; .NET/GW2 API literacy", "Very Low"],
                ["Gv Visuospatial", "110–130", "Overlay UI / GW2-themed controls", "Very Low"],
                ["Gsm Working memory", "110–130", "Multi-module runtime + contributor coordination (weak signal)", "Very Low"],
                ["Gs Processing speed", "N/A (85–130)", "Not inferable from Discord/org pace", "—"],
                ["Gq Quantitative", "105–125", "Limited direct metrics engineering vs Arc ecosystem", "Very Low"],
                ["Full-scale heuristic", "112–132", "Wide composite; team effort confounds individual attribution", "Very Low"],
            ],
        ),
        (
            "10.4 DeltaRaidcore (Nexus)",
            [
                ["Gf Fluid reasoning", "115–138", "Hot-load loader/framework design under DX proxy constraints", "Very Low"],
                ["Gc Crystallized", "110–130", "Addon API surface; wiki/README domain language", "Very Low"],
                ["Gv Visuospatial", "108–128", "ImGui host / in-game manager UX", "Very Low"],
                ["Gsm Working memory", "112–135", "Loader + update + chainload + event bus concurrency (inferred)", "Very Low"],
                ["Gs Processing speed", "N/A (85–130)", "Not inferable from release tags", "—"],
                ["Gq Quantitative", "105–125", "Sparse public quantitative artefacts", "Very Low"],
                ["Full-scale heuristic", "112–133", "Wide; ARR/closed contribution model limits evidence", "Very Low"],
            ],
        ),
        (
            "10.5 Friendly0Fire",
            [
                ["Gf Fluid reasoning", "112–132", "Radial interaction model; Clarity grid constraint solving", "Very Low"],
                ["Gc Crystallized", "110–130", "README domain terms; loader/API literacy", "Very Low"],
                ["Gv Visuospatial", "115–138", "Radial menus and buff-grid spatial layout (stronger Gv signal)", "Very Low"],
                ["Gsm Working memory", "105–125", "Moderate system scope vs loaders/meters", "Very Low"],
                ["Gs Processing speed", "N/A (85–130)", "Not inferable", "—"],
                ["Gq Quantitative", "100–120", "Little public stats-engine surface", "Very Low"],
                ["Full-scale heuristic", "110–130", "Wide; feature-addon scope", "Very Low"],
            ],
        ),
        (
            "10.6 arcnmx &amp; kittywitch (TaimiHUD) — team-level proxies",
            [
                ["Gf Fluid reasoning", "115–138", "Rust rewrite of pathing/timers on dual hosts", "Very Low"],
                ["Gc Crystallized", "110–130", "Nexus/Arc domain literacy; CONTRIBUTING norms (incl. LLM policy)", "Very Low"],
                ["Gv Visuospatial", "112–132", "Markers/pathing overlays", "Very Low"],
                ["Gsm Working memory", "112–133", "Cross-platform + dual-host integration complexity", "Very Low"],
                ["Gs Processing speed", "N/A (85–130)", "Not inferable; team confounds", "—"],
                ["Gq Quantitative", "108–128", "Timer/encounter logic (moderate)", "Very Low"],
                ["Full-scale heuristic", "112–132", "Team aggregate—do not assign to one person", "Very Low"],
            ],
        ),
        (
            "10.7 Krappa322 (Extras; adjacent knoxfighter OSS noted separately)",
            [
                ["Gf Fluid reasoning", "112–135", "Extension bus design over Arc hooks", "Very Low"],
                ["Gc Crystallized", "110–132", "Precise disclaimer/API header literacy", "Very Low"],
                ["Gv Visuospatial", "100–120", "Limited UI; options menus only", "Very Low"],
                ["Gsm Working memory", "112–135", "Runtime hook + event export complexity (inferred)", "Very Low"],
                ["Gs Processing speed", "N/A (85–130)", "Not inferable", "—"],
                ["Gq Quantitative", "105–125", "Indirect via plugin economy, not primary metrics UI", "Very Low"],
                ["Full-scale heuristic", "110–132", "Closed extras widen uncertainty", "Very Low"],
            ],
        ),
        (
            "10.8 Xydroc-IO (GW2 In-Game Helper)",
            [
                ["Gf Fluid reasoning", "115–138", "OSR CEF + shared-memory IPC + Proton constraints decomposition", "Very Low"],
                ["Gc Crystallized", "118–142", "Extreme doc density; compliance/API/web domain vocabulary", "Very Low"],
                ["Gv Visuospatial", "110–130", "ImGui chrome + OSR frame composite (moderate)", "Very Low"],
                ["Gsm Working memory", "115–138", "Multi-process IPC, multi-domain pads, stamp discipline", "Very Low"],
                ["Gs Processing speed", "N/A (85–135)", "High commit density ≠ Gs; explicitly non-inferable", "—"],
                ["Gq Quantitative", "108–128", "Log/parse tooling adjacent; not primary identity", "Very Low"],
                ["Full-scale heuristic", "114–135", "Wide; docs inflate Gc proxy—adoption/outcome unknown", "Very Low"],
            ],
        ),
    ]

    for title, rows in iq_blocks:
        block = [P(title, sty["h2"]), iq_table(sty, rows), Spacer(1, 4)]
        story.append(KeepTogether(block))

    story.append(
        P(
            "<b>Interpretation rule:</b> If two developers’ ranges overlap almost completely—as they do—the data "
            "do not support ranking individuals by “IQ.” The tables exist only to satisfy a structured reporting "
            "request while documenting why the numbers are scientifically invalid as IQ.",
            sty["caveat"],
        )
    )

    # ----- Threats -----
    story.append(P("11. Threats to validity", sty["h1"]))
    story.append(
        P(
            "No install counts; stars ≠ users; list corpus incomplete; Frame B lacks usability trials; Frame C "
            "favors markdown-heavy projects; Helper packaging affiliation risks advocacy bias (mitigated by "
            "explicit non-must-have labeling and Frame A exclusion); IQ proxies are non-scientific for individual differences.",
            sty["body"],
        )
    )

    # ----- Reproducibility -----
    story.append(P("12. Reproducibility", sty["h1"]))
    story.append(
        P(
            "Re-query listed URLs and GitHub metadata; rebuild Tables A–C; keep frames separate; never merge into "
            "a single quality composite for new-entrant comparison. Cite as: <i>Multi-frame GW2 addon ecosystem "
            "report, 3 August 2026</i>.",
            sty["body"],
        )
    )

    # ----- References -----
    story.append(P("13. Selected references", sty["h1"]))
    refs = [
        "ArenaNet Help Center — Policy: Third-Party Programs.",
        "deltaconnected — arcdps (deltaconnected.com/arcdps).",
        "blish-hud/Blish-HUD; blishhud.com.",
        "RaidcoreGG/Nexus; raidcore.gg/gw2/nexus.",
        "Dexerto — Best Guild Wars 2 add-ons (2025 update).",
        "Mukluk Labs — GW2 Add-Ons; Convergence Corp — Addons; Hardstuck — ArcDPS guide.",
        "BoyC/GW2TacO; Friendly0Fire/GW2Radial & GW2Clarity; TaimiHUD/TaimiHUD;",
        "Krappa322 arcdps_unofficial_extras_releases; knoxfighter Arc plugins; baaron4 Elite Insights;",
        "PieOrCake/hoard_and_seek; Xydroc-IO/GW2-InGame-Helper.",
        "McGrew, K. S. — CHC theory overviews; Wechsler Adult Intelligence Scale technical manuals "
        "(construct definitions only; not applied as tests here).",
    ]
    for i, r in enumerate(refs, 1):
        story.append(P(f"[{i}] {r}", sty["small"]))

    story.append(Spacer(1, 12))
    story.append(
        P(
            "End of report. IQ-scale figures throughout §10 are speculative artefact proxies only.",
            sty["subtitle"],
        )
    )

    OUT.parent.mkdir(parents=True, exist_ok=True)
    doc = SimpleDocTemplate(
        str(OUT),
        pagesize=letter,
        leftMargin=MARGIN,
        rightMargin=MARGIN,
        topMargin=0.65 * inch,
        bottomMargin=0.6 * inch,
        title="GW2 Addon Ecosystem Academic Report (2026-08)",
        author="Independent technical synthesis",
    )
    doc.build(story, onFirstPage=add_footer, onLaterPages=add_footer)
    return OUT


def sync_md_note():
    """Append IQ caveat pointer to companion markdown if present."""
    note = """

---

## Appendix — Speculative IQ proxies

See the PDF §10 for full CHC-scaffold tables. **Public repos cannot measure IQ.** All numeric ranges in the PDF are non-psychometric heuristics with Very Low confidence. Processing Speed (Gs) is marked N/A / non-inferable from commit velocity.

"""
    if MD_OUT.exists():
        text = MD_OUT.read_text(encoding="utf-8")
        if "Speculative IQ proxies" not in text:
            MD_OUT.write_text(text.rstrip() + note, encoding="utf-8")


if __name__ == "__main__":
    path = build()
    sync_md_note()
    print(f"Wrote {path}")
    print(f"Size bytes: {path.stat().st_size}")
