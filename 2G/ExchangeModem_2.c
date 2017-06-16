#include <termios.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

#define len_message 128
#define len_number 16
#define len_swap 32

#define F_PATH "/dev/modem0"

struct message_info                
{
    char phnu[len_number];
    char message[len_message];
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
    options.c_oflag  = 0;
    options.c_lflag  = 0;

    cfsetispeed(&options, B115200);                
    cfsetospeed(&options, B115200);
    tcsetattr(fd, TCSANOW, &options);
};


int kbhit(void)                    
{
    struct termios oldt, newt;
    int ch;
    int oldf;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);
    ch = getchar();
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    if(ch != EOF)
    {
        ungetc(ch, stdin);
        return 1;
    }
    return 0;
}


int send(int fd, char *cmgf, char *cmgs, char *message)            
{
    int nwrite;
    int nread;
    char buff[len_message];            //用于向串口中写数据的buff
    char replay[len_message];        //用于读串口中数据的buff

    memset(buff, 0, len_message);
    strcpy(buff, "AT\r");
    nwrite = write(fd, buff, strlen(buff));        //将buff中的数据写入串口
    printf("nwrite = %d, %s\n", nwrite, buff);

    memset(replay, 0, len_message);
    sleep(1);
    nread = read(fd, replay, sizeof(replay));    //读取串口中的数据到replay
    printf("nread = %d, %s\n", nread, replay);

    memset(buff, 0, len_message);
    strcpy(buff, "AT+CMGF=");
    strcat(buff, cmgf);
    strcpy(buff, "\r");
    nwrite = write(fd, buff, strlen(buff));
    printf("nwrite = %d, %s\n", nwrite, buff);

    memset(replay, 0, len_message);
    sleep(1);
    nread = read(fd, replay, sizeof(replay));
    printf("nread = %d, %s\n", nread, replay);

    memset(buff, 0, len_message);
    strcpy(buff, "AT+CMGS=");
    strcat(buff, cmgs);
    strcat(buff, "\r");
    nwrite = write(fd, buff, strlen(buff));
    printf("nwrite = %d, %s\n", nwrite, buff);

    memset(replay, 0, len_message);
    sleep(1);
    nread = read(fd, replay, sizeof(replay));
    printf("nread = %d, %s\n", nread, replay);

    memset(buff, 0, len_message);
    strcpy(buff, message);
    nwrite = write(fd, buff, strlen(buff));
    printf("nwrite = %d, %s\n", nwrite, buff);

    memset(replay, 0, len_message);
    sleep(1);
    nread = read(fd, replay, sizeof(replay));
    printf("nread = %d, %s\n", nread, replay);
    
    return 0;
}

int send_en_message()
{
    int fd;
    struct message_info info;
    fd = open(F_PATH, O_RDWR|O_NOCTTY|O_NDELAY);
    
    if(fd < 0)
    {
        printf("Can not open %s!\n", F_PATH);
        return -1;
    }
    getchar();                        //读取缓冲区中的一个回车，不然下面读取缓冲区数据的时候会出错
    char cmgf[] = "1";
    char cmgs[16] = {'\0'};
    int conter = 0;

    printf("Please enter your number:\n");
    fgets(info.phnu, (len_number-1), stdin);
    while(strlen(info.phnu) != 12)
    {
        if(conter == 3)
        {
            printf("contr out!\n");
            return -1;
        }
        printf("Your number is not standard,please enter again.\n");
        fgets(info.phnu, (len_number - 1), stdin);
        conter++;
    }
    printf("Enter your message:\n");
    fgets(info.message, (len_message), stdin);
    strcat(info.message, "\x1a");
    strcat(cmgs, "\"");
    strcat(cmgs, info.phnu);
    cmgs[12] = (char){'\"'};

    send(fd, cmgf, cmgs, info.message);
    close(fd);

    return 0;
}

void send_ch_message()                    //这里没有支持发送中文短信
{
    printf("send_ch_message.\n");
}

int call(int fd, char *atd)
{
    int nread;
    int nwrite;
    char buff[len_message];
    char replay[len_message];

    memset(buff, 0, len_message);
    strcpy(buff, "at\r");
    nwrite = write(fd, buff, strlen(buff));
    printf("nwrite = %d, %s\n", nwrite, buff);

    memset(replay, 0, len_message);
    sleep(1);
    nread = read(fd, replay, sizeof(replay));
    printf("nread = %d, %s\n", nread, replay);

    memset(buff, 0, strlen(buff));
    strcpy(buff, "atd");
    strcat(buff, atd);
    strcat(buff, "\r");
    nwrite = write(fd, buff, strlen(buff));
    printf("nwrite = %d, %s\n", nwrite, buff);

    memset(replay, 0, len_message);
    sleep(1);
    nread = read(fd, replay, sizeof(replay));
    printf("nread = %d, %s\n", nread, replay);

    printf("Enter 4 to Hang up!\n");
    char choice = getchar();
    getchar();
    switch(choice)
    {
        case'4':
            memset(buff, 0, len_number);
            strcpy(buff, "ath\r");
            nwrite = write(fd, buff, strlen(buff));
            printf("nwrite = %d, %s\n", nwrite, buff);

            memset(replay, 0, len_number);
            sleep(1);
            nread = read(fd, replay, sizeof(replay));
            printf("nread = %d, %s\n", nread, replay);
        default:
            break;
    }
    return 0;
}

//打电话的函数
int call_phone()
{
    int fd;
    int conter = 0;
    char atd[16] = {'\0'};
    struct message_info info;

    fd = open(F_PATH, O_RDWR | O_NOCTTY | O_NDELAY);
    {
        printf("Can not open %s!\n", F_PATH);
        return -1;
    }

    getchar();
    printf("Enter your phonenumber:\n");
    fgets(info.phnu, (len_number - 1), stdin);
    while(strlen(info.phnu) != 12)
    {
        if(conter == 3)
        {
            printf("conter out!\n");
            return -1;
        }
        printf("Your number is not standard,please enter again.\n");
        fgets(info.phnu, (len_number - 1), stdin);
        conter++;
    }
    strcat(atd, info.phnu);
    atd[11] = (char){';'};
    call(fd, atd);
    close(fd);
    return 0;
}

//来电时拒接电话的函数
int refuse(int fd)
{
    int nwrite;
    int nread;
    char buff[len_number];
    char replay[len_number];

    getchar();
    memset(buff, 0, len_number);
    strcpy(buff, "ath\r");
    nwrite = write(fd, buff, strlen(buff));
    printf("nwrite = %d, %s\n", nwrite, buff);

    memset(replay, 0, len_number);
    sleep(1);
    nread = read(fd, replay, sizeof(replay));
    printf("nread = %d, %s\n", nread, replay);

    return 0;
}

//来电时接电话的函数
int receive(int fd)
{
    char replay[len_number];
    char buff[len_number];
    int nwrite;
    int nread;
    int choice;

    getchar();
    memset(buff, 0, len_number);
    strcpy(buff, "ata\r");
    nwrite = write(fd, buff, strlen(buff));
    printf("nwrite = %d, %s\n", nwrite, buff);

    memset(replay, 0, len_number);
    sleep(1);
    nread = read(fd, replay, sizeof(replay));
    printf("nread = %d, %s\n", nread, replay);

    printf("Enter 4 to Hang up!\n");
    choice = getchar();
    getchar();
    switch(choice)
    {
        case'4':
            memset(buff, 0, len_number);
            strcpy(buff, "ath\r");
            nwrite = write(fd, buff, strlen(buff));
            printf("nwrite = %d, %s\n", nwrite, buff);

            memset(replay, 0, len_number);
            sleep(1);
            nread = read(fd, replay, sizeof(replay));
            printf("nread = %d, %s\n", nread, replay);
        default:
            break;
    }
    return 0;
}

//等待电话来的函数
int waitphone(void)
{
    int i = 4;
    int fd;
    int choice;
    int nwrite;
    int nread;
    char replay[len_number];
    char str[] = "\n\nRING";
    char buff[len_number];

    fd = open(F_PATH, O_RDWR|O_NOCTTY|O_NDELAY);
    if(fd < 0)
    {
        perror("Can not open ttyS1!\n");
    }

    serial_init(fd);
    memset(buff, 0, len_number);
    strcpy(buff, "at\r");
    nwrite = write(fd, buff, strlen(buff));
    printf("nwrite = %d, %s\n", nwrite, buff);

    while(i)
    {
        memset(replay, 0, len_number);
        sleep(1);
        nread = read(fd, replay, sizeof(replay));
        printf("nread = %d, %s\n", nread, replay);

        memset(replay, 0, len_number);
        sleep(2);
        nread = read(fd, replay, sizeof(replay));
        if(!(strncmp(replay, str, 6)))
        {
            printf("there is a ring.\n");
            printf("1.to receive.\n");
            printf("2.to refuse.\n");

            choice = getchar();
            switch(choice)
            {
                case'1':
                    receive(fd);
                    break;
                case'2':
                    refuse(fd);
                    break;
                default:
                    break;
            }
        }
        i--;
    }
    close(fd);
    return 0;
}

//设置为来短信提示状态
int message(void)
{
    int fd;
    int nwrite;
    char buff[len_number];
    char replay[len_message];

    fd = open(F_PATH, O_RDWR|O_NOCTTY|O_NDELAY);
    if(fd < 0)
    {
        perror("Can not open ttyS1!\n");
    }
    serial_init(fd);
    memset(buff, 0, len_number);
    strcpy(buff, "AT+CNMI=3,2\r");
    nwrite = write(fd, buff, strlen(buff));
    sleep(1);
    memset(replay, 0, len_message);
    read(fd, replay, sizeof(replay));
    close(fd);
}

int main(void)
{
    printf("Please enter your choice:\n");
    printf("1:for english message.\n");
    printf("2:for chinese message.\n");
    printf("3:for a call.\n");
    printf("enter nothing for a waiting.\n");
    message();
    send_en_message();
    //send_ch_message();
    return 0;
}
