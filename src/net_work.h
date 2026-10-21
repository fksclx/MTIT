#pragma once
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <sys/epoll.h> //EPOLL

#define VI_DELIMITER "||%vi_end%||"
int deinit_tcp();
void ProcessCmd(uint8_t *cmdBuf, int cmdLen,int sockfd);
void CatPacketAndPipe(int sockfd, unsigned char *pbuf, int *rsize);
int init_tcp();
void close_client();
int send_data(char *data);
void do_recieve(struct epoll_event *ev);
void do_error(struct epoll_event *ev);
int FindNewClient();
void sendRtStart();
void sendRtStop();
void sendSaveStart();
void sendSaveUpdate();
void sendSaveStop();
void sendConnected();
void sendUnconnected();
void sendLimit();
void sendUnLimit();
void sendUpdate();
void Reply(int replytype, int cmdNo,int sockfd);

