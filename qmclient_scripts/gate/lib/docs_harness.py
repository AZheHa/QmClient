from __future__ import annotations

import re
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import unquote

from lib.agents_sync import sync_files


REPO_ROOT = Path(__file__).resolve().parents[3]

AI_WORKFLOW_ALLOWED_FILES = frozenset(
    {
        "docs/ai-workflow/meta.md",
        "docs/ai-workflow/ddnet-development.md",
        "docs/ai-workflow/verification.md",
        "docs/ai-workflow/review.md",
        "docs/ai-workflow/git-workflow.md",
        "docs/ai-workflow/advanced/README.md",
        "docs/ai-workflow/advanced/feature-introduction.md",
        "docs/ai-workflow/advanced/memory-lifetime.md",
        "docs/ai-workflow/advanced/observability-debugging.md",
        "docs/ai-workflow/advanced/performance-workflow.md",
        "docs/ai-workflow/advanced/perf-system-workflow.md",
        "docs/ai-workflow/advanced/refactor-workflow.md",
        "docs/ai-workflow/advanced/regression-prevention.md",
        "docs/ai-workflow/advanced/safety-security.md",
        "docs/ai-workflow/advanced/threading-jobs.md",
    }
)
SUPERPOWERS_ACTIVITY_DIRS = ("explore", "plans", "specs")
SUPERPOWERS_FORBIDDEN_DIRS = frozenset({"archive", "reports", "reviews"})
SUPERPOWERS_ALLOWED_STATUSES = frozenset({"active", "draft"})
SUPERPOWERS_INDEX_RE = re.compile(
    r"^\s*-\s+\[[^\]]+\]\(((?:explore|plans|specs)/[^)#]+\.md)"
    r"(?:#[^)]+)?\)(?:[：:].*)?$",
    re.MULTILINE,
)
MARKDOWN_LOCAL_DOC_RE = re.compile(r"(?<!!)\[[^\]]+\]\(([^)#]+\.md)(?:#[^)]+)?\)")


@dataclass
class CheckResult:
    ok: bool
    title: str
    detail: str
    blocking: bool = True


def display_path(path: Path, repo_root: Path) -> str:
    return path.relative_to(repo_root).as_posix()


def check_ai_workflow_manifest(repo_root: Path = REPO_ROOT) -> CheckResult:
    workflow_root = repo_root / "docs" / "ai-workflow"
    actual = (
        {
            display_path(path, repo_root)
            for path in workflow_root.rglob("*")
            if path.is_file()
        }
        if workflow_root.exists()
        else set()
    )
    missing = sorted(AI_WORKFLOW_ALLOWED_FILES - actual)
    unexpected = sorted(actual - AI_WORKFLOW_ALLOWED_FILES)
    issues = []
    if missing:
        issues.append(f"缺少: {', '.join(missing)}")
    if unexpected:
        issues.append(f"未登记: {', '.join(unexpected)}")
    return CheckResult(
        not issues,
        "AI workflow 文件清单",
        "; ".join(issues) if issues else f"通过 ({len(actual)} 个文件)",
    )


def superpowers_markdown_files(repo_root: Path) -> set[Path]:
    root = repo_root / "docs" / "superpowers"
    files: set[Path] = set()
    for directory in SUPERPOWERS_ACTIVITY_DIRS:
        activity_root = root / directory
        if activity_root.exists():
            files.update(path for path in activity_root.rglob("*.md") if path.is_file())
    return files


def check_superpowers_layout(repo_root: Path = REPO_ROOT) -> CheckResult:
    root = repo_root / "docs" / "superpowers"
    if not root.is_dir():
        return CheckResult(False, "Superpowers 目录边界", "缺少: docs/superpowers")

    invalid: set[str] = set()
    for path in root.iterdir():
        if path.is_dir() and path.name not in SUPERPOWERS_ACTIVITY_DIRS:
            invalid.add(display_path(path, repo_root))
        elif path.is_file() and path.name != "README.md":
            invalid.add(display_path(path, repo_root))

    for path in root.rglob("*"):
        if path.is_dir() and path.name.casefold() in SUPERPOWERS_FORBIDDEN_DIRS:
            invalid.add(display_path(path, repo_root))
        elif path.is_file() and path.suffix.lower() != ".md":
            invalid.add(display_path(path, repo_root))

    invalid_list = sorted(invalid)
    return CheckResult(
        not invalid_list,
        "Superpowers 目录边界",
        f"禁止路径: {', '.join(invalid_list)}" if invalid_list else "通过",
    )


def read_frontmatter_status(path: Path) -> str | None:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeError):
        return None
    if not lines or lines[0].strip() != "---":
        return None

    closing_index = next(
        (index for index, line in enumerate(lines[1:], start=1) if line == "---"),
        None,
    )
    if closing_index is None:
        return None

    statuses = []
    for line in lines[1:closing_index]:
        if not line or line[0].isspace():
            continue
        stripped = line.strip()
        key, separator, value = stripped.partition(":")
        if separator and key.strip() == "status":
            statuses.append(value.strip().strip("\"'").lower())
    return statuses[0] if len(statuses) == 1 else None


def check_superpowers_statuses(repo_root: Path = REPO_ROOT) -> CheckResult:
    root = repo_root / "docs" / "superpowers"
    readme = root / "README.md"
    documents = superpowers_markdown_files(repo_root)
    if readme.is_file():
        documents.add(readme)

    invalid = []
    for path in sorted(documents):
        status = read_frontmatter_status(path)
        if status not in SUPERPOWERS_ALLOWED_STATUSES:
            shown_status = status if status is not None else "missing"
            invalid.append(f"{display_path(path, repo_root)} (status={shown_status})")
    return CheckResult(
        not invalid,
        "Superpowers 活动状态",
        f"无效状态: {', '.join(invalid)}" if invalid else "通过",
    )


def check_superpowers_index(repo_root: Path = REPO_ROOT) -> CheckResult:
    root = repo_root / "docs" / "superpowers"
    readme = root / "README.md"
    if not readme.is_file():
        return CheckResult(
            False, "Superpowers 活动索引", "缺少: docs/superpowers/README.md"
        )

    try:
        readme_text = readme.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        return CheckResult(False, "Superpowers 活动索引", f"README 无法读取: {error}")

    listed = {
        reference.strip() for reference in SUPERPOWERS_INDEX_RE.findall(readme_text)
    }
    actual = {
        path.relative_to(root).as_posix()
        for path in superpowers_markdown_files(repo_root)
    }
    missing = sorted(listed - actual)
    unlisted = sorted(actual - listed)
    issues = []
    if missing:
        missing_paths = [f"docs/superpowers/{path}" for path in missing]
        issues.append(f"索引目标不存在: {', '.join(missing_paths)}")
    if unlisted:
        unlisted_paths = [f"docs/superpowers/{path}" for path in unlisted]
        issues.append(f"活动文档未登记: {', '.join(unlisted_paths)}")
    return CheckResult(
        not issues,
        "Superpowers 活动索引",
        "; ".join(issues) if issues else f"通过 ({len(actual)} 个活动文档)",
    )


def check_superpowers_references(repo_root: Path = REPO_ROOT) -> CheckResult:
    root = repo_root / "docs" / "superpowers"
    documents = superpowers_markdown_files(repo_root)
    readme = root / "README.md"
    if readme.is_file():
        documents.add(readme)

    repo_root_resolved = repo_root.resolve()
    broken = []
    for source in sorted(documents):
        try:
            text = source.read_text(encoding="utf-8")
        except (OSError, UnicodeError) as error:
            broken.append(f"{display_path(source, repo_root)} (无法读取: {error})")
            continue

        for raw_target in MARKDOWN_LOCAL_DOC_RE.findall(text):
            target = unquote(raw_target.strip())
            if re.match(r"^[A-Za-z][A-Za-z0-9+.-]*:", target):
                continue
            if target.startswith("/"):
                resolved = (repo_root_resolved / target.lstrip("/")).resolve()
            else:
                resolved = (source.parent / target).resolve()
            try:
                shown_target = display_path(resolved, repo_root_resolved)
            except ValueError:
                broken.append(
                    f"{display_path(source, repo_root)} -> {target} (仓库外路径)"
                )
                continue
            if not resolved.is_file():
                broken.append(f"{display_path(source, repo_root)} -> {shown_target}")

    return CheckResult(
        not broken,
        "Superpowers 本地引用",
        f"断链: {', '.join(broken)}" if broken else "通过",
    )


def run_checks(prefer: str = "auto") -> list[CheckResult]:
    results: list[CheckResult] = []

    sync_result = sync_files(prefer)
    results.append(
        CheckResult(sync_result.ok, "AGENTS / CLAUDE 镜像同步", sync_result.detail)
    )
    if not sync_result.ok:
        return results

    results.extend(
        [
            check_ai_workflow_manifest(),
            check_superpowers_layout(),
            check_superpowers_statuses(),
            check_superpowers_index(),
            check_superpowers_references(),
        ]
    )
    return results
