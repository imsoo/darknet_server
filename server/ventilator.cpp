#include <zmq.h>
#include <iostream>
#include <assert.h>
#include <pthread.h>

// ZMQ
void *sock_pull;
void *sock_push;

#define MAX_BUF_LEN 128000
#define MAX_SEQ_LEN 20
void *recv_in_thread(void *ptr)
{
  int recv_msg_len;
  int recv_seq_len;
  unsigned char recv_buf[MAX_BUF_LEN];
  unsigned char seq_buf[MAX_SEQ_LEN] = {0};
  while(1) {
    recv_seq_len = zmq_recv(sock_pull, seq_buf, MAX_SEQ_LEN, ZMQ_NOBLOCK);

    if (recv_seq_len > 0) {
      recv_msg_len = zmq_recv(sock_pull, recv_buf, MAX_BUF_LEN, ZMQ_NOBLOCK);
      if (recv_msg_len > 0) {
        //std::cout << "Ventilator | Recv From Client | SEQ : " << seq_buf << " LEN : " << recv_msg_len << std::endl;
        zmq_send(sock_push, seq_buf, recv_seq_len, ZMQ_SNDMORE);
        zmq_send(sock_push, recv_buf, recv_msg_len, 0);
        //std::cout << "Ventilator | Send To Worker | SEQ : " << seq_buf << " LEN : " << recv_msg_len << std::endl;
      }
    }
  }
}

int main()
{
  int ret;
  void *context = zmq_ctx_new(); 
  sock_pull = zmq_socket(context, ZMQ_PULL);
  ret = zmq_bind(sock_pull, "tcp://*:5575");
  assert(ret != -1);

  sock_push = zmq_socket(context, ZMQ_PUSH);
  ret = zmq_bind(sock_push, "ipc://unprocessed");
  assert(ret != -1);


  // Thread
  pthread_t recv_thread;
  if (pthread_create(&recv_thread, 0, recv_in_thread, 0))
    std::cerr << "Thread creation failed (recv_thread)" << std::endl;

  pthread_detach(recv_thread);

  while(1);

  zmq_close(sock_pull);
  zmq_close(sock_push);
  zmq_ctx_destroy(context);
}
