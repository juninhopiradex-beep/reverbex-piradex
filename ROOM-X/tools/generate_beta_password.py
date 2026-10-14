#!/usr/bin/env python3
"""
Gera uma nova senha para o ROOM-X by Piradex.

Uso:
  python3 tools/generate_beta_password.py 2027-03-31 [id]     -> senha BETA válida até essa data
  python3 tools/generate_beta_password.py --master [id]       -> senha MASTER (sem validade)

Imprime a senha (para dar ao tester / guardar num local seguro) e a linha a colar
em kKeys no ficheiro Source/licensing/License.cpp. Depois é só fazer push: o
GitHub Actions compila. NUNCA escrevas a senha em ficheiros do repositório.
"""
import datetime
import hashlib
import secrets
import sys

ITER = 60000                     # tem de ser igual a LocalPasswordLicenseProvider::kIterations
ALFA = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"


def nova(prefixo: str) -> str:
    return prefixo + "-" + "-".join("".join(secrets.choice(ALFA) for _ in range(4)) for _ in range(3))


def main() -> None:
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    master = sys.argv[1] == "--master"
    ident = sys.argv[2] if len(sys.argv) > 2 else ("master-" if master else "beta-") + secrets.token_hex(2)
    senha = nova("ROOMX" if master else "BETA")
    salt = secrets.token_bytes(16)
    h = hashlib.pbkdf2_hmac("sha256", senha.encode(), salt, ITER)

    if master:
        print(f"\nSenha MASTER (sem validade):  {senha}\n")
        linha = f'    {{ "{ident}", Kind::Master,\n      "{salt.hex()}",\n      "{h.hex()}", 0, 0, 0 }},'
    else:
        data = datetime.date.fromisoformat(sys.argv[1])
        print(f"\nSenha BETA (válida até {data:%d/%m/%Y}):  {senha}\n")
        linha = f'    {{ "{ident}", Kind::Beta,\n      "{salt.hex()}",\n      "{h.hex()}", {data.year}, {data.month}, {data.day} }},'
    print("Linha para Source/licensing/License.cpp (dentro de kKeys):\n")
    print(linha + "\n")


if __name__ == "__main__":
    main()
