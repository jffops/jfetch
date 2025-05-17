#include <stdio.h>
#include <time.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <sys/statvfs.h>

#include "jorb.h" // Jorb animation object 
#include "jfetch.h"

// IMPLEMENTED:
// Username [x]
// Host [x]
// Datetime [x]
// OS [x]
// Kernel [x]
// Desktop [x]
// Shell [x]
// Terminal [x]
// CPU [x]
// CPU usage [x]
// Memory [x]
// Swap [x]
// Disk [x]
// Processes [x]
// Uptime [x]
// Battery [x]

static system_stats sysstats = { 0 }; 

char *yield_frame(animation_object *ao) {
    char *frame = ao->frames[ao->current_frame];
    ao->current_frame = (ao->current_frame + 1) % ao->frame_count;
    return frame;
}

void fetch_user_name(char *user_name) {
    NULL_RETURN(user_name);
    strncpy(user_name, "Unknown", BUFFERSIZE);

    char *value = getenv("USER");
    if (value)
        strncpy(user_name, value, BUFFERSIZE);
}

void fetch_host_name(char *host_name) {
    NULL_RETURN(host_name);
    strncpy(host_name, "Unknown", BUFFERSIZE);

    char buffer[BUFFERSIZE] = { 0 };
    if (!gethostname(buffer, BUFFERSIZE)) {
        buffer[BUFFERSIZE - 1] = '\0';
        strncpy(host_name, buffer, BUFFERSIZE);
    }
}

void fetch_datetime(char *datetime) {
    NULL_RETURN(datetime);
    strncpy(datetime, "Unknown", BUFFERSIZE);

    time_t dt;
    if (time(&dt) != -1) {
        struct tm *local = localtime(&dt);
        strftime(datetime, BUFFERSIZE, "%Y-%m-%d %H:%M:%S", local);
    }
}

void fetch_os_name(char *os_name) {
    NULL_RETURN(os_name);
    strncpy(os_name, "Unknown", BUFFERSIZE);
    
    FILE *f = fopen("/etc/os-release", "r");
    if (!f)
        return;
    
    char buffer[BUFFERSIZE] = { 0 };
    char *tmp = NULL;
    while (fgets(buffer, BUFFERSIZE, f) != NULL) {
        if (!strncmp(buffer, "PRETTY_NAME=", 12)) {
            tmp = buffer + 12;
            if (tmp[0] == '\"') tmp++;
            size_t length = strlen(tmp);
            if (tmp[length - 1] == '\n') length--;
            if (tmp[length - 1] == '\"') length--;
            memcpy(os_name, tmp, length);
            break;
        }
    }
    
    fclose(f);
}

void fetch_kernel_version(char *kernel_version) {
    NULL_RETURN(kernel_version);
    strncpy(kernel_version, "Unknown", BUFFERSIZE);

    struct utsname data;
    if (!uname(&data)) {
        strncpy(kernel_version, data.sysname, BUFFERSIZE);
        size_t length = strlen(kernel_version);
        if (length < BUFFERSIZE)
            kernel_version[length] = '-';
        strncpy(kernel_version + length + 1, data.release, BUFFERSIZE - length - 1);
    }
}

void fetch_desktop_name(char *desktop_name) {
    NULL_RETURN(desktop_name);
    strncpy(desktop_name, "Unknown", BUFFERSIZE);

    char *desktop = getenv("XDG_SESSION_DESKTOP");
    char *session = getenv("XDG_SESSION_TYPE");
    
    if (desktop && session) {
        snprintf(desktop_name, BUFFERSIZE, "%s (%s)", desktop, session);
    }
}

void fetch_shell_name(char *shell_name) {
    NULL_RETURN(shell_name);
    strncpy(shell_name, "Unknown", BUFFERSIZE);

    char *value = getenv("SHELL");
    value = strrchr(value, '/');
    if (value) 
        strncpy(shell_name, value + 1, BUFFERSIZE);
}

void fetch_terminal_name(char *terminal_name) {
    NULL_RETURN(terminal_name);
    strncpy(terminal_name, "Unknown", BUFFERSIZE);

    char buffer[BUFFERSIZE] = { 0 };
    snprintf(buffer, BUFFERSIZE, "/proc/%d/stat", getppid());

    FILE *f = fopen(buffer, "r");
    if (!f)
        return;

    pid_t ppid;
    if (fscanf(f, "%*d %*s %*c %d", &ppid) != 1) {
        fclose(f);
        return;
    }
    fclose(f);
    
    snprintf(buffer, BUFFERSIZE, "/proc/%d/comm", ppid);
    f = fopen(buffer, "r");
    if (!f)
        return;
    if (fgets(buffer, BUFFERSIZE, f) != NULL) {
        strncpy(terminal_name, buffer, BUFFERSIZE);
        printf("%s", terminal_name);
    }
    fclose(f);
}

void fetch_cpu_name(char *cpu_name) {
    NULL_RETURN(cpu_name);
    strncpy(cpu_name, "Unknown", BUFFERSIZE);

    FILE *f = fopen("/proc/cpuinfo", "r");
    if (!f)
        return;
    
    char buffer[BUFFERSIZE] = { 0 };
    char *tmp = NULL;
    while (fgets(buffer, BUFFERSIZE, f) != NULL) {
        if (!strncmp(buffer, "model name", 10)) {
            tmp = strstr(buffer, ": ") + 2;
            size_t length = strlen(tmp);
            if (tmp[length - 1] == '\n') length--;
            memcpy(cpu_name, tmp, length);
            break;
        }
    }
    
    fclose(f);
}

void fetch_cpu_usage(char *cpu_usage) {
    NULL_RETURN(cpu_usage);
    strncpy(cpu_usage, "Unknown", BUFFERSIZE);

    FILE *f = fopen("/proc/stat", "r");
    if (!f)
        return;

    static size_t prev_total = 0, prev_idle = 0;
    char buffer[BUFFERSIZE];
    if (fgets(buffer, BUFFERSIZE, f) != NULL) {
        size_t user, nice, system, idle, iowait, irq, softirq, steal, guest, guest_nice;
        sscanf(buffer, "cpu %zd %zd %zd %zd %zd %zd %zd %zd %zd %zd", 
            &user, &nice, &system, &idle, &iowait, &irq, &softirq, &steal, &guest, &guest_nice
        );
        size_t total = user + nice + system + idle + iowait + irq + softirq + steal + guest + guest_nice;
        snprintf(cpu_usage, BUFFERSIZE, 
            "%.0f%%", (1 - (double)(idle - prev_idle) / (total - prev_total)) * 100
        );
        prev_total = total;
        prev_idle = idle;
    }
        
    fclose(f);
}

void fetch_ram_usage(char *ram_usage) {
    NULL_RETURN(ram_usage);
    strncpy(ram_usage, "Unknown", BUFFERSIZE);

    FILE *f = fopen("/proc/meminfo", "r");
    if (!f)
        return;
        
    char buffer[BUFFERSIZE] = { 0 };
    size_t total_kB = 0, used_kB = 0;
    while (fgets(buffer, BUFFERSIZE, f) != NULL) {
        if (total_kB && used_kB) 
            break;
        sscanf(buffer, "MemTotal: %zd kB", &total_kB);
        sscanf(buffer, "MemAvailable: %zd kB", &used_kB);
    }
    
    if (total_kB && used_kB) {
        used_kB = total_kB - used_kB;
        snprintf(ram_usage, BUFFERSIZE, 
            "%.2fGB / %.2fGB (%.0f%%)", 
            (double)used_kB / 1024 / 1024,
            (double)total_kB / 1024 / 1024,
            (double)used_kB / total_kB * 100 
        );
    }
    
    fclose(f);
}

void fetch_swap_usage(char *swap_usage) {
    NULL_RETURN(swap_usage);
    strncpy(swap_usage, "Unknown", BUFFERSIZE);

    FILE *f = fopen("/proc/meminfo", "r");
    if (!f)
        return;
        
    char buffer[BUFFERSIZE] = { 0 };
    size_t total_kB = 0, used_kB = 0;
    while (fgets(buffer, BUFFERSIZE, f) != NULL) {
        if (total_kB && used_kB) 
            break;
        sscanf(buffer, "SwapTotal: %zd kB", &total_kB);
        sscanf(buffer, "SwapFree: %zd kB", &used_kB);
    }
    
    if (total_kB && used_kB) {
        used_kB = total_kB - used_kB;
        snprintf(swap_usage, BUFFERSIZE, 
            "%.2fGB / %.2fGB (%.0f%%)", 
            (double)used_kB / 1024 / 1024,
            (double)total_kB / 1024 / 1024,
            (double)used_kB * 100 / total_kB
        );
    }

    fclose(f);
}

void fetch_disk_usage(char *disk_usage) {
    NULL_RETURN(disk_usage);
    strncpy(disk_usage, "Unknown", BUFFERSIZE);

    struct statvfs data;
    if (!statvfs("/", &data)) {
        size_t total_bytes = data.f_frsize * data.f_blocks;
        size_t used_bytes = total_bytes - data.f_frsize * data.f_bfree;

        snprintf(disk_usage, BUFFERSIZE,
            "%.1fGB / %.1fGB (%.0f%%)", 
            (double)used_bytes / 1024 / 1024 / 1024, 
            (double)total_bytes / 1024 / 1024 / 1024,
            (double)used_bytes * 100 / total_bytes
        );
    }
}

void fetch_process_count(char *process_count) {
    NULL_RETURN(process_count);
    strncpy(process_count, "Unknown", BUFFERSIZE);
    char buffer[BUFFERSIZE] = { 0 };
    
    FILE *f = popen("ps -aux | wc -l", "r");
    if (fgets(buffer, BUFFERSIZE, f) != NULL) {
        strncpy(process_count, buffer, BUFFERSIZE);
    }
    pclose(f);
}

void fetch_uptime(char *uptime) {
    NULL_RETURN(uptime);
    strncpy(uptime, "Unknown", BUFFERSIZE);

    struct sysinfo data;
    if (!sysinfo(&data)) {
        int h = data.uptime / 3600,
            m = data.uptime % 3600 / 60,
            s = data.uptime % 60;
        snprintf(uptime, BUFFERSIZE, "%02d:%02d:%02d", h, m, s);
    }
}

void fetch_battery_charge(char *battery_charge) {
    NULL_RETURN(battery_charge);
    strncpy(battery_charge, "Unknown", BUFFERSIZE);

    FILE *f = fopen("/sys/class/power_supply/BAT0/capacity", "r");
    if (!f)
        return;
    
    char buffer[BUFFERSIZE] = { 0 };
    fgets(buffer, BUFFERSIZE, f);
    strncpy(battery_charge, buffer, BUFFERSIZE);
    size_t length = strlen(battery_charge);
    if (battery_charge[length - 1] == '\n') battery_charge[--length] = '\0';
    if (length < BUFFERSIZE - 1)
        battery_charge[length] = '%';
    fclose(f);
}

void fetch_stats(system_stats *stats) {
    fetch_user_name(stats->user_name);
    fetch_host_name(stats->host_name);
    fetch_datetime(stats->datetime);
    fetch_os_name(stats->os_name);
    fetch_kernel_version(stats->kernel_version);
    fetch_desktop_name(stats->desktop_name);
    fetch_shell_name(stats->shell_name);
    fetch_terminal_name(stats->terminal_name);
    fetch_cpu_name(stats->cpu_name);
    fetch_cpu_usage(stats->cpu_usage);
    fetch_ram_usage(stats->ram_usage);
    fetch_swap_usage(stats->swap_usage);
    fetch_disk_usage(stats->disk_usage);
    fetch_process_count(stats->process_count);
    fetch_uptime(stats->uptime);
    fetch_battery_charge(stats->battery_charge);
}

void update_dynamic_stats(system_stats *stats) {
    fetch_datetime(stats->datetime);
    fetch_cpu_usage(stats->cpu_usage);
    fetch_ram_usage(stats->ram_usage);
    fetch_swap_usage(stats->swap_usage);
    fetch_process_count(stats->process_count);
    fetch_uptime(stats->uptime);
    fetch_battery_charge(stats->battery_charge);    
}

void get_terminal_size(int *columns, int *lines) {
    *columns = 0; *lines = 0;
    char buffer[BUFFERSIZE] = { 0 };
    FILE *f = popen("tput cols", "r");
    if (!f) 
        return;
    if (fgets(buffer, BUFFERSIZE, f) != NULL)
        *columns = atoi(buffer);
    pclose(f);

    f = popen("tput lines", "r");
    if (!f) 
        return;
    if (fgets(buffer, BUFFERSIZE, f) != NULL)
        *lines = atoi(buffer);
    pclose(f);
}

void clear_screen(int columns, int lines) {
    printf(POS COLOR_RESET, 0, 0);
    for (int y = 1; y < lines + 1; y++) {
        printf(POS, y, 0);
        for (int x = 0; x < columns; x++)
            putchar(' ');
    }
}

void print_logo() {
    //printf(POS "%s", 0, 0, arch_logo_8x15);
    //printf(POS COLOR_CYAN "%s" COLOR_RESET, 0, 0, arch_logo_wide);
    printf(yield_frame(&jorb));
}

void draw_line(int length) {
    int x = 0;
    while (x++ < length)
        putchar('-');
}

void print_stats(system_stats stats) {
    int line = 1, 
        column = PADDING + 2;
    int namelen = strlen(stats.user_name) + strlen(stats.host_name) + 1;
    printf(POS COLOR_RESET COLOR_CYAN "%*sjfetch🌠🎀" COLOR_RESET, line++, column, (namelen - 8) / 2, "");
    printf(POS COLOR_CYAN "%s" COLOR_RESET "@" COLOR_CYAN "%s" COLOR_RESET, line++, column, stats.user_name, stats.host_name);
    printf(POS, line++, column); 
    draw_line(namelen);
    printf(POS COLOR_CYAN "Datetime: " COLOR_RESET " %s", line++, column, stats.datetime);
    printf(POS COLOR_CYAN "OS:       " COLOR_RESET " %s", line++, column, stats.os_name);
    printf(POS COLOR_CYAN "Kernel:   " COLOR_RESET " %s", line++, column, stats.kernel_version);
    printf(POS COLOR_CYAN "Desktop:  " COLOR_RESET " %s", line++, column, stats.desktop_name);
    printf(POS COLOR_CYAN "Shell:    " COLOR_RESET " %s", line++, column, stats.shell_name);
    printf(POS COLOR_CYAN "Terminal: " COLOR_RESET " %s", line++, column, stats.terminal_name);
    printf(POS COLOR_CYAN "CPU:      " COLOR_RESET " %s", line++, column, stats.cpu_name);
    printf(POS COLOR_CYAN "CPU Usage:" COLOR_RESET " %s", line++, column, stats.cpu_usage);
    printf(POS COLOR_CYAN "Memory:   " COLOR_RESET " %s", line++, column, stats.ram_usage);
    printf(POS COLOR_CYAN "Swap:     " COLOR_RESET " %s", line++, column, stats.swap_usage);
    printf(POS COLOR_CYAN "Disk:     " COLOR_RESET " %s", line++, column, stats.disk_usage);
    printf(POS COLOR_CYAN "Processes:" COLOR_RESET " %s", line++, column, stats.process_count);
    printf(POS COLOR_CYAN "Uptime:   " COLOR_RESET " %s", line++, column, stats.uptime);
    printf(POS COLOR_CYAN "Battery:  " COLOR_RESET " %s", line++, column, stats.battery_charge);
}

void handle_exit(int /*signal*/) {
    int columns, lines;
    get_terminal_size(&columns, &lines);
    clear_screen(columns, lines);
    print_stats(sysstats);
    print_logo();
    
    printf("\n");
    system("tput cnorm");
    exit(0);
}

int main() {
    signal(SIGINT, handle_exit);
    system("tput civis");

    fetch_stats(&sysstats);
///*
    size_t frame = 0;
    int prev_columns = 0, prev_lines = 0;
    int columns, lines;
    while (1) { 
        get_terminal_size(&columns, &lines);
        if (prev_columns != columns || prev_lines != lines) {
            clear_screen(columns, lines);
            prev_columns = columns; 
            prev_lines = lines;
        }
        print_stats(sysstats);
        print_logo();
        fflush(stdout);
        if (frame % (FPS / 2) == 0) // update stats every 0.5s
            update_dynamic_stats(&sysstats);
        usleep(1000000 / FPS);
        frame++;
    }

    handle_exit(0);
//*/
    return 0;
}
