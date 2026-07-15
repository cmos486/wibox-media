#!/usr/bin/env python3
import csv
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SPEC_ROOT = ROOT / "openspec" / "specs"
MAPPING = ROOT / "openspec" / "test-coverage.tsv"
EVIDENCE = ROOT / "openspec" / "test-evidence.tsv"
VALID_MODES = {"host", "contract", "device"}


def fail(message):
    raise SystemExit(f"OpenSpec test coverage error: {message}")


def read_tsv(path):
    with path.open(newline="", encoding="utf-8") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def read_scenarios():
    scenarios = []
    for spec in sorted(SPEC_ROOT.glob("*/spec.md")):
        capability = spec.parent.name
        requirement = None
        for line in spec.read_text(encoding="utf-8").splitlines():
            if line.startswith("### Requirement:"):
                requirement = line.split(":", 1)[1].strip()
            elif line.startswith("#### Scenario:"):
                if not requirement:
                    fail(f"scenario before requirement in {spec}")
                scenario = line.split(":", 1)[1].strip()
                scenarios.append((capability, requirement, scenario))
    return scenarios


def main():
    evidence_rows = read_tsv(EVIDENCE)
    evidence = {}
    for row in evidence_rows:
        evidence_id = row.get("evidence_id", "").strip()
        if not evidence_id or evidence_id in evidence:
            fail(f"duplicate or empty evidence id {evidence_id!r}")
        mode = row.get("mode", "").strip()
        if mode not in VALID_MODES:
            fail(f"invalid mode {mode!r} for {evidence_id}")
        artifact = ROOT / row.get("artifact", "").strip()
        if not artifact.exists():
            fail(f"missing evidence artifact for {evidence_id}: {artifact}")
        if not row.get("command", "").strip() or not row.get("description", "").strip():
            fail(f"incomplete evidence record {evidence_id}")
        evidence[evidence_id] = row

    expected = read_scenarios()
    expected_set = set(expected)
    if len(expected) != len(expected_set):
        fail("duplicate capability/requirement/scenario tuple in specs")

    mapping_rows = read_tsv(MAPPING)
    mapped = {}
    used = Counter()
    modes = Counter()
    device_only = []
    for row in mapping_rows:
        key = (row.get("capability", "").strip(),
               row.get("requirement", "").strip(),
               row.get("scenario", "").strip())
        if key in mapped:
            fail(f"duplicate mapping {key}")
        ids = [item.strip() for item in row.get("evidence_ids", "").split(",") if item.strip()]
        if not ids:
            fail(f"scenario has no evidence: {key}")
        unknown = [item for item in ids if item not in evidence]
        if unknown:
            fail(f"unknown evidence {unknown} for {key}")
        mapped[key] = ids
        scenario_modes = {evidence[item]["mode"] for item in ids}
        for item in ids:
            used[item] += 1
        for mode in scenario_modes:
            modes[mode] += 1
        if scenario_modes == {"device"}:
            device_only.append(key)

    missing = sorted(expected_set - set(mapped))
    stale = sorted(set(mapped) - expected_set)
    if missing:
        fail("unmapped scenarios:\n  " + "\n  ".join(" | ".join(item) for item in missing))
    if stale:
        fail("stale mappings:\n  " + "\n  ".join(" | ".join(item) for item in stale))

    unused = sorted(set(evidence) - set(used))
    if unused:
        fail(f"unused evidence records: {', '.join(unused)}")

    capabilities = {item[0] for item in expected}
    if len(capabilities) != 18:
        fail(f"expected 18 capabilities, found {len(capabilities)}")
    if len(expected) < 100:
        fail(f"unexpectedly small scenario inventory: {len(expected)}")

    print(f"OpenSpec scenario test coverage OK: {len(expected)} scenarios, "
          f"{len(evidence)} evidence records")
    print(f"Evidence reach: host={modes['host']} contract={modes['contract']} "
          f"device={modes['device']} device-only={len(device_only)}")
    if device_only:
        print("Device-only acceptance scenarios:")
        for key in device_only:
            print("  " + " | ".join(key))


if __name__ == "__main__":
    main()
