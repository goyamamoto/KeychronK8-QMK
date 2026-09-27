#!/usr/bin/env python3
"""Writes an SPDX 2.3 JSON SBOM for the K8 release images.

Usage: sbom.py <dist dir> <version> [<toolchain image>]

<version> is the release tag, e.g. QMK-K8BLE-v1.0.0.

Run from the repository root after build_all.sh, in the same toolchain. The
SBOM lists each image with its SHA-256, the source it was built from (this
repository and the submodules compiled into the firmware, at their commits)
and the compiler.
"""
import datetime
import hashlib
import json
import pathlib
import subprocess
import sys
import uuid

REPO_URL = "https://github.com/goyamamoto/KeychronK8-QMK"

# Submodules that end up in the K8 image; the others (LUFA, V-USB, pico-sdk,
# LVGL, googletest) are not compiled for it.
SUBMODULES = {
    "lib/chibios": "https://github.com/qmk/ChibiOS",
    "lib/chibios-contrib": "https://github.com/SonixQMK/ChibiOS-Contrib",
    "lib/printf": "https://github.com/qmk/printf",
}


def run(*cmd):
    return subprocess.run(cmd, check=True, capture_output=True, text=True).stdout.strip()


def spdx_id(name):
    return "SPDXRef-" + "".join(c if c.isalnum() or c in ".-" else "-" for c in name)


def main():
    dist = pathlib.Path(sys.argv[1])
    version = sys.argv[2]
    image = sys.argv[3] if len(sys.argv) > 3 else "NOASSERTION"

    commit = run("git", "rev-parse", "HEAD")
    packages = [{
        "SPDXID": "SPDXRef-KeychronK8-QMK",
        "name": "KeychronK8-QMK",
        "versionInfo": version,
        "downloadLocation": f"git+{REPO_URL}@{commit}",
        "supplier": "Person: Go Yamamoto",
        "licenseDeclared": "GPL-2.0-or-later",
        "licenseConcluded": "NOASSERTION",
        "copyrightText": "NOASSERTION",
        "filesAnalyzed": False,
        "externalRefs": [],
    }]
    relationships = [{
        "spdxElementId": "SPDXRef-DOCUMENT",
        "relationshipType": "DESCRIBES",
        "relatedSpdxElement": "SPDXRef-KeychronK8-QMK",
    }]

    for path, url in SUBMODULES.items():
        sub = run("git", "-C", path, "rev-parse", "HEAD")
        sid = spdx_id(pathlib.Path(path).name)
        packages.append({
            "SPDXID": sid,
            "name": pathlib.Path(path).name,
            "versionInfo": sub,
            "downloadLocation": f"git+{url}@{sub}",
            "licenseDeclared": "NOASSERTION",
            "licenseConcluded": "NOASSERTION",
            "copyrightText": "NOASSERTION",
            "filesAnalyzed": False,
        })
        relationships.append({
            "spdxElementId": sid,
            "relationshipType": "CONTAINED_BY",
            "relatedSpdxElement": "SPDXRef-KeychronK8-QMK",
        })

    gcc = run("arm-none-eabi-gcc", "--version").splitlines()[0]
    packages.append({
        "SPDXID": "SPDXRef-toolchain",
        "name": "arm-none-eabi-gcc",
        "versionInfo": gcc,
        "downloadLocation": "NOASSERTION",
        "comment": f"Toolchain image: {image}",
        "licenseDeclared": "NOASSERTION",
        "licenseConcluded": "NOASSERTION",
        "copyrightText": "NOASSERTION",
        "filesAnalyzed": False,
    })

    for binary in sorted(dist.glob("*.bin")):
        sid = spdx_id(binary.stem)
        packages.append({
            "SPDXID": sid,
            "name": binary.name,
            "versionInfo": version,
            "packageFileName": binary.name,
            "primaryPackagePurpose": "FIRMWARE",
            "downloadLocation": f"{REPO_URL}/releases/download/{version}/{binary.name}",
            "checksums": [{
                "algorithm": "SHA256",
                "checksumValue": hashlib.sha256(binary.read_bytes()).hexdigest(),
            }],
            "licenseDeclared": "GPL-2.0-or-later",
            "licenseConcluded": "NOASSERTION",
            "copyrightText": "NOASSERTION",
            "filesAnalyzed": False,
        })
        relationships += [
            {"spdxElementId": sid, "relationshipType": "GENERATED_FROM",
             "relatedSpdxElement": "SPDXRef-KeychronK8-QMK"},
            {"spdxElementId": "SPDXRef-toolchain", "relationshipType": "BUILD_TOOL_OF",
             "relatedSpdxElement": sid},
        ]

    doc = {
        "spdxVersion": "SPDX-2.3",
        "dataLicense": "CC0-1.0",
        "SPDXID": "SPDXRef-DOCUMENT",
        "name": version,
        "documentNamespace": f"{REPO_URL}/spdx/{version}-{uuid.uuid4()}",
        "creationInfo": {
            "created": datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "creators": ["Tool: keyboards/keychron/k8/release/sbom.py"],
        },
        "packages": packages,
        "relationships": relationships,
    }
    out = dist / f"{version}.spdx.json"
    out.write_text(json.dumps(doc, indent=2) + "\n")
    print(out)


if __name__ == "__main__":
    main()
