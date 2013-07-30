mouse_data_capture
======================

简单解析/dev/input/mice数据格式，转换鼠标动作到字符，方便无线鼠标远程控制程序(比如在mini2440上做的xiami歌曲播放器，用无线鼠标控制)。

输出对应字符：

    z  double click left button
    x  double click right button
    <  click left button
    >  click right button
    p  click middle button
    9  rolling down
    0  rolling up
    q  click left button and click right button immediately, program will exit
