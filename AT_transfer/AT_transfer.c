#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>
#include <stdlib.h>
#include <termios.h>
#include <getopt.h>

static int ap_fd = -1;
static int bp_fd = -1;

#define TRANS_BUF_SIZE (1024*64)

static int serial_write(int fd, char *data, int length)
{
    int written = 0;
    int c = 0;

    while(written < length){
        do {
            c = write(fd, data + written, length - written);
        } while (c < 0 && ((errno == EAGAIN)||(errno == EINTR)));

        if (c < 0){
            printf("serial_write error, err = %d", c);
            return -1;
        }
        written += c;
    }
    return written;
}
static void set_tty_device(int fd)
{
    struct termios options;
    tcgetattr(fd, &options);
    options.c_cflag |= CRTSCTS;
    cfmakeraw(&options);
    tcsetattr(fd, TCSANOW, &options);
}

static void *pthread_ap_to_bp(void *data)
{
    int len = 0;
    char buf[TRANS_BUF_SIZE] = {0};

    if (bp_fd < 0 || ap_fd < 0) {
        printf("error bp_fd = %d, ap_fd=%d", bp_fd, ap_fd);
        return NULL;
    }

    while (1) {
        memset(buf, 0x00, TRANS_BUF_SIZE);
        do {
            len = read(ap_fd, buf, TRANS_BUF_SIZE);
			printf("buf %s\n", buf);
        } while (len < 0 && ((errno == EAGAIN)||(errno == EINTR)));
        if (len < 0) {
            printf("read AT command error");
            break;
        }
        if (serial_write(bp_fd, buf, len) < 0) {
            printf("write AT command error");
            break;
        }
    }

    return NULL;
}

void usage(const char *program_name)
{
    printf("%s 1.0.0(2016-06-13)\n",program_name);
    printf("This is used to send message or call phone via GSM\n");
    printf("Usage:%s -p <modem port>\n",
        program_name);
    printf(" -p --port    the port name of ttyMux\n");
}

int main(int argc, char *argv[])
{
	int len = 0;
	char buf[TRANS_BUF_SIZE] = {0};
	pthread_t pth_ap_to_bp;

	const int BUF_SIZE = 512;
	int fd = -1;
	int error = -1;

	int i = 0;
	char cmd_buf[BUF_SIZE];


	char *file_name   = NULL;
    char *output_name = NULL;
    char *config_name = NULL;
    char *keyword     = NULL;

    const char *short_opts = "hp:";
    const struct option long_opts[] = {
		{"help",no_argument,NULL,'h'},
		{"port",required_argument,NULL,'p'},
		{0,0,0,0}
	};
    int hflag = 0;
    int c;
    opterr = 0;

    while((c = getopt_long(argc,argv,short_opts,long_opts,NULL)) != -1) {
		switch (c) {
		case 'h':
			hflag = 1;
			break;
		case 'p':
			file_name = optarg;
        break;
        case '?':
			if(isprint(optopt))
				printf("Error:unknow option '-%c'\n",optopt);
			else
				printf("Error:unknow option character '\\x%x\n'",optopt);
			return 1;
        default:
			abort();
		}
    }

	printf("Wanna open %s\n", file_name);
	memset(cmd_buf, 0, sizeof(cmd_buf));
	fd = open(file_name, O_RDWR | O_NONBLOCK);
	if (fd < 0){
		printf("open %s error\n", file_name);
		return -1;
	}
	set_tty_device(fd);

	printf("Succeed to open %s!\n", file_name);
    sleep(2);

#ifdef CONFIG_CALL
	printf("Start Call.....\n");
    memset(cmd_buf, 0, sizeof(cmd_buf));
    strcpy(cmd_buf, "ATD13530375221");
    write(fd, cmd_buf, strlen(cmd_buf));
    sleep(2);

    memset(cmd_buf, 0, sizeof(cmd_buf));
    strcpy(cmd_buf, "\r");
    write(fd, cmd_buf, strlen(cmd_buf));

    memset(cmd_buf, 0, sizeof(cmd_buf));
    strcpy(cmd_buf, "\032");
    write(fd, cmd_buf, strlen(cmd_buf));
	return 0;

#else /* Send Message */
	
	printf("Send Message\n");
#ifdef CONFIG_SEND_ENGLIST_MESSAGE
	printf("Send English Message\n");
	memset(cmd_buf, 0, sizeof(cmd_buf));
	strcpy(cmd_buf, "AT+CMGF=1");
	write(fd, cmd_buf, strlen(cmd_buf));
	sleep(2);
	
	memset(cmd_buf, 0, sizeof(cmd_buf));
	strcpy(cmd_buf, "\r");
	write(fd, cmd_buf, strlen(cmd_buf));
	sleep(2);
	
	memset(cmd_buf, 0, sizeof(cmd_buf));
	strcpy(cmd_buf, "AT+CMGS=13530375221");
	write(fd, cmd_buf, strlen(cmd_buf));
	sleep(2);
	
	memset(cmd_buf, 0, sizeof(cmd_buf));
	strcpy(cmd_buf, "\r");
	write(fd, cmd_buf, strlen(cmd_buf));
	sleep(2);

	memset(cmd_buf, 0, sizeof(cmd_buf));	
	strcpy(cmd_buf, "0000");
	write(fd, cmd_buf, strlen(cmd_buf));
	sleep(2);

	memset(cmd_buf, 0, sizeof(cmd_buf));
	strcpy(cmd_buf, "\032");
	write(fd, cmd_buf, strlen(cmd_buf));
    
	printf("Mux4 send message end  \r\n  ");

	return 0;

#else /* PUD Message */

	printf("Send PUD Message\n");
    printf("AT+CMGS=16 \r\n");
    memset(cmd_buf, 0, sizeof(cmd_buf));
    strcpy(cmd_buf, "AT+CMGS=16");
    write(fd, cmd_buf, strlen(cmd_buf));
    sleep(2);

    printf("CMGS 2 \r\n");
    memset(cmd_buf, 0, sizeof(cmd_buf));
    strcpy(cmd_buf, "\r");
    write(fd, cmd_buf, strlen(cmd_buf));
    sleep(2);

    printf("CMGS 3 \r\n");
    memset(cmd_buf, 0, sizeof(cmd_buf));
    strcpy(cmd_buf, "0001000b813530375221f100000331d90c");
    write(fd, cmd_buf, strlen(cmd_buf));
    sleep(2);

    printf("CMGS 4 \r\n");
    memset(cmd_buf, 0, sizeof(cmd_buf));
    strcpy(cmd_buf, "\032");
    write(fd, cmd_buf, strlen(cmd_buf));
    sleep(2);
	return 0;    
#endif /* End CONFIG_SEND_MESSAGE */
#endif /* End CONFI_CALL */
}
