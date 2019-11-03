#include <zmq.h>
#include <string>
#include <cstring>
#include <iostream>
#include <assert.h>
#include <unistd.h>
#include <pthread.h>
#include "shmringbuffer.hh"

#define MAX_FRAME_LEN 256000
struct FrameNode {
  unsigned int len;
  unsigned char frame[MAX_FRAME_LEN];
};

#define MAX_DARKNET_NUM 4
#define MAX_BUF_LEN 256000
char recv_buf[MAX_BUF_LEN];

// shmRingbuffer
const int CAPACITY = 1000;
FrameNode f_n;
ShmRingBuffer<FrameNode> recv_buffer(CAPACITY, true);
ShmRingBuffer<FrameNode> send_buffer(CAPACITY, true, "/send_buffer");

// ZMQ
int msg_len;
void *context;
void *responder;
zmq_pollitem_t items[1];

void *recv_in_thread(void *ptr)
{
  while (1) {
      msg_len = zmq_recv(responder, recv_buf, MAX_BUF_LEN, 0);
      if (msg_len > 0) {
      f_n.len = msg_len;
      memcpy(f_n.frame, recv_buf, f_n.len);
      recv_buffer.push_back(f_n);
      std::cout << "[RECV] " << f_n.len << " " << std::endl;
      }
  }
}

void *send_in_thread(void *ptr)
{
  while (1) {
    if (send_buffer.begin() != send_buffer.end()) {
      std::cout << "[SEND] " << (int)f_n.len << std::endl;
      f_n = send_buffer.dump_front();
      //zmq_send(responder, "ims", 3, ZMQ_SNDMORE);
      zmq_send(responder, f_n.frame, f_n.len, ZMQ_NOBLOCK);
    }
  }
}

int main()
{
  // fthread
  pthread_t recv_thread;
  pthread_t send_thread;

  int n1 = fork();

  // root run server
  if (n1 > 0)
  {
    // ZMQ
    int msg_len;
    context = zmq_ctx_new();
    responder = zmq_socket(context, ZMQ_REP);   
    items[0] = { static_cast<void*>(responder), 0, ZMQ_POLLIN, 0 };
    int rc = zmq_bind(responder, "tcp://*:5570");
    if(pthread_create(&recv_thread, 0, recv_in_thread, 0))
      std::cerr << "Thread creation failed" << std::endl;

    if(pthread_create(&send_thread, 0, send_in_thread, 0))
      std::cerr << "Thread creation failed" << std::endl;

    // thread start
    pthread_detach(recv_thread);
    pthread_detach(send_thread);

    while(1);

  }
  // child run darkent
  else {
    int ret;
    const char *darknet_path = "/home/smkongdo/darknet/darknet";
    chdir("/home/smkongdo/darknet");
    ret = execl(darknet_path, darknet_path, "detector", "demo", "cfg/coco.data", 
        "cfg/yolov3.cfg", "yolov3.weights", "-use_shm", NULL);
    if (ret == -1) {
      std::cerr << "Darknet execl error " << std::endl;
      exit(EXIT_FAILURE);
    } 
  }

  return 0;
}
