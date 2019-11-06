#include <zmq.h>
#include <iostream>
#include <assert.h>

// ZMQ
void *sock_pull;
void *sock_pub;

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
        //std::cout << "Sync | Recv From Worker | SEQ : " << seq_buf << " LEN : " << recv_msg_len << std::endl;
        zmq_send(sock_pub, seq_buf, recv_seq_len, ZMQ_SNDMORE);
        zmq_send(sock_pub, recv_buf, recv_msg_len, 0);
        //std::cout << "Sync | Pub To Client | SEQ : " << seq_buf << " LEN : " << recv_msg_len << std::endl;
      }
    }
  }
}

int main()
{
  // ZMQ
  int ret;
  void *context = zmq_ctx_new();

  sock_pull = zmq_socket(context, ZMQ_PULL);
  ret = zmq_bind(sock_pull, "ipc://processed");
  assert(ret != -1);

  sock_pub = zmq_socket(context, ZMQ_PUB);
  ret = zmq_bind(sock_pub, "tcp://*:5570");
  assert(ret != -1);

  // thread
  pthread_t recv_thread;
  if (pthread_create(&recv_thread, 0, recv_in_thread, 0))
    std::cerr << "Thread creation failed (recv_thread)" << std::endl;

  pthread_detach(recv_thread);

  while(1);

  zmq_close(sock_pull);
  zmq_close(sock_pub);
  zmq_ctx_destroy(context);
  return 0;
}
