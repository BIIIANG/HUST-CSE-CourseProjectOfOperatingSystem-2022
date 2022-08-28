#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

char* randStr(char* str, const int len) {
    int i;
    for (i = 0; i < len; ++i) {
        switch ((rand() % 3)) {
            case 1:
                str[i] = 'A' + rand() % 26;
                break;
            case 2:
                str[i] = 'a' + rand() % 26;
                break;
            default:
                str[i] = '0' + rand() % 10;
                break;
        }
    }
    str[i] = '\0';
    return str;
}

void showInfo() {
    time_t rawtime;
    struct tm* timeinfo;
    time(&rawtime);
    timeinfo = localtime(&rawtime);
    printf("[%02d:%02d:%02d@%d]: ", timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, getpid());
}

int main() {
    char buf[256];
    int a, b, op = 0, fd;
    srand(time(NULL));

    showInfo();
    printf("请选择打开模式(1-阻塞/2-不阻塞/3-结束): ");
    scanf("%d", &op);
    while (op != 1 && op != 2 && op != 3) {
        showInfo();
        printf("输入错误, 请重新输入: ");
        scanf("%d", &op);
    }
    switch (op) {
        case 1: fd = open("/dev/ChrDev_XBA", O_RDWR); break;
        case 2: fd = open("/dev/ChrDev_XBA", O_RDWR | O_NONBLOCK); break;
        case 3: return 0;
    }

    if (fd < 0) {
        showInfo();
        perror("打开设备失败.\n");
        return -1;
    }

    showInfo();
    printf("请选择角色(1-生产者/2-消费者/3-结束): ");
    scanf("%d", &op);
    while (op != 1 && op != 2 && op != 3) {
        showInfo();
        printf("输入错误, 请重新输入: ");
        scanf("%d", &op);
    }
    if (op == 3) { return 0; }
    if (op == 1) {
        while (1) {
            randStr(buf, rand() % 16 + 1);
            showInfo();
            printf("尝试写入 %ld 个字符: %s ...\n", strlen(buf), buf);
            int ret = write(fd, buf, strlen(buf));
            if (ret >= 0) {
                showInfo();
                printf("写入了 %d 个字符.\n", ret);
            } else {
                showInfo();
                printf("写入失败, 错误码: %d.\n", ret);
            }
            for (int i = 0; i < 0x2FFFFFFF; i++) {}
            // usleep(rand() % 1000000 + 1000000);
        }
    }
    if (op == 2) {
        while (1) {
            size_t len = rand() % 16 + 1;
            showInfo();
            printf("尝试读取 %ld 个字符 ...\n", len);
            int ret = read(fd, buf, len);
            if (ret >= 0) {
                buf[ret] = '\0';
                showInfo();
                printf("读取了 %d 个字符: %s.\n", ret, buf);
            } else {
                showInfo();
                printf("读取失败, 错误码: %d.\n", ret);
            }
            for (int i = 0; i < 0x2FFFFFFF; i++) {}
            // usleep(rand() % 1000000 + 1000000);
        }
    }

    close(fd);

    return 0;
}