#!/usr/bin/env python3
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests" / "fixtures"
sys.path.insert(0, str(ROOT / "tools"))
import ttl_parse


def load_json(path: Path):
    with path.open() as fh:
        return json.load(fh)


def main():
    ratatouille = load_json(FIXTURES / "ratatouille-feature-snapshot.json")
    atom_path = load_json(FIXTURES / "atom-path-fixture.json")
    parsed = json.loads(ttl_parse.parse_lv2_ttl_to_json(ROOT / "src" / "Ratatouille.lv2"))
    plugin = parsed[0]
    control_port = next(port for port in plugin["ports"] if port["name"] == "CONTROL")
    notify_port = next(port for port in plugin["ports"] if port["name"] == "NOTIFY")

    assert ratatouille["pluginUri"] == "urn:brummer:ratatouille"
    assert ratatouille["requiredFeatures"]
    assert atom_path["pathProperties"]
    assert atom_path["workerFeatures"]
    assert atom_path["stateFeatures"]
    assert atom_path["patchMessages"]
    assert control_port["type"] == "atom"
    assert notify_port["type"] == "atom"

    print("feature fixtures validated")


if __name__ == "__main__":
    main()
