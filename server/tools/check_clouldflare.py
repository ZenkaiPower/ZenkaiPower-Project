import json
from pathlib import Path
import uuid

p = Path(__file__).resolve().parents[1] / "cloudflare.local.json"

try:
    c = json.loads(p.read_text())
    print("Credentials JSON: OK")

    account = c["account_id"]
    uuid.UUID(c["database_id"])
    token = c.get("deploy_api_token") or c.get("api_token")

    assert isinstance(account, str) and len(account) == 32
    assert isinstance(token, str) and bool(token.strip())

    print("Account ID: OK")
    print("Database UUID: OK")
    print("API token: OK (not displayed)")
except Exception as e:
    print("Credentials check failed:", type(e).__name__)