// @author: lbzhung
// @brief: capture mouse data by /dev/input/mice 
// @see: linux kernel drivers/input/mousedev.c
//       http://www.computer-engineering.org/ps2mouse/
//       
// @output:
//    z  double click left button
//    x  double click right button
//    <  click left button
//    >  click right button
//    p  click middle button
//    9  rolling down
//    0  rolling up
//    q  click left button and click right button immediately, program will exit

#include <cstdio>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

typedef unsigned char BYTE;

#pragma pack(1)
struct imps2_data
{
    BYTE btn_left:1;
    BYTE btn_right:1;
    BYTE btn_middle:1;
    BYTE NONE:1;
    BYTE x_sign:1;  // x offset sign
    BYTE y_sign:1;  // y offset sign
    BYTE x_overflow:1; // x offset is overflow
    BYTE y_overflow:1; // y offset is overflow

    // x/y movement offset relative to its position
    signed char x;
    signed char y;
    signed char z;
};
#pragma pack()

// button double click process
class ButtonProcess
{
    public:
        enum {
            BTN_LEFT,
            BTN_RIGHT,
            BTN_NULL
        };

    public:
        ButtonProcess()
        :m_btnPre(BTN_NULL),
        m_bTimer(false)
        {
            m_tv_cur.tv_sec = 0;
            m_tv_cur.tv_usec = 0;
        }

        void EnableTimer()
        {
            m_bTimer = true;
            gettimeofday(&m_tv_cur, NULL);
        }

        void DisableTimer()
        {
            m_bTimer = false;
        }

        bool IsTimerEnable()
        {
            return m_bTimer;
        }

        bool Button(int type)
        {
            assert(type == BTN_LEFT || type == BTN_RIGHT);
            if (m_btnPre == BTN_NULL)
            {
                // save btn wait timeout and send
                m_btnPre = type;
                EnableTimer();
            }
            else if (m_btnPre != type)
            {
                // left and right, quit
                printf("q\n");
                fflush(stdout);
                m_btnPre = BTN_NULL;
                DisableTimer();
                return false;
            }
            else
            {
                if (m_btnPre == BTN_LEFT)
                {
                    // double click left
                    printf("z\n");
                    fflush(stdout);
                }
                else
                {
                    // double click right
                    printf("x\n");
                    fflush(stdout);
                }
                m_btnPre = BTN_NULL;
                DisableTimer();
            }
            return true;
        }

        void Timer()
        {
            if (IsTimerEnable())
            {
                //printf("in Timer\n");
                //
                // timeout 300ms
                struct timeval tv;
                gettimeofday(&tv, NULL);
                long tv_interval_ms = (tv.tv_sec*1000 + tv.tv_usec/1000) - (m_tv_cur.tv_sec*1000 + m_tv_cur.tv_usec/1000);
                if (tv_interval_ms >= 300)
                {
                    assert(m_btnPre != BTN_NULL);
                    DisableTimer();

                    if (m_btnPre == BTN_LEFT)
                    {
                        // left
                        printf("<\n");
                        fflush(stdout);
                    }
                    else
                    {
                        // right
                        printf(">\n");
                        fflush(stdout);
                    }
                    // reset m_btnPre
                    m_btnPre = BTN_NULL;
                }
            }
        }

    private:
        int m_btnPre;
        // time for double click check
        struct timeval m_tv_cur;
        bool m_bTimer;
};

int main(int argc, char *argv[])
{
    BYTE mousedev_imps_seq[] = { 0xf3, 200, 0xf3, 100, 0xf3, 80 };
    int mice_fd = open("/dev/input/mice", O_RDWR|O_NONBLOCK);
    if (mice_fd == -1)
    {
        //fprintf(stderr, "Open mice fail");
        return 1;
    }

    // set mice mode, so can read rolling wheels
    int nRet = write(mice_fd, mousedev_imps_seq, sizeof(mousedev_imps_seq));
    if (nRet < 0)
    {
        //fprintf(stderr, "set mice imps2 fail\n");
        return 1;
    }

    ButtonProcess btnProcess;
    while (true)
    {
        fd_set fdset;
        // 20 ms
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 20000;

        FD_ZERO(&fdset);
        FD_SET(mice_fd, &fdset);

        int ret = select(mice_fd+1, &fdset, NULL, NULL, &tv);
        if (ret < 0)
        {
            //fprintf(stderr, "select return error\n");
            return 1;
        }
        else
        {
            if (FD_ISSET(mice_fd, &fdset))
            {
                imps2_data data;
                while (true)
                {
                    int nLen = read(mice_fd, &data, sizeof(data));
                    if (nLen == 0)
                    {
                        //fprintf(stderr, "End of file :)\n");
                        return 1;
                    }
                    else if (nLen == -1)
                    {
                        if (errno != EAGAIN)
                        {
                            //fprintf(stderr, "read fail\n");
                            return 1;
                        }
                        break;
                    }
                    else
                    {
                        // nLen = 1 is ack
                        if (nLen == sizeof(data))
                        {
                            //printf("nLen:%d, left:%d right:%d middle:%d X:%d Y:%d Z:%d\n",
                            //nLen, data.btn_left, data.btn_right, data.btn_middle, data.x, data.y, data.z);

                            // rolling wheels
                            if (data.z == 0 && data.x == 0 && data.y == 0)
                            {
                                if (data.btn_left)
                                {
                                    if (!btnProcess.Button(ButtonProcess::BTN_LEFT))
                                    {
                                        // OK quit
                                        close(mice_fd);
                                        return 0;
                                    }
                                }
                                else if (data.btn_right)
                                {
                                    if (!btnProcess.Button(ButtonProcess::BTN_RIGHT))
                                    {
                                        // OK quit
                                        close(mice_fd);
                                        return 0;
                                    }
                                }
                                else if (data.btn_middle)
                                {
                                    // pause
                                    printf("p\n");
                                    fflush(stdout);
                                }
                            }
                            else
                            {
                                if (data.z != 0)
                                {
                                    if (data.z > 0)
                                    {
                                        // rolling down
                                        printf("9\n");
                                        fflush(stdout);
                                    }
                                    else
                                    {
                                        // rolling up
                                        printf("0\n");
                                        fflush(stdout);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        btnProcess.Timer();
    }

    return 0;
}
