#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <errno.h>
#include <poll.h>


#include "ssap_protocol.h"
#include "string_convert.h"
#include "log.h"
#include "microphone.h"
#include "user_voice.h"

#define SHELL_LINE_NUMBER 25
#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define POLL_TIMEOUT (3 * 1000)  /* 3 seconds */
#define PTIMEOUT (1000000)  /* 3 seconds */
#define POLL_ALWAYS_WAIT -1      //-1 means allways wait
#define MAX_BUF 64

static bool mic_wake_enable_flag = true;

/* set mic wake enable flag */
void set_mic_enable(bool flag)
{
	mic_wake_enable_flag = flag;
	LOGD("set_mic_enable flag=%d\n", mic_wake_enable_flag);
}

/****************************************************************
 * gpio_export
 ****************************************************************/
int gpio_export(unsigned int gpio)
{
    int fd, len;
    char buf[MAX_BUF];

    fd = open(SYSFS_GPIO_DIR "/export", O_WRONLY);
    if (fd < 0) {
        perror("gpio/export");
        return fd;
    }

    len = snprintf(buf, sizeof(buf), "%d", gpio);
    write(fd, buf, len);
    close(fd);

    return 0;
}

/****************************************************************
 * gpio_unexport
 ****************************************************************/
int gpio_unexport(unsigned int gpio)
{
    int fd, len;
    char buf[MAX_BUF];

    fd = open(SYSFS_GPIO_DIR "/unexport", O_WRONLY);
    if (fd < 0) {
        perror("gpio/export");
        return fd;
    }

    len = snprintf(buf, sizeof(buf), "%d", gpio);
    write(fd, buf, len);
    close(fd);
    return 0;
}

/****************************************************************
 * gpio_set_dir
 ****************************************************************/
int gpio_set_dir(unsigned int gpio, unsigned int out_flag)
{
    int fd;
    char buf[MAX_BUF];

    snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR  "/gpio%d/direction", gpio);

    printf("the direction file:%s \n", buf);

    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        perror("gpio/direction");
        return fd;
    }

    if (out_flag)
        write(fd, "out", 4);
    else
        write(fd, "in", 3);

    close(fd);
    return 0;
}

/****************************************************************
 * gpio_set_value
 ****************************************************************/
int gpio_set_value(unsigned int gpio, unsigned int value)
{
    int fd;
    char buf[MAX_BUF];

    snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);

    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        perror("gpio/set-value");
        return fd;
    }

    if (value)
        write(fd, "1", 2);
    else
        write(fd, "0", 2);

    close(fd);
    return 0;
}

/****************************************************************
 * gpio_get_value
 ****************************************************************/
int gpio_get_value(unsigned int gpio, unsigned int *value)
{
    int fd;
    char buf[MAX_BUF];
    char ch;

    snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);

    fd = open(buf, O_RDONLY);
    if (fd < 0) {
        perror("gpio/get-value");
        return fd;
    }

    read(fd, &ch, 1);

    if(ch != '0')
    {
        *value = 1;
    } else {
        *value = 0;
    }

    close(fd);
    return 0;
}

/****************************************************************
 * gpio_set_edge
 ****************************************************************/

int gpio_set_edge(unsigned int gpio, char *edge)
{
    int fd;
    char buf[MAX_BUF];

    snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/edge", gpio);

    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        perror("gpio/set-edge");
        return fd;
    }

    write(fd, edge, strlen(edge) + 1);
    close(fd);
    return 0;
}

/****************************************************************
 * gpio_fd_open
 ****************************************************************/

int gpio_fd_open(unsigned int gpio)
{
    int fd;
    char buf[MAX_BUF];

    snprintf(buf, sizeof(buf), SYSFS_GPIO_DIR "/gpio%d/value", gpio);

    fd = open(buf, O_RDONLY | O_NONBLOCK );
    if (fd < 0) {
        perror("gpio/fd_open");
    }
    return fd;
}

/****************************************************************
 * gpio_fd_close
 ****************************************************************/

int gpio_fd_close(int fd)
{
    return close(fd);
}

/****************************************************************
 * poll gpio
 * @descript: monitor mic array wake up interrupt.
 ****************************************************************/
int microphone_wakeup_poll_thread(void)
{
    //struct pollfd fdset[2];
    struct pollfd fdset[1];
    int nfds = 1;
    int gpio_fd, timeout;
    char *buf[MAX_BUF];
    unsigned int gpio;
    int ret = 0;
	char signal_user1_cmd[128] = {0};
	bool first_poll_flag = true;

    gpio = 11;//atoi(argv[1]);

    printf("the gpio%d was export\n",gpio);
    gpio_export(gpio);

    printf("set gpio%d to input direction!\n",gpio);
    gpio_set_dir(gpio, 0);        //1 output

    printf("set gpio%d to interrupt port!\n",gpio);
    gpio_set_edge(gpio, "rising");//both

    printf("get the gpio%d value file fd!\n",gpio);
    gpio_fd = gpio_fd_open(gpio);

    //timeout = POLL_TIMEOUT;
    timeout = POLL_ALWAYS_WAIT;

    while (1) {
        memset((void*)fdset, 0, sizeof(fdset));

        fdset[0].fd = gpio_fd;
        fdset[0].events = POLLPRI;//POLLIN;//POLLOUT;

        //fdset[1].fd = STDIN_FILENO;
        //fdset[1].events = POLLIN;

        ret = poll(fdset, nfds, timeout);

        if (ret < 0) {
            printf("\npoll() failed!\n");
            return -1;
        }

        if (ret == 0) {
            printf(".");
        }

		/* monitor interrupt */
        if (fdset[0].revents & POLLPRI)
        {
            ret = lseek(gpio_fd,0,SEEK_SET);

            if(ret == -1){
               perror("lseek");
            }

            ret = read(fdset[0].fd, buf, MAX_BUF);

            if(ret == -1){
               perror("read");
            }
			LOGD("\n");
            LOGD("events 0x%x monitor,events 0x%x received, gpio%d interrupt has occurred.\n", POLLPRI,fdset[0].revents,gpio);

			/* notice mic wake interrupt */
			if(!first_poll_flag) {
				/* when post wake up sem, the device enter into recording mode, while can to be awaken again.
				after recording, isr thread  will enable this flag, and recovery status */
				if(mic_wake_enable_flag) {
					post_wakeup();
					mic_wake_enable_flag = false;
				}
			} else {
				first_poll_flag = false;
			}
        }

        fflush(stdout);
    }
    gpio_fd_close(gpio_fd);

    return 0;
}

