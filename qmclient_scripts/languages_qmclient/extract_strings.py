#!/usr/bin/env python3
"""Extract all unique Localize() literal strings from QmClient source files."""

import os
import re
import sys


EXTRA_LOCALIZE_STRINGS = {
    "%c Team %d",
    "%d players",
    "%d teams",
    "- Save codes in order:",
    "- Save owners in order:",
    "- You have %d saves on this map!",
    "Axiom auto login failed",
    "Axiom auto login failed, retrying",
    "Axiom auto login succeeded",
    "Auto reply",
    "Hold left click for free camera",
    "Live director",
    "No director players available",
    "Pet",
    "QmClient",
    "Save failed!",
    "Team save in progress. You'll be able to load with '/load %s'",
    "Team save in progress. You'll be able to load with '/load %s' if save is successful or with '/load %s' if it fails",
    "Team successfully saved by %s. The database connection failed, using generated save code instead to avoid collisions. Use '/load %s' on %s to continue",
    "Team successfully saved by %s. The database connection failed, using generated save code instead to avoid collisions. Use '/load %s' to continue",
    "Temporary free camera",
    "Trying Axiom auto login",
    "Trying Axiom dummy auto login",
    "Update notice",
    "You are already on the latest version",
    "Your current version is outdated. Please update from the QQ group.",
    "_ or ' ' = blank spacer",
    "a = View angle",
    "c = Player position",
    "d = Prediction latency",
    "f = Frame rate",
    "i = Receive rate",
    "j = Latency jitter",
    "k = Resend loss",
    "l = Local time",
    "n = Prediction latency",
    "o = Send rate",
    "p = Ping latency",
    "q = Connection quality",
    "r = Race time",
    "u = Snapshot latency",
    "v = Velocity",
    "x = DDNet CPU% / total CPU%",
    "y = DDNet memory usage",
    "z = Zoom",
}


def strip_cpp_comments(content):
    """Strip C/C++ comments while preserving string literals."""
    out = []
    i = 0
    length = len(content)
    while i < length:
        ch = content[i]
        if ch == '"':
            out.append(ch)
            i += 1
            while i < length:
                out.append(content[i])
                if content[i] == "\\" and i + 1 < length:
                    i += 1
                    out.append(content[i])
                elif content[i] == '"':
                    i += 1
                    break
                i += 1
        elif ch == "/" and i + 1 < length and content[i + 1] == "/":
            i += 2
            while i < length and content[i] not in "\r\n":
                i += 1
        elif ch == "/" and i + 1 < length and content[i + 1] == "*":
            i += 2
            while i + 1 < length and not (content[i] == "*" and content[i + 1] == "/"):
                if content[i] in "\r\n":
                    out.append(content[i])
                i += 1
            i += 2
        else:
            out.append(ch)
            i += 1
    return "".join(out)


def extract_localize_strings(root_dir):
    """Walk root_dir and extract all literal strings from Localize() calls."""
    strings = set()
    pattern = re.compile(r'Localize\(\s*"((?:[^"\\]|\\.)*)"')

    if os.path.isfile(root_dir):
        paths = [root_dir]
    else:
        paths = []
        for dirpath, dirs, files in os.walk(root_dir):
            dirs.sort()
            for fname in sorted(files):
                if fname.endswith((".cpp", ".h")):
                    paths.append(os.path.join(dirpath, fname))

    for fpath in paths:
        try:
            with open(fpath, "r", encoding="utf-8") as f:
                content = strip_cpp_comments(f.read())
            for m in pattern.finditer(content):
                s = m.group(1)
                strings.add(s)
            strings.update(extract_known_indirect_strings(fpath, content))
        except Exception as e:
            print(f"Error reading {fpath}: {e}", file=sys.stderr)

    # Also scan gameclient.cpp for QmClient-specific strings (language loading etc.)
    return strings


def extract_function_body(content, function_name):
    match = re.search(rf'\b{re.escape(function_name)}\s*\([^)]*\)\s*\{{', content)
    if not match:
        return ""
    start = match.end()
    depth = 1
    i = start
    while i < len(content) and depth > 0:
        if content[i] == "{":
            depth += 1
        elif content[i] == "}":
            depth -= 1
        i += 1
    return content[start : i - 1]


def extract_known_indirect_strings(path, content):
    """Extract project-specific strings that are later passed to Localize(pointer)."""
    strings = set()
    normalized = path.replace("\\", "/")
    string_literal = r'"((?:[^"\\]|\\.)*)"'

    if normalized.endswith("src/game/client/components/qmclient/menus_qmclient.cpp"):
        helper_pattern = re.compile(
            rf'DoFocus(?:SectionLabel|Checkbox)\([^;\n]*,\s*{string_literal}\s*\)'
        )
        strings.update(m.group(1) for m in helper_pattern.finditer(content))

    if normalized.endswith("src/game/client/components/qmclient/monitoring.cpp"):
        for function_name in ("LocalizeGradeSummary", "LocalizeCauseDetail", "GradeBadgeText"):
            body = extract_function_body(content, function_name)
            strings.update(re.findall(rf'return\s+{string_literal}\s*;', body))
        strings.update(
            re.findall(rf'\{{\s*{string_literal}\s*,\s*m_Snapshot\.', content)
        )
        strings.update(
            re.findall(rf'\{{\s*{string_literal}\s*,\s*a[A-Za-z]+Buf\s*\}}', content)
        )

    if normalized.endswith("src/game/client/components/qmclient/hud_notification_static_rules.h"):
        static_rule_pattern = re.compile(
            rf'\bX\(\s*{string_literal}\s*,\s*{string_literal}\s*\)'
        )
        strings.update(match.group(2) for match in static_rule_pattern.finditer(content))

    if normalized.endswith("src/game/client/components/qmclient/hud_notification_catalog.cpp"):
        catalog_pattern = re.compile(
            rf'\{{\s*EServerMessageRoute::[A-Za-z]+,\s*'
            rf'EServerMessageClass::[A-Za-z]+,\s*'
            rf'EServerMessageDomain::[A-Za-z]+,\s*'
            rf'(?:true|false),\s*{string_literal}\s*\}}'
        )
        strings.update(match.group(1) for match in catalog_pattern.finditer(content) if match.group(1))

    if normalized.endswith("src/game/client/components/tclient/statusbar.cpp"):
        body = extract_function_body(content, "ConnectionGradeLabel")
        strings.update(re.findall(rf'return\s+{string_literal}\s*;', body))

    if normalized.endswith("src/game/client/components/tclient/statusbar.h"):
        status_item_pattern = re.compile(
            rf'CStatusItem\([^;]*,\s*{string_literal}\s*,\s*{string_literal}\s*\)',
            re.S,
        )
        for match in status_item_pattern.finditer(content):
            strings.update(group for group in match.groups() if group)

    return strings


def main():
    os.chdir(os.path.dirname(__file__) + "/../..")

    # Extract from qmclient components
    strings = extract_localize_strings("src/game/client/components/qmclient")

    strings |= extract_localize_strings("src/game/client/components/tclient/statusbar.cpp")
    strings |= extract_localize_strings("src/game/client/components/tclient/statusbar.h")
    strings.update(EXTRA_LOCALIZE_STRINGS)

    # Sort
    sorted_strings = sorted(strings)

    # Write output
    outpath = "qmclient_scripts/languages_qmclient/extracted_strings.txt"
    with open(outpath, "w", encoding="utf-8", newline="\n") as f:
        for s in sorted_strings:
            f.write(s + "\n")

    print(f"Extracted {len(sorted_strings)} unique Localize strings to {outpath}")

    # Also print them
    for i, s in enumerate(sorted_strings):
        print(f"  {i + 1:3d}. {s}")


if __name__ == "__main__":
    main()
