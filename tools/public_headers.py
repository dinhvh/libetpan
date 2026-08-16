#!/usr/bin/env python3
"""Audit, validate, and export libEtPan public headers."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import sys
import tempfile
from collections import defaultdict, deque
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable


PUBLIC_VAR = "etpaninclude_HEADERS"
PRIVATE_VAR = "noinst_HEADERS"
PROTECTED_VAR = "etpanpublic_noexport"
CONDITION_VAR = "etpaninclude_CONDITION"
HEADER_VARS = (PUBLIC_VAR, PRIVATE_VAR, PROTECTED_VAR)
GENERATED_HEADERS = {"libetpan-config.h", "libetpan_version.h"}
INCLUDE_RE = re.compile(
    rb"^[ \t]*#[ \t]*include[ \t]*[<\"]libetpan/([^>\"\r\n]+\.h)[>\"]",
    re.MULTILINE,
)
EXPORT_RE = re.compile(rb"\bLIBETPAN_EXPORT\b")
ASSIGN_RE = re.compile(r"^([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*)$")


class PublicHeadersError(RuntimeError):
    pass


@dataclass
class VariableBlock:
    name: str
    start: int
    end: int
    values: list[str]


@dataclass
class MakefileData:
    path: Path
    text: str
    blocks: dict[str, VariableBlock] = field(default_factory=dict)
    condition: str | None = None


@dataclass
class Header:
    name: str
    path: Path
    owner: Path
    exports: int
    includes: set[str]


@dataclass
class Model:
    root: Path
    build_root: Path
    umbrella: Path
    makefiles: dict[Path, MakefileData]
    headers: dict[str, Header]
    public: set[str]
    private: set[str]
    protected: set[str]
    proposed: set[str]
    direct_umbrella: set[str]
    transitive_umbrella: set[str]
    duplicate_public: dict[str, list[Path]]
    public_owners: dict[str, Path]
    diagnostics: list[str]


def decode(path: Path) -> str:
    return path.read_bytes().decode("latin-1")


def parse_variable_blocks(path: Path) -> MakefileData:
    text = decode(path)
    lines = text.splitlines(keepends=True)
    result = MakefileData(path=path, text=text)
    index = 0
    while index < len(lines):
        match = ASSIGN_RE.match(lines[index].rstrip("\r\n"))
        if not match:
            index += 1
            continue
        name, first = match.groups()
        start = index
        logical = first
        while lines[index].rstrip("\r\n").rstrip().endswith("\\"):
            index += 1
            if index >= len(lines):
                raise PublicHeadersError(f"unterminated assignment in {path}:{start + 1}")
            logical += " " + lines[index].strip()
        logical = logical.replace("\\", " ")
        values = [token for token in logical.split() if token and not token.startswith("#")]
        if name in HEADER_VARS:
            if name in result.blocks:
                raise PublicHeadersError(f"duplicate {name} assignment in {path}")
            result.blocks[name] = VariableBlock(name, start, index + 1, values)
        elif name == CONDITION_VAR:
            result.condition = logical.strip()
        index += 1
    return result


def distributed_makefiles(root: Path) -> list[Path]:
    ignored = {".git", "build-spm", "build-mac", "build-android", "build-windows"}
    paths = []
    for path in root.rglob("Makefile.am"):
        relative = path.relative_to(root)
        if any(part in ignored for part in relative.parts):
            continue
        paths.append(path)
    return sorted(paths, key=lambda item: item.relative_to(root).as_posix().encode())


def resolve_declared(root: Path, build_root: Path, owner: Path, token: str) -> Path:
    if token == "libetpan-config.h":
        return build_root / token
    candidate = owner.parent / token
    if candidate.exists() or token == "libetpan_version.h":
        return candidate
    raise PublicHeadersError(f"missing declared header {token!r} in {owner}")


def header_bytes(path: Path) -> bytes:
    try:
        return path.read_bytes()
    except OSError as exc:
        raise PublicHeadersError(f"cannot read header {path}: {exc}") from exc


def discover_headers(root: Path, build_root: Path, makefiles: dict[Path, MakefileData]) -> tuple[dict[str, Header], dict[str, Path]]:
    candidates: dict[str, list[tuple[Path, Path]]] = defaultdict(list)
    owner_by_path: dict[Path, Path] = {}
    for makefile_path, data in makefiles.items():
        for variable in HEADER_VARS:
            block = data.blocks.get(variable)
            if not block:
                continue
            for token in block.values:
                path = resolve_declared(root, build_root, makefile_path, token)
                candidates[path.name].append((path, makefile_path))
                owner_by_path[path.resolve(strict=False)] = makefile_path

    # Audit undeclared headers too, but exclude vendored and generated packaging trees.
    for path in sorted((root / "src").rglob("*.h")):
        resolved = path.resolve()
        if resolved not in owner_by_path:
            owner = nearest_makefile(path.parent, root, makefiles)
            if owner:
                candidates[path.name].append((path, owner))
                owner_by_path[resolved] = owner

    headers: dict[str, Header] = {}
    unique_paths: dict[str, Path] = {}
    for name, entries in sorted(candidates.items()):
        distinct = {}
        for path, owner in entries:
            distinct[path.resolve(strict=False)] = (path, owner)
        if len(distinct) > 1:
            # Ambiguous private basenames are allowed until referenced or declared public.
            continue
        path, owner = next(iter(distinct.values()))
        if not path.exists():
            if name in GENERATED_HEADERS:
                continue
            raise PublicHeadersError(f"missing header {path}")
        data = header_bytes(path)
        headers[name] = Header(
            name=name,
            path=path,
            owner=owner,
            exports=len(EXPORT_RE.findall(data)),
            includes={match.decode("ascii") for match in INCLUDE_RE.findall(data)},
        )
        unique_paths[name] = path
    return headers, unique_paths


def nearest_makefile(directory: Path, root: Path, makefiles: dict[Path, MakefileData]) -> Path | None:
    current = directory
    while current == root or root in current.parents:
        candidate = current / "Makefile.am"
        if candidate in makefiles:
            return candidate
        if current == root:
            break
        current = current.parent
    return None


def declared_sets(root: Path, build_root: Path, makefiles: dict[Path, MakefileData]):
    values: dict[str, list[tuple[Path, Path]]] = {name: [] for name in HEADER_VARS}
    for makefile_path, data in makefiles.items():
        for variable in HEADER_VARS:
            block = data.blocks.get(variable)
            if not block:
                continue
            for token in block.values:
                path = resolve_declared(root, build_root, makefile_path, token)
                values[variable].append((path, makefile_path))
    return values


def closure(seeds: Iterable[str], headers: dict[str, Header], errors: list[str]) -> set[str]:
    reached = set(seeds)
    queue = deque(sorted(reached))
    while queue:
        name = queue.popleft()
        header = headers.get(name)
        if not header:
            if name not in GENERATED_HEADERS:
                errors.append(f"referenced public header not found uniquely: {name}")
            continue
        for dependency in sorted(header.includes):
            if dependency not in reached:
                reached.add(dependency)
                queue.append(dependency)
    return reached


def public_header_closure(
    headers: dict[str, Header],
    protected: set[str],
    umbrella_name: str,
    errors: list[str],
) -> set[str]:
    """Infer public headers without treating the generated umbrella as evidence."""
    export_seeds = {
        name for name, header in headers.items()
        if header.exports and name != umbrella_name
    }
    seeds = (export_seeds | protected | GENERATED_HEADERS) - {umbrella_name}
    proposed = closure(seeds, headers, errors)
    proposed.add(umbrella_name)
    return proposed


def build_model(args) -> Model:
    root = Path(args.source_root).resolve() if args.source_root else Path(__file__).resolve().parents[1]
    build_root = Path(args.build_root).resolve() if args.build_root else root
    umbrella = Path(args.umbrella).resolve() if args.umbrella else root / "src/main/libetpan.h"
    makefiles = {path: parse_variable_blocks(path) for path in distributed_makefiles(root)}
    declared = declared_sets(root, build_root, makefiles)
    headers, _ = discover_headers(root, build_root, makefiles)
    diagnostics: list[str] = []

    duplicate_public: dict[str, list[Path]] = defaultdict(list)
    public_owners: dict[str, Path] = {}
    for path, owner in declared[PUBLIC_VAR]:
        duplicate_public[path.name].append(owner)
        public_owners.setdefault(path.name, owner)
    duplicates = {name: owners for name, owners in duplicate_public.items() if len(owners) > 1}
    for name, owners in sorted(duplicates.items()):
        diagnostics.append(f"public header declared more than once: {name}: " + ", ".join(map(str, owners)))

    public = {path.name for path, _ in declared[PUBLIC_VAR]}
    private = {path.name for path, _ in declared[PRIVATE_VAR]}
    protected = {path.name for path, _ in declared[PROTECTED_VAR]}
    for name in sorted(public & private):
        diagnostics.append(f"header is both public and noinst: {name}")
    for name in sorted(protected - public):
        diagnostics.append(f"protected no-export header is not public: {name}")

    umbrella_name = umbrella.name
    proposed = public_header_closure(headers, protected, umbrella_name, diagnostics)

    umbrella_data = header_bytes(umbrella)
    direct = {match.decode("ascii") for match in INCLUDE_RE.findall(umbrella_data)}
    transitive = closure(direct, headers, diagnostics)

    return Model(
        root=root,
        build_root=build_root,
        umbrella=umbrella,
        makefiles=makefiles,
        headers=headers,
        public=public,
        private=private,
        protected=protected,
        proposed=proposed,
        direct_umbrella=direct,
        transitive_umbrella=transitive,
        duplicate_public=duplicates,
        public_owners=public_owners,
        diagnostics=diagnostics,
    )


def relative(model: Model, path: Path) -> str:
    try:
        return path.resolve(strict=False).relative_to(model.root).as_posix()
    except ValueError:
        return str(path)


def format_names(model: Model, names: Iterable[str], output_format: str) -> str:
    ordered = sorted(set(names), key=lambda value: value.encode())
    if output_format == "names":
        return "\n".join(ordered) + ("\n" if ordered else "")
    if output_format == "source-paths":
        rows = []
        for name in ordered:
            if name == "libetpan-config.h":
                rows.append(relative(model, model.build_root / name))
            elif name in model.headers:
                rows.append(relative(model, model.headers[name].path))
            else:
                rows.append(name)
        return "\n".join(rows) + ("\n" if rows else "")
    if output_format == "json":
        return json.dumps(ordered, indent=2) + "\n"
    raise PublicHeadersError(f"unsupported format: {output_format}")


def emit(text: str, output: str | None) -> None:
    if output and output != "-":
        atomic_write(Path(output), text.encode("utf-8"))
    else:
        sys.stdout.write(text)


def command_list(model: Model, args) -> int:
    emit(format_names(model, model.public, args.format), args.output)
    return 0


def command_graph(model: Model, args) -> int:
    edges = []
    for name in sorted(model.public):
        for dependency in sorted(model.headers.get(name, Header(name, Path(), Path(), 0, set())).includes):
            if dependency in model.public:
                edges.append({"from": name, "to": dependency})
    if args.format == "json":
        text = json.dumps(edges, indent=2) + "\n"
    else:
        text = "".join(f"{edge['from']} -> {edge['to']}\n" for edge in edges)
    emit(text, args.output)
    return 0


def audit_rows(model: Model):
    all_names = model.public | model.proposed | model.private | model.protected
    for name in sorted(all_names):
        header = model.headers.get(name)
        reasons = []
        if header and header.exports:
            reasons.append(f"{header.exports} LIBETPAN_EXPORT")
        if name in model.protected:
            reasons.append("protected no-export public API")
        incoming = sorted(
            parent for parent, item in model.headers.items()
            if name in item.includes
            and parent in model.proposed
            and parent != model.umbrella.name
        )
        if incoming:
            reasons.append("required by " + ", ".join(incoming))
        if name in model.proposed and name in model.public:
            status = "public"
        elif name in model.proposed:
            status = "promote"
        elif name in model.public:
            status = "demote"
            reasons.append("no public evidence")
        else:
            status = "internal"
        yield {"header": name, "status": status, "reasons": reasons}


def audit_actions(model: Model) -> dict:
    rows = {row["header"]: row for row in audit_rows(model)}
    promote = sorted(model.proposed - model.public, key=lambda value: value.encode())
    demote = sorted(model.public - model.proposed, key=lambda value: value.encode())
    umbrella_add = sorted(
        model.proposed - model.direct_umbrella - {"libetpan.h"},
        key=lambda value: value.encode(),
    )
    umbrella_remove = sorted(model.direct_umbrella - model.proposed, key=lambda value: value.encode())
    makefile_edits = []
    for action, names in (("promote", promote), ("demote", demote)):
        for name in names:
            header = model.headers.get(name)
            owner = model.public_owners.get(name) or (header.owner if header else None)
            makefile_edits.append(
                {
                    "action": action,
                    "header": name,
                    "makefile": relative(model, owner) if owner else None,
                    "reasons": rows[name]["reasons"],
                }
            )
    umbrella_stale = False
    try:
        umbrella_stale = header_bytes(model.umbrella) != render_umbrella(model, model.public)
    except PublicHeadersError:
        umbrella_stale = True
    return {
        "makefile_edits": makefile_edits,
        "umbrella": {
            "path": relative(model, model.umbrella),
            "add": umbrella_add,
            "remove": umbrella_remove,
            "rewrite": umbrella_stale or bool(promote) or bool(demote),
        },
        "diagnostics": list(model.diagnostics),
    }


def command_audit(model: Model, args) -> int:
    actions = audit_actions(model)
    if args.format == "json":
        emit(json.dumps(actions, indent=2) + "\n", args.output)
    else:
        lines = []
        for edit in actions["makefile_edits"]:
            verb = "ADD TO etpaninclude_HEADERS" if edit["action"] == "promote" else "MOVE TO noinst_HEADERS"
            reason = f" ({'; '.join(edit['reasons'])})" if edit["reasons"] else ""
            lines.append(f"{verb}: {edit['header']} in {edit['makefile']}{reason}")
        umbrella = actions["umbrella"]
        for name in umbrella["add"]:
            lines.append(f"ADD UMBRELLA INCLUDE: {name}")
        for name in umbrella["remove"]:
            lines.append(f"REMOVE UMBRELLA INCLUDE: {name}")
        if umbrella["rewrite"] and not umbrella["add"] and not umbrella["remove"]:
            lines.append(f"REWRITE UMBRELLA: {umbrella['path']}")
        lines.extend(f"CONFLICT: {item}" for item in actions["diagnostics"])
        if not lines:
            lines.append("No public header changes required.")
        emit("\n".join(lines) + "\n", args.output)
    if not args.write:
        return 0
    promote = model.proposed - model.public
    demote = model.public - model.proposed if args.force else set()
    if model.public - model.proposed and not args.force:
        print("audit: demotions require --force; applying promotions only", file=sys.stderr)
    apply_classification(model, promote, demote)
    return 0


def render_assignment(name: str, values: Iterable[str]) -> str:
    ordered = sorted(set(values), key=lambda value: value.encode())
    if not ordered:
        return f"{name} =\n"
    if len(ordered) == 1:
        return f"{name} = {ordered[0]}\n"
    body = " \\\n".join(f"\t{value}" for value in ordered)
    return f"{name} = \\\n{body}\n"


def rewrite_variables(data: MakefileData, updates: dict[str, set[str]]) -> bytes:
    lines = data.text.splitlines(keepends=True)
    replacements = []
    append = []
    for name, values in updates.items():
        rendered = render_assignment(name, values)
        block = data.blocks.get(name)
        if block:
            replacements.append((block.start, block.end, rendered))
        elif values:
            append.append(rendered)
    for start, end, rendered in sorted(replacements, reverse=True):
        lines[start:end] = [rendered]
    text = "".join(lines)
    if append:
        if text and not text.endswith("\n"):
            text += "\n"
        text += "\n" + "\n".join(append)
    return text.encode("latin-1")


def render_umbrella(model: Model, public: set[str]) -> bytes:
    raw = header_bytes(model.umbrella)
    text = raw.decode("latin-1")
    lines = text.splitlines(keepends=True)
    include_indexes = [i for i, line in enumerate(lines) if re.match(r"^[ \t]*#[ \t]*include[ \t]*<libetpan/", line)]
    if not include_indexes:
        raise PublicHeadersError(f"no libetpan include region found in {model.umbrella}")
    begin_marker = "/* Public headers: generated by tools/public_headers.py. */"
    end_marker = "/* End generated public headers. */"
    begin_indexes = [i for i, line in enumerate(lines) if line.strip() == begin_marker]
    end_indexes = [i for i, line in enumerate(lines) if line.strip() == end_marker]
    if begin_indexes:
        start = begin_indexes[0]
        end = end_indexes[0] + 1 if end_indexes and end_indexes[0] > start else max(include_indexes) + 1
    else:
        start, end = min(include_indexes), max(include_indexes) + 1
    owners: dict[Path, list[str]] = defaultdict(list)
    for name in public:
        if name == "libetpan.h":
            continue
        owner = model.public_owners.get(name)
        if not owner and name in model.headers:
            owner = model.headers[name].owner
        if owner:
            owners[owner].append(name)
    generated = [begin_marker + "\n"]
    for owner in sorted(owners, key=lambda path: relative(model, path).encode()):
        names = sorted(owners[owner], key=lambda value: value.encode())
        generated.append(f"\n/* {owner.parent.relative_to(model.root).as_posix() or '.'} */\n")
        condition = model.makefiles[owner].condition
        if condition:
            generated.append(f"#if {condition}\n")
        generated.extend(f"#include <libetpan/{name}>\n" for name in names)
        if condition:
            generated.append("#endif\n")
    generated.append("\n" + end_marker + "\n")
    lines[start:end] = generated
    return "".join(lines).encode("latin-1")


def apply_classification(model: Model, promote: set[str], demote: set[str]) -> None:
    updates_by_makefile: dict[Path, dict[str, set[str]]] = {}
    final_public = (model.public | promote) - demote
    all_makefiles = set()
    for name in promote | demote:
        header = model.headers.get(name)
        owner = model.public_owners.get(name) or (header.owner if header else None)
        if not owner:
            raise PublicHeadersError(f"cannot determine Makefile.am owner for {name}")
        all_makefiles.add(owner)
    for owner in all_makefiles:
        data = model.makefiles[owner]
        public_values = set(data.blocks.get(PUBLIC_VAR, VariableBlock(PUBLIC_VAR, 0, 0, [])).values)
        private_values = set(data.blocks.get(PRIVATE_VAR, VariableBlock(PRIVATE_VAR, 0, 0, [])).values)
        for name in promote:
            header = model.headers.get(name)
            if header and header.owner == owner:
                public_values.add(name)
                private_values.discard(name)
        for name in demote:
            if model.public_owners.get(name) == owner:
                public_values.discard(name)
                private_values.add(name)
        updates_by_makefile[owner] = {PUBLIC_VAR: public_values, PRIVATE_VAR: private_values}

    writes: dict[Path, bytes] = {}
    for owner, updates in updates_by_makefile.items():
        writes[owner] = rewrite_variables(model.makefiles[owner], updates)
    # Use final owners so newly promoted headers appear in the generated umbrella.
    for name in promote:
        if name in model.headers:
            model.public_owners[name] = model.headers[name].owner
    writes[model.umbrella] = render_umbrella(model, final_public)
    atomic_write_many(writes)
    print(f"Promote: {len(promote)} headers", file=sys.stderr)
    print(f"Demote:  {len(demote)} headers", file=sys.stderr)
    print(f"Update:  {len(updates_by_makefile)} Makefile.am files", file=sys.stderr)
    print(f"Rewrite: {relative(model, model.umbrella)}", file=sys.stderr)


def check_model(model: Model, include_dir: str | None) -> list[str]:
    errors = list(model.diagnostics)
    for name in sorted(model.proposed - model.public):
        errors.append(f"header should be promoted by audit policy: {name}")
    for name in sorted(model.public - model.proposed):
        errors.append(f"header should be demoted by audit policy: {name}")
    missing_direct = model.public - model.direct_umbrella - {"libetpan.h"}
    extra_direct = model.direct_umbrella - model.public
    for name in sorted(missing_direct):
        errors.append(f"public header not referenced directly by umbrella: {name}")
    for name in sorted(extra_direct):
        errors.append(f"umbrella references non-public header: {name}")
    try:
        if header_bytes(model.umbrella) != render_umbrella(model, model.public):
            errors.append("umbrella generated include region is stale")
    except PublicHeadersError as exc:
        errors.append(str(exc))
    if include_dir:
        directory = Path(include_dir)
        if not directory.is_absolute():
            directory = model.root / directory
        if not directory.is_dir():
            errors.append(f"include directory does not exist: {directory}")
        else:
            actual = {path.name for path in directory.glob("*.h")}
            expected = active_public_headers(model)
            for name in sorted(expected - actual):
                errors.append(f"materialized public header missing: {name}")
            for name in sorted(actual - expected):
                errors.append(f"stale or non-public materialized header: {name}")
            for path in directory.glob("*.h"):
                if path.is_symlink() and not path.exists():
                    errors.append(f"broken public-header symlink: {path}")
    return errors


def active_public_headers(model: Model) -> set[str]:
    config = model.build_root / "libetpan-config.h"
    macros: dict[str, int] = {}
    if config.is_file():
        for name, value in re.findall(
            rb"^[ \t]*#[ \t]*define[ \t]+([A-Za-z_][A-Za-z0-9_]*)[ \t]+([0-9]+)",
            config.read_bytes(),
            re.MULTILINE,
        ):
            macros[name.decode("ascii")] = int(value)
    active = set()
    for name in model.public:
        owner = model.public_owners.get(name)
        if owner:
            owner_relative = owner.relative_to(model.root)
            configured_makefile = model.build_root / owner_relative.with_name("Makefile")
            if not configured_makefile.is_file():
                continue
        condition = model.makefiles[owner].condition if owner else None
        if not condition or evaluate_condition(condition, macros):
            active.add(name)
    return active


def evaluate_condition(condition: str, macros: dict[str, int]) -> bool:
    expression = re.sub(
        r"defined\s*\(\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)",
        lambda match: "1" if macros.get(match.group(1), 0) else "0",
        condition,
    )
    expression = re.sub(
        r"\b[A-Za-z_][A-Za-z0-9_]*\b",
        lambda match: str(macros.get(match.group(0), 0)),
        expression,
    )
    expression = expression.replace("&&", " and ").replace("||", " or ")
    expression = re.sub(r"!(?!=)", " not ", expression)
    if not re.fullmatch(r"[0-9() \t<>=!&|.+\-*/%andornot]+", expression):
        raise PublicHeadersError(f"unsupported public-header condition: {condition}")
    return bool(eval(expression, {"__builtins__": {}}, {}))


def command_check(model: Model, args) -> int:
    errors = check_model(model, args.include_dir)
    if args.format == "json":
        emit(json.dumps({"ok": not errors, "errors": errors}, indent=2) + "\n", args.output)
    elif errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
    if errors:
        return 1
    print(f"public headers OK ({len(model.public)})")
    return 0


def source_for(model: Model, name: str, platform: str) -> Path:
    if platform == "windows":
        if name == "libetpan-config.h":
            return model.root / "build-windows/libetpan-config.h"
        if name == "libetpan_version.h":
            return model.root / "build-windows/libetpan_version.h"
    if name == "libetpan-config.h":
        return model.build_root / name
    header = model.headers.get(name)
    if not header:
        raise PublicHeadersError(f"cannot resolve public header: {name}")
    return header.path


def export_headers(model: Model, destination: Path, platform: str) -> None:
    destination.mkdir(parents=True, exist_ok=True)
    expected = set(model.public)
    for path in destination.glob("*.h"):
        if path.name not in expected:
            path.unlink()
    for name in sorted(expected):
        source = source_for(model, name, platform)
        if not source.is_file():
            raise PublicHeadersError(f"missing source header for export: {source}")
        target = destination / name
        data = source.read_bytes()
        if not target.exists() or target.read_bytes() != data:
            atomic_write(target, data)


def command_export_spm(model: Model, args) -> int:
    destination = Path(args.destination) if args.destination else model.root / "build-spm/include/libetpan"
    export_headers(model, destination, "spm")
    readme = destination / "README.me"
    atomic_write(readme, b"Generated public libEtPan headers. Run tools/public_headers.py export-spm.\n")
    print(f"exported {len(model.public)} headers to {destination}")
    return 0


def command_export_android(model: Model, args) -> int:
    destination = Path(args.destination) if args.destination else model.root / "build-android/include/libetpan"
    export_headers(model, destination, "android")
    print(f"exported {len(model.public)} headers to {destination}")
    return 0


def command_windows(model: Model, args) -> int:
    rows = []
    for name in sorted(model.public - {"libetpan-config.h", "libetpan_version.h"}):
        source = source_for(model, name, "windows")
        rows.append(relative(model, source).replace("/", "\\"))
    rows.extend([
        "src\\windows\\win_etpan.h",
        "build-windows\\libetpan-config.h",
        "build-windows\\libetpan_version.h",
    ])
    text = "\n".join(sorted(set(rows), key=lambda value: value.encode())) + "\n"
    output = args.output or str(model.root / "build-windows/build_headers.list")
    emit(text, output)
    print(f"generated {'stdout' if output == '-' else output}", file=sys.stderr)
    return 0


def atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
        os.replace(temporary, path)
    except Exception:
        try:
            os.unlink(temporary)
        except FileNotFoundError:
            pass
        raise


def atomic_write_many(writes: dict[Path, bytes]) -> None:
    temporaries: dict[Path, str] = {}
    try:
        for path, data in writes.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            fd, temporary = tempfile.mkstemp(prefix=f".{path.name}.", dir=path.parent)
            with os.fdopen(fd, "wb") as stream:
                stream.write(data)
            temporaries[path] = temporary
        for path, temporary in temporaries.items():
            os.replace(temporary, path)
    finally:
        for temporary in temporaries.values():
            try:
                os.unlink(temporary)
            except FileNotFoundError:
                pass


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--source-root")
    result.add_argument("--build-root")
    result.add_argument("--umbrella")
    subparsers = result.add_subparsers(dest="command", required=True)

    def output_options(command, formats=("names", "source-paths", "json")):
        command.add_argument("--format", choices=formats, default="names")
        command.add_argument("--output")

    list_parser = subparsers.add_parser("list")
    output_options(list_parser)
    graph_parser = subparsers.add_parser("graph")
    output_options(graph_parser)
    audit_parser = subparsers.add_parser("audit")
    output_options(audit_parser)
    audit_parser.add_argument("--write", action="store_true")
    audit_parser.add_argument("--force", action="store_true")
    check_parser = subparsers.add_parser("check")
    output_options(check_parser)
    check_parser.add_argument("--include-dir")
    spm_parser = subparsers.add_parser("export-spm")
    spm_parser.add_argument("--destination")
    windows_parser = subparsers.add_parser("generate-windows-list")
    windows_parser.add_argument("--output")
    android_parser = subparsers.add_parser("export-android")
    android_parser.add_argument("--destination")
    return result


def main(argv: list[str] | None = None) -> int:
    args = parser().parse_args(argv)
    if getattr(args, "force", False) and not getattr(args, "write", False):
        raise PublicHeadersError("--force requires audit --write")
    model = build_model(args)
    commands = {
        "list": command_list,
        "graph": command_graph,
        "audit": command_audit,
        "check": command_check,
        "export-spm": command_export_spm,
        "generate-windows-list": command_windows,
        "export-android": command_export_android,
    }
    return commands[args.command](model, args)


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except PublicHeadersError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(2)
