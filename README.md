# SimpleHttpServer

1. 本项目实现了一个基于线程池 + 非阻塞socket + epoll (ET和LT均实现) + 事件处理(Reactor) 的并发模型，传输层使用IO多路复用技术管理TCP连接，同时在应用层搭建了HTTP服务器，同时支持长连接和短连接，能主动断开不活跃的长连接，使用状态机解析HTTP请求报文，支持GET请求，能返回图片视频等静态资源，同时使用cgi脚本处理POST请求，并支持文件上传；
2. 线程间的工作模式：主线程负责Accept请求，然后采用Round-bin方式异步调用线程池中的其他线程去管理请求的IO事件。利用eventfd(内部是一个计数器)实现线程间异步唤醒。IO请求事件会被分发到固定的队列中，由负责管理该IO请求的子线程来取走，需要用到互斥锁（主线程放，子线程取。锁由特定线程的Eventloop类创建，只会被该线程和主线程竞争）;
3. 单例模式的异步日志：用专门的一个线程负责收集后端buffer日志信息并将其写入磁盘，其他业务只负责生存日志信息，存到前台日志buffer里。刷盘策略：前台日志buffer满（前后端buffer互换）或者距离上次刷盘过了3秒，notify日志线程来刷盘。（每条日志都是临时类，在析构函数里将内容写入前台buffer，要加互斥锁保证日志信息的同步性）;
4. 定时器：用的优先队列，选择小根堆模式。插入一个定时器O(logN)，pop一个定时器O(1)，但是需要调整堆，故删除一个定时器O(logN)。


----------
## Start

>**sh build.sh**


>.**/Run [-p port] [-l log\_name]  [-t thread\_num]  [-a listen\_mode]  [-b connect\_mode]**


- -p，自定义端口号.默认8888
- -l，日志文件名, 默认WebServer
- -t，线程池中的线程数量, 默认为4
- -a，监听套接字模式，0代表LT模式，1代表ET模式
- -b，连接套接字模式，0代表LT模式，1代表ET模式


## Pressure testing

使用Webbench对服务器进行压力测试，对listenfd和connfd分别采用ET和LT模式

LT+LT

![avatar](https://raw.githubusercontent.com/Huangggjian/SimpleHttpServer/master/imgs/a0b0.png)

LT+ET

![avatar](https://raw.githubusercontent.com/Huangggjian/SimpleHttpServer/master/imgs/a0b1.png)


ET+LT

![avatar](https://raw.githubusercontent.com/Huangggjian/SimpleHttpServer/master/imgs/a1b0.png)

ET+ET

![avatar](https://raw.githubusercontent.com/Huangggjian/SimpleHttpServer/master/imgs/a1b1.png)
