#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <termios.h>

#define F_PATH "/dev/ttyMux3"
#define MAX_LEN_OF_SHORT_MESSAGE    140

#define RECEIVE_BUF_WAIT_1S        1
#define RECEIVE_BUF_WAIT_2S        2  
#define RECEIVE_BUF_WAIT_3S        3  
#define RECEIVE_BUF_WAIT_4S        4  
#define RECEIVE_BUF_WAIT_5S        5

/*
 * Initialize TTY device
 */
void init_ttyS(int fd)
{
    struct termios options;

    bzero(&options, sizeof(options));       // clear options  
    cfsetispeed(&options,B115200);            // setup baud rate  
    cfsetospeed(&options,B115200);
    options.c_cflag |= (CS8 | CREAD);
    options.c_iflag = IGNPAR;
    tcflush(fd, TCIFLUSH);
    tcsetattr(fd, TCSANOW, &options);
    printf("Initialize tty succeed!\n");
}

/*
 * Set AT command
  */
int send_GSM_GPRS_cmd(int fd, char *send_buf)
{
    ssize_t ret;

    ret = write(fd, send_buf, strlen(send_buf));
    if (ret == -1) {
        printf ("write device %s error\n", F_PATH);
        return -1;
    }
    return 1;
}

/*
 * The result of read AT.
 */
int read_GSM_GPRS_datas(int fd, char *rcv_buf,int rcv_wait)
{
    int retval;
    fd_set rfds;
    struct timeval tv;
    int ret,pos;

    tv.tv_sec = rcv_wait;   // wait 2.5s  
    tv.tv_usec = 0;
    pos = 0; // point to rceeive buf  

    while (1) {
        FD_ZERO(&rfds);
        FD_SET(fd, &rfds);
        retval = select(fd+1 , &rfds, NULL, NULL, &tv);

        if (retval == -1) {        //超时  
            perror("select()");
            break;
        } else if (retval) {           //判断是否还有数据  
            ret = read(fd, rcv_buf+pos, 2048);
            pos += ret;     //读出的数据长度  

            if (rcv_buf[pos-2] == '\r' && rcv_buf[pos-1] == '\n') {
                FD_ZERO(&rfds);
                FD_SET(fd, &rfds);
                retval = select(fd+1 , &rfds, NULL, NULL, &tv);
                if (!retval)
                    break;     // no datas, break  
            }
        } else {
            printf("No data\n");
            break;
        }
    }
    return 1;
}


/*
 * Get Sent result.
 */
void GSM_GPRS_send_cmd_read_result(int fd, char *send_buf, int rcv_wait)
{
    char rcv_buf[2048];

    if ((send_buf==NULL) || (send_GSM_GPRS_cmd(fd, send_buf))) {
        bzero(rcv_buf,sizeof(rcv_buf));
        if (read_GSM_GPRS_datas(fd, rcv_buf, rcv_wait)) {
            printf ("%s\n",rcv_buf);
        } else {
            printf ("read error\n");
        }
    } else{
        printf("write error\n");
    }
}

/*
 * Simple test AT
 */
void GSM_simple_test(int fd)
{
    char *send_buf="at\r";

    GSM_GPRS_send_cmd_read_result(fd,send_buf,RECEIVE_BUF_WAIT_1S);
}

/*
 * Read card ID.
 */
void GSM_read_sim_card_id(int fd)
{
    char *send_buf="at+ccid\r";

    GSM_GPRS_send_cmd_read_result(fd, send_buf, RECEIVE_BUF_WAIT_1S);
}

/*
 * Send Message
 */
void GSM_Send_Message(int fd)
{
    char cmd_buf[23];
    char short_message_buf[MAX_LEN_OF_SHORT_MESSAGE];
    int  i;
    char rcv_buf;

    bzero(cmd_buf, sizeof(cmd_buf));
    bzero(short_message_buf, sizeof(short_message_buf));

    printf ("send short message:\n");

    cmd_buf[0] = 'a';
    cmd_buf[1] = 't';
    cmd_buf[2] = '+';
    cmd_buf[3] = 'c';
    cmd_buf[4] = 'm';
    cmd_buf[5] = 'g';
    cmd_buf[6] = 's';
    cmd_buf[7] = '=';
    cmd_buf[8] = '"';     //AT+CMGS发送短信息命令  

    printf ("please input telephone number:");
    i = 9;
    while (1) {
        cmd_buf[i]=getchar();
        if (cmd_buf[i]=='\n')
            break;
        i++;                //输入号码  
    }

    cmd_buf[i]='"';
    cmd_buf[i+1]='\r';
    cmd_buf[i+2]='\0';          //加上结束符  

    // send cmd :  at+cmgs="(telephone number)"  
    GSM_GPRS_send_cmd_read_result(fd,cmd_buf, RECEIVE_BUF_WAIT_1S);

    // input short message  
    printf("please input short message:");

    i = 0;

    while(i < MAX_LEN_OF_SHORT_MESSAGE-2) {
        short_message_buf[i] = getchar();
        if (short_message_buf[i]=='\n')
            break;
        i++;
    }

    short_message_buf[i] = 0x1A;
    short_message_buf[i+1] = '\r';
    short_message_buf[i+2] = '\0';

    // send short message  
    GSM_GPRS_send_cmd_read_result(fd, short_message_buf, RECEIVE_BUF_WAIT_4S);

    printf("\nend send short message\n");
}
 

int main()
{
    int fd = 0;
    
    fd = open(F_PATH, O_RDWR);
    if (fd < 0) {
        printf("%s can't open\n", F_PATH);
        return -1;    
    }    
    printf("Open %s Successfule!\n", F_PATH);
    init_ttyS(fd);
    GSM_simple_test(fd);
   // GSM_read_sim_card_id(fd);
    GSM_Send_Message(fd);
    close(fd);
    return 0;
}
