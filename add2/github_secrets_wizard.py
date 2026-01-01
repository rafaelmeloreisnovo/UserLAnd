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
            try:
                with open(CREDENTIALS_FILE, "r", encoding="utf-8") as f:
                    data = json.load(f)
                username = data.get("username")
                token = data.get("token")
            except Exception:
                pass

    if not username or not token:
        print("=== Configuração inicial do GitHub PAT ===")
        username = input("GitHub username: ").strip()
        token = getpass.getpass("GitHub PAT (não será exibido): ").strip()
        save_credentials(username, token)

    return username, token


def github_request(method, url, token, **kwargs):
    headers = kwargs.pop("headers", {})
    headers["Accept"] = "application/vnd.github+json"
    headers["Authorization"] = f"Bearer {token}"
    headers["X-GitHub-Api-Version"] = "2022-11-28"
    return requests.request(method, url, headers=headers, **kwargs)


def list_repositories(token):
    """
    Lista repositórios que você possui (owner).
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
    return repos


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


def put_secret(owner, repo, token, public_key_id, public_key_b64, secret_name, secret_value):
    """
    Cria/atualiza um secret em um repositório específico.
    """
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


def choose_repositories(repos):
    """
    Pergunta se aplica em todos os repositórios ou em alguns.
    Retorna a lista de repositórios selecionados.
    """
    print("\n=== Repositórios encontrados ===")
    for i, r in enumerate(repos, start=1):
        print(f"{i:3d}) {r['owner']['login']}/{r['name']}")

    if not repos:
        print("Nenhum repositório encontrado para este usuário (affiliation=owner).")
        return []

    print("\nOpções:")
    print("  1 - Usar TODOS os repositórios da lista")
    print("  2 - Escolher alguns pelo número")

    choice = input("Escolha (1 ou 2) [1]: ").strip() or "1"

    if choice == "1":
        return repos

    # escolha manual
    idx_str = input("Informe os números separados por vírgula (ex: 1,3,5): ").strip()
    if not idx_str:
        return []

    indices = set()
    for part in idx_str.split(","):
        part = part.strip()
        if not part:
            continue
        try:
            n = int(part)
            if 1 <= n <= len(repos):
                indices.add(n - 1)
        except ValueError:
            pass

    selected = [repos[i] for i in sorted(indices)]
    print(f"{len(selected)} repositório(s) selecionado(s).")
    return selected


def collect_secrets_interactively():
    """
    Faz perguntas para definir N secrets.
    Permite valor a partir de texto ou arquivo (convertido em base64).
    Retorna dict {nome: valor}.
    """
    secrets = {}
    print("\n=== Assistente de criação de secrets ===")
    print("Para terminar, deixe o nome do secret em branco e pressione ENTER.\n")

    while True:
        name = input("Nome do secret (ex: ANDROID_KEYSTORE_BASE64) [ENTER para encerrar]: ").strip()
        if not name:
            if secrets:
                print("Finalizando entrada de secrets.\n")
                break
            else:
                print("Nenhum secret definido ainda. Informe pelo menos um.")
                continue

        print("Tipo de valor:")
        print("  1 - Texto direto (colar senha, token, URL etc.)")
        print("  2 - Arquivo → base64 (keystore, chave SSH, JSON, .p8 etc.)")
        t = input("Escolha (1 ou 2) [1]: ").strip() or "1"

        if t == "2":
            path = input("Caminho do arquivo: ").strip()
            if not path:
                print("Caminho vazio, tente novamente.")
                continue
            try:
                with open(path, "rb") as f:
                    data = f.read()
                value = base64.b64encode(data).decode("utf-8")
                print(f"Arquivo lido e convertido para base64 ({len(value)} caracteres).")
            except FileNotFoundError:
                print("Arquivo não encontrado, tente novamente.")
                continue
            except Exception as e:
                print(f"Erro ao ler arquivo: {e}")
                continue
        else:
            # Texto direto; aqui podemos usar getpass para esconder se quiser
            print("Digite/cole o valor do secret. (Ele será armazenado na memória do script e enviado para o GitHub.)")
            value = input("Valor: ").strip()

        if not value:
            print("Valor vazio, secret ignorado.")
            continue

        secrets[name] = value
        print(f"Secret '{name}' adicionado.\n")

    return secrets


def main():
    print("=== GitHub Secrets Wizard (RAFAELIA) ===")
    username, token = load_credentials()
    print(f"Autenticado como: {username}\n")

    # Lista repositórios
    try:
        repos = list_repositories(token)
    except requests.HTTPError as e:
        print(f"Erro ao listar repositórios: {e}")
        return

    selected_repos = choose_repositories(repos)
    if not selected_repos:
        print("Nenhum repositório selecionado. Encerrando.")
        return

    # Coleta secrets
    secrets = collect_secrets_interactively()
    if not secrets:
        print("Nenhum secret definido. Encerrando.")
        return

    print("Resumo dos secrets que serão enviados:")
    for name in secrets:
        print(f"  - {name}")
    print()

    confirm = input("Continuar e enviar secrets para os repositórios selecionados? (s/N): ").strip().lower()
    if confirm != "s":
        print("Operação cancelada pelo usuário.")
        return

    # Envia para cada repo
    for repo_info in selected_repos:
        owner = repo_info["owner"]["login"]
        repo_name = repo_info["name"]

        try:
            # Obtém public key do repositório
            url = f"https://api.github.com/repos/{owner}/{repo_name}/actions/secrets/public-key"
            resp = github_request("GET", url, token)
            resp.raise_for_status()
            pk_json = resp.json()
            public_key_id = pk_json["key_id"]
            public_key = pk_json["key"]

            # Envia todos os secrets
            for sec_name, sec_value in secrets.items():
                put_secret(owner, repo_name, token, public_key_id, public_key, sec_name, sec_value)

        except requests.HTTPError as e:
            print(f"[{owner}/{repo_name}] Falha ao processar secrets: {e}")
        except Exception as e:
            print(f"[{owner}/{repo_name}] Erro inesperado: {e}")


if __name__ == "__main__":
    main()
