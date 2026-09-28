# Runs a Ghidra script through the bridge without the 30-second timeout of the ghidra-re CLI.
#     python scripts/BridgeRun.py <script.java> [arg ...] [--program moresampler.exe] [--timeout 1200]

import argparse
import sys

from BridgeCall import call


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("script")
    parser.add_argument("args", nargs="*")
    parser.add_argument("--program", default="moresampler.exe")
    parser.add_argument("--timeout", type=int, default=1200)
    options = parser.parse_args()

    body = {"script": options.script, "write": True, "args": options.args}
    result = call("/script/run", body, options.program, options.timeout).get("result", {})
    print(result.get("output", ""))
    if result.get("error"):
        sys.exit("error: " + result["error"])


if __name__ == "__main__":
    main()
