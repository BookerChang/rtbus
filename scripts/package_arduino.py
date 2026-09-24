#!/usr/bin/env python3
"""Create an Arduino Boards Manager package index for RTDuo."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import shutil
import tarfile
import tempfile
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import quote


REPO_ROOT = Path(__file__).resolve().parents[1]
PLATFORM_FILES = (
    "cores",
    "variants",
    "libraries",
    "system",
    "boards.txt",
    "platform.txt",
    "programmers.txt",
)
DEFAULT_VENDOR = "rtbus"
DEFAULT_ARCHITECTURE = "rtduo"
ARM_ZEPHYR_EABI_WINDOWS_URL = (
    "https://github.com/BookerChang/rtbus/releases/download/"
    "tools-v0.0.1/arm-zephyr-eabi-1.0.1-windows-x86_64.zip"
)


@dataclass(frozen=True)
class ArchiveInfo:
    path: Path
    url: str
    name: str
    checksum: str
    size: str


@dataclass(frozen=True)
class ToolArchive:
    name: str
    version: str
    host: str
    path: Path


@dataclass(frozen=True)
class ToolSystem:
    name: str
    version: str
    host: str
    url: str
    archive_file_name: str
    checksum: str
    size: str


DEFAULT_TOOL_SYSTEMS = (
    ToolSystem(
        name="arm-zephyr-eabi",
        version="1.0.1",
        host="x86_64-mingw32",
        url=ARM_ZEPHYR_EABI_WINDOWS_URL,
        archive_file_name="arm-zephyr-eabi-1.0.1-windows-x86_64.zip",
        checksum="SHA-256:6d5bcaa2b507c2dcf3ad3296a08b58e179f5ba8bca53435e5d92928902db059f",
        size="215653219",
    ),
    ToolSystem(
        name="arm-zephyr-eabi",
        version="1.0.1",
        host="amd64-mingw32",
        url=ARM_ZEPHYR_EABI_WINDOWS_URL,
        archive_file_name="arm-zephyr-eabi-1.0.1-windows-x86_64.zip",
        checksum="SHA-256:6d5bcaa2b507c2dcf3ad3296a08b58e179f5ba8bca53435e5d92928902db059f",
        size="215653219",
    ),
    ToolSystem(
        name="arm-zephyr-eabi",
        version="1.0.1",
        host="x86_64-cygwin",
        url=ARM_ZEPHYR_EABI_WINDOWS_URL,
        archive_file_name="arm-zephyr-eabi-1.0.1-windows-x86_64.zip",
        checksum="SHA-256:6d5bcaa2b507c2dcf3ad3296a08b58e179f5ba8bca53435e5d92928902db059f",
        size="215653219",
    ),
    ToolSystem(
        name="arm-zephyr-eabi",
        version="1.0.1",
        host="i686-mingw32",
        url=ARM_ZEPHYR_EABI_WINDOWS_URL,
        archive_file_name="arm-zephyr-eabi-1.0.1-windows-x86_64.zip",
        checksum="SHA-256:6d5bcaa2b507c2dcf3ad3296a08b58e179f5ba8bca53435e5d92928902db059f",
        size="215653219",
    ),
    ToolSystem(
        name="tcc",
        version="0.9.27",
        host="x86_64-mingw32",
        url="https://download-mirror.savannah.gnu.org/releases/tinycc/tcc-0.9.27-win32-bin.zip",
        archive_file_name="tcc-0.9.27-win32-bin.zip",
        checksum="SHA-256:02e2bfe8c272a549b15e4bfa4507bd7e05304692af1761db6c1e8e88af675651",
        size="483247",
    ),
    ToolSystem(
        name="tcc",
        version="0.9.27",
        host="amd64-mingw32",
        url="https://download-mirror.savannah.gnu.org/releases/tinycc/tcc-0.9.27-win32-bin.zip",
        archive_file_name="tcc-0.9.27-win32-bin.zip",
        checksum="SHA-256:02e2bfe8c272a549b15e4bfa4507bd7e05304692af1761db6c1e8e88af675651",
        size="483247",
    ),
    ToolSystem(
        name="tcc",
        version="0.9.27",
        host="x86_64-cygwin",
        url="https://download-mirror.savannah.gnu.org/releases/tinycc/tcc-0.9.27-win32-bin.zip",
        archive_file_name="tcc-0.9.27-win32-bin.zip",
        checksum="SHA-256:02e2bfe8c272a549b15e4bfa4507bd7e05304692af1761db6c1e8e88af675651",
        size="483247",
    ),
    ToolSystem(
        name="tcc",
        version="0.9.27",
        host="i686-mingw32",
        url="https://download-mirror.savannah.gnu.org/releases/tinycc/tcc-0.9.27-win32-bin.zip",
        archive_file_name="tcc-0.9.27-win32-bin.zip",
        checksum="SHA-256:02e2bfe8c272a549b15e4bfa4507bd7e05304692af1761db6c1e8e88af675651",
        size="483247",
    ),
)


def read_properties(path: Path) -> dict[str, str]:
    props: dict[str, str] = {}

    with path.open("r", encoding="utf-8") as stream:
        for raw_line in stream:
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue

            key, sep, value = line.partition("=")
            if sep:
                props[key.strip()] = value.strip()

    return props


def parse_boards(path: Path) -> list[dict[str, str]]:
    boards: list[dict[str, str]] = []
    pattern = re.compile(r"^[^.]+\.name=(.+)$")

    with path.open("r", encoding="utf-8") as stream:
        for raw_line in stream:
            match = pattern.match(raw_line.strip())
            if match:
                boards.append({"name": match.group(1)})

    return boards


def sha256(path: Path) -> str:
    digest = hashlib.sha256()

    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)

    return digest.hexdigest()


def copy_platform_tree(destination: Path) -> None:
    destination.mkdir(parents=True)

    for name in PLATFORM_FILES:
        source = REPO_ROOT / name
        target = destination / name

        if source.is_dir():
            shutil.copytree(source, target, symlinks=False)
        else:
            shutil.copy2(source, target)

    for optional in ("LICENSE", "NOTICE", "README.md"):
        source = REPO_ROOT / optional
        if source.exists():
            shutil.copy2(source, destination / optional)


def make_tar_gz(source_dir: Path, archive_path: Path, root_name: str) -> None:
    archive_path.parent.mkdir(parents=True, exist_ok=True)

    with tarfile.open(archive_path, "w:gz") as archive:
        for path in sorted(source_dir.rglob("*")):
            archive.add(
                path,
                arcname=Path(root_name) / path.relative_to(source_dir),
                recursive=False,
            )


def url_for(base_url: str, file_name: str) -> str:
    if base_url.endswith("/"):
        return f"{base_url}{quote(file_name)}"
    return f"{base_url}/{quote(file_name)}"


def file_base_url(path: Path) -> str:
    return path.resolve().as_uri()


def archive_info(path: Path, base_url: str) -> ArchiveInfo:
    return ArchiveInfo(
        path=path,
        url=url_for(base_url, path.name),
        name=path.name,
        checksum=f"SHA-256:{sha256(path)}",
        size=str(path.stat().st_size),
    )


def copy_tool_archive(tool: ToolArchive, output_dir: Path) -> ToolArchive:
    source = tool.path.resolve()
    target = output_dir / source.name

    if not source.is_file():
        raise FileNotFoundError(f"tool archive not found: {source}")

    if source != target.resolve():
        shutil.copy2(source, target)

    return ToolArchive(tool.name, tool.version, tool.host, target)


def parse_tool_specs(specs: list[list[str]]) -> list[ToolArchive]:
    tools: list[ToolArchive] = []

    for name, version, host, archive in specs:
        tools.append(ToolArchive(name, version, host, Path(archive)))

    return tools


def tool_definitions(tools: list[ToolArchive], base_url: str) -> list[dict[str, object]]:
    grouped: dict[tuple[str, str], list[dict[str, str]]] = {}

    for tool in tools:
        info = archive_info(tool.path, base_url)
        grouped.setdefault((tool.name, tool.version), []).append(
            {
                "host": tool.host,
                "url": info.url,
                "archiveFileName": info.name,
                "checksum": info.checksum,
                "size": info.size,
            }
        )

    for system in DEFAULT_TOOL_SYSTEMS:
        grouped.setdefault((system.name, system.version), []).append(
            {
                "host": system.host,
                "url": system.url,
                "archiveFileName": system.archive_file_name,
                "checksum": system.checksum,
                "size": system.size,
            }
        )

    definitions: list[dict[str, object]] = []
    for (name, version), group in sorted(grouped.items()):
        definitions.append(
            {
                "name": name,
                "version": version,
                "systems": sorted(group, key=lambda item: item["host"]),
            }
        )

    return definitions


def tool_dependencies(tools: list[ToolArchive], packager: str) -> list[dict[str, str]]:
    seen = {(system.name, system.version) for system in DEFAULT_TOOL_SYSTEMS}
    deps: list[dict[str, str]] = []

    for name, version in sorted(seen):
        deps.append({"packager": packager, "name": name, "version": version})

    for tool in sorted(tools, key=lambda item: (item.name, item.version)):
        key = (tool.name, tool.version)
        if key in seen:
            continue

        seen.add(key)
        deps.append({"packager": packager, "name": tool.name, "version": tool.version})

    return deps


def build_index(
    *,
    vendor: str,
    maintainer: str,
    website_url: str,
    email: str,
    architecture: str,
    platform_name: str,
    version: str,
    platform_archive: ArchiveInfo,
    boards: list[dict[str, str]],
    tools: list[ToolArchive],
    base_url: str,
) -> dict[str, object]:
    platform: dict[str, object] = {
        "name": platform_name,
        "architecture": architecture,
        "version": version,
        "category": "Contributed",
        "help": {"online": website_url},
        "url": platform_archive.url,
        "archiveFileName": platform_archive.name,
        "checksum": platform_archive.checksum,
        "size": platform_archive.size,
        "boards": boards,
    }

    deps = tool_dependencies(tools, vendor)
    if deps:
        platform["toolsDependencies"] = deps

    return {
        "packages": [
            {
                "name": vendor,
                "maintainer": maintainer,
                "websiteURL": website_url,
                "email": email,
                "help": {"online": website_url},
                "platforms": [platform],
                "tools": tool_definitions(tools, base_url),
            }
        ]
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", default="dist/arduino")
    parser.add_argument("--index-file", default="package_rtbus_index.json")
    parser.add_argument("--base-url", default=None)
    parser.add_argument("--vendor", default=DEFAULT_VENDOR)
    parser.add_argument("--architecture", default=DEFAULT_ARCHITECTURE)
    parser.add_argument("--maintainer", default="RTBus contributors")
    parser.add_argument("--website-url", default="https://github.com/BookerChang/rtbus")
    parser.add_argument("--email", default="packages@example.com")
    parser.add_argument(
        "--tool-archive",
        action="append",
        nargs=4,
        metavar=("NAME", "VERSION", "HOST", "ARCHIVE"),
        default=[],
        help=(
            "Add a Boards Manager tool archive. Repeat for each host. "
            "Example: --tool-archive arm-zephyr-eabi 1.0.0 x86_64-pc-linux-gnu dist/arm-zephyr-eabi.tar.gz"
        ),
    )
    args = parser.parse_args()

    output_dir = (REPO_ROOT / args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    base_url = args.base_url or file_base_url(output_dir)

    platform_props = read_properties(REPO_ROOT / "platform.txt")
    version = platform_props.get("version", "0.1.0")
    platform_name = platform_props.get("name", "RTDuo Arduino Platform")
    archive_name = f"{args.architecture}-{version}.tar.gz"
    archive_path = output_dir / archive_name

    with tempfile.TemporaryDirectory(prefix="rtduo-arduino-package-") as temp_name:
        package_root = Path(temp_name) / args.architecture
        copy_platform_tree(package_root)
        make_tar_gz(package_root, archive_path, args.architecture)

    tools = [
        copy_tool_archive(tool, output_dir)
        for tool in parse_tool_specs(args.tool_archive)
    ]

    index = build_index(
        vendor=args.vendor,
        maintainer=args.maintainer,
        website_url=args.website_url,
        email=args.email,
        architecture=args.architecture,
        platform_name=platform_name,
        version=version,
        platform_archive=archive_info(archive_path, base_url),
        boards=parse_boards(REPO_ROOT / "boards.txt"),
        tools=tools,
        base_url=base_url,
    )

    index_path = output_dir / args.index_file
    with index_path.open("w", encoding="utf-8") as stream:
        json.dump(index, stream, indent=2)
        stream.write("\n")

    print(f"Wrote {archive_path.relative_to(REPO_ROOT)}")
    print(f"Wrote {index_path.relative_to(REPO_ROOT)}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
