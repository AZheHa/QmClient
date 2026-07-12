# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3
"""
根据 tag 区间内的 commit 生成 GitHub Release 说明。

默认输出格式：

# 更新日志
## feat
- feat(scope): 中文文本

## fix
- fix(scope): 中文文本

# What's Changed
## feat
- feat(scope): english text

## fix
- fix(scope): english text

优先从 commit body 中读取：
- Release-ZH: ...
- Release-EN: ...

如果缺失，则退回到 commit subject 的 description。
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
COMMIT_SEPARATOR = "\x1e"
FIELD_SEPARATOR = "\x1f"

SUBJECT_RE = re.compile(
    r"^(?P<type>[A-Za-z]+)(?:\((?P<scope>[^)]+)\))?(?P<breaking>!)?[:：]\s*(?P<desc>.+)$"
)
TRAILER_RE = re.compile(r"^Release-(?P<lang>ZH|EN):\s*(?P<text>.+)$")


@dataclass
class CommitNote:
    commit_hash: str
    commit_type: str
    scope: str
    description: str
    release_zh: str
    release_en: str
    group: str

    def format_prefix(self) -> str:
        if self.scope:
            return f"{self.commit_type}({self.scope})"
        return self.commit_type


def run_git(args: list[str]) -> str:
    result = subprocess.run(
        ["git", *args],
        cwd=REPO_ROOT,
        check=True,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    return result.stdout.strip()


def git_ref_exists(ref: str) -> bool:
    result = subprocess.run(
        ["git", "rev-parse", "--verify", "--quiet", ref],
        cwd=REPO_ROOT,
        capture_output=True,
        text=True,
        encoding="utf-8",
    )
    return result.returncode == 0


def normalize_group(commit_type: str) -> str | None:
    commit_type = commit_type.lower()
    if commit_type == "feat":
        return "feat"
    if commit_type in {"fix", "perf", "refactor", "revert"}:
        return "fix"
    return None


def parse_trailers(body: str) -> tuple[str, str]:
    zh = ""
    en = ""
    for line in body.splitlines():
        match = TRAILER_RE.match(line.strip())
        if not match:
            continue
        if match.group("lang") == "ZH":
            zh = match.group("text").strip()
        elif match.group("lang") == "EN":
            en = match.group("text").strip()
    return zh, en


def parse_commit(commit_hash: str, subject: str, body: str) -> CommitNote | None:
    match = SUBJECT_RE.match(subject.strip())
    if not match:
        return None

    commit_type = match.group("type").lower()
    scope = (match.group("scope") or "").strip()
    description = match.group("desc").strip()
    group = normalize_group(commit_type)
    if group is None:
        return None

    release_zh, release_en = parse_trailers(body)
    if not release_zh:
        release_zh = description
    if not release_en:
        release_en = description

    return CommitNote(
        commit_hash=commit_hash,
        commit_type=commit_type,
        scope=scope,
        description=description,
        release_zh=release_zh,
        release_en=release_en,
        group=group,
    )


def resolve_previous_tag(current_tag: str) -> str | None:
    tags = run_git(["tag", "--sort=-creatordate", "--merged", current_tag]).splitlines()
    for tag in tags:
        tag = tag.strip()
        if tag and tag != current_tag:
            return tag
    return None


def collect_notes(current_tag: str, previous_tag: str | None) -> list[CommitNote]:
    revspec = f"{previous_tag}..{current_tag}" if previous_tag else current_tag
    raw = run_git(
        [
            "log",
            "--reverse",
            f"--format=%H{FIELD_SEPARATOR}%s{FIELD_SEPARATOR}%b{COMMIT_SEPARATOR}",
            revspec,
        ]
    )
    notes: list[CommitNote] = []
    for entry in raw.split(COMMIT_SEPARATOR):
        if not entry.strip():
            continue
        parts = entry.split(FIELD_SEPARATOR, 2)
        if len(parts) != 3:
            continue
        note = parse_commit(parts[0].strip(), parts[1].strip(), parts[2])
        if note is not None:
            notes.append(note)
    return notes


def render_section(group: str, notes: list[CommitNote], lang: str) -> list[str]:
    heading = (
        "##feat."
        if group == "feat" and lang == "zh"
        else ("## feat." if group == "feat" else "## fix")
    )
    lines = [heading]
    grouped = [note for note in notes if note.group == group]
    if not grouped:
        lines.append("- None")
        lines.append("")
        return lines

    for note in grouped:
        text = note.release_zh if lang == "zh" else note.release_en
        lines.append(f"- {note.format_prefix()}: {text}")
    lines.append("")
    return lines


def render_markdown(
    version: str, current_tag: str, previous_tag: str | None, notes: list[CommitNote]
) -> str:
    lines: list[str] = []
    lines.append("# 更新日志")
    lines.append(f"> Version: {version}")
    if previous_tag:
        lines.append(f"> Range: {previous_tag}..{current_tag}")
    else:
        lines.append(f"> Range: {current_tag}")
    lines.append("")
    lines.extend(render_section("feat", notes, "zh"))
    lines.extend(render_section("fix", notes, "zh"))
    lines.append("# What's Changed")
    lines.append("")
    lines.extend(render_section("feat", notes, "en"))
    lines.extend(render_section("fix", notes, "en"))
    return "\n".join(lines).rstrip() + "\n"


def resolve_repo_path(path: Path | None) -> Path | None:
    if path is None:
        return None
    if path.is_absolute():
        return path
    return REPO_ROOT / path


def main() -> int:
    parser = argparse.ArgumentParser(description="生成 GitHub Release 说明")
    parser.add_argument("--version", default="UNRELEASED", help="展示用版本号")
    parser.add_argument("--current-tag", required=True, help="当前发布 tag，如 v2.52.3")
    parser.add_argument(
        "--previous-tag", default=None, help="上一个 tag；不传则自动推断"
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="输出 Markdown 文件路径；不传则打印到 stdout",
    )
    args = parser.parse_args()

    if not git_ref_exists(args.current_tag):
        print(
            f"错误：当前 tag/ref 不存在：{args.current_tag}。"
            "CI 场景请传真实 tag；本地预演请改用仓库里已存在的 tag。",
            file=sys.stderr,
        )
        return 2
    if args.previous_tag and not git_ref_exists(args.previous_tag):
        print(f"错误：上一个 tag/ref 不存在：{args.previous_tag}", file=sys.stderr)
        return 2

    previous_tag = args.previous_tag or resolve_previous_tag(args.current_tag)
    output_path = resolve_repo_path(args.output)
    notes = collect_notes(args.current_tag, previous_tag)
    markdown = render_markdown(args.version, args.current_tag, previous_tag, notes)

    if output_path is not None:
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(markdown, encoding="utf-8")
        print(f"已写入发布说明：{output_path}")
    else:
        print(markdown, end="")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
