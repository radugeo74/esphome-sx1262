"""Checks that native ESP-IDF keeps only configured wM-Bus drivers in the ELF.

Run against a completed build, for example:

    $env:WMBUS_TEST_ELF = ".../firmware.elf"
    $env:WMBUS_SELECTED_DRIVERS = "fhkvdataiii,mkradio4"
    pytest tests/test_driver_linker.py

The test is skipped when no ELF is supplied so it remains useful in source-only
test runs.
"""

from __future__ import annotations

import os
import shutil
import subprocess
from pathlib import Path

import pytest


def _anchor(driver: str) -> str:
    return "wmbus_driver_anchor_" + "".join(
        character if character.isalnum() else "_" for character in driver
    )


def _elf_symbols(elf: Path) -> str:
    nm = os.environ.get("WMBUS_NM") or shutil.which("xtensa-esp32s3-elf-nm")
    if not nm:
        pytest.skip("xtensa-esp32s3-elf-nm is not available")
    result = subprocess.run(
        [nm, "-C", str(elf)], check=True, capture_output=True, text=True
    )
    return result.stdout


@pytest.fixture
def built_elf() -> Path:
    value = os.environ.get("WMBUS_TEST_ELF")
    if not value:
        pytest.skip("set WMBUS_TEST_ELF to a completed native ESP-IDF ELF")
    elf = Path(value)
    if not elf.is_file():
        pytest.fail(f"ELF does not exist: {elf}")
    return elf


def test_selected_drivers_are_linked(built_elf: Path) -> None:
    selected = {
        driver.strip()
        for driver in os.environ.get("WMBUS_SELECTED_DRIVERS", "").split(",")
        if driver.strip()
    }
    if not selected:
        pytest.fail("WMBUS_SELECTED_DRIVERS must list the configured drivers")
    symbols = _elf_symbols(built_elf)
    missing = [driver for driver in sorted(selected) if _anchor(driver) not in symbols]
    assert not missing, f"selected driver anchors missing from ELF: {missing}"


def test_unselected_drivers_are_not_linked(built_elf: Path) -> None:
    selected = {
        driver.strip()
        for driver in os.environ.get("WMBUS_SELECTED_DRIVERS", "").split(",")
        if driver.strip()
    }
    source_dir = Path(__file__).parents[1] / "components" / "wmbus_common"
    unused = {
        path.stem.removeprefix("driver_")
        for path in source_dir.glob("driver_*.cc")
    } - selected
    symbols = _elf_symbols(built_elf)
    linked = [driver for driver in sorted(unused) if _anchor(driver) in symbols]
    assert not linked, f"unselected driver anchors found in ELF: {linked}"
