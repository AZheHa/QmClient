#!/usr/bin/env python3
"""统一更新 QmClient 仓库内的版本定义。"""

from __future__ import annotations

import argparse
import json
import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
VERSION_H_PATH = REPO_ROOT / "src/game/version.h"
DOCS_INFO_PATH = REPO_ROOT / "docs/info.json"
VERSION_RE = re.compile(r"^\d+\.\d+(?:\.\d+)?$")
VERSION_DEFINE_RE = re.compile(
    r'^(#define\s+QMCLIENT_VERSION\s+)"[^"]+"$', re.MULTILINE
)


def configure_stdio() -> None:
    for stream in (sys.stdout, sys.stderr):
        reconfigure = getattr(stream, "reconfigure", None)
        if reconfigure is not None:
            reconfigure(encoding="utf-8", errors="replace")


def normalize_version(version: str | None, tag: str | None) -> str:
    if bool(version) == bool(tag):
        raise ValueError("必须且只能提供 --version 或 --tag 其中之一。")

    raw = version if version is not None else tag
    assert raw is not None
    normalized = raw[1:] if raw[:1] in {"v", "V"} else raw
    if not VERSION_RE.fullmatch(normalized):
        raise ValueError(
            f"版本格式非法：{raw}。期望格式为 X.Y 或 X.Y.Z，tag 可写成 vX.Y.Z。"
        )
    return normalized


def update_version_h(version: str) -> None:
    content = VERSION_H_PATH.read_text(encoding="utf-8")
    updated, count = VERSION_DEFINE_RE.subn(rf'\1"{version}"', content, count=1)
    if count != 1:
        raise RuntimeError("未找到 QMCLIENT_VERSION 宏，无法更新 src/game/version.h。")
    VERSION_H_PATH.write_text(updated, encoding="utf-8")


def update_docs_info(version: str) -> None:
    data = json.loads(DOCS_INFO_PATH.read_text(encoding="utf-8-sig"))
    data["version"] = version
    DOCS_INFO_PATH.write_text(
        json.dumps(data, ensure_ascii=False, indent=4) + "\n", encoding="utf-8"
    )


def main() -> int:
    configure_stdio()

    parser = argparse.ArgumentParser(description="统一更新 QmClient 版本号")
    parser.add_argument("--version", help="目标版本号，如 2.58.1")
    parser.add_argument("--tag", help="目标 tag，如 v2.58.1")
    parser.add_argument(
        "--dry-run", action="store_true", help="只打印解析后的版本，不写回文件"
    )
    args = parser.parse_args()

    normalized = normalize_version(args.version, args.tag)
    print(f"目标版本：{normalized}")
    if args.dry_run:
        print("Dry-run：未写回文件。")
        return 0

    update_version_h(normalized)
    update_docs_info(normalized)
    print(f"已更新：{VERSION_H_PATH}")
    print(f"已更新：{DOCS_INFO_PATH}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
