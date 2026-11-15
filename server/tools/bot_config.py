#!/usr/bin/env python3
"""Generate local Worker bot settings from the operator credential environment."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re

HERE = Path(__file__).resolve().parents[1]


def generate(uid, credential, destination, username=None):
    if username is not None and (not re.fullmatch(r"[A-Za-z][A-Za-z0-9_-]{1,14}[A-Za-z0-9]", username) or
                                 len(re.findall(r"[A-Za-z]", username)) < 3 or username.lower() == "player"):
        raise ValueError("Enter a valid account username.")
    if username is None and not re.fullmatch(r"[1-9][0-9]*", uid or ""):
        raise ValueError("UID must be a positive integer.")
    if not re.fullmatch(r"[a-f0-9]{64}", credential):
        raise ValueError("Set ZENKAIPOWER_BOT_CREDENTIAL to 64 lowercase hex characters first.")
    settings = {("BOT_ACCOUNT_USERNAME" if username is not None else "BOT_ACCOUNT_UID"): (username.lower() if username is not None else uid),
                "BOT_CREDENTIAL_SHA256": hashlib.sha256(credential.encode()).hexdigest()}
    # Refuse accidental replacement: use a new path or explicitly remove the old hash file.
    with destination.open("x", encoding="utf-8") as output:
        json.dump(settings, output, indent=2)
        output.write("\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    identity = parser.add_mutually_exclusive_group(required=True)
    identity.add_argument("--username", help="Existing bot account username, for example SparkingBot")
    identity.add_argument("--uid", help="Legacy numeric account UID")
    parser.add_argument("--output", type=Path, default=HERE / "bot.local.json")
    args = parser.parse_args()
    try:
        generate(args.uid, os.environ.get("ZENKAIPOWER_BOT_CREDENTIAL", ""), args.output, args.username)
    except FileExistsError:
        parser.exit(1, "Configuration already exists. Keep the matching credential or remove the file explicitly before rotation.\n")
    except (ValueError, OSError) as error:
        parser.exit(1, str(error) + "\n")
    print("Saved bot account and credential hash. No raw credential was written.")


if __name__ == "__main__":
    main()
