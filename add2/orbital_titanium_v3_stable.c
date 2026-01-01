/* 
 * SYSTEM: ORBITAL TITANIUM STREAM V3-STABLE (FULL DUPLEX)
 * ARCH: ARM64 Optimized (Android 15 / Termux)
 * FEATURES: Backup Stream, Restore Stream, Menu System, Network Hardening
 */

#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/utsname.h>

// --- [ CONFIGURATION ] ---
#define R_REMOTE_DEFAULT "gdrive"
#define R_FOLDER "Backup_Android_Stream"

// --- [ COLORS ] ---
#define C_CYAN  "\033[1;36m"
#define C_GRN   "\033[1;32m"
#define C_RED   "\033[1;31m"
#define C_YEL   "\033[1;33m"
#define C_RST   "\033[0m"

char TARGET_REMOTE[128];
struct utsname host;

// --- [ UTILS ] ---

void clear_screen() { system("clear"); }

void header(const char *subtitle) {
    clear_screen();
    printf(C_CYAN);
    printf("   _______  __  ___  _  __  ____  __  __ \n");
    printf("  /_  __/ ||  |/  / / |/ / / __/ /  |/  |\n");
    printf("   / / / / / / / / /    / / _/  / /|_/ / \n");
    printf("  /_/ /_/  |__/_/ /_/|_/ /___/ /_/  /_/  \n");
    printf("  >> %s << \n", subtitle);
    printf(C_RST);
    printf("----------------------------------------\n");
}

int run_silent(const char *cmd) {
    char buf[512];
    snprintf(buf, sizeof(buf), "%s > /dev/null 2>&1", cmd);
    return system(buf);
}

// --- [ NETWORKING & INIT ] ---

void init_system() {
    if (uname(&host) != 0) {
        perror("[ERR] uname");
        strcpy(host.nodename, "android_termux");
    }

    // Valida Rclone
    if (run_silent("command -v rclone") != 0) {
        printf(C_RED "[CRIT] Rclone não instalado. Rode: pkg install rclone\n" C_RST);
        exit(1);
    }
    
    // Seleciona Remote
    if (run_silent("rclone listremotes | grep -q '^" R_REMOTE_DEFAULT ":$'") == 0) {
        strcpy(TARGET_REMOTE, R_REMOTE_DEFAULT);
    } else {
        printf(C_YEL "[CONF] Remote padrão '%s' não achado. Remotes disponíveis:\n" C_RST, R_REMOTE_DEFAULT);
        system("rclone listremotes");
        printf("\nDigite o nome exato do remote (sem ':'): ");
        if (scanf("%127s", TARGET_REMOTE) != 1) {
            printf(C_RED "[ERR] Falha ao ler remote.\n" C_RST);
            exit(1);
        }
    }

    // Teste de Conexão (Ping)
    printf("[NET ] Testando link com '%s'...\n", TARGET_REMOTE);
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "rclone lsd %s: --max-depth 1 > /dev/null 2>&1", TARGET_REMOTE);
    if (system(cmd) != 0) {
        printf(C_RED "[FAIL] Falha de conexão/token com remote '%s'.\n" C_RST, TARGET_REMOTE);
        printf(C_YEL "       Rode: rclone config\n" C_RST);
        exit(1);
    }

    printf(C_GRN "[OK  ] Remote '%s' pronto.\n" C_RST, TARGET_REMOTE);
    sleep(1);
}

// --- [ MODULE: BACKUP ] ---

long long get_size() {
    printf(C_YEL "[CALC] Scaneando filesystem (pode demorar)...\n" C_RST);
    // Exclui pastas virtuais / pesadas para cálculo de tamanho
    FILE *fp = popen(
        "du -sb "
        "--exclude='./.cache' "
        "--exclude='./tmp' "
        "--exclude='./proc' "
        "--exclude='./sys' "
        "--exclude='./dev' "
        "--exclude='./mnt' "
        "--exclude='./storage' "
        "--exclude='./sdcard' "
        ". 2>/dev/null | cut -f1",
        "r"
    );
    if (!fp) {
        printf(C_RED "[WARN] Não foi possível calcular tamanho total. Seguindo assim mesmo.\n" C_RST);
        return 0;
    }
    char path[64];
    long long size = 0;
    if (fgets(path, sizeof(path), fp)) size = atoll(path);
    pclose(fp);
    return size;
}

void do_backup() {
    header("BACKUP STREAM MODE");

    long long total_bytes = get_size();

    // Timestamp Generation
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char timestamp[32];
    sprintf(timestamp, "%04d%02d%02d_%02d%02d",
            tm.tm_year + 1900, tm.tm_mon + 1,
            tm.tm_mday, tm.tm_hour, tm.tm_min);

    // Opções rclone (SEM FLAG INVÁLIDA)
    char rclone_opts[1024];
    snprintf(rclone_opts, sizeof(rclone_opts), 
        "--stats 10s --stats-one-line "
        "--buffer-size 32M "
        "--drive-chunk-size 64M "
        "--timeout 30m "
        "--retries 10 "
        "--low-level-retries 20 "
        "-P"
    );

    const char *excludes = 
        "--exclude='./.cache' --exclude='./tmp' --exclude='./.git' "
        "--exclude='./proc' --exclude='./sys' --exclude='./dev' "
        "--exclude='./mnt' --exclude='./storage' --exclude='./sdcard' ";

    if (total_bytes > 0) {
        printf(C_GRN "\n[EXEC] Payload estimado: %.2f MB -> Nuvem\n" C_RST,
               total_bytes / (1024.0 * 1024.0));
    } else {
        printf(C_YEL "\n[EXEC] Payload: tamanho não estimado (seguindo assim mesmo).\n" C_RST);
    }

    printf(C_YEL ">> NÃO FECHE O TERMINAL. STREAM NÃO TEM RESUME.\n" C_RST);
    sleep(2);

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), 
        "tar -czf - %s . 2>/dev/null | "
        "rclone rcat %s:%s/%s_%s.tar.gz %s",
        excludes,
        TARGET_REMOTE, R_FOLDER, host.nodename, timestamp,
        rclone_opts
    );

    int ret = system(cmd);
    if (ret != 0) {
        printf(C_RED "\n[ERR] Falha durante o backup (código %d).\n" C_RST, ret);
        printf("Verifique conexão, espaço no remote e flags do rclone.\n");
    } else {
        printf(C_GRN "\n[DONE] Backup concluído com sucesso.\n" C_RST);
    }

    printf("\nPressione Enter para voltar ao menu.");
    getchar(); getchar();
}

// --- [ MODULE: RESTORE ] ---

void do_restore() {
    header("RESTORE / RETRIEVE MODE");
    printf("[INFO] Listando backups em: %s:%s\n\n", TARGET_REMOTE, R_FOLDER);
    
    char cmd_ls[512];
    snprintf(cmd_ls, sizeof(cmd_ls),
             "rclone lsl %s:%s | sort -k 2",
             TARGET_REMOTE, R_FOLDER);
    int ret = system(cmd_ls);
    if (ret != 0) {
        printf(C_RED "[ERR] Falha ao listar backups (código %d).\n" C_RST, ret);
        printf("Verifique se a pasta '%s' existe no remote.\n", R_FOLDER);
    }

    printf(C_YEL "\nCopie e cole o nome do arquivo (ex: android_2025...tar.gz)\nNome: " C_RST);
    char filename[256];
    if (scanf("%255s", filename) != 1) {
        printf(C_RED "[ERR] Leitura de nome de arquivo falhou.\n" C_RST);
        return;
    }

    printf(C_RED "\n[WARN] Isso irá sobrescrever arquivos na pasta atual!\n" C_RST);
    printf("Você está em: ");
    system("pwd");
    printf("Continuar? (y/n): ");
    char confirm;
    scanf(" %c", &confirm);
    if (confirm != 'y' && confirm != 'Y') {
        printf(C_YEL "[ABORT] Restore cancelado pelo usuário.\n" C_RST);
        sleep(1);
        return;
    }

    // Restore Pipeline: Rclone (Download) -> Tar (Extract)
    char cmd_restore[4096];
    snprintf(cmd_restore, sizeof(cmd_restore),
        "rclone cat %s:%s/%s | tar -xzvf -", 
        TARGET_REMOTE, R_FOLDER, filename);

    printf(C_CYAN "\n>> BAIXANDO E EXTRAINDO SIMULTANEAMENTE <<\n" C_RST);
    ret = system(cmd_restore);
    if (ret != 0) {
        printf(C_RED "\n[ERR] Falha durante o restore (código %d).\n" C_RST, ret);
        printf("Verifique nome do arquivo, espaço em disco e permissões.\n");
    } else {
        printf(C_GRN "\n[DONE] Restore concluído com sucesso.\n" C_RST);
    }
    
    printf("\nPressione Enter para voltar ao menu.");
    getchar(); getchar();
}

// --- [ MAIN LOOP ] ---

int main() {
    init_system();

    while (1) {
        header("MAIN MENU");
        printf(" [1] INICIAR STREAM BACKUP (Upload)\n");
        printf(" [2] RESTAURAR BACKUP (Download)\n");
        printf(" [3] SAIR\n\n");
        printf(C_CYAN " OPÇÃO > " C_RST);

        char opt;
        if (scanf(" %c", &opt) != 1) {
            printf(C_RED "[ERR] Falha ao ler opção.\n" C_RST);
            exit(1);
        }

        switch (opt) {
            case '1': do_backup(); break;
            case '2': do_restore(); break;
            case '3': 
                printf(C_GRN "[EXIT] Saindo...\n" C_RST);
                exit(0);
            default:
                printf(C_YEL "[WARN] Opção inválida.\n" C_RST);
                sleep(1);
                break;
        }
    }
    return 0;
}
