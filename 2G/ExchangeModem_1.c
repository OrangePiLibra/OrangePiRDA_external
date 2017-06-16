#include <stdio.h>
#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#define F_PATH     "/dev/modem0" 

struct message_info {
    char cnnu[16];
    char phnu[16];
    char message[128];
};

struct pdu_info {
    char cnswap[32];
    char phswap[32];
};

void serial_init(int fd)
{
    struct termios options;
                
    tcgetattr(fd, &options);
    options.c_cflag |= (CLOCAL | CREAD);
    options.c_cflag &= ~CSIZE;
    options.c_cflag &= ~CRTSCTS;
    options.c_cflag |= CS8;
    options.c_cflag &= ~CSTOPB; 
    options.c_iflag |= IGNPAR;
    options.c_oflag = 0;
    options.c_lflag = 0;

    cfsetispeed(&options, B9600);
    cfsetospeed(&options, B9600);
    tcsetattr(fd, TCSANOW, &options);
}

void swap(char number[], char swap[])
{
    char ch1[] = "86";
    char tmp[16];
    int i;

    memset(swap,0,32);
    memset(tmp,0,16);
    strcpy(swap,number);
    strcat(swap,"f");
    strcat(ch1,swap);
    strcpy(swap,ch1);

    for(i = 0;i <= strlen(swap) - 1;i += 2) {
        tmp[i + 1] = swap[i];
        tmp[i] = swap[i + 1];
    }
                
    strcpy(swap,tmp);
}

int send(int fd, char *cmgf, char *cmgs, char *message)
{
    int nread, nwrite;
    char buff[128];
    char reply[128];

    memset(buff, 0, sizeof(buff));
    strcpy(buff, "at\r");
    nwrite = write(fd, buff, strlen(buff));
    printf("nwrite=%d,%s\n", nwrite, buff);

    memset(reply, 0, sizeof(reply));
    sleep(1);
    nread = read(fd, reply, sizeof(reply));
    printf("nread=%d,%s\n", nread, reply);

    memset(buff, 0, sizeof(buff));
    strcpy(buff, "AT+CMGF=");
    strcat(buff, cmgf);
    strcat(buff, "\r");
    nwrite = write(fd, buff, strlen(buff));
    printf("nwrite=%d,%s\n", nwrite, buff);

    memset(reply, 0, sizeof(reply));
    sleep(1);
    nread = read(fd, reply, sizeof(reply));
    printf("nread=%d,%s\n", nread, reply);

    memset(buff, 0, sizeof(buff));
    strcpy(buff, "AT+CMGS=");
    strcat(buff, cmgs);
    strcat(buff, "\r");
    nwrite = write(fd, buff, strlen(buff));
    printf("nwrite=%d,%s\n", nwrite, buff);

    memset(reply, 0, sizeof(reply));
    sleep(1);
    nread = read(fd, reply, sizeof(reply));
    printf("nread=%d,%s\n", nread, reply);

    memset(buff, 0, sizeof(buff));
    strcpy(buff, message);
    nwrite = write(fd, buff, strlen(buff));
    printf("nwrite=%d,%s\n", nwrite, buff);

    memset(reply, 0, sizeof(reply));
    sleep(1);
    nread = read(fd, reply, sizeof(reply));
    printf("nread=%d,%s\n", nread, reply);
}

int send_en_message(int fd, struct message_info info)
{
    char cmgf[] = "1";
    int conter = 0;
    char cmgs[16] = {'\0'};

    getchar();
    printf("enter recever phnumber :\n");
    gets(info.phnu);
                
    while(strlen(info.phnu) != 11) {
        if(conter >= 3) {
            printf("conter out !\n");
            return -1;
        }
        printf("number shuld be --11-- bits ! enter agin :\n");
        gets(info.phnu);
        conter++;
    }

    printf("enter you message !\n");
    gets(info.message);
    strcat(info.message, "\x1a");
    strcat(cmgs, info.phnu);

    send(fd, cmgf, cmgs, info.message);
}

int send_zh_message(int fd, struct message_info info)
{
    char cmgf[] = "0";
    char cmgs[4] = {'\0'};
    char ch2[] = "0891";
    char ch3[] = "1100";
    char ch4[] = "000800";
    char ch5[] = "0d91";
    char final[128];
    char *message[3] = {
        "0a5BB691CC7740706BFF01",
        "0a5BB691CC67098D3CFF01",
        "1a676866539E4FFF0C4F605988558A4F6056DE5BB65403996DFF01"
        };
    struct pdu_info pdu;
    int conter = 0, flag, len;
    
    getchar();
    memset(final, 0, 80);

    printf("enter your centre phone number :\n");
    gets(info.cnnu);
    
    while(strlen(info.cnnu) != 11) {
        if(conter >= 3) {
            printf("conter out !\n");
            return -1;
        }
        printf("number shuld be --11-- bits ! enter agin :\n");
        gets(info.cnnu);
        conter++;
    }

    printf("enter your recever phnumber :\n");
    gets(info.phnu);
    while(strlen(info.phnu) != 11) {
        if(conter >= 3) {
            printf("conter out !\n");
            return -1;
        }
        printf("number shuld be --11-- bits ! enter agin :\n");
        gets(info.phnu);
        conter++;
    }
    printf("choice message :\n");
    printf("1.fire.\n");
    printf("2.thief.\n");
    printf("3.mother@home.\n");
    scanf("%d", &flag);
    swap(info.phnu, pdu.phswap);
    swap(info.cnnu, pdu.cnswap);

    strcpy(final, ch2);
    strcat(final, pdu.cnswap);
    strcat(final, ch3);
    strcat(final, ch5);
    strcat(final, pdu.phswap);
    strcat(final, ch4);
    strcat(final, message[flag - 1]);
    strcat(final, "\x1a");

    len = strlen(ch3) + strlen(ch4) + strlen(ch5) + strlen(pdu.phswap)
            + strlen(message[flag - 1]);
    puts(final);
    sprintf(cmgs,"%d", len/2);
    puts(final);
    send(fd, cmgf, cmgs, final);
}

int main()
{
    int fd;
    char choice;
    struct message_info info;
                
    fd = open(F_PATH, O_RDWR | O_NOCTTY | O_NDELAY);
    if (-1 == fd) {
        printf("Can't Open %s\n", F_PATH);
        return -1;
    }
    serial_init(fd);
    printf("\n============================================\n");
    printf("\tthis is a gprs test program !\n");
    printf("\tcopyright fj@farsight 2011\n");
    printf("============================================\n");
    printf("enter your selete :\n");
    printf("1.send english message.\n");
    printf("2.send chinese message.\n");
    printf("3.exit.\n");
    choice = getchar();
                
    switch (choice) {
        case '1': send_en_message(fd, info);
            break;
        case '2': send_zh_message(fd, info);
            break;
        case '3': 
            break;
        default : 
            break;
    }
    close(fd);
    return 0;
}
