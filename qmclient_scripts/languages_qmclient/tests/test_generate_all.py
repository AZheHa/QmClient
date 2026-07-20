# 请抬头享受阳光｜日子很好 我很我---------致咩子
#!/usr/bin/env python3

import tempfile
import unittest
from pathlib import Path
from unittest import mock

from qmclient_scripts.languages_qmclient import generate_all


class GenerateAllTest(unittest.TestCase):
    def test_runtime_language_path_uses_data_languages_directory(self):
        self.assertEqual(
            generate_all.runtime_language_path("russian").name,
            "russian.txt",
        )

    def test_format_language_entry_escapes_translation_newlines(self):
        self.assertEqual(
            generate_all.format_language_entry(
                "Line one\\nLine two", "ctx", "第一行\n第二行"
            ),
            "[ctx]\nLine one\\nLine two\n== 第一行\\n第二行",
        )

    def test_language_parser_accepts_bracket_prefixed_editor_keys(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "simplified_chinese.txt"
            path.write_text(
                "[Editor]\n"
                "[Ctrl+S] Save the current map.\n"
                "== 按下[Ctrl+S] 保存当前地图.\n",
                encoding="utf-8",
            )

            self.assertEqual(
                generate_all.read_existing_language_entries(path),
                {
                    (
                        "[Ctrl+S] Save the current map.",
                        "Editor",
                    ): "按下[Ctrl+S] 保存当前地图."
                },
            )

    def test_non_chinese_languages_omit_editor_bilingual_keys(self):
        strings = [
            generate_all.SourceString("Open map", "Editor"),
            generate_all.SourceString("Server"),
        ]
        store = {
            "editor": {
                ("Open map", "Editor"): {
                    "simplified_chinese": "打开地图",
                },
            },
            "menus": {
                ("Server", ""): {
                    "russian": "Сервер",
                },
            },
        }
        with mock.patch.object(
            generate_all.i18n_store, "load_language_store", return_value=store
        ):
            self.assertEqual(
                generate_all.generate_language_entries(strings, "russian"),
                [(("Server", ""), "Сервер")],
            )

    def test_simplified_chinese_preserves_editor_translation_verbatim(self):
        strings = [generate_all.SourceString("%dms", "Editor")]
        store = {
            "editor": {
                ("%dms", "Editor"): {
                    "simplified_chinese": "%d毫秒",
                },
            },
        }
        with mock.patch.object(
            generate_all.i18n_store, "load_language_store", return_value=store
        ):
            self.assertEqual(
                generate_all.generate_language_entries(strings, "simplified_chinese"),
                [(("%dms", "Editor"), "%d毫秒")],
            )

    def test_generate_configured_languages_writes_each_language(self):
        strings = [
            generate_all.SourceString("Server"),
            generate_all.SourceString("Clan"),
        ]
        store = {
            "menus": {
                ("Server", ""): {
                    "simplified_chinese": "服务器",
                    "russian": "Сервер",
                },
                ("Clan", ""): {
                    "simplified_chinese": "战队",
                    "russian": "Клан",
                },
            }
        }

        with tempfile.TemporaryDirectory() as tmp:
            out_dir = Path(tmp)
            with (
                mock.patch.object(generate_all, "BASE_LANGUAGES_DIR", out_dir),
                mock.patch.object(
                    generate_all.i18n_store,
                    "load_language_store",
                    return_value=store,
                ),
            ):
                written = generate_all.generate_configured_languages(
                    strings, ["simplified_chinese", "russian"]
                )

            self.assertEqual(written, 2)
            self.assertIn(
                "== 服务器",
                (out_dir / "simplified_chinese.txt").read_text(encoding="utf-8"),
            )
            self.assertIn(
                "== Сервер", (out_dir / "russian.txt").read_text(encoding="utf-8")
            )

    def test_simplified_chinese_generation_drops_stale_runtime_entries(self):
        strings = [generate_all.SourceString("Server")]
        store = {
            "menus": {
                ("Server", ""): {
                    "simplified_chinese": "服务器",
                },
            }
        }

        with tempfile.TemporaryDirectory() as tmp:
            out_dir = Path(tmp)
            (out_dir / "simplified_chinese.txt").write_text(
                "Old notification key\n== 旧通知\n\nServer\n== 旧服务器\n",
                encoding="utf-8",
            )
            with (
                mock.patch.object(generate_all, "BASE_LANGUAGES_DIR", out_dir),
                mock.patch.object(
                    generate_all.i18n_store,
                    "load_language_store",
                    return_value=store,
                ),
            ):
                generate_all.generate_configured_languages(
                    strings, ["simplified_chinese"]
                )

            generated = (out_dir / "simplified_chinese.txt").read_text(encoding="utf-8")
            self.assertIn("Server\n== 服务器", generated)
            self.assertNotIn("Old notification key", generated)

    def test_write_language_file_uses_stable_tiebreaker_for_casefold_equal_keys(self):
        entries = [
            (("Classic next", ""), "经典 Next"),
            (("Classic Next", ""), "经典 next"),
        ]

        with tempfile.TemporaryDirectory() as tmp:
            out_path = Path(tmp) / "simplified_chinese.txt"
            generate_all.write_language_file(out_path, entries, "simplified_chinese")
            generated = out_path.read_text(encoding="utf-8")

        self.assertLess(
            generated.index("Classic Next\n"),
            generated.index("Classic next\n"),
        )

    def test_read_strings_uses_stable_tiebreaker_for_casefold_equal_keys(self):
        with tempfile.TemporaryDirectory() as tmp:
            strings_file = Path(tmp) / "extracted_strings.txt"
            strings_file.write_text(
                "Classic next\nClassic Next\n",
                encoding="utf-8",
                newline="\n",
            )
            with mock.patch.object(generate_all, "STRINGS_FILE", strings_file):
                strings = generate_all.read_strings()

        self.assertEqual(
            [item.key for item in strings],
            ["Classic Next", "Classic next"],
        )


if __name__ == "__main__":
    unittest.main()
