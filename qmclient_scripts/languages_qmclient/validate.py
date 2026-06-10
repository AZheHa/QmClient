#!/usr/bin/env python3
"""Validate all generated QmClient language files."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
sys.path.insert(0, os.path.dirname(__file__))

import extract_strings
import generate_all
import twlang_qmclient as twlang

errors = []
langs_dir = os.path.join(
    os.path.dirname(__file__), "..", "..", "data", "qmclient", "languages"
)

extracted_strings = generate_all.read_strings()
current_strings = extract_strings.collect_strings()
if extracted_strings != current_strings:
    errors.append(
        "extracted_strings.txt is out of date. Run "
        "python qmclient_scripts/languages_qmclient/extract_strings.py"
    )

count = 0
for fname in sorted(os.listdir(langs_dir)):
    if not fname.endswith(".txt") or fname in ("index.txt", "README.txt"):
        continue
    fpath = os.path.join(langs_dir, fname)
    try:
        trans = twlang.translations(fpath)
        filename = fname[:-4]
        actual_keys = [key[0] for key in trans]
        expected_keys = generate_all.expected_overlay_keys(extracted_strings, filename)
        if actual_keys != expected_keys:
            actual_set = set(actual_keys)
            expected_set = set(expected_keys)
            missing = sorted(expected_set - actual_set)
            extra = sorted(actual_set - expected_set)
            detail = []
            if missing:
                detail.append(f"missing={missing[:10]}")
            if extra:
                detail.append(f"extra={extra[:10]}")
            if not detail:
                detail.append("order differs")
            raise RuntimeError("; ".join(detail))
        print(f"  OK: {fname} ({len(trans)} entries)")
        count += 1
    except Exception as e:
        errors.append(f"{fname}: {e}")
        print(f"  FAIL: {fname}: {e}")

print()
if errors:
    print(f"{len(errors)} files with errors!")
    sys.exit(1)
else:
    print(f"All {count} language files parse correctly!")
