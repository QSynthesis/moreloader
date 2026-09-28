# Calls a Ghidra bridge endpoint on the live session of moresampler.exe and prints the JSON
# response. The session is resolved by project and program name from the bridge session registry,
# therefore the script remains valid after Ghidra restarts and the port changes. The request
# bypasses the HTTP proxy configured in the environment because the bridge listens on loopback.
#     python scripts/BridgeCall.py /endpoint ['{"json": "body"}'] [--program moresampler.exe]

import argparse
import glob
import json
import os
import sys

import requests

PROJECT = "moresampler_exe_084"


def find_session(program):
    """Returns the newest bridge session record of program in the moresampler project."""
    pattern = os.path.expanduser("~/.config/ghidra-re/bridge-sessions/*.json")
    for path in sorted(glob.glob(pattern), key=os.path.getmtime, reverse=True):
        with open(path, encoding="utf-8") as f:
            session = json.load(f)
        if session.get("project_name") == PROJECT and session.get("program_name") == program:
            return session
    sys.exit(f"No bridge session for {program} in the {PROJECT} project.")


def call(endpoint, body=None, program="moresampler.exe", timeout=1200):
    """Posts body to endpoint and returns the decoded response."""
    session = find_session(program)
    payload = dict(body or {})
    payload["session_id"] = session["session_id"]
    http = requests.Session()
    http.trust_env = False
    response = http.post(
        session["bridge_url"] + endpoint,
        json=payload,
        headers={"Authorization": "Bearer " + session["token"]},
        timeout=timeout,
    )
    if not response.ok:
        sys.exit(f"HTTP {response.status_code}: {response.text}")
    return response.json()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("endpoint")
    parser.add_argument("body", nargs="?", default="{}")
    parser.add_argument("--program", default="moresampler.exe")
    options = parser.parse_args()
    result = call(options.endpoint, json.loads(options.body), options.program)
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
