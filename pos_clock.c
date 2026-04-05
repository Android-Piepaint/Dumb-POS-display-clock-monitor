#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <time.h>

#define SERIAL_PORT "/dev/ttyS1"

//***************************** IMPORTANT INFO ********************************
// You can use "\033@" to clear display, or write your contents.
// For example, use echo "\033@8964" > /dev/ttyS1 to display "8964" on display.
// Or just using raw hex codes: printf "\x1b\x40" | sudo tee /dev/ttyS1 will clear the display contents.
// The display made by an unknown manufacturer seems using ASCII-based protocol (Epson ETC/POS?) to communicate with computer under 2400 baudrate.
//*****************************************************************************

// Function to configure the serial port
int setup_serial(int fd) {
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) return -1;
    cfsetospeed(&tty, B2400); 
    cfsetispeed(&tty, B2400);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8; 
    tty.c_lflag = 0; tty.c_oflag = 0; tty.c_iflag &= ~IGNBRK;
    tty.c_cc[VMIN] = 0; tty.c_cc[VTIME] = 5;
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
    if (tcsetattr(fd, TCSANOW, &tty) != 0) return -1;
    return 0;
}

// Get average CPU frequency in GHz
float get_cpu_ghz() {
    FILE *fp = fopen("/proc/cpuinfo", "r");
    char line[256];
    float sum = 0;
    int count = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "cpu MHz", 7) == 0) {
            float mhz;
            sscanf(line, "cpu MHz : %f", &mhz);
            sum += mhz;
            count++;
        }
    }
    fclose(fp);
    return (count > 0) ? (sum / count / 1000.0) : 0;
}

// Get Used Memory in MiB
long get_mem_used_mib() {
    FILE *fp = fopen("/proc/meminfo", "r");
    char label[32];
    long value, total = 0, free = 0, buffers = 0, cached = 0;
    while (fscanf(fp, "%s %ld kB", label, &value) != EOF) {
        if (strcmp(label, "MemTotal:") == 0) total = value;
        else if (strcmp(label, "MemFree:") == 0) free = value;
        else if (strcmp(label, "Buffers:") == 0) buffers = value;
        else if (strcmp(label, "Cached:") == 0) cached = value;
    }
    fclose(fp);
    // Used = Total - (Free + Buffers + Cached)
    return (total - (free + buffers + cached)) / 1024;
}

int main() {
    int fd = open(SERIAL_PORT, O_RDWR | O_NOCTTY);
    if (fd < 0 || setup_serial(fd) < 0) {
        perror("Serial Error");
        return 1;
    }

    char display[64];
    time_t rawtime;
    struct tm *info;

    while (1) {
        time(&rawtime);
        info = localtime(&rawtime);
        int sec = info->tm_sec % 12;

        dprintf(fd, "\x1b\x40"); // Clear screen

        if (sec < 3) {
            // DATE: 04.04.2026
            strftime(display, sizeof(display), "%d.%m.%Y", info);
        } 
        else if (sec < 6) {
            // TIME: 12.42.14
            strftime(display, sizeof(display), "%I.%M.%S", info);
        } 
        else if (sec < 9) {
            // CPU: C 2.40 G (7-segment 'C' and 'G')
            snprintf(display, sizeof(display), "C %.3f ", get_cpu_ghz());
        } 
        else {
            // RAM: 1024 Mb (or similar)
            snprintf(display, sizeof(display), "r %ld", get_mem_used_mib());
        }

        write(fd, display, strlen(display));
        sleep(1);
    }
    close(fd);
    return 0;
}
