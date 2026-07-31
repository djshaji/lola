#!/usr/bin/env python3
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests" / "fixtures"


def load_json(path: Path):
    with path.open() as fh:
        return json.load(fh)


def main():
    ratatouille = load_json(FIXTURES / "ratatouille-feature-snapshot.json")
    atom_path = load_json(FIXTURES / "atom-path-fixture.json")

    assert ratatouille["pluginUri"] == "urn:brummer:ratatouille"
    assert ratatouille["requiredFeatures"]
    assert atom_path["pathProperties"]
    assert atom_path["workerFeatures"]
    assert atom_path["stateFeatures"]
    assert atom_path["patchMessages"]

    print("feature fixtures validated")


if __name__ == "__main__":
    main()
