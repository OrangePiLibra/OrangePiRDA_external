#include <stdio.h>  
#include <string.h>  
#include <stdlib.h>  
#include <fcntl.h>       
#include <unistd.h>     
#include <termios.h>  // set baud rate，交叉编一起必须带这种库  
#include <fcntl.h>  
#include <sys/select.h>  
#include <sys/time.h>  
#include <sys/types.h>  
  
#define FUNC_RUN            0  
#define FUNC_NOT_RUN        1  
#define SIMPLE_TEST         1  
#define READ_SIM_CARD_ID    2  
#define GSM_GPRS_SIGNAL     3  
#define MAKE_A_CALL         4  
#define WAIT_A_CALL         5  
#define SHORT_MESSAGE       6  
#define POWER_MANAGE        7  
#define FUNC_QUIT           8  
#define SEND_SHORT_MESSAGE      1  
#define READ_SHORT_MESSAGE      2  
#define CONFIG_SHORT_MESSAGE_ENV        3  
#define QUIT_SHORT_MESSAGE      4  
  
#define DEVICE_TTYS       "/dev/modeo1"      //modem分配的UART口  
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
}

/*
 * Print prompt
 */
void print_prompt(void)  
{  
    printf ("Select what you want to do:\n");  
    printf ("1 : Simple Test\n");  
    printf ("2 : Read SIM CARD ID\n");  
    printf ("3 : Test GSM/GPRS signal\n");  
    printf ("4 : Make A Call\n");  
    printf ("5 : Wait A Call\n");  
    printf ("6 : Short message\n");  
    printf ("7 : Power manage\n");  
    printf ("8 : Quit\n");  
    printf (">");  
}

/*
 * Set AT command
 */
int send_GSM_GPRS_cmd(int fd, char *send_buf)  
{  
    ssize_t ret;  
    
    ret = write(fd, send_buf, strlen(send_buf));  
    if (ret == -1) {  
        printf ("write device %s error\n", DEVICE_TTYS);  
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

void GSM_read_sim_card_id(int fd)  
{  
    char *send_buf="at+ccid\r";  
                    
    GSM_GPRS_send_cmd_read_result(fd, send_buf, RECEIVE_BUF_WAIT_1S);  
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
 * Get network arguments
 */
void GSM_gprs_signal(int fd)  
{  
    char *send_buf="at+creg?\r";   //网络注册  
                 
    GSM_GPRS_send_cmd_read_result(fd,send_buf,RECEIVE_BUF_WAIT_1S);  
    
    send_buf="at+cgreg?\r";  
    GSM_GPRS_send_cmd_read_result(fd,send_buf,RECEIVE_BUF_WAIT_1S);  
                                       
    send_buf="at+csq\r";       //信号质量  
    GSM_GPRS_send_cmd_read_result(fd,send_buf,RECEIVE_BUF_WAIT_1S);  
                                                    
    send_buf="at+cops?\r";     //运营商信息  
    GSM_GPRS_send_cmd_read_result(fd,send_buf,RECEIVE_BUF_WAIT_1S);  
}

/*
 * Call function
 */
void GSM_call(int fd)  
{  
    char send_buf[17];          //发送缓冲区  
    char *send_cmd_ath = "ath\r";   //挂机命令    
    int i;  
      
    // input telephone number  
    bzero(send_buf,sizeof(send_buf));  
          
    send_buf[0] = 'a';  
    send_buf[1] = 't';  
    send_buf[2] = 'd';  
    send_buf[16] = '\0';  
      
    printf("please input telephone number:");  
    i = 3;  
    
    while (1) {  
        send_buf[i] = getchar();      //从标准输入设备“键盘”获取输入  
        if (send_buf[i] == '\n') 
            break;  
        i++;  
    }  
      
    send_buf[i] = ';';            //发送命令后加符号;  
    send_buf[i+1] = '\r';  
    // end input telphone number  
      
    // send cmd  
    GSM_GPRS_send_cmd_read_result(fd, send_buf, RECEIVE_BUF_WAIT_1S);     //拨号  
      
    //quit call  
    printf("press ENTER for quit:");  
    getchar();      //按任意键退出拨号  
      
    // send cmd  
    GSM_GPRS_send_cmd_read_result(fd, send_cmd_ath, RECEIVE_BUF_WAIT_1S); //挂机  
}  

/*
 * Wait call
 */
void GSM_wait_call(int fd)  
{  
    char rcv_buf[2048];  
    char *send_cmd_ath = "ath\r";       //挂机  
    int wait_RING;  
  
    wait_RING = 10;        //等待10次  
    while (wait_RING!=0) {    
        bzero(rcv_buf,sizeof(rcv_buf));  
        if (read_GSM_GPRS_datas(fd, rcv_buf, RECEIVE_BUF_WAIT_1S)) {  
            printf ("%s\n",rcv_buf);    //等待应答  
        } else {  
            printf ("read error\n");  
        }  
        wait_RING--;  
    }  
      
    GSM_GPRS_send_cmd_read_result(fd, send_cmd_ath, RECEIVE_BUF_WAIT_1S);  
    printf("quit wait_call\n");  
} 

/****************************************
 *   Message Service
 ***************************************/
 
/*
 * Configure Message center
 */ 
void GSM_Conf_Message(int fd)  
{  
    char *send_buf="at+cmgf=1\r";           //设定SMS的控制方式  
    char *send_center_buf="at+csca=\"+8613800755500\"\r";   //短信服务中心地址  
                 
    GSM_GPRS_send_cmd_read_result(fd, send_buf, RECEIVE_BUF_WAIT_1S);  
    // set short message center number  
    GSM_GPRS_send_cmd_read_result(fd, send_center_buf, RECEIVE_BUF_WAIT_1S);  
                                    
    printf("end config short message env\n");  
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
    GSM_GPRS_send_cmd_read_result(fd,cmd_buf,RECEIVE_BUF_WAIT_1S);  
      
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
    GSM_GPRS_send_cmd_read_result(fd, short_message_buf,RECEIVE_BUF_WAIT_4S);  
  
    printf("\nend send short message\n");  
}  

void GSM_Read_Message(int fd)  
{  
    char *send_buf = "at+cmgl=\"ALL\"\r";         //读出SIM卡的短信息  
    char rcv_buf[2048];  
                     
    GSM_GPRS_send_cmd_read_result(fd, send_buf, RECEIVE_BUF_WAIT_3S);  
    printf("end read all short message\n");  
}

/*
 * Short message service
 */
void GSM_short_mesg(int fd)  
{  
    int flag_sm_run, flag_sm_select;  
  
    flag_sm_run = FUNC_RUN;  
    while (flag_sm_run == FUNC_RUN) {  
        printf ("\n Select:\n");  
        printf ("1 : Send short message \n");  
        printf ("2 : Read all short message \n");  
        printf ("3 : Config short message env\n");  //短消息中心  
        printf ("4 : quit\n");  
        printf (">");  
        scanf("%d",&flag_sm_select);        //输入选择选项  
        getchar();              //等待键盘输入  
          
        switch (flag_sm_select) {  
            case SEND_SHORT_MESSAGE: { GSM_Send_Message(fd); break;}  
            case READ_SHORT_MESSAGE: { GSM_Read_Message(fd); break;}  
            case CONFIG_SHORT_MESSAGE_ENV: { GSM_Conf_Message(fd); break;}  
            case QUIT_SHORT_MESSAGE: { flag_sm_run = FUNC_NOT_RUN; break;}  
            default: {  
                printf("please input your select use 1 to 3\n");  
            }  
        }  
    }  
    printf ("\n");  
}

void func_GSM(int fd)  
{  
    int flag_func_run;  
    int flag_select_func;  
    ssize_t ret;  
        
    flag_func_run = FUNC_RUN;  
  
    while (flag_func_run == FUNC_RUN) {  
        print_prompt();         // print select functions txt  
        scanf("%d",&flag_select_func);  // user input select  
        getchar();  
  
        switch(flag_select_func) {  
            case SIMPLE_TEST: { GSM_simple_test(fd); break;}  
            case READ_SIM_CARD_ID: { GSM_read_sim_card_id(fd); break;}  
            case GSM_GPRS_SIGNAL: { GSM_gprs_signal(fd); break;}  
            case MAKE_A_CALL: { GSM_call(fd); break;}  
            case WAIT_A_CALL: { GSM_wait_call(fd); break;}  
            case SHORT_MESSAGE: { GSM_short_mesg(fd); break;}  
            case FUNC_QUIT: {  
                flag_func_run = FUNC_NOT_RUN;  
                printf("Quit GSM/GPRS function.  byebye\n");  
                break;  
                }  
            default: {  
                printf("please input your select use 1 to 7\n");  
            }  
        }  
    }  
} 

int main(void)  
{  
    FILE *fp = NULL;  
    int fd;

    fp = fopen(DEVICE_TTYS, "r+"); //打开TTY设备  
    if (fp == NULL) {  
        printf("open device %s error\n", DEVICE_TTYS);  
        return -1;
    } else {
        fd = fileno(fp);  
        init_ttyS(fd);  // init device  
        func_GSM(fd);   // GSM/GPRS functions         
        fclose(fp);
        fp = NULL;
    }  
    return 0;  
} 


