#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Build firmware for all supported mybot ESP32-S3 board profiles.

Display capability and video capability are derived from each board profile:
display boards automatically select the LVGL Kconfig, and a board that adds
the CoreS3 camera source receives the video defaults automatically.

Build directories and merged firmware images are kept together under the
repository's ``releases/`` directory by default.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parent.parent
BOARDS_CMAKE = PROJECT_ROOT / "components/mybot_platform/boards/boards.cmake"
RELEASES_DIR = PROJECT_ROOT / "releases"


def format_elapsed(seconds: float) -> str:
    total = max(0, int(seconds))
    hours, remainder = divmod(total, 3600)
    minutes, seconds = divmod(remainder, 60)
    return f"{hours:02d}:{minutes:02d}:{seconds:02d}"


@dataclass(frozen=True)
class BoardProfile:
    name: str
    has_display: bool
    video_capable: bool


def supported_boards() -> list[BoardProfile]:
    text = BOARDS_CMAKE.read_text(encoding="utf-8")
    names = re.findall(r'^\s+"([^"\n]+)"\s*$', text, re.MULTILINE)
    profiles: list[BoardProfile] = []
    for name in names:
        profile_path = PROJECT_ROOT / "components/mybot_platform/boards" / name / "board.cmake"
        if not profile_path.is_file():
            raise RuntimeError(f"board profile is missing: {profile_path}")
        profile = profile_path.read_text(encoding="utf-8")
        profiles.append(
            BoardProfile(
                name=name,
                has_display="mybot_display_add_" in profile,
                video_capable="cores3_camera_video.c" in profile,
            )
        )
    if not profiles:
        raise RuntimeError(f"no boards found in {BOARDS_CMAKE}")
    return profiles


def idf_command() -> str:
    idf_path = os.environ.get("IDF_PATH")
    if not idf_path:
        raise RuntimeError("ESP-IDF is not sourced; run source /path/to/esp-idf/export.sh first")
    idf = shutil.which("idf.py")
    if not idf:
        raise RuntimeError("idf.py is not in PATH; source the ESP-IDF export script first")
    version = subprocess.run([idf, "--version"], check=True, capture_output=True, text=True).stdout.strip()
    if version != "ESP-IDF v5.5.2":
        raise RuntimeError(f"expected ESP-IDF v5.5.2, got {version}")
    return idf


def run(idf: str, args: list[str], dry_run: bool) -> None:
    command = [idf, *args]
    print(">>>", " ".join(command))
    if dry_run:
        return
    subprocess.run(command, cwd=PROJECT_ROOT, check=True)


def clean_generated_root(path: Path, label: str, dry_run: bool) -> None:
    """Clean one explicitly selected generated-output root before a build batch."""
    path = path.resolve()
    if path == PROJECT_ROOT or path == Path(path.anchor):
        raise RuntimeError(f"refusing to clean unsafe {label} path: {path}")
    print(f"Cleaning {label}: {path}")
    if not dry_run and path.exists():
        if path.is_symlink() or not path.is_dir():
            raise RuntimeError(f"refusing to clean non-directory {label} path: {path}")
        shutil.rmtree(path)
    if not dry_run:
        path.mkdir(parents=True, exist_ok=True)


def variant_name(board: BoardProfile, language: str, video: bool) -> str:
    suffix = "-video" if video else ""
    return f"{board.name}-{language}{suffix}"


def build_variant(
    idf: str,
    board: BoardProfile,
    language: str,
    video: bool,
    build_root: Path,
    output_root: Path,
    dry_run: bool,
) -> Path:
    started_at = time.perf_counter()
    name = variant_name(board, language, video)
    build_dir = build_root / f"build-{name}"
    sdkconfig = build_dir / "sdkconfig"
    output_path = output_root / f"{name}.bin"
    defaults = ["sdkconfig.defaults", "ci/ptime60.defaults"]
    if language == "en-US":
        defaults.append("ci/en-us.defaults")
    if video:
        defaults.append("ci/video.defaults")
    defaults_value = ";".join(defaults)

    base_args = [
        "-B",
        str(build_dir),
        f"-DMYBOT_BOARD={board.name}",
        f"-DSDKCONFIG={sdkconfig}",
        f"-DSDKCONFIG_DEFAULTS={defaults_value}",
    ]
    print(f"\nBuilding {name} (display={'yes' if board.has_display else 'no'}, "
          f"video={'yes' if video else 'no'})")
    try:
        run(idf, [*base_args, "reconfigure"], dry_run)
        run(idf, [*base_args, "build"], dry_run)
        run(idf, [*base_args, "size"], dry_run)
        run(idf, [*base_args, "merge-bin", "-o", str(output_path)], dry_run)
    except Exception:
        print(f"[FAIL] {name} elapsed={format_elapsed(time.perf_counter() - started_at)}")
        raise
    print(f"[OK] {name} elapsed={format_elapsed(time.perf_counter() - started_at)}")
    return output_path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--board",
        action="append",
        help="board profile to build; repeat for multiple boards (default: all)",
    )
    parser.add_argument(
        "--language",
        choices=("zh-CN", "en-US", "both"),
        default="both",
        help="firmware language (default: both)",
    )
    parser.add_argument(
        "--video",
        choices=("auto", "on", "off"),
        default="auto",
        help="video policy: auto enables it on capable boards (default: auto)",
    )
    parser.add_argument(
        "--build-root",
        type=Path,
        default=PROJECT_ROOT / "build",
        help="build directory root (default: repository build/)",
    )
    parser.add_argument(
        "--output-root",
        type=Path,
        default=RELEASES_DIR,
        help="firmware output directory (default: repository releases/)",
    )
    parser.add_argument("--dry-run", action="store_true", help="print commands without building")
    parser.add_argument(
        "--no-clean",
        action="store_true",
        help="keep existing build and releases directories for an incremental build",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    profiles = supported_boards()
    by_name = {profile.name: profile for profile in profiles}
    selected_names = args.board or [profile.name for profile in profiles]
    unknown = sorted(set(selected_names) - set(by_name))
    if unknown:
        raise RuntimeError(f"unsupported board(s): {', '.join(unknown)}")
    selected = [by_name[name] for name in selected_names]
    languages = ("zh-CN", "en-US") if args.language == "both" else (args.language,)

    if args.video == "on" and any(not board.video_capable for board in selected):
        invalid = [board.name for board in selected if not board.video_capable]
        raise RuntimeError(f"video is unsupported on: {', '.join(invalid)}")

    idf = idf_command()
    build_root = args.build_root.expanduser().resolve()
    output_root = args.output_root.expanduser().resolve()
    if not args.no_clean:
        clean_generated_root(build_root, "build", args.dry_run)
        if output_root != build_root:
            clean_generated_root(output_root, "output", args.dry_run)
    elif not args.dry_run:
        build_root.mkdir(parents=True, exist_ok=True)
        output_root.mkdir(parents=True, exist_ok=True)

    outputs: list[Path] = []
    batch_started_at = time.perf_counter()
    try:
        for board in selected:
            for language in languages:
                video = board.video_capable and args.video != "off"
                outputs.append(
                    build_variant(idf, board, language, video, build_root, output_root, args.dry_run)
                )
    finally:
        print(f"\nTotal elapsed: {format_elapsed(time.perf_counter() - batch_started_at)}")

    print(f"\nPlanned variants: {len(outputs)}")
    print(f"Output directory: {output_root}")
    for output in outputs:
        print(f"  {output}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
