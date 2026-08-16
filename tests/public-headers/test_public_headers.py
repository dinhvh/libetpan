#!/usr/bin/env python3

import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / "tools/public_headers.py"
SPEC = importlib.util.spec_from_file_location("public_headers", SCRIPT)
public_headers = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = public_headers
SPEC.loader.exec_module(public_headers)


class VariableParserTests(unittest.TestCase):
    def test_parses_multiline_header_variables_and_condition(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "Makefile.am"
            path.write_text(
                "etpaninclude_HEADERS = \\\n\talpha.h \\\n\tbeta.h\n"
                "noinst_HEADERS = private.h\n"
                "etpaninclude_CONDITION = defined(HAVE_ALPHA) || HAVE_BETA\n"
            )
            data = public_headers.parse_variable_blocks(path)
            self.assertEqual(data.blocks[public_headers.PUBLIC_VAR].values, ["alpha.h", "beta.h"])
            self.assertEqual(data.blocks[public_headers.PRIVATE_VAR].values, ["private.h"])
            self.assertEqual(data.condition, "defined(HAVE_ALPHA) || HAVE_BETA")

    def test_rewrites_only_recognized_variable_blocks(self):
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary) / "Makefile.am"
            path.write_text("etpaninclude_HEADERS = z.h a.h\n\nlibfoo_la_SOURCES = foo.c\n")
            data = public_headers.parse_variable_blocks(path)
            result = public_headers.rewrite_variables(
                data, {public_headers.PUBLIC_VAR: {"a.h", "b.h"}}
            ).decode("latin-1")
            self.assertIn("etpaninclude_HEADERS = \\\n\ta.h \\\n\tb.h\n", result)
            self.assertIn("libfoo_la_SOURCES = foo.c", result)


class IncludeGraphTests(unittest.TestCase):
    def test_closure_handles_cycles(self):
        owner = Path("Makefile.am")
        headers = {
            "a.h": public_headers.Header("a.h", Path("a.h"), owner, 1, {"b.h"}),
            "b.h": public_headers.Header("b.h", Path("b.h"), owner, 0, {"a.h"}),
        }
        errors = []
        self.assertEqual(public_headers.closure({"a.h"}, headers, errors), {"a.h", "b.h"})
        self.assertEqual(errors, [])

    def test_include_parser_accepts_crlf_and_spacing(self):
        data = b" # include <libetpan/alpha.h>\r\n#include\t<libetpan/beta.h>\r\n"
        self.assertEqual(
            {item.decode("ascii") for item in public_headers.INCLUDE_RE.findall(data)},
            {"alpha.h", "beta.h"},
        )

    def test_umbrella_is_output_not_public_evidence(self):
        owner = Path("Makefile.am")
        headers = {
            "libetpan.h": public_headers.Header(
                "libetpan.h", Path("libetpan.h"), owner, 0, {"stale.h"}
            ),
            "public.h": public_headers.Header(
                "public.h", Path("public.h"), owner, 1, {"dependency.h"}
            ),
            "dependency.h": public_headers.Header(
                "dependency.h", Path("dependency.h"), owner, 0, set()
            ),
            "stale.h": public_headers.Header(
                "stale.h", Path("stale.h"), owner, 0, set()
            ),
        }
        errors = []
        result = public_headers.public_header_closure(
            headers, {"libetpan.h"}, "libetpan.h", errors
        )
        self.assertEqual(
            result,
            {
                "libetpan.h",
                "libetpan-config.h",
                "libetpan_version.h",
                "public.h",
                "dependency.h",
            },
        )
        self.assertEqual(errors, [])


class RepositoryIntegrationTests(unittest.TestCase):
    def run_tool(self, *arguments, check=True):
        return subprocess.run(
            [sys.executable, str(SCRIPT), *arguments],
            cwd=ROOT,
            check=check,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

    def test_current_static_public_set_matches_audited_result(self):
        result = self.run_tool("list")
        names = result.stdout.splitlines()
        self.assertEqual(len(names), 146)
        self.assertEqual(names, sorted(names, key=lambda value: value.encode()))
        self.assertIn("mailjmap.h", names)

    def test_audit_is_clean_after_migration(self):
        result = self.run_tool("audit", "--format", "json")
        actions = json.loads(result.stdout)
        self.assertEqual(actions["makefile_edits"], [])
        self.assertEqual(actions["umbrella"]["add"], [])
        self.assertEqual(actions["umbrella"]["remove"], [])
        self.assertFalse(actions["umbrella"]["rewrite"])
        self.assertEqual(actions["diagnostics"], [])

    def test_windows_generation_is_stable_and_platform_specific(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "headers.list"
            self.run_tool("generate-windows-list", "--output", str(output))
            rows = output.read_text().splitlines()
            self.assertEqual(rows, sorted(set(rows), key=lambda value: value.encode()))
            self.assertIn("src\\windows\\win_etpan.h", rows)
            self.assertIn("build-windows\\libetpan-config.h", rows)


if __name__ == "__main__":
    unittest.main()
