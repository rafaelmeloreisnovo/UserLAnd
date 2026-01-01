import os
import json
import base64
import getpass
from pathlib import Path

import requests

try:
    import keyring
except ImportError:
    keyring = None

from nacl import public, encoding


SERVICE_NAME = "rafaelia_github_pat"
CREDENTIALS_FILE = Path.home() / ".github_pat_rafaelia.json"
SECRETS_FILE = Path.cwd() / ".github_secrets_rafaelia.json"  # onde você guarda os segredos


def save_credentials(username: str, token: str):
    """
    Grava usuário e PAT de forma razoavelmente segura.
    1) Se keyring disponível: usa o keyring do sistema.
    2) Caso contrário, grava em arquivo com permissão 600.
    """
    if keyring is not None:
        keyring.set_password(SERVICE_NAME, "github_username", username)
        keyring.set_password(SERVICE_NAME, "github_pat", token)
        print("Credenciais salvas no keyring do sistema.")
    else:
        data = {"username": username, "token": token}
        with open(CREDENTIALS_FILE, "w", encoding="utf-8") as f:
            json.dump(data, f)
        os.chmod(CREDENTIALS_FILE, 0o600)
        print(f"Credenciais salvas em {CREDENTIALS_FILE} com permissão 600.")


def load_credentials():
    """
    Carrega usuário e PAT. Se não existir, pergunta e salva.
    """
    username = None
    token = None

    if keyring is not None:
        try:
            username = keyring.get_password(SERVICE_NAME, "github_username")
            token = keyring.get_password(SERVICE_NAME, "github_pat")
        except Exception:
            pass

    if not username or not token:
        if CREDENTIALS_FILE.exists():
            with open(CREDENTIALS_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
            username = data.get("username")
            token = data.get("token")

    if not username or not token:
        print("=== Configuração inicial do GitHub PAT ===")
        username = input("GitHub username: ").strip()
        token = getpass.getpass("GitHub PAT (não será exibido): ").strip()
        save_credentials(username, token)

    return username, token


def load_local_secrets():
    """
    Lê segredos de um arquivo JSON local.
    Exemplo de .github_secrets_rafaelia.json:
    {
        "SIGNING_KEY": "conteudo_base64_da_chave_ou_arquivo",
        "SIGNING_PASSWORD": "minha_senha",
        "BUILD_PROFILE": "release"
    }
    """
    if not SECRETS_FILE.exists():
        raise FileNotFoundError(
            f"Arquivo de segredos {SECRETS_FILE} não encontrado. "
            "Crie um JSON com os secrets que deseja enviar."
        )
    with open(SECRETS_FILE, "r", encoding="utf-8") as f:
        data = json.load(f)
    if not isinstance(data, dict):
        raise ValueError("O arquivo de segredos deve conter um objeto JSON {nome: valor}.")
    return data


def github_request(method, url, token, **kwargs):
    headers = kwargs.pop("headers", {})
    headers["Accept"] = "application/vnd.github+json"
    headers["Authorization"] = f"Bearer {token}"
    headers["X-GitHub-Api-Version"] = "2022-11-28"
    return requests.request(method, url, headers=headers, **kwargs)


def list_repositories(username, token):
    """
    Lista repositórios que você possui.
    Você pode ajustar os parâmetros 'affiliation' e 'visibility'
    se quiser incluir orgs, privados, etc.
    """
    repos = []
    page = 1
    while True:
        url = f"https://api.github.com/user/repos?per_page=100&page={page}&affiliation=owner"
        resp = github_request("GET", url, token)
        resp.raise_for_status()
        batch = resp.json()
        if not batch:
            break
        repos.extend(batch)
        page += 1
    print(f"Encontrados {len(repos)} repositórios.")
    return repos


def get_repo_public_key(owner, repo, token):
    """
    Pega a public key do Actions secrets de um repositório.
    """
    url = f"https://api.github.com/repos/{owner}/{repo}/actions/secrets/public-key"
    resp = github_request("GET", url, token)
    resp.raise_for_status()
    data = resp.json()
    return data["key_id"], data["key"]  # key_id (id da key no GitHub) e key (base64)


def encrypt_secret(public_key_b64: str, secret_value: str) -> str:
    """
    Criptografa o valor do secret usando libsodium/SealedBox,
    como o GitHub exige. Retorna base64.
    """
    public_key_bytes = base64.b64decode(public_key_b64)
    pk = public.PublicKey(public_key_bytes, encoder=encoding.RawEncoder)
    sealed_box = public.SealedBox(pk)
    encrypted = sealed_box.encrypt(secret_value.encode("utf-8"))
    return base64.b64encode(encrypted).decode("utf-8")


def put_secret(owner, repo, token, key_id, secret_name, secret_value):
    """
    Cria/atualiza um secret em um repositório específico.
    """
    encrypted_value = encrypt_secret(key_id[1] if isinstance(key_id, tuple) else key_id, secret_value)


def put_secret(owner, repo, token, public_key_id, public_key_b64, secret_name, secret_value):
    encrypted_value = encrypt_secret(public_key_b64, secret_value)
    url = f"https://api.github.com/repos/{owner}/{repo}/actions/secrets/{secret_name}"
    payload = {
        "encrypted_value": encrypted_value,
        "key_id": public_key_id,
    }
    resp = github_request("PUT", url, token, json=payload)
    if resp.status_code in (201, 204):
        print(f"[{owner}/{repo}] Secret {secret_name} atualizado/criado.")
    else:
        print(f"[{owner}/{repo}] ERRO ao criar {secret_name}: {resp.status_code} {resp.text}")


def main():
    username, token = load_credentials()
    secrets = load_local_secrets()

    # Escolha aqui: trabalhar com TODOS os repos ou filtrar por prefixo/organização
    repos = list_repositories(username, token)

    print("\nSegredos que serão enviados:")
    for name in secrets:
        print(f"  - {name}")
    print()

    for repo_info in repos:
        owner = repo_info["owner"]["login"]
        repo_name = repo_info["name"]

        # Se quiser limitar, descomente e ajuste:
        # if not repo_name.startswith("meu_prefixo_"):
        #     continue

        try:
            pk_data = github_request(
                "GET",
                f"https://api.github.com/repos/{owner}/{repo_name}/actions/secrets/public-key",
                token,
            )
            pk_data.raise_for_status()
            pk_json = pk_data.json()
            public_key_id = pk_json["key_id"]
            public_key = pk_json["key"]

            for sec_name, sec_value in secrets.items():
                put_secret(owner, repo_name, token, public_key_id, public_key, sec_name, sec_value)

        except requests.HTTPError as e:
            print(f"[{owner}/{repo_name}] Falha ao obter public key ou enviar secrets: {e}")


if __name__ == "__main__":
    main()
