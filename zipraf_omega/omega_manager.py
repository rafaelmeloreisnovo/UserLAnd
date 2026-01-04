#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
ZIPRAF OMEGA Manager
====================
Sistema de gerenciamento automatizado de assinatura e releases
para o repositório UserLAnd.

Autor: Rafaelia System
Versão: 1.0.0
"""

import os
import sys
import json
import sqlite3
import hashlib
import getpass
import argparse
import subprocess
import secrets
from pathlib import Path
from typing import Optional, Dict, List, Tuple
from datetime import datetime
import base64

try:
    from cryptography.fernet import Fernet
    from cryptography.hazmat.primitives import hashes
    from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
    CRYPTO_AVAILABLE = True
except ImportError:
    CRYPTO_AVAILABLE = False
    print("⚠️  Aviso: cryptography não instalado. Criptografia desabilitada.")
    print("   Execute: pip install cryptography")

# Cores para terminal
class Colors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'
    NEON = '\033[38;5;82m'
    PINK = '\033[38;5;198m'

class ZipRafOmegaManager:
    """Gerenciador principal do sistema Zipraf Omega"""
    
    def __init__(self, db_path: str = None):
        """Inicializa o gerenciador"""
        self.root_dir = Path(__file__).parent.parent.absolute()
        self.omega_dir = Path(__file__).parent.absolute()
        self.db_dir = self.omega_dir / "db"
        self.db_dir.mkdir(parents=True, exist_ok=True)
        
        if db_path is None:
            db_path = str(self.db_dir / "omega.db")
        
        self.db_path = db_path
        self.cipher_suite = None
        self._init_database()
        
    def _init_database(self):
        """Inicializa o banco de dados SQLite"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        # Tabela de configurações
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS config (
                key TEXT PRIMARY KEY,
                value TEXT NOT NULL,
                encrypted INTEGER DEFAULT 0,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                updated_at TEXT DEFAULT CURRENT_TIMESTAMP
            )
        ''')
        
        # Tabela de assinaturas
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS signatures (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                apk_path TEXT NOT NULL,
                signature_hash TEXT,
                keystore_alias TEXT,
                signed_at TEXT DEFAULT CURRENT_TIMESTAMP,
                status TEXT DEFAULT 'success',
                notes TEXT
            )
        ''')
        
        # Tabela de logs
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS logs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                level TEXT NOT NULL,
                message TEXT NOT NULL,
                context TEXT,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP
            )
        ''')
        
        # Tabela de releases
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS releases (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                tag TEXT NOT NULL,
                commit_sha TEXT,
                release_url TEXT,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                status TEXT DEFAULT 'draft'
            )
        ''')
        
        conn.commit()
        conn.close()
        
    def _get_cipher(self, password: str) -> Optional[object]:
        """Cria cipher suite para criptografia"""
        if not CRYPTO_AVAILABLE:
            return None
            
        # Deriva uma chave da senha usando PBKDF2
        # Obtém ou gera um salt único para esta instalação
        salt_config = self.get_config('encryption_salt')
        if salt_config:
            salt = base64.b64decode(salt_config.encode())
        else:
            # Gerar salt aleatório na primeira vez
            # NOTA: Existe uma potencial race condition se múltiplas instâncias
            # executarem simultaneamente. Para uso típico (single-user), isto é aceitável.
            # Em ambientes multi-usuário, considere implementar locking apropriado.
            salt = secrets.token_bytes(32)
            # Armazenar para uso futuro
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()
            cursor.execute('''
                INSERT OR REPLACE INTO config (key, value, encrypted, updated_at)
                VALUES (?, ?, 0, CURRENT_TIMESTAMP)
            ''', ('encryption_salt', base64.b64encode(salt).decode()))
            conn.commit()
            conn.close()
            
        kdf = PBKDF2HMAC(
            algorithm=hashes.SHA256(),
            length=32,
            salt=salt,
            iterations=100000,
        )
        key = base64.urlsafe_b64encode(kdf.derive(password.encode()))
        return Fernet(key)
        
    def _encrypt_value(self, value: str, password: str) -> str:
        """Criptografa um valor"""
        if not CRYPTO_AVAILABLE:
            return value
            
        cipher = self._get_cipher(password)
        encrypted = cipher.encrypt(value.encode())
        return base64.b64encode(encrypted).decode()
        
    def _decrypt_value(self, encrypted_value: str, password: str) -> str:
        """Descriptografa um valor"""
        if not CRYPTO_AVAILABLE:
            return encrypted_value
            
        cipher = self._get_cipher(password)
        decoded = base64.b64decode(encrypted_value.encode())
        decrypted = cipher.decrypt(decoded)
        return decrypted.decode()
        
    def log(self, level: str, message: str, context: str = None):
        """Registra um log no banco de dados"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute('''
            INSERT INTO logs (level, message, context)
            VALUES (?, ?, ?)
        ''', (level, message, context))
        
        conn.commit()
        conn.close()
        
        # Também imprime no console
        color = Colors.OKGREEN if level == 'INFO' else Colors.WARNING if level == 'WARNING' else Colors.FAIL
        print(f"{color}[{level}]{Colors.ENDC} {message}")
        
    def set_config(self, key: str, value: str, encrypt: bool = False, password: str = None):
        """Define uma configuração"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        stored_value = value
        if encrypt and password:
            stored_value = self._encrypt_value(value, password)
            
        cursor.execute('''
            INSERT OR REPLACE INTO config (key, value, encrypted, updated_at)
            VALUES (?, ?, ?, CURRENT_TIMESTAMP)
        ''', (key, stored_value, 1 if encrypt else 0))
        
        conn.commit()
        conn.close()
        
        self.log('INFO', f"Configuração '{key}' atualizada")
        
    def get_config(self, key: str, password: str = None) -> Optional[str]:
        """Obtém uma configuração"""
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute('''
            SELECT value, encrypted FROM config WHERE key = ?
        ''', (key,))
        
        row = cursor.fetchone()
        conn.close()
        
        if not row:
            return None
            
        value, encrypted = row
        
        if encrypted and password:
            try:
                value = self._decrypt_value(value, password)
            except Exception as e:
                self.log('ERROR', f"Erro ao descriptografar '{key}': {e}")
                return None
                
        return value
        
    def setup_wizard(self):
        """Assistente de configuração inicial"""
        print(f"{Colors.NEON}{'='*60}{Colors.ENDC}")
        print(f"{Colors.NEON}🪉 ZIPRAF OMEGA - Assistente de Configuração 🪉{Colors.ENDC}")
        print(f"{Colors.NEON}{'='*60}{Colors.ENDC}\n")
        
        if not CRYPTO_AVAILABLE:
            print(f"{Colors.WARNING}⚠️  Criptografia não disponível!{Colors.ENDC}")
            print(f"{Colors.WARNING}   Execute: pip install cryptography{Colors.ENDC}\n")
            use_encryption = False
        else:
            print(f"{Colors.OKGREEN}✓ Criptografia disponível{Colors.ENDC}\n")
            use_encryption = True
        
        # GitHub PAT
        print(f"{Colors.OKCYAN}[1/4] GitHub Personal Access Token{Colors.ENDC}")
        print("   Obtenha seu PAT em: https://github.com/settings/tokens")
        print("   Permissões necessárias: repo, workflow, write:packages\n")
        
        github_pat = getpass.getpass("   Digite seu GitHub PAT: ")
        
        if use_encryption:
            master_password = getpass.getpass("   Digite uma senha mestra para criptografia: ")
            master_password_confirm = getpass.getpass("   Confirme a senha mestra: ")
            
            if master_password != master_password_confirm:
                print(f"{Colors.FAIL}✗ Senhas não coincidem!{Colors.ENDC}")
                return False
            
            # Nota de segurança: SHA-256 é usado aqui apenas para verificação rápida
            # da senha mestra, não para armazenamento de senha. A senha real nunca é
            # armazenada - apenas seu hash para verificação. Para criptografia dos dados,
            # usamos PBKDF2HMAC com salt aleatório.
            self.set_config('master_password_hash', hashlib.sha256(master_password.encode()).hexdigest())
            self.set_config('github_pat', github_pat, encrypt=True, password=master_password)
        else:
            self.set_config('github_pat', github_pat)
        
        print(f"{Colors.OKGREEN}✓ GitHub PAT configurado{Colors.ENDC}\n")
        
        # Keystore
        print(f"{Colors.OKCYAN}[2/4] Configuração de Keystore{Colors.ENDC}")
        keystore_file = input("   Caminho do keystore (.jks): ").strip()
        key_alias = input("   Alias da chave: ").strip()
        
        if use_encryption:
            keystore_password = getpass.getpass("   Senha do keystore: ")
            key_password = getpass.getpass("   Senha da chave: ")
            
            self.set_config('keystore_file', keystore_file)
            self.set_config('key_alias', key_alias)
            self.set_config('keystore_password', keystore_password, encrypt=True, password=master_password)
            self.set_config('key_password', key_password, encrypt=True, password=master_password)
        else:
            self.set_config('keystore_file', keystore_file)
            self.set_config('key_alias', key_alias)
            print(f"{Colors.WARNING}   ⚠️  Senhas não serão criptografadas{Colors.ENDC}")
            
        print(f"{Colors.OKGREEN}✓ Keystore configurado{Colors.ENDC}\n")
        
        # GitHub Repository
        print(f"{Colors.OKCYAN}[3/4] Repositório GitHub{Colors.ENDC}")
        repo_owner = input("   Owner do repositório (ex: rafaelmeloreisnovo): ").strip()
        repo_name = input("   Nome do repositório (ex: UserLAnd): ").strip()
        
        self.set_config('repo_owner', repo_owner)
        self.set_config('repo_name', repo_name)
        
        print(f"{Colors.OKGREEN}✓ Repositório configurado{Colors.ENDC}\n")
        
        # Opções adicionais
        print(f"{Colors.OKCYAN}[4/4] Opções Adicionais{Colors.ENDC}")
        auto_sign = input("   Assinatura automática ao build? (s/n): ").strip().lower()
        self.set_config('auto_sign', 'true' if auto_sign == 's' else 'false')
        
        print(f"\n{Colors.OKGREEN}{'='*60}{Colors.ENDC}")
        print(f"{Colors.OKGREEN}✓ Configuração concluída com sucesso!{Colors.ENDC}")
        print(f"{Colors.OKGREEN}{'='*60}{Colors.ENDC}\n")
        
        self.log('INFO', 'Sistema configurado com sucesso', 'setup_wizard')
        
        return True
        
    def sign_apk(self, apk_path: str, output_path: str = None) -> bool:
        """Assina um APK usando as configurações armazenadas"""
        print(f"{Colors.OKCYAN}🔒 Assinando APK: {apk_path}{Colors.ENDC}")
        
        # Obter senha mestra se criptografia estiver habilitada
        master_password = None
        if CRYPTO_AVAILABLE:
            stored_hash = self.get_config('master_password_hash')
            if stored_hash:
                master_password = getpass.getpass("Senha mestra: ")
                if hashlib.sha256(master_password.encode()).hexdigest() != stored_hash:
                    print(f"{Colors.FAIL}✗ Senha mestra incorreta{Colors.ENDC}")
                    return False
        
        # Obter configurações
        keystore_file = self.get_config('keystore_file')
        key_alias = self.get_config('key_alias')
        keystore_password = self.get_config('keystore_password', master_password)
        key_password = self.get_config('key_password', master_password)
        
        if not all([keystore_file, key_alias, keystore_password, key_password]):
            print(f"{Colors.FAIL}✗ Configurações de assinatura incompletas{Colors.ENDC}")
            print(f"   Execute: python omega_manager.py --setup")
            return False
        
        # Chamar script de assinatura
        sign_script = self.root_dir / "scripts" / "sign_apk.sh"
        
        if not sign_script.exists():
            print(f"{Colors.FAIL}✗ Script de assinatura não encontrado: {sign_script}{Colors.ENDC}")
            return False
        
        # Configurar variáveis de ambiente
        # NOTA DE SEGURANÇA: Passar credenciais via variáveis de ambiente tem riscos:
        # - Podem ser visíveis em listagens de processos
        # - Podem vazar em logs de sistema
        # Esta é uma limitação do script sign_apk.sh subjacente.
        # Em produção, considere usar arquivos temporários com permissões restritas.
        env = os.environ.copy()
        env['KEYSTORE_FILE'] = keystore_file
        env['KEY_ALIAS'] = key_alias
        env['KEYSTORE_PASSWORD'] = keystore_password
        env['KEY_PASSWORD'] = key_password
        
        cmd = [str(sign_script), apk_path]
        if output_path:
            cmd.append(output_path)
        
        try:
            result = subprocess.run(
                cmd,
                env=env,
                capture_output=True,
                text=True,
                check=True
            )
            
            print(result.stdout)
            print(f"{Colors.OKGREEN}✓ APK assinado com sucesso{Colors.ENDC}")
            
            # Registrar assinatura
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()
            cursor.execute('''
                INSERT INTO signatures (apk_path, keystore_alias, status)
                VALUES (?, ?, 'success')
            ''', (apk_path, key_alias))
            conn.commit()
            conn.close()
            
            self.log('INFO', f'APK assinado: {apk_path}', 'sign_apk')
            
            return True
            
        except subprocess.CalledProcessError as e:
            print(f"{Colors.FAIL}✗ Erro ao assinar APK:{Colors.ENDC}")
            print(e.stderr)
            
            self.log('ERROR', f'Erro ao assinar APK: {apk_path}', str(e))
            
            return False
    
    def show_status(self):
        """Mostra o status do sistema"""
        print(f"{Colors.NEON}{'='*60}{Colors.ENDC}")
        print(f"{Colors.NEON}🪉 ZIPRAF OMEGA - Status do Sistema 🪉{Colors.ENDC}")
        print(f"{Colors.NEON}{'='*60}{Colors.ENDC}\n")
        
        # Verificar configurações
        has_pat = self.get_config('github_pat') is not None
        has_keystore = self.get_config('keystore_file') is not None
        has_repo = self.get_config('repo_owner') is not None
        
        print(f"{Colors.OKCYAN}Configurações:{Colors.ENDC}")
        print(f"  GitHub PAT: {Colors.OKGREEN + '✓ Configurado' + Colors.ENDC if has_pat else Colors.FAIL + '✗ Não configurado' + Colors.ENDC}")
        print(f"  Keystore: {Colors.OKGREEN + '✓ Configurado' + Colors.ENDC if has_keystore else Colors.FAIL + '✗ Não configurado' + Colors.ENDC}")
        print(f"  Repositório: {Colors.OKGREEN + '✓ Configurado' + Colors.ENDC if has_repo else Colors.FAIL + '✗ Não configurado' + Colors.ENDC}")
        print(f"  Criptografia: {Colors.OKGREEN + '✓ Disponível' + Colors.ENDC if CRYPTO_AVAILABLE else Colors.WARNING + '⚠️  Não disponível' + Colors.ENDC}\n")
        
        # Estatísticas
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        cursor.execute('SELECT COUNT(*) FROM signatures WHERE status = "success"')
        total_signatures = cursor.fetchone()[0]
        
        cursor.execute('SELECT COUNT(*) FROM releases')
        total_releases = cursor.fetchone()[0]
        
        cursor.execute('SELECT COUNT(*) FROM logs WHERE level = "ERROR"')
        total_errors = cursor.fetchone()[0]
        
        conn.close()
        
        print(f"{Colors.OKCYAN}Estatísticas:{Colors.ENDC}")
        print(f"  Assinaturas realizadas: {total_signatures}")
        print(f"  Releases criados: {total_releases}")
        print(f"  Erros registrados: {total_errors}\n")
        
        print(f"{Colors.OKCYAN}Banco de dados:{Colors.ENDC}")
        print(f"  Localização: {self.db_path}")
        print(f"  Tamanho: {os.path.getsize(self.db_path) / 1024:.2f} KB\n")
        
    def show_logs(self, limit: int = 20, level: str = None):
        """Mostra os logs do sistema"""
        print(f"{Colors.NEON}{'='*60}{Colors.ENDC}")
        print(f"{Colors.NEON}📋 ZIPRAF OMEGA - Logs do Sistema 🪉{Colors.ENDC}")
        print(f"{Colors.NEON}{'='*60}{Colors.ENDC}\n")
        
        conn = sqlite3.connect(self.db_path)
        cursor = conn.cursor()
        
        if level:
            cursor.execute('''
                SELECT level, message, created_at FROM logs
                WHERE level = ?
                ORDER BY created_at DESC
                LIMIT ?
            ''', (level, limit))
        else:
            cursor.execute('''
                SELECT level, message, created_at FROM logs
                ORDER BY created_at DESC
                LIMIT ?
            ''', (limit,))
        
        rows = cursor.fetchall()
        conn.close()
        
        if not rows:
            print(f"{Colors.WARNING}Nenhum log encontrado{Colors.ENDC}")
            return
        
        for level, message, created_at in rows:
            color = Colors.OKGREEN if level == 'INFO' else Colors.WARNING if level == 'WARNING' else Colors.FAIL
            print(f"{color}[{level}]{Colors.ENDC} {created_at} - {message}")

def main():
    """Função principal"""
    parser = argparse.ArgumentParser(
        description='ZIPRAF OMEGA Manager - Sistema de gerenciamento automatizado',
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    parser.add_argument('--setup', action='store_true',
                       help='Executar assistente de configuração')
    parser.add_argument('--sign', metavar='APK',
                       help='Assinar um APK')
    parser.add_argument('--output', metavar='PATH',
                       help='Caminho do APK assinado (usado com --sign)')
    parser.add_argument('--status', action='store_true',
                       help='Mostrar status do sistema')
    parser.add_argument('--logs', action='store_true',
                       help='Mostrar logs do sistema')
    parser.add_argument('--log-level', metavar='LEVEL',
                       choices=['INFO', 'WARNING', 'ERROR'],
                       help='Filtrar logs por nível')
    parser.add_argument('--log-limit', type=int, default=20,
                       help='Número de logs a mostrar (padrão: 20)')
    
    args = parser.parse_args()
    
    # Se nenhum argumento, mostrar ajuda
    if len(sys.argv) == 1:
        parser.print_help()
        print(f"\n{Colors.OKCYAN}Exemplo de uso:{Colors.ENDC}")
        print(f"  python omega_manager.py --setup")
        print(f"  python omega_manager.py --sign app-release-unsigned.apk")
        print(f"  python omega_manager.py --status")
        print(f"  python omega_manager.py --logs")
        return
    
    manager = ZipRafOmegaManager()
    
    if args.setup:
        manager.setup_wizard()
    elif args.sign:
        manager.sign_apk(args.sign, args.output)
    elif args.status:
        manager.show_status()
    elif args.logs:
        manager.show_logs(args.log_limit, args.log_level)

if __name__ == '__main__':
    main()
