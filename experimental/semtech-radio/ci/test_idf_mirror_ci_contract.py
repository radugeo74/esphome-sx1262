#!/usr/bin/env python3
"""CI lock: mirror fallback changes transport, not ESP-IDF/ESPHome versions."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
wf = (ROOT / ".github/workflows/native-sx126x-adapter.yml").read_text()
assert "ESPHome 2026.10.1" in wf
assert "run: python -m pip install 'esphome==2026.10.1'" in wf
assert "esp_idf_version: v5.5.6" in wf
assert "ESPHOME_IDF_FRAMEWORK_MIRRORS:" in wf
primary = "https://github.com/esphome-libs/esp-idf/releases/download/v{VERSION}/esp-idf-v{VERSION}.tar.xz"
fallback = "https://dl.espressif.com/github_assets/espressif/esp-idf/releases/download/v{VERSION}/esp-idf-v{VERSION}.zip"
assert f'{primary};{fallback}' in wf
assert "IDF_GITHUB_ASSETS: dl.espressif.com/github_assets" in wf
assert "uses: actions/cache@v4" in wf
assert "path: ~/.cache/esphome/idf" in wf
assert "esphome-2026.10.1-idf-5.5.6-py312-v1" in wf
assert "experimental/semtech-radio/ci/test_heltec_lab_source_ref.py" in wf
print("PASS ESP-IDF framework mirrors + toolchain mirror + cache: "
      "ESPHome 2026.10.1 and ESP-IDF 5.5.6 unchanged")
