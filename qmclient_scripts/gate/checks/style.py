#!/usr/bin/env python3
"""代码格式干跑检查（fix_style.py）。"""

from __future__ import annotations

import re
from pathlib import Path

from lib import runner
from lib.report import ResultCollector

REPO_ROOT = Path(__file__).resolve().parents[3]


def run(results: ResultCollector, included: list[str], dry_run: bool = False) -> None:
    if dry_run:
        results.add("INFO", "代码格式检查", "DryRun，仅展示命令")
        return
    if not included:
        results.add(
            "WARN",
            "代码格式检查",
            "未收集到改动范围内可供 fix_style.py 检查的首方 C/C++ 文件",
        )
        return
    results.add(
        "INFO", "代码格式检查范围", f"按收敛后的首方源码范围传入 {len(included)} 个文件"
    )
    fix_style = REPO_ROOT / "qmclient_scripts" / "fix_style.py"
    py = runner.resolve_python_cmd()
    if not py:
        results.add("FAIL", "代码格式检查", "未找到可用的 Python")
        return

    batch: list[str] = []
    batch_len = 0
    for file in included:
        file_len = len(file) + 3
        if batch and batch_len + file_len > 6000:
            args = [str(fix_style), "-n"] + batch
            code, out = runner.run(
                [py]
                + (
                    ["-3"]
                    if re.search(r"py\.exe|py$", py, re.IGNORECASE)
                    and not py.lower().endswith("python.exe")
                    else []
                )
                + args,
                title="代码格式干跑检查",
                check=False,
            )
            if code != 0:
                results.add("FAIL", "代码格式干跑检查", out)
                return
            batch = []
            batch_len = 0
        batch.append(file)
        batch_len += file_len
    if batch:
        args = [str(fix_style), "-n"] + batch
        code, out = runner.run(
            [py]
            + (
                ["-3"]
                if re.search(r"py\.exe|py$", py, re.IGNORECASE)
                and not py.lower().endswith("python.exe")
                else []
            )
            + args,
            title="代码格式干跑检查",
            check=False,
        )
        if code != 0:
            results.add("FAIL", "代码格式干跑检查", out)
            return
    results.add("PASS", "代码格式检查", "命名风格检查通过")
