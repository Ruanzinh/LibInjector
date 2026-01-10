#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/uio.h>    // Necessário para struct iovec
#include <dlfcn.h>
#include <errno.h>
#include <asm/ptrace.h> // Definições de registradores ARM64

// Correção para Termux: Definir NT_PRSTATUS se não existir
#ifndef NT_PRSTATUS
#define NT_PRSTATUS 1
#endif

// Helper para ler mapas de memória
long get_module_base(pid_t pid, const char* module_name) {
    FILE* fp;
    long addr = 0;
    char filename[32];
    char line[1024];

    if (pid < 0) {
        snprintf(filename, sizeof(filename), "/proc/self/maps");
    } else {
        snprintf(filename, sizeof(filename), "/proc/%d/maps", pid);
    }

    fp = fopen(filename, "r");
    if (fp != NULL) {
        while (fgets(line, sizeof(line), fp)) {
            if (strstr(line, module_name)) {
                addr = strtoul(line, NULL, 16);
                break;
            }
        }
        fclose(fp);
    }
    return addr;
}

// Helper para calcular endereço remoto
void* get_remote_addr(pid_t target_pid, const char* module_name, void* local_addr) {
    long local_base = get_module_base(-1, module_name);
    long remote_base = get_module_base(target_pid, module_name);

    if (local_base == 0 || remote_base == 0) return NULL;

    return (void*)((long)local_addr - local_base + remote_base);
}

// Escrever na memória do processo alvo
int ptrace_writedata(pid_t pid, uint8_t *dest, uint8_t *data, size_t size) {
    long i, j, remaining;
    uint8_t *laddr;

    union u {
        long val;
        char chars[sizeof(long)];
    } d;

    j = size / sizeof(long);
    remaining = size % sizeof(long);

    laddr = data;

    for (i = 0; i < j; i++) {
        memcpy(d.chars, laddr, sizeof(long));
        if (ptrace(PTRACE_POKETEXT, pid, dest, d.val) < 0) return -1;
        dest += sizeof(long);
        laddr += sizeof(long);
    }

    if (remaining > 0) {
        d.val = ptrace(PTRACE_PEEKTEXT, pid, dest, 0);
        for (i = 0; i < remaining; i++) {
            d.chars[i] = *laddr++;
        }
        if (ptrace(PTRACE_POKETEXT, pid, dest, d.val) < 0) return -1;
    }
    return 0;
}

int inject_remote_library(pid_t pid, const char *path) {
    struct iovec io;
    // No Android ARM64, usamos user_pt_regs, mas user_regs_struct costuma ser um typedef dele ou similar
    struct user_regs_struct regs, original_regs;
    long parameters[1];
    void *remote_dlopen_addr;

    printf("[*] Attachando ao processo %d...\n", pid);
    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) < 0) {
        perror("[-] Ptrace attach falhou");
        return -1;
    }
    waitpid(pid, NULL, 0);

    // Salvar registradores originais
    io.iov_base = &original_regs;
    io.iov_len = sizeof(original_regs);
    if (ptrace(PTRACE_GETREGSET, pid, NT_PRSTATUS, &io) < 0) {
        perror("[-] Falha ao ler registros");
        ptrace(PTRACE_DETACH, pid, NULL, NULL);
        return -1;
    }
    memcpy(&regs, &original_regs, sizeof(regs));

    // Encontrar dlopen
    void* local_dlopen = dlsym(RTLD_DEFAULT, "dlopen");
    Dl_info info;
    if (dladdr(local_dlopen, &info) == 0) {
        printf("[-] dladdr falhou\n");
        return -1;
    }
    
    remote_dlopen_addr = get_remote_addr(pid, info.dli_fname, local_dlopen);
    if (!remote_dlopen_addr) {
        printf("[-] Falha ao encontrar dlopen remoto. Lib: %s\n", info.dli_fname);
        return -1;
    }
    printf("[+] dlopen remoto: %p\n", remote_dlopen_addr);

    // Preparar stack
    long stack_pointer = regs.sp; 
    long lib_path_addr = stack_pointer - 0x1000;

    printf("[*] Escrevendo caminho em %lx...\n", lib_path_addr);
    if (ptrace_writedata(pid, (uint8_t *)lib_path_addr, (uint8_t *)path, strlen(path) + 1) < 0) {
        perror("[-] Falha ao escrever na memoria");
        return -1;
    }

    // Configurar chamada ARM64
    // x0 = argumento 1
    // x1 = argumento 2
    // x30 = LR (Link Register)
    
    regs.regs[0] = lib_path_addr;
    regs.regs[1] = 2; // RTLD_NOW
    regs.pc = (long)remote_dlopen_addr;
    
    // AQUI ESTAVA O ERRO: 'lr' não existe diretamente na struct, é o regs[30]
    regs.regs[30] = 0; 

    io.iov_base = &regs;
    if (ptrace(PTRACE_SETREGSET, pid, NT_PRSTATUS, &io) < 0) {
        perror("[-] Falha ao definir registros");
        return -1;
    }

    printf("[*] Executando dlopen...\n");
    ptrace(PTRACE_CONT, pid, NULL, NULL);
    
    waitpid(pid, NULL, 0);

    printf("[*] Restaurando processo...\n");
    io.iov_base = &original_regs;
    ptrace(PTRACE_SETREGSET, pid, NT_PRSTATUS, &io);

    ptrace(PTRACE_DETACH, pid, NULL, NULL);
    printf("[SUCCESS] Injeção concluída.\n");

    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Uso: %s <PID> <PATH_TO_LIB>\n", argv[0]);
        return 1;
    }

    pid_t pid = atoi(argv[1]);
    const char* lib_path = argv[2];

    if (access(lib_path, F_OK) == -1) {
        printf("[-] Erro: Arquivo .so não existe: %s\n", lib_path);
        return 1;
    }

    inject_remote_library(pid, lib_path);
    return 0;
}