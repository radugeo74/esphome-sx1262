#!/usr/bin/env python3
"""Local, secret-safe, fail-closed preflight against user's WORKING YAML.

Usage:
 python tools/preflight_canary.py --reference /local/working.yaml \
     --candidate /local/heltec-xmq-canary-private.yaml

Both files are validated by ESPHome. ESPHome's expanded configuration can
contain passwords, meter IDs and AES keys; it is held in memory only and NEVER
printed or written to disk by this script. Do NOT run with shell tracing.

The only allowed semantic additions are the disabled XMQ shadow component.
Other changes (including node name, radio mode, meter keys, fields, encryption,
OTA, Wi-Fi, and sensor names) fail. External component source declarations are
reviewed separately by a strict allowlist, NOT assumed equivalent by this test.
"""
import argparse
import pathlib
import subprocess
import sys

import yaml

ALLOWED_EXTERNAL_COMPONENTS = frozenset({
    "sx126x", "wmbus_common", "wmbus_radio", "wmbus_meter",
    "xmq_shadow_probe",
})

def validate_and_read(path: pathlib.Path, already_resolved: bool):
    if not path.is_file():
        raise ValueError("configuration file not found")
    if already_resolved:
        # Used in tests only: fixtures contain no confidential data.
        output=path.read_text(encoding="utf-8")
    else:
        # Never echo subprocess stdout/stderr; ESPHome config may print secrets.
        proc=subprocess.run(
            ["esphome", "config", str(path.resolve())],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            cwd=path.parent, timeout=180, check=False,
        )
        if proc.returncode:
            raise ValueError("ESPHome configuration validation failed (details suppressed)")
        output=proc.stdout.decode("utf-8",errors="strict")
    config=yaml.safe_load(output)
    if not isinstance(config,dict):
        raise ValueError("ESPHome config output was not a YAML mapping")
    return config

def external_components(config):
    entries=config.get("external_components", [])
    if entries is None:
        entries=[]
    if not isinstance(entries,list):
        raise ValueError("unexpected external_components configuration")
    names=set()
    for entry in entries:
        if not isinstance(entry,dict) or not isinstance(entry.get("components"),list):
            raise ValueError("external component declaration cannot be audited")
        names.update(str(name) for name in entry["components"])
    if not names.issubset(ALLOWED_EXTERNAL_COMPONENTS):
        raise ValueError("external component not in reviewed allowlist")
    return names

def differences(left,right, path="", budget=16):
    """Only structural paths are returned. No actual values, IDs or keys."""
    if type(left) is not type(right):
        return [path or "<root>"]
    if isinstance(left,dict):
        ans=[]
        for key in sorted(set(left)|set(right),key=str):
            sub=f"{path}.{key}" if path else str(key)
            if key not in left or key not in right:
                ans.append(sub)
            else:
                ans.extend(differences(left[key],right[key],sub,budget))
            if len(ans)>=budget:
                return ans[:budget]
        return ans
    if isinstance(left,list):
        if len(left)!=len(right):
            return [f"{path}[count]"]
        ans=[]
        for i,(a,b) in enumerate(zip(left,right)):
            ans.extend(differences(a,b,f"{path}[{i}]",budget))
            if len(ans)>=budget:
                return ans[:budget]
        return ans
    return [] if left==right else [path]

def check(reference,candidate):
    if "xmq_shadow_probe" in reference:
        raise ValueError("reference is not an original no-XMQ configuration")
    probe=candidate.get("xmq_shadow_probe")
    if not isinstance(probe,dict) or probe.get("enabled") is not False:
        raise ValueError("XMQ must be explicitly disabled for first flash")
    old_sources=external_components(reference)
    new_sources=external_components(candidate)
    if not {"wmbus_meter","wmbus_common","wmbus_radio","sx126x"}.issubset(new_sources):
        raise ValueError("candidate misses a required radio/meter component")
    if "xmq_shadow_probe" not in new_sources:
        raise ValueError("candidate does not load the opt-in shadow component")
    if old_sources-new_sources:
        raise ValueError("candidate drops an original external component")
    # Source refs and locations may legitimately switch to the audited lab
    # clone; they are NOT proven equivalent by this comparison. The dedicated
    # lab clone parity test must also pass and the user must review these refs.
    ignored={"external_components","xmq_shadow_probe"}
    ref={k:v for k,v in reference.items() if k not in ignored}
    can={k:v for k,v in candidate.items() if k not in ignored}
    mismatch=differences(ref,can)
    if mismatch:
        # Paths only; never print values (including meter IDs or AES keys).
        raise ValueError("unintended changes detected at: "+", ".join(mismatch))
    print("PREFLIGHT_OK: operational config identical; shadow explicitly disabled")
    print("MANUAL_GATE: review external component source URLs/paths and verified rollback")
    print("NO_FLASH: comparison does not verify hardware, radio reception or dynamic heap")

def main():
    p=argparse.ArgumentParser(description="Local confidential canary parity gate")
    p.add_argument("--reference",type=pathlib.Path,required=True)
    p.add_argument("--candidate",type=pathlib.Path,required=True)
    p.add_argument("--resolved-fixtures",action="store_true",
                   help=argparse.SUPPRESS)
    args=p.parse_args()
    try:
        old=validate_and_read(args.reference,args.resolved_fixtures)
        new=validate_and_read(args.candidate,args.resolved_fixtures)
        check(old,new)
    except (ValueError, subprocess.TimeoutExpired, yaml.YAMLError, UnicodeError) as err:
        # Values in exceptions are never echoed. Only known safe path phrases.
        msg=str(err) if isinstance(err,ValueError) and (
            str(err).startswith("unintended changes detected at:") or
            str(err) in {
                "configuration file not found",
                "ESPHome configuration validation failed (details suppressed)",
                "ESPHome config output was not a YAML mapping",
                "unexpected external_components configuration",
                "external component declaration cannot be audited",
                "external component not in reviewed allowlist",
                "reference is not an original no-XMQ configuration",
                "XMQ must be explicitly disabled for first flash",
                "candidate misses a required radio/meter component",
                "candidate does not load the opt-in shadow component",
                "candidate drops an original external component",
            }) else "cannot safely validate configuration"
        print("PREFLIGHT_BLOCKED: "+msg,file=sys.stderr)
        return 1
    return 0

if __name__=="__main__":
    raise SystemExit(main())
