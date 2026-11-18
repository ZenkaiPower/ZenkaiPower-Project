"""Create and verify detached RSA/SHA-256 release signatures with OpenSSL."""
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def sign(artifact):
    artifact = Path(artifact)
    key = Path(os.environ.get("ZENKAIPOWER_SIGNING_KEY", ROOT / ".signing/release-private.pem"))
    if not key.is_file():
        raise FileNotFoundError("Release signing key missing: configure ZENKAIPOWER_SIGNING_KEY")
    openssl = shutil.which("openssl")
    if not openssl and os.name == "nt":
        candidate = Path(os.environ.get("ProgramFiles", "C:/Program Files")) / "Git/usr/bin/openssl.exe"
        if candidate.is_file():
            openssl = str(candidate)
    if not openssl:
        raise RuntimeError("OpenSSL is required to sign releases")
    signature = artifact.with_name(artifact.name + ".sig")
    pending = signature.with_name(signature.name + ".tmp")
    try:
        subprocess.run([openssl, "dgst", "-sha256", "-sign", str(key),
                        "-out", str(pending), str(artifact)], check=True)
        subprocess.run([openssl, "dgst", "-sha256", "-verify",
                        str(ROOT / "port/release/signing-public.pem"), "-signature",
                        str(pending), str(artifact)], check=True)
        pending.replace(signature)
        artifact.with_name(artifact.name + ".sha256").unlink(missing_ok=True)
    finally:
        pending.unlink(missing_ok=True)
    return signature


if __name__ == "__main__":
    for filename in sys.argv[1:]:
        print(sign(filename))
