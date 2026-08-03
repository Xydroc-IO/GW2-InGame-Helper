#!/usr/bin/env python3
"""Generate GW2 Addon Ecosystem Academic Report PDF (August 2026)."""

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

OUT = Path("/home/xydroc/Desktop/GW2-InGame-Helper/docs/GW2_Addon_Ecosystem_Academic_Report_2026-08.pdf")

INK = colors.HexColor("#1a1a1a")
MUTED = colors.HexColor("#444444")
RULE = colors.HexColor("#888888")
HDR_BG = colors.HexColor("#f0f0f0")
ACCENT = colors.HexColor("#2c3e50")


def styles():
    base = getSampleStyleSheet()
    s = {
        "cover_title": ParagraphStyle(
            "cover_title",
            parent=base["Title"],
            fontName="Times-Bold",
            fontSize=16,
            leading=20,
            alignment=TA_CENTER,
            textColor=INK,
            spaceAfter=12,
        ),
        "cover_sub": ParagraphStyle(
            "cover_sub",
            parent=base["Normal"],
            fontName="Times-Roman",
            fontSize=11,
            leading=14,
            alignment=TA_CENTER,
            textColor=MUTED,
            spaceAfter=6,
        ),
        "h1": ParagraphStyle(
            "h1",
            parent=base["Heading1"],
            fontName="Times-Bold",
            fontSize=13,
            leading=16,
            textColor=ACCENT,
            spaceBefore=14,
            spaceAfter=8,
            borderPadding=2,
        ),
        "h2": ParagraphStyle(
            "h2",
            parent=base["Heading2"],
            fontName="Times-Bold",
            fontSize=11.5,
            leading=14,
            textColor=INK,
            spaceBefore=10,
            spaceAfter=6,
        ),
        "h3": ParagraphStyle(
            "h3",
            parent=base["Heading3"],
            fontName="Times-Bold",
            fontSize=10.5,
            leading=13,
            textColor=INK,
            spaceBefore=8,
            spaceAfter=4,
        ),
        "body": ParagraphStyle(
            "body",
            parent=base["Normal"],
            fontName="Times-Roman",
            fontSize=9.5,
            leading=12.5,
            alignment=TA_JUSTIFY,
            textColor=INK,
            spaceAfter=6,
        ),
        "body_left": ParagraphStyle(
            "body_left",
            parent=base["Normal"],
            fontName="Times-Roman",
            fontSize=9.5,
            leading=12.5,
            alignment=TA_LEFT,
            textColor=INK,
            spaceAfter=6,
        ),
        "bullet": ParagraphStyle(
            "bullet",
            parent=base["Normal"],
            fontName="Times-Roman",
            fontSize=9.5,
            leading=12.5,
            alignment=TA_LEFT,
            textColor=INK,
            leftIndent=8,
            spaceAfter=2,
        ),
        "meta": ParagraphStyle(
            "meta",
            parent=base["Normal"],
            fontName="Times-Roman",
            fontSize=8.5,
            leading=11,
            textColor=MUTED,
            spaceAfter=3,
        ),
        "caption": ParagraphStyle(
            "caption",
            parent=base["Normal"],
            fontName="Times-Italic",
            fontSize=8,
            leading=10,
            textColor=MUTED,
            spaceBefore=2,
            spaceAfter=8,
            alignment=TA_CENTER,
        ),
        "cell": ParagraphStyle(
            "cell",
            parent=base["Normal"],
            fontName="Times-Roman",
            fontSize=7.5,
            leading=9.5,
            textColor=INK,
        ),
        "cell_h": ParagraphStyle(
            "cell_h",
            parent=base["Normal"],
            fontName="Times-Bold",
            fontSize=7.5,
            leading=9.5,
            textColor=INK,
        ),
        "warn": ParagraphStyle(
            "warn",
            parent=base["Normal"],
            fontName="Times-Bold",
            fontSize=9.5,
            leading=12.5,
            textColor=INK,
            alignment=TA_JUSTIFY,
            spaceAfter=6,
            borderColor=RULE,
            borderWidth=0.5,
            borderPadding=6,
            backColor=colors.HexColor("#f7f7f7"),
        ),
        "footer": ParagraphStyle(
            "footer",
            parent=base["Normal"],
            fontName="Times-Roman",
            fontSize=8,
            textColor=MUTED,
            alignment=TA_CENTER,
        ),
        "abstract_label": ParagraphStyle(
            "abstract_label",
            parent=base["Normal"],
            fontName="Times-Bold",
            fontSize=10,
            leading=12,
            alignment=TA_CENTER,
            spaceAfter=6,
        ),
    }
    return s


def P(text: str, style):
    return Paragraph(text, style)


def bullets(items, st):
    return ListFlowable(
        [ListItem(P(i, st["bullet"]), leftIndent=12, bulletColor=INK) for i in items],
        bulletType="bullet",
        start="•",
        leftIndent=15,
        bulletFontName="Times-Roman",
        bulletFontSize=9,
    )


def table(headers, rows, col_widths):
    st = styles()
    data = [[P(h, st["cell_h"]) for h in headers]]
    for row in rows:
        data.append([P(str(c), st["cell"]) for c in row])
    t = Table(data, colWidths=col_widths, repeatRows=1)
    t.setStyle(
        TableStyle(
            [
                ("BACKGROUND", (0, 0), (-1, 0), HDR_BG),
                ("TEXTCOLOR", (0, 0), (-1, -1), INK),
                ("FONTNAME", (0, 0), (-1, 0), "Times-Bold"),
                ("FONTSIZE", (0, 0), (-1, -1), 7.5),
                ("ALIGN", (0, 0), (-1, 0), "LEFT"),
                ("VALIGN", (0, 0), (-1, -1), "TOP"),
                ("GRID", (0, 0), (-1, -1), 0.4, RULE),
                ("LEFTPADDING", (0, 0), (-1, -1), 3),
                ("RIGHTPADDING", (0, 0), (-1, -1), 3),
                ("TOPPADDING", (0, 0), (-1, -1), 3),
                ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
                ("ROWBACKGROUNDS", (0, 1), (-1, -1), [colors.white, colors.HexColor("#fafafa")]),
            ]
        )
    )
    return t


def add_page_number(canvas, doc):
    canvas.saveState()
    page = canvas.getPageNumber()
    text = f"GW2 Addon Ecosystem Academic Report — 3 Aug 2026  |  {page}"
    canvas.setFont("Times-Roman", 8)
    canvas.setFillColor(MUTED)
    canvas.drawCentredString(letter[0] / 2, 0.55 * inch, text)
    canvas.setStrokeColor(RULE)
    canvas.setLineWidth(0.4)
    canvas.line(0.75 * inch, 0.7 * inch, letter[0] - 0.75 * inch, 0.7 * inch)
    canvas.restoreState()


def build():
    st = styles()
    story = []
    W = letter[0] - 1.5 * inch

    # --- Cover ---
    story.append(Spacer(1, 1.2 * inch))
    story.append(P("Guild Wars 2 Third-Party Addon Ecosystem:", st["cover_title"]))
    story.append(
        P(
            "Multi-Frame Ranking, Must-Have Stacks,<br/>Repository-Derived Cognitive-Engineering Profiles,<br/>and Speculative CHC Proxies",
            st["cover_title"],
        )
    )
    story.append(Spacer(1, 0.35 * inch))
    story.append(P("Technical report (academic style; not peer-reviewed)", st["cover_sub"]))
    story.append(P("Research date: 3 August 2026", st["cover_sub"]))
    story.append(
        P(
            "Scope: Public third-party overlays, loaders, meters, pathing tools, and companion parsers",
            st["cover_sub"],
        )
    )
    story.append(Spacer(1, 0.5 * inch))
    meta_rows = [
        ["Document type", "Independent technical synthesis"],
        ["Peer review", "None"],
        ["Affiliation", "Packaged with GW2 In-Game Helper documentation"],
        ["Keywords", "GW2; ranking bias; age-normalized evaluation; CHC proxies; ArcDPS; Blish; Nexus"],
    ]
    story.append(
        table(["Field", "Value"], meta_rows, [1.6 * inch, W - 1.6 * inch])
    )
    story.append(Spacer(1, 0.6 * inch))
    story.append(
        P(
            "<b>Epistemic notice.</b> ArenaNet provides neither official rankings nor install counts. "
            "A single composite of list frequency and stars measures incumbency, not quality. "
            "Repositories cannot measure IQ; CHC-style ranges in §10 are speculative non-psychometric proxies "
            "at Very Low confidence.",
            st["body"],
        )
    )
    story.append(PageBreak())

    # --- Abstract ---
    story.append(P("Abstract", st["abstract_label"]))
    story.append(
        P(
            "Community “top addon” lists and single weighted composites that blend curated-list frequency, "
            "GitHub stars, and ecosystem centrality systematically disadvantage new entrants. List lag, star "
            "accumulation time, platform lock-in, and reviewer familiarity are structural biases. This report "
            "therefore <b>rejects a single composite score as a claim of overall quality or ranking fairness</b>. "
            "It presents three separate frames: (A) established maturity/adoption proxies; (B) category-relative "
            "capability comparison within shared jobs; (C) emerging tools (≤12 months) evaluated with age-aware "
            "engineering proxies. Must-have stacks are stratified by player archetype. Developer profiles are "
            "inferences from public repository artefacts only—not clinical psychology. A separate section presents "
            "speculative CHC-style ranges on an IQ-like scale, led by the explicit claim that "
            "<b>software repositories cannot measure IQ</b>. Under Frame A, <b>GW2 In-Game Helper is outside the "
            "established Top 10</b>. Under Frame B, it is a <b>capable peer in the in-game reference / browser / "
            "QoL-aggregator category</b>. Under Frame C, it ranks among the <b>most documentation-dense and "
            "feature-surface-rich new Nexus addons</b> in the candidate set, while remaining "
            "<b>optional / emerging</b> for must-have purposes.",
            st["body"],
        )
    )

    # --- §1 ---
    story.append(P("1. Epistemic stance and ToS boundary", st["h1"]))
    story.append(
        P(
            "ArenaNet publishes a Third-Party Programs policy and does <b>not</b> provide official rankings or "
            "install counts. Use of third-party client modifications is unsupported; Support will not assist with "
            "issues caused by such tools; automation and unfair-advantage tools are prohibited. Tolerance of benign "
            "overlays in practice is not endorsement. All tools below are discussed as community artefacts at the "
            "account holder’s risk. Closed-source network/runtime hooks (ArcDPS, Unofficial Extras) carry additional "
            "supply-chain and policy surface. This document is not affiliated with ArenaNet, NCSoft, deltaconnected, "
            "Raidcore, or Blish HUD maintainers except insofar as public artefacts are cited.",
            st["body"],
        )
    )

    # --- §2 ---
    story.append(P("2. Why a single composite score is unfair to new addons", st["h1"]))
    story.append(
        P(
            "A prior draft used "
            "<i>S</i> = 0.25<i>L</i> + 0.20<i>A</i> + 0.25<i>E</i> + 0.15<i>M</i> + 0.15<i>U</i>, "
            "with <i>L</i> = curated-list frequency, <i>A</i> = star-scaled adoption proxy, "
            "<i>E</i> = ecosystem-platform role, <i>M</i> = maintenance recency, <i>U</i> = uniqueness. "
            "Under that index, ArcDPS scored approximately 98 and GW2 In-Game Helper approximately 32. "
            "That index is <b>informative about incumbency</b>, not engineering merit or player fit for recent tools.",
            st["body"],
        )
    )
    story.append(P("2.1 Structural failure modes of list-frequency + raw stars", st["h2"]))
    story.append(
        bullets(
            [
                "<b>List lag.</b> Consumer and creator lists (Dexerto, January 2025 update; Mukluk Labs; Convergence Corp) "
                "update slowly relative to greenfield Nexus modules. A tool created in July 2026 cannot appear on a "
                "January 2025 journalism list. Low <i>L</i> is chronological, not evaluative.",
                "<b>Star accumulation time.</b> GitHub stars are a stock variable. Repos created 2017–2019 "
                "(GW2Radial, Addon Manager lineage, TacO-era attention) had years to accrue attention. Helper "
                "(created 2026-07-19, <b>1★</b> on the research date) is incomparable on raw <i>A</i>.",
                "<b>Ecosystem lock-in (<i>E</i>).</b> Platforms that other tools require—ArcDPS as meter/plugin host, "
                "Nexus as loader, Blish as module host—score high by definition. New feature addons cannot outrank "
                "platforms on <i>E</i> without becoming platforms: a category error if <i>S</i> is read as “quality.”",
                "<b>Reviewer familiarity.</b> Guide authors recommend tools they already teach. New tools lack tutorial "
                "inventory and clip culture, depressing list and social signals irrespective of capability.",
                "<b>Maintenance weight under-corrects.</b> Even with <i>M</i> = 100, a 15% weight cannot offset "
                "near-zero <i>L</i> and <i>A</i>. The composite mathematically buries new work while rewarding list "
                "inertia (e.g., Addon Manager’s high stars coexist with a last push of May 2024).",
                "<b>Missing data asymmetry.</b> ArcDPS has no public GitHub repository; any star-based ranking either "
                "excludes the ecosystem’s most central meter or invents a proxy that then dominates <i>A</i>.",
            ],
            st,
        )
    )
    story.append(Spacer(1, 6))
    story.append(
        P(
            "<b>Conclusion.</b> Frame A below keeps maturity/adoption ranking but <b>labels it as such</b>. "
            "Frames B and C answer different questions. No single “Top 10 quality” list is asserted.",
            st["body"],
        )
    )

    # --- §3 ---
    story.append(P("3. Data sources and metrics (research date 3 August 2026)", st["h1"]))
    story.append(
        P(
            "Candidates were drawn from Dexerto, Mukluk Labs, Convergence Corp, Hardstuck ArcDPS materials, "
            "Raidcore Nexus documentation, Blish HUD site materials, Linux community guides (2026), and GitHub "
            "project pages. The official forum compilation of external resources could not be retrieved "
            "(Cloudflare HTTP 403).",
            st["body"],
        )
    )
    story.append(
        table(
            ["Project", "Repo / site", "Stars", "Created", "Last push"],
            [
                ["ArcDPS", "deltaconnected.com/arcdps", "— (no GH)", "long-standing", "patch-driven 2026"],
                ["Blish HUD", "blish-hud/Blish-HUD", "407", "2019-01", "2026-02-19"],
                ["Raidcore Nexus", "RaidcoreGG/Nexus", "165", "2021-11", "2026-08-02"],
                ["GW2Radial", "Friendly0Fire/GW2Radial", "369", "2017-09", "2025-11-16"],
                ["GW2 Addon Manager", "gw2-addon-loader/…", "471", "2019-08", "2024-05-04"],
                ["GW2TacO", "BoyC/GW2TacO", "255", "long-standing", "2025-12-01"],
                ["Elite Insights", "baaron4/GW2-Elite-Insights-Parser", "153", "long-standing", "2026-08-03"],
                ["Burrito", "AsherGlick/Burrito", "118", "—", "2025-10-13"],
                ["Unofficial Extras", "Krappa322/…_releases", "~68–69", "2021-09", "2026-07-19"],
                ["killproof plugin", "knoxfighter/…", "59", "2020-12", "~2026"],
                ["TaimiHUD", "TaimiHUD/TaimiHUD", "49", "2025-06-17", "2026-07-27"],
                ["GW2Clarity", "Friendly0Fire/GW2Clarity", "36", "2022-04", "2025-03-31"],
                ["Hoard & Seek", "PieOrCake/hoard_and_seek", "5", "2026-03-21", "2026-07-06"],
                ["Alter Ego", "PieOrCake/alter_ego", "3", "2026-04-07", "2026-07-01"],
                ["GW2 In-Game Helper", "Xydroc-IO/GW2-InGame-Helper", "1", "2026-07-19", "2026-08-03"],
            ],
            [1.35 * inch, 2.0 * inch, 0.7 * inch, 1.0 * inch, 1.15 * inch],
        )
    )
    story.append(P("Table 1. Candidate universe metrics observed 3 August 2026.", st["caption"]))
    story.append(
        P(
            "<b>Helper local engineering signals</b> (workspace checkout): MIT-licensed Raidcore Nexus CEF browser / "
            "reference / QoL aggregator, version <b>2.2.0.0</b>; signature HELP / 0x48454C50; private CEF 150 OSR "
            "(does not write game bin64/cef); approximately 45 975 lines across src/ C++/headers; domain LOC approx. "
            "pathing 9 186, account 9 088, browse 6 432, logs 5 598, helper 4 813, browser 3 181; ≥19 markdown docs; "
            "WHITEPAPER alone ~4 481 words; README ~2 828 words; high commit density July–August 2026; 1 GitHub star.",
            st["body"],
        )
    )

    # --- §4 Frame A ---
    story.append(P("4. Frame A — Established maturity / adoption (not quality)", st["h1"]))
    story.append(
        P(
            "<b>Question answered:</b> Which tools currently dominate community recommendation and dependency graphs? "
            "<b>Not answered:</b> Engineering quality of new tools; fitness for a given player; safety; “best addon.”",
            st["body"],
        )
    )
    story.append(P("Table A — Top 10 by maturity / adoption consensus", st["h2"]))
    story.append(
        table(
            ["Rk", "Tool", "Role", "Adoption / list signals", "Stars"],
            [
                ["1", "ArcDPS", "Combat meter, EVTC, plugin host", "Near-universal in endgame guides", "Consensus proxy"],
                ["2", "Blish HUD", "External overlay + modules", "Dexerto, Mukluk, Convergence", "407"],
                ["3", "Raidcore Nexus", "In-game loader / manager / API", "Convergence; modern guides", "165"],
                ["4", "Elite Insights*", "EVTC parser companion", "Implied by log workflow", "153"],
                ["5", "GW2TacO", "Marker/pathing format ancestor", "Dexerto; Convergence", "255"],
                ["6", "GW2 Addon Manager", "Desktop installer (legacy)", "Dexerto, Mukluk; stale 2024-05", "471"],
                ["7", "GW2Radial", "Mount/utility radial", "Mukluk; high stars", "369"],
                ["8", "Unofficial Extras", "ArcDPS extension data channel", "Enables dependent plugins", "~68"],
                ["9", "TaimiHUD", "Nexus/Arc pathing + timers", "Convergence; Linux guides", "49"],
                ["10", "Burrito", "Native Linux tactical overlay", "Convergence; Linux niche", "118"],
            ],
            [0.35 * inch, 1.2 * inch, 1.55 * inch, 2.15 * inch, 0.95 * inch],
        )
    )
    story.append(
        P(
            "Table 2. Frame A maturity/adoption Top 10. *Companion tool, not an in-client overlay. "
            "Addon Manager’s sixth place partly reflects legacy list inertia; TacO uniqueness is declining.",
            st["caption"],
        )
    )
    story.append(
        P(
            "<b>GW2 In-Game Helper under Frame A.</b> Outside the Top 10. Not listed on Dexerto, Mukluk, Convergence, "
            "or Hardstuck as a pillar. Not featured as an ecosystem pillar on Raidcore’s Nexus marketing page in the "
            "materials reviewed. <b>1</b> GitHub star. Estimated band among named public addons: far below incumbents "
            "on adoption stock metrics. This is expected for a ≤1-month-old public repository and "
            "<b>must not be read as a quality verdict</b>.",
            st["body"],
        )
    )

    # --- §5 Frame B ---
    story.append(P("5. Frame B — Category-relative capability peers", st["h1"]))
    story.append(
        P(
            "<b>Question answered:</b> Within a shared job, how do tools compare on stated capabilities and "
            "architectural fit? Comparisons use public README/docs claims and observable architecture—not controlled "
            "user studies, not install counts.",
            st["body"],
        )
    )

    story.append(P("5.1 B1 — Combat metrics and logging", st["h2"]))
    story.append(
        table(
            ["Tool", "Capability notes", "Relative placement"],
            [
                ["ArcDPS", "Live DPS/boons/CC; EVTC; plugin API", "Category standard"],
                ["Elite Insights", "Offline/deep parse of EVTC", "Required companion for analysis"],
                ["Unofficial Extras", "Squad/keybind/chat events for plugins", "Infrastructure, not a meter"],
                ["killproof plugin", "killproof.me credentials display", "Narrow specialist"],
            ],
            [1.4 * inch, 2.8 * inch, 2.0 * inch],
        )
    )
    story.append(
        P(
            "Helper participates only as a <b>consumer</b> (DPS Logs pad + optional Elite Insights CLI), not as a meter.",
            st["body"],
        )
    )

    story.append(P("5.2 B2 — Overlay platforms and loaders", st["h2"]))
    story.append(
        table(
            ["Tool", "Capability notes", "Relative placement"],
            [
                ["Nexus", "Hot-load, library, API, chainload ArcDPS", "Dominant in-game loader (2026)"],
                ["Blish HUD", "Module repo, Gw2Sharp, MumbleLink, Arc pipe", "Dominant external feature host"],
                ["Addon Manager", "Historical desktop installer", "Declining maintenance"],
            ],
            [1.4 * inch, 2.8 * inch, 2.0 * inch],
        )
    )

    story.append(P("5.3 B3 — Pathing / markers / timers", st["h2"]))
    story.append(
        table(
            ["Tool", "Capability notes", "Relative placement"],
            [
                ["Blish Pathing (+ packs)", "Mature module ecosystem; TacO-compatible", "High adoption path"],
                ["TaimiHUD", "Rust; Nexus or Arc; timers + markers; Linux-friendly", "Strong modern Nexus-native option"],
                ["GW2TacO", "Format progenitor", "Relevant; uniqueness declining"],
                ["Burrito", "Linux-native overlay", "Linux specialist"],
                [
                    "Helper Pathing",
                    "Curated packs + user .taco; compass; MumbleLink display-only",
                    "Peer for Nexus-in-Helper pathing; does not displace Blish/Taimi leaders",
                ],
            ],
            [1.4 * inch, 2.6 * inch, 2.2 * inch],
        )
    )

    story.append(P("5.4 B4 — Input QoL; B5 — Account / inventory API", st["h2"]))
    story.append(
        P(
            "<b>B4:</b> GW2Radial remains the established radial; Nexus RadialMenus and Blish equivalents overlap; "
            "GW2Clarity provides buff/cooldown grids with lower list frequency. <b>B5:</b> Hoard &amp; Seek "
            "(multi-account search, cross-addon Nexus API), Blish Item Search modules, and Helper Account hub "
            "(unlocks, inventory, wallet, vault, TP, crafting, progress via official API) are category peers—"
            "not a single winner without user studies.",
            st["body"],
        )
    )

    story.append(P("5.5 B6 — In-game reference / browser / QoL aggregator", st["h2"]))
    story.append(
        table(
            ["Tool", "Architecture", "Relative placement"],
            [
                ["External desktop browser", "Isolation; context-switch cost", "Default for many players"],
                ["Blish modules (trackers/QoL)", "Overlay modules; no full Chromium", "High adoption for discrete QoL"],
                [
                    "GW2 In-Game Helper",
                    "Nexus ImGui + out-of-process CEF 150 OSR; curated catalog + pads",
                    "Category-capable peer; CEF-in-Nexus differentiator; adoption unproven",
                ],
                ["Older wiki-browser Nexus addons", "Narrower / superseded per Helper README", "Historical"],
            ],
            [1.55 * inch, 2.55 * inch, 2.1 * inch],
        )
    )
    story.append(
        P(
            "<b>Helper under Frame B.</b> Within B6, Helper is a legitimate capability peer and, on public "
            "documentation, the most explicitly engineered full Chromium-in-client option in the Nexus sample. "
            "It is <b>not</b> the category leader by users. In B3/B5 it is a secondary option. In B1/B2 it is "
            "dependent, not competing. Honest peer set: in-game browser/wiki/build-site aggregators; Nexus QoL "
            "panels; Blish tracker modules; standalone sites opened externally.",
            st["body"],
        )
    )

    # --- §6 Frame C ---
    story.append(P("6. Frame C — Emerging / new entrants (≤12 months)", st["h1"]))
    story.append(
        P(
            "<b>Inclusion:</b> Public creation or major public launch within approximately 12 months of the research "
            "date (approx. August 2025–August 2026): TaimiHUD (2025-06; borderline at ~13 months), Hoard &amp; Seek "
            "(2026-03), Alter Ego (2026-04), GW2 In-Game Helper (2026-07). "
            "<b>Age-aware proxies (ordinal; not a fake-precision composite):</b> maintenance intensity, documentation "
            "density, feature surface, integration depth; adoption stock is reported but down-weighted.",
            st["body"],
        )
    )
    story.append(
        table(
            ["Tool", "Age", "Doc / eng. signals", "Feature surface", "Adoption", "Frame C note"],
            [
                [
                    "TaimiHUD",
                    "~13 mo",
                    "Site + CONTRIBUTING; Rust",
                    "Pathing, timers, markers",
                    "49★; lists",
                    "Strongest emerging pathing adoption",
                ],
                [
                    "Hoard & Seek",
                    "~4.5 mo",
                    "API + integration docs",
                    "Account search + proxy API",
                    "5★",
                    "Strongest new cross-addon API",
                ],
                [
                    "Alter Ego",
                    "~4 mo",
                    "README; LLM disclosure",
                    "Characters/builds",
                    "3★",
                    "Depends on Hoard & Seek",
                ],
                [
                    "GW2 In-Game Helper",
                    "~2–3 wk",
                    "WHITEPAPER/ARCHITECTURE/COMPLIANCE/KERNEL; ~46k LOC",
                    "Browser + Account + Pathing + Logs + Events + Notes",
                    "1★; no lists",
                    "Highest doc density + feature surface; unproven adoption",
                ],
            ],
            [1.15 * inch, 0.6 * inch, 1.35 * inch, 1.35 * inch, 0.7 * inch, 1.05 * inch],
        )
    )
    story.append(P("Table 3. Frame C emerging tools (ordinal discussion).", st["caption"]))
    story.append(
        P(
            "<b>Helper under Frame C.</b> Among ≤12-month Nexus addons sampled, Helper shows exceptional documentation "
            "and multi-subsystem surface for its age, with explicit compliance boundaries (Nexus APIs only; no "
            "game-memory R/W; no Present hooks; no bin64/cef writes). Frame C does <b>not</b> license calling it a "
            "must-have. Correct label: <b>optional emerging reference shell</b>.",
            st["body"],
        )
    )

    # --- §7 ---
    story.append(P("7. Where GW2 In-Game Helper stands (summary)", st["h1"]))
    story.append(
        table(
            ["Frame", "Placement"],
            [
                ["A Maturity/adoption Top 10", "Not included; far outside on stock metrics"],
                ["B Category (B6 reference/browser)", "Capable peer; Chromium-OSR differentiator; adoption unproven"],
                ["C Emerging", "High engineering-doc / feature intensity; optional"],
            ],
            [2.4 * inch, 3.8 * inch],
        )
    )
    story.append(Spacer(1, 6))
    story.append(
        P(
            "<b>Strengths:</b> Single-DLL Nexus install; private CEF 150; curated site catalog; Account/DPS "
            "Logs/Events/Pathing/Notes pads; stated compliance posture; Windows + Linux via Wine/Proton claimed; "
            "active maintenance on research date. <b>Limitations:</b> Adoption near-zero; absent from major curated "
            "lists; substantial overlap with external browsers and Blish/Taimi pathing; CEF first-run download cost; "
            "Defender false-positive notes for unsigned MinGW DLL; category honesty—a reference shell, not a "
            "foundational meter, loader, or format standard.",
            st["body"],
        )
    )

    # --- §8 Must-have ---
    story.append(P("8. Must-have stacks by player archetype", st["h1"]))
    story.append(
        P(
            "“Must-have” means <b>commonly expected or strongly enabling</b> for that archetype in 2026 community "
            "practice—not marketing. Emerging tools are marked optional and are <b>not</b> elevated to must-have "
            "without evidence.",
            st["body"],
        )
    )
    story.append(P("8.1 New player", st["h3"]))
    story.append(
        bullets(
            [
                "Blish HUD <i>or</i> Nexus (+ optional TaimiHUD for markers)",
                "Skip ArcDPS until comfortable with combat fundamentals",
                "Helper: <b>optional emerging</b> (in-game guides/wiki)",
            ],
            st,
        )
    )
    story.append(P("8.2 Open-world / map completion / metas", st["h3"]))
    story.append(
        bullets(
            [
                "Blish Pathing / Event Table <i>or</i> Nexus + TaimiHUD",
                "Radial (GW2Radial or Nexus/Blish equivalent)",
                "Helper: <b>optional</b> (wiki/guides + pathing packs)",
            ],
            st,
        )
    )
    story.append(P("8.3 Organized PvE (raids / fractals / strikes)", st["h3"]))
    story.append(
        bullets(
            [
                "ArcDPS (<b>expected</b> by many statics for logs)",
                "Elite Insights + dps.report workflow",
                "Unofficial Extras if using dependent plugins; killproof plugin if groups require it",
                "Nexus for update hygiene; Blish optional for timers/pathing",
                "Helper: <b>optional</b> (DPS Logs UI / reference)—not a substitute for ArcDPS",
            ],
            st,
        )
    )
    story.append(P("8.4 Linux (Proton / Wine)", st["h3"]))
    story.append(
        bullets(
            [
                "Nexus; TaimiHUD preferred for markers in recent Linux guides; Burrito as fallback",
                "ArcDPS with extra friction; do not assume Blish Windows parity",
                "Helper: <b>optional</b> if the Proton CEF path works for the user",
            ],
            st,
        )
    )
    story.append(P("8.5 Minimalist", st["h3"]))
    story.append(
        bullets(
            [
                "Nexus alone, or nothing; ArcDPS only if required by groups",
                "Do not stack Blish + TacO + many Nexus modules without need",
                "Helper: <b>not</b> must-have",
            ],
            st,
        )
    )

    # --- §9 Profiles ---
    story.append(PageBreak())
    story.append(P("9. Repository-derived cognitive-engineering profiles", st["h1"]))
    story.append(
        P(
            "<b>Methodological caveat.</b> Profiles describe observable engineering cognition—problem framing, "
            "abstraction preference, documentation style, risk posture, collaboration pattern—as inferred from public "
            "artefacts (sites, READMEs, licenses, release notes, repository trees, shallow-clone shortlogs). They are "
            "<b>not</b> personality diagnoses, clinical assessments, or IQ claims. Uncertainty is high for "
            "closed-source authors. Style labels are artefact-based engineering metaphors only. Data gaps: "
            "GitHub API rate limits; ArcDPS has no public source; Unofficial Extras implementation is closed; "
            "shallow clones undercount historical contributors.",
            st["body"],
        )
    )

    profiles = [
        (
            "9.1 deltaconnected — ArcDPS",
            [
                "<b>Artefacts:</b> Closed distribution at deltaconnected.com/arcdps; DirectX proxy DLL; extension C API; "
                "EVTC docs; dense changelogs (sample through mid-2026).",
                "<b>Problem framing:</b> Accurate combat accounting and a stable ImGui-hosted extension bus under "
                "continuous client churn; limitations framed by what the server does not notify.",
                "<b>Abstraction:</b> Kernel / loader / metrics engine—not a UX aggregator.",
                "<b>Communication:</b> Terse, lowercase, imperative, non-marketing; hedged operational changelogs.",
                "<b>Risk posture:</b> Explicit unsupported / no-warranty warnings; closed binary; graphics-proxy class.",
                "<b>Collaboration:</b> Effectively solo public face; decentralized extension ecosystem.",
                "<b>Label:</b> <i>Systems minimalist / protocol realist</i>. Uncertainty high—source unavailable.",
            ],
        ),
        (
            "9.2 dlamkins (Freesnöw) & Blish HUD core (+ agaertner, entrhopi)",
            [
                "<b>Artefacts:</b> blish-hud/Blish-HUD (C# ~98%, MonoGame, net472); module template; blishhud.com; MIT; "
                "~407★; multi-contributor org; satellites (bhud-pkgs, ArcDPS bridge).",
                "<b>Problem framing:</b> Separate-process, module-extensible overlay so authors need not reinvent "
                "windowing, API keys, input, packaging.",
                "<b>Abstraction:</b> Overlay platform + module runtime (external process).",
                "<b>Communication:</b> Polished user site + structured developer docs; Discord-centric support.",
                "<b>Collaboration:</b> Org-based; dlamkins ≈ platform owner; agaertner ≈ module-ecosystem specialist; "
                "entrhopi ≈ core product engineering—do not collapse into one psychology.",
                "<b>Label:</b> <i>Platform builder / ecosystem gardener</i>.",
            ],
        ),
        (
            "9.3 DeltaRaidcore — Raidcore Nexus",
            [
                "<b>Artefacts:</b> RaidcoreGG/Nexus (C++ ~97%); d3d11.dll proxy; hot-load; Event PubSub; chainload; "
                "All Rights Reserved; CONTRIBUTING refuses external PRs; ~165★; push activity into August 2026.",
                "<b>Problem framing:</b> Host, load, update, and manage addons so developers focus on features.",
                "<b>Abstraction:</b> Loader + framework + addon library.",
                "<b>Communication:</b> Marketing-clear README; wiki; informal release voice atop formal license.",
                "<b>Risk posture:</b> Proxy DLL class; vendor claims of policy-aware design (claim, not audit).",
                "<b>Label:</b> <i>Proprietary platform steward</i>.",
            ],
        ),
        (
            "9.4 Friendly0Fire — GW2Radial, GW2Clarity",
            [
                "<b>Artefacts:</b> C++/HLSL overlays on addon-loader stack; Radial ~369★; Clarity ~36★; FAQ with "
                "ArenaNet dialogue anecdotes; Clarity mode restrictions.",
                "<b>Problem framing:</b> Reduce mount/novelty selection friction; improve buff/skill readability.",
                "<b>Abstraction:</b> Focused feature addons, not hosts.",
                "<b>Communication:</b> Conversational Radial FAQ; terse Clarity README.",
                "<b>Risk posture:</b> Declines deeper game-function hooks for Action Camera automation; Clarity "
                "unavailable in competitive modes.",
                "<b>Label:</b> <i>Interaction designer / constraint negotiator</i>.",
            ],
        ),
        (
            "9.5 TaimiHUD — arcnmx, kittywitch (+ Connicpu in Cargo authors)",
            [
                "<b>Artefacts:</b> Rust workspace; Bevy ECS feature flags; dual extension-arcdps / extension-nexus; "
                "Nix; Fluent i18n; created 2025-06-17; ~49★; CONTRIBUTING LLM exclusion policy.",
                "<b>Problem framing:</b> Cross-host world-space guidance; bridge TacO/Blish-era formats into modern hosts.",
                "<b>Abstraction:</b> Feature addon with engine-like internals.",
                "<b>Communication:</b> Minimal GitHub README; external site; self-aware “User Guide: Hopefully someday?”",
                "<b>Collaboration:</b> Small core; shallow shortlog plurality arcnmx; kittywitch authorship incompletely "
                "evidenced by git shortlog alone (uncertainty).",
                "<b>Label:</b> <i>Systems-leaning feature team / format bridge</i>.",
            ],
        ),
        (
            "9.6 Krappa322 — Unofficial Extras (+ healing); knoxfighter on recent extras releases",
            [
                "<b>Artefacts:</b> Closed extras binary + public Definitions.h; open arcdps_healing_stats (C++, gRPC); "
                "extras releases authored by knoxfighter in mid-2026 sample.",
                "<b>Problem framing:</b> Supply squad/keybind/chat events ArcDPS does not expose; fill healing-metrics gaps.",
                "<b>Abstraction:</b> Infrastructure plugin + feature plugin on Arc ABI.",
                "<b>Communication:</b> Disclaimer-heavy; separates “what it does” vs “what it provides for others.”",
                "<b>Risk posture:</b> Explicit network hooks and runtime modifications; closed source narrated as "
                "compliance with community-manager requests for similar readers.",
                "<b>Label:</b> <i>Extension infrastructure realist</i> (attribute recent extras publishing carefully).",
            ],
        ),
        (
            "9.7 Xydroc-IO — GW2 In-Game Helper",
            [
                "<b>Artefacts:</b> MIT C++ Nexus addon; out-of-process CEF 150 OSR; HLI5 IPC; WHITEPAPER, ARCHITECTURE, "
                "COMPLIANCE, KERNEL, ONBOARDING; version 2.2.0.0; created 2026-07-19; 1★; solo shortlog dominant.",
                "<b>Problem framing:</b> In-game access to guides, tools, official API account data, pathing, and log "
                "review without game CEF writes or game-memory reads.",
                "<b>Abstraction:</b> Hybrid—restricted browser kernel + feature pads on Nexus APIs.",
                "<b>Communication:</b> Exhaustive, formal, compliance-forward; documentation as control surface "
                "(opposite of Arc minimalism).",
                "<b>Risk posture:</b> Explicit Allowed/Forbidden tables; still inherits Nexus inject host risk; large "
                "Chromium attack surface mitigated by process-isolation claims.",
                "<b>Label:</b> <i>Compliance-forward UX aggregator / hybrid browser engineer</i>. Uncertainty medium—"
                "docs rich; adoption outcomes absent. Author affiliation with this packaging context is disclosed in §11.",
            ],
        ),
    ]
    for title, items in profiles:
        story.append(P(title, st["h2"]))
        story.append(bullets(items, st))
        story.append(Spacer(1, 4))

    story.append(P("9.8 Comparative matrix (ordinal within set)", st["h2"]))
    story.append(
        table(
            ["Developer/team", "Platform vs feature", "Openness", "Centrality", "Doc intensity", "Risk"],
            [
                ["deltaconnected", "Platform-kernel", "Closed", "Very high", "Very low (public)", "High (hooks)"],
                ["Blish core", "Platform", "Open (MIT)", "Very high", "High", "Medium (overlay)"],
                ["DeltaRaidcore", "Platform", "Source-visible / ARR", "Very high", "Medium", "Med-high (proxy)"],
                ["Friendly0Fire", "Feature", "Open", "Medium-high", "Medium", "Medium"],
                ["TaimiHUD", "Feature (+ dual host)", "Open", "Rising", "Low–medium", "Medium"],
                ["Krappa322 (+knoxfighter)", "Infra feature", "Closed extras / open healing", "High", "Medium", "High"],
                ["Xydroc-IO", "Hybrid kernel+pads", "Open (MIT)", "Low (new)", "Very high", "Lower hooks; CEF/AV"],
            ],
            [1.35 * inch, 1.15 * inch, 1.2 * inch, 0.85 * inch, 0.95 * inch, 0.7 * inch],
        )
    )
    story.append(P("Table 4. Ordinal comparative matrix of engineering style signals.", st["caption"]))

    # --- §10 IQ ---
    story.append(PageBreak())
    story.append(
        P(
            "10. Speculative CHC-style ranges (non-psychometric; repositories cannot measure IQ)",
            st["h1"],
        )
    )
    story.append(P("10.1 Truth statement (read first)", st["h2"]))
    story.append(
        P(
            "<b>Software repositories, commit histories, documentation corpora, and star counts cannot measure "
            "intelligence quotient (IQ), general intelligence (g), or any clinical/cognitive construct.</b> "
            "No Wechsler, Stanford–Binet, WAIS, Woodcock–Johnson, or other standardized instrument was administered. "
            "No testing conditions, norms, age corrections, or reliability coefficients apply. The ranges below are "
            "<b>speculative engineering proxies mapped onto an IQ-like scale</b> (population mean 100, SD 15 "
            "<i>by analogy only</i>) so that readers who requested “IQ report–style constructs” have an explicitly "
            "labeled, <b>Very Low confidence</b> artefact. Wide intervals are intentional. False precision "
            "(e.g., “128”) is refused. Language such as “genius,” “gifted,” or clinical diagnosis is refused. "
            "<b>Do not cite these numbers as psychometrics.</b>",
            st["warn"],
        )
    )
    story.append(
        table(
            ["Construct", "Usual IQ-report meaning", "Proxy used here (weak)"],
            [
                ["Gf Fluid Reasoning", "Novel problem solving", "Architectural novelty; constraint juggling in design"],
                ["Gc Crystallized Knowledge", "Acquired domain knowledge", "Longevity; API mastery; domain vocabulary in docs"],
                ["Gv Visuospatial", "Spatial / visual processing", "Graphics/UI/overlay/spatial-pathing complexity"],
                ["Gsm Working Memory", "Hold/manipulate information", "Concurrent subsystem / IPC / state-machine signals"],
                ["Gs Processing Speed", "Timed clerical/perceptual speed", "Generally N/A — commit velocity ≠ Gs"],
                ["Gq Quantitative", "Quantitative reasoning", "Metrics, parsing, statistics, numeric protocol work"],
                ["Full-scale heuristic", "Composite IQ analogue", "Midpoint-of-ranges intuition only; not FSIQ"],
            ],
            [1.55 * inch, 1.85 * inch, 2.8 * inch],
        )
    )
    story.append(P("Table 5. Construct glossary for §10 (orientation only).", st["caption"]))

    story.append(P("10.2 Estimated ranges by developer / team", st["h2"]))
    story.append(
        P(
            "All intervals are speculative. Confidence: <b>Very Low</b> for every cell. "
            "agaertner / entrhopi are not separately ranged (insufficient isolated public proxies).",
            st["body"],
        )
    )

    iq_blocks = [
        (
            "deltaconnected (ArcDPS)",
            [
                ["Gf", "118–145", "Sustained novel constraint solving; extension ABI design"],
                ["Gc", "125–148", "Multi-year combat-protocol and EVTC domain depth"],
                ["Gv", "112–138", "DXGI/D3D proxy + ImGui-hosted UI surfaces"],
                ["Gsm", "115–140", "Concurrent combat accounting, logging, extension bus"],
                ["Gs", "N/A", "Not inferable from patch-aligned release speed"],
                ["Gq", "120–145", "DPS/boon/CC accounting and log schemas"],
                ["Full-scale heuristic", "118–142", "Wide; closed source inflates uncertainty"],
            ],
        ),
        (
            "dlamkins & Blish HUD core (collaborators not separately scored)",
            [
                ["Gf", "115–140", "Module platform design; cross-cutting overlay services"],
                ["Gc", "118–142", "Long-running .NET/MonoGame GW2 overlay domain"],
                ["Gv", "112–136", "Overlay layout, module UX, content pipeline"],
                ["Gsm", "112–136", "Multi-module runtime coordination"],
                ["Gs", "N/A", "Not inferable"],
                ["Gq", "108–130", "Present but less central than Arc metrics work"],
                ["Full-scale heuristic", "114–136", "Org product; individual differentiation limited"],
            ],
        ),
        (
            "DeltaRaidcore (Nexus)",
            [
                ["Gf", "118–143", "Loader/hot-load/API consolidation under DX proxy constraints"],
                ["Gc", "115–138", "Multi-year addon-host domain knowledge"],
                ["Gv", "110–135", "In-game ImGui manager / UX shell"],
                ["Gsm", "115–140", "Host, loader, update, pub/sub concurrency"],
                ["Gs", "N/A", "Not inferable"],
                ["Gq", "110–132", "Secondary to systems design in public artefacts"],
                ["Full-scale heuristic", "115–138", "Source-visible but governance closed"],
            ],
        ),
        (
            "Friendly0Fire (GW2Radial / GW2Clarity)",
            [
                ["Gf", "112–136", "Input-state machines; conditional radial queuing"],
                ["Gc", "112–135", "Long Radial maintenance; ArenaNet-constraint lore"],
                ["Gv", "118–142", "Radial geometry, HLSL, Clarity grid/atlas work"],
                ["Gsm", "108–132", "Stateful input UX; less platform-scale concurrency"],
                ["Gs", "N/A", "Not inferable"],
                ["Gq", "105–128", "Light relative to meters/parsers"],
                ["Full-scale heuristic", "110–134", "Feature-focused artefact set"],
            ],
        ),
        (
            "TaimiHUD team (arcnmx primary commit signal; kittywitch in authors)",
            [
                ["Gf", "118–142", "Rust ECS pathing engine; dual-host abstraction"],
                ["Gc", "110–134", "Pathing-format bridging; younger than Arc/Blish"],
                ["Gv", "120–145", "World-space markers, trails, spatial overlays"],
                ["Gsm", "115–138", "Engine-like subsystem fan-out"],
                ["Gs", "N/A", "Not inferable"],
                ["Gq", "110–132", "Timers/numerics present; not primary public signal"],
                ["Full-scale heuristic", "115–138", "Team aggregate; roles incompletely separable"],
            ],
        ),
        (
            "Krappa322 (+ knoxfighter on extras releases)",
            [
                ["Gf", "115–140", "Hook/integration design under policy constraints"],
                ["Gc", "115–138", "Arc extension ABI + network-event domain"],
                ["Gv", "105–128", "Secondary (ImGui); less spatial than pathing tools"],
                ["Gsm", "115–140", "Live share / plugin bus complexity (healing + extras)"],
                ["Gs", "N/A", "Not inferable"],
                ["Gq", "118–142", "Healing stats, EVTC enrichment, protobuf/gRPC stacks"],
                ["Full-scale heuristic", "114–138", "Closed extras block code inspection"],
            ],
        ),
        (
            "Xydroc-IO (GW2 In-Game Helper)",
            [
                ["Gf", "115–140", "CEF OSR + IPC + multi-pad integration under Nexus constraints"],
                ["Gc", "108–132", "Strong written GW2/API/compliance corpus; short calendar tenure"],
                ["Gv", "112–136", "ImGui pads + browser compositing; less world-space than Taimi"],
                ["Gsm", "115–140", "Multi-process browser kernel + many feature domains"],
                ["Gs", "N/A", "High commit density ≠ Gs"],
                ["Gq", "108–130", "Log/account numerics secondary to integration work"],
                ["Full-scale heuristic", "112–136", "Doc richness can inflate perceived Gc; adoption unproven"],
            ],
        ),
    ]
    for title, rows in iq_blocks:
        block = [
            P(title, st["h3"]),
            table(["Construct", "Speculative range", "Proxy rationale (weak)"], rows, [1.4 * inch, 1.1 * inch, 3.7 * inch]),
            Spacer(1, 4),
        ]
        story.append(KeepTogether(block))

    story.append(P("10.3 Interpretation rules", st["h2"]))
    story.append(
        bullets(
            [
                "Overlapping intervals mean <b>no ranking by IQ is supported</b>.",
                "Higher Gv for pathing/radial authors vs higher Gq for meter/extras authors is a proxy story, not a test result.",
                "Helper’s ranges do <b>not</b> place it “above” incumbents; Frame A adoption and Frame B category "
                "leadership remain independent conclusions.",
                "If any range is republished, it must retain the Very Low confidence and non-psychometric labels.",
            ],
            st,
        )
    )

    # --- §11–13 ---
    story.append(P("11. Threats to validity", st["h1"]))
    story.append(
        bullets(
            [
                "No install counts; stars ≠ users; ArcDPS cannot be star-ranked.",
                "List corpus incomplete (official forum compilation blocked).",
                "Frame B lacks controlled usability trials.",
                "Frame C proxies favor projects that invest in markdown—may correlate with author style more than player value.",
                "CHC/IQ-like section has <b>no psychometric validity</b>; included only because requested, with maximal caveats.",
                "Author of Helper is affiliated with this report’s packaging context; Frame A/B/C separations, explicit "
                "non-must-have labeling, and honest Frame A exclusion are used to reduce advocacy bias. Readers should "
                "still treat Helper self-metrics cautiously.",
                "Shallow clones and API rate limits undercount contribution graphs.",
            ],
            st,
        )
    )

    story.append(P("12. Reproducibility", st["h1"]))
    story.append(
        P(
            "Re-query listed URLs and GitHub metadata on a chosen research date; rebuild Tables A–C with updated "
            "stars/pushed_at; keep frames separate; do not collapse into a single <i>S</i> if the research question is "
            "quality or newcomer fairness. Cite as: <i>Multi-frame GW2 addon ecosystem report, 3 August 2026</i>, "
            "not as an industry standard. Speculative CHC ranges must be reproduced only with §10.1 intact.",
            st["body"],
        )
    )

    story.append(P("13. References (selected)", st["h1"]))
    refs = [
        "ArenaNet Help Center — Policy: Third-Party Programs.",
        "deltaconnected — arcdps distribution and API README.",
        "blish-hud/Blish-HUD; blishhud.com.",
        "RaidcoreGG/Nexus; raidcore.gg/gw2/nexus.",
        "Dexerto — Best Guild Wars 2 add-ons (2025 update).",
        "Mukluk Labs — GW2 Add-Ons.",
        "Convergence Corp — Addons.",
        "Hardstuck — ArcDPS guide; related Snow Crows logging practice.",
        "BoyC/GW2TacO; Friendly0Fire/GW2Radial &amp; GW2Clarity; TaimiHUD/TaimiHUD; "
        "Krappa322 unofficial extras releases &amp; healing stats; knoxfighter killproof plugin; "
        "baaron4 Elite Insights; AsherGlick/Burrito; PieOrCake/hoard_and_seek &amp; alter_ego; "
        "Xydroc-IO/GW2-InGame-Helper.",
        "Linux community addon practice notes (Bazzite / Universal Blue discourse, January 2026 update sample).",
    ]
    for i, r in enumerate(refs, 1):
        story.append(P(f"{i}. {r}", st["meta"]))

    story.append(Spacer(1, 18))
    story.append(P("<i>End of report.</i>", st["cover_sub"]))

    OUT.parent.mkdir(parents=True, exist_ok=True)
    doc = SimpleDocTemplate(
        str(OUT),
        pagesize=letter,
        leftMargin=0.75 * inch,
        rightMargin=0.75 * inch,
        topMargin=0.7 * inch,
        bottomMargin=0.85 * inch,
        title="GW2 Addon Ecosystem Academic Report (August 2026)",
        author="Independent technical synthesis (GW2 In-Game Helper documentation package)",
        subject="Multi-frame ranking, cognitive-engineering profiles, speculative CHC proxies",
    )
    doc.build(story, onFirstPage=add_page_number, onLaterPages=add_page_number)
    print(f"Wrote {OUT} ({OUT.stat().st_size} bytes)")


if __name__ == "__main__":
    build()
