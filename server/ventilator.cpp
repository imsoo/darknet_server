#include <zmq.h>
#include <iostream>
#include <assert.h>
#include <pthread.h>
#include "mem_pool.h"
#include "share_queue.h"

// ZMQ
void *sock_pull;
void *sock_push;

// ShareQueue
struct Frame {
  int seq_len;
  int msg_len;
  unsigned char *seq_buf;
  unsigned char *msg_buf;
};
SharedQueue<Frame> frame_queue;

// pool
#define MEM_POOL_UNIT_NUM 1000
CMemPool *mem_pool_msg;
CMemPool *mem_pool_seq;

#define MAX_BUF_LEN 51200
#define MAX_SEQ_LEN 20
void *recv_in_thread(void *ptr)
{
  int recv_msg_len;
  int recv_seq_len;
  unsigned char *recv_buf_ptr = (unsigned char *)(mem_pool_msg->Alloc(MAX_BUF_LEN, true));
  unsigned char *seq_buf_ptr = (unsigned char *)(mem_pool_seq->Alloc(MAX_SEQ_LEN, true));
  Frame frame;

  while(1) {
    recv_seq_len = zmq_recv(sock_pull, seq_buf_ptr, MAX_SEQ_LEN, ZMQ_NOBLOCK);

    if (recv_seq_len > 0) {
      recv_msg_len = zmq_recv(sock_pull, recv_buf_ptr, MAX_BUF_LEN, ZMQ_NOBLOCK);
      if (recv_msg_len > 0) {
        //std::cout << "Ventilator | Recv From Client | SEQ : " << seq_buf_ptr 
        //  << " LEN : " << recv_msg_len << std::endl;

        frame.seq_len = recv_seq_len;
        frame.msg_len = recv_msg_len;
        frame.seq_buf = seq_buf_ptr;
        frame.msg_buf = recv_buf_ptr;
        frame_queue.push_back(frame);

        seq_buf_ptr = (unsigned char *)(mem_pool_seq->Alloc(MAX_SEQ_LEN, true));
        recv_buf_ptr = (unsigned char *)(mem_pool_msg->Alloc(MAX_BUF_LEN, true));
      }
    }
  }
}

void *send_in_thread(void *ptr)
{
  int send_msg_len;
  int send_seq_len;
  unsigned char *send_buf_ptr;
  unsigned char *seq_buf_ptr;
  Frame frame;

  while(1) {
    if (frame_queue.size() > 0) {
      frame = frame_queue.front();
      frame_queue.pop_front();

      send_seq_len = frame.seq_len;
      send_msg_len = frame.msg_len;
      send_buf_ptr = frame.msg_buf;
      seq_buf_ptr = frame.seq_buf;

      zmq_send(sock_push, seq_buf_ptr, send_seq_len, ZMQ_SNDMORE);
      zmq_send(sock_push, send_buf_ptr, send_msg_len, 0);
      //std::cout << "Ventilator | Send To Worker | SEQ : " << seq_buf_ptr 
      //  << " LEN : " << send_msg_len << std::endl;

      mem_pool_seq->Free((void *)seq_buf_ptr);
      mem_pool_msg->Free((void *)send_buf_ptr);
    }
  }
}
int main()
{
  // ZMQ
  int ret;
  void *context = zmq_ctx_new(); 
  sock_pull = zmq_socket(context, ZMQ_PULL);
  ret = zmq_bind(sock_pull, "tcp://*:5575");
  assert(ret != -1);

  sock_push = zmq_socket(context, ZMQ_PUSH);
  ret = zmq_bind(sock_push, "ipc://unprocessed");
  assert(ret != -1);

  // mem_pool
  mem_pool_msg = new CMemPool(MEM_POOL_UNIT_NUM, MAX_BUF_LEN);
  mem_pool_seq = new CMemPool(MEM_POOL_UNIT_NUM, MAX_SEQ_LEN);

  // Thread
  pthread_t recv_thread;
  if (pthread_create(&recv_thread, 0, recv_in_thread, 0))
    std::cerr << "Thread creation failed (recv_thread)" << std::endl;

  pthread_t send_thread;
  if (pthread_create(&send_thread, 0, send_in_thread, 0))
    std::cerr << "Thread creation failed (recv_thread)" << std::endl;

  pthread_detach(send_thread);
  pthread_detach(recv_thread);

  while(1);

  zmq_close(sock_pull);
  zmq_close(sock_push);
  zmq_ctx_destroy(context);
}
