#include <zmq.h>
#include <iostream>
#include <vector>
#include <memory>
#include <assert.h>
#include <pthread.h>
#include "mem_pool.h"
#include "share_queue.h"
#include "yolo_v2_class.hpp"
// opencv
#ifdef OPENCV
#include <opencv2/opencv.hpp>			// C++
#pragma comment(lib, "opencv_core249.lib")
#pragma comment(lib, "opencv_imgproc249.lib")
#pragma comment(lib, "opencv_highgui249.lib")

void draw_boxes(cv::Mat mat_img, std::vector<bbox_t> result_vec, std::vector<std::string> obj_names,
    int current_det_fps = -1, int current_cap_fps = -1)
{
    int const colors[6][3] = { { 1,0,1 },{ 0,0,1 },{ 0,1,1 },{ 0,1,0 },{ 1,1,0 },{ 1,0,0 } };

    for (auto &i : result_vec) {
        cv::Scalar color = obj_id_to_color(i.obj_id);
        cv::rectangle(mat_img, cv::Rect(i.x, i.y, i.w, i.h), color, 3);
        if (obj_names.size() > i.obj_id) {
            std::string obj_name = obj_names[i.obj_id];
            if (i.track_id > 0) obj_name += " - " + std::to_string(i.track_id);
            cv::Size const text_size = getTextSize(obj_name, cv::FONT_HERSHEY_DUPLEX, 1.0, 2, 0);
            int max_width = (text_size.width > i.w + 2) ? text_size.width : (i.w + 2);
            max_width = std::max(max_width, (int)i.w + 2);
            max_width = text_size.width + 2;
            //max_width = std::max(max_width, 283);
            std::string coords_3d;
            if (!std::isnan(i.z_3d)) {
                std::stringstream ss;
                ss << std::fixed << std::setprecision(2) << "x:" << i.x_3d << "m y:" << i.y_3d << "m z:" << i.z_3d << "m ";
                coords_3d = ss.str();
                cv::Size const text_size_3d = getTextSize(ss.str(), cv::FONT_HERSHEY_DUPLEX, 0.8, 1, 0);
                int const max_width_3d = (text_size_3d.width > i.w + 2) ? text_size_3d.width : (i.w + 2);
                if (max_width_3d > max_width) max_width = max_width_3d;
            }

            cv::rectangle(mat_img, cv::Point2f(std::max((int)i.x - 1, 0), std::max((int)i.y - 30, 0)),
                cv::Point2f(std::min((int)i.x + max_width, mat_img.cols - 1), std::min((int)i.y, mat_img.rows - 1)),
                color, CV_FILLED, 8, 0);
            putText(mat_img, obj_name, cv::Point2f(i.x + 10, i.y - 10), cv::FONT_HERSHEY_DUPLEX, 0.8, cv::Scalar(0, 0, 0), 2);
            if(!coords_3d.empty()) putText(mat_img, coords_3d, cv::Point2f(i.x, i.y-1), cv::FONT_HERSHEY_COMPLEX_SMALL, 0.8, cv::Scalar(0, 0, 0), 1);
        }
    }
    if (current_det_fps >= 0 && current_cap_fps >= 0) {
        std::string fps_str = "FPS detection: " + std::to_string(current_det_fps) + "   FPS capture: " + std::to_string(current_cap_fps);
        putText(mat_img, fps_str, cv::Point2f(10, 20), cv::FONT_HERSHEY_DUPLEX, 1.0, cv::Scalar(50, 255, 0), 2);
    }
}

#endif	// OPENCV

std::vector<std::string> objects_names_from_file(std::string const filename) {
  std::ifstream file(filename);
  std::vector<std::string> file_lines;
  if (!file.is_open()) return file_lines;
  for(std::string line; file >> line;) file_lines.push_back(line);
  std::cout << "object names loaded \n";
  return file_lines;
}

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
SharedQueue<Frame> unprocessed_frame_queue;
SharedQueue<Frame> processed_frame_queue;;

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
      //std::cout << "Worker | Recv From Ventilator | SEQ : " << seq_buf_ptr 
      //  << " LEN : " << recv_msg_len << std::endl;

      frame.seq_len = recv_seq_len;
      frame.msg_len = recv_msg_len;
      frame.seq_buf = seq_buf_ptr;
      frame.msg_buf = recv_buf_ptr;
      unprocessed_frame_queue.push_back(frame);

      seq_buf_ptr = (unsigned char *)(mem_pool_seq->Alloc(MAX_SEQ_LEN, true));
      recv_buf_ptr = (unsigned char *)(mem_pool_msg->Alloc(MAX_BUF_LEN, true));
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
    if (processed_frame_queue.size() > 0) {
      frame = processed_frame_queue.front();
      processed_frame_queue.pop_front();

      send_seq_len = frame.seq_len;
      send_msg_len = frame.msg_len;
      send_buf_ptr = frame.msg_buf;
      seq_buf_ptr = frame.seq_buf;

      zmq_send(sock_push, seq_buf_ptr, send_seq_len, ZMQ_SNDMORE);
      zmq_send(sock_push, send_buf_ptr, send_msg_len, 0);
      //std::cout << "Worker | Send To Sink | SEQ : " << seq_buf_ptr
      //  << " LEN : " << send_msg_len << std::endl;

      mem_pool_seq->Free((void *)seq_buf_ptr);
      mem_pool_msg->Free((void *)send_buf_ptr);
    }
  }
}

int main()
{
  // opencv
  std::vector<int> param = {cv::IMWRITE_JPEG_QUALITY, 80 };

  // ZMQ
  int ret;

  void *context = zmq_ctx_new();

  sock_pull = zmq_socket(context, ZMQ_PULL);
  ret = zmq_connect(sock_pull, "ipc://unprocessed");
  assert(ret != -1);

  sock_push = zmq_socket(context, ZMQ_PUSH);
  ret = zmq_connect(sock_push, "ipc://processed");
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

  // darkent
  //Detector detector("./yolov3.cfg", "./yolov3.weights");
  Detector detector("./yolov3-tiny.cfg", "./yolov3-tiny.weights");
  auto obj_names = objects_names_from_file("./coco.names");

  Frame frame;
  int frame_len;
  unsigned char *frame_buf_ptr;

  while(1) {
    // recv from sync
    if (unprocessed_frame_queue.size() > 0) {
      frame = unprocessed_frame_queue.front();
      unprocessed_frame_queue.pop_front();

      frame_len = frame.msg_len;
      frame_buf_ptr = frame.msg_buf;
      //std::cout << "Darknet | Detect start | LEN : " << frame_len << std::endl; 


      // darknet
      // unsigned char array -> vector
      std::vector<unsigned char> raw_vec(frame_buf_ptr, frame_buf_ptr + frame_len);

      // vector -> mat
      cv::Mat raw_mat = cv::imdecode(cv::Mat(raw_vec), 1);

      // mat -> image_t
      std::shared_ptr<image_t> raw_img = detector.mat_to_image(raw_mat);

      // detect
      std::vector<bbox_t> det_vec = detector.detect(*raw_img);

      // rectangle draw
      draw_boxes(raw_mat, det_vec, obj_names);

      // mat -> vector
      std::vector<unsigned char> res_vec;
      cv::imencode(".jpg", raw_mat, res_vec, param);

      // vector -> frame array
      frame.msg_len = res_vec.size();
      std::copy(res_vec.begin(), res_vec.end(), frame.msg_buf);

      //std::cout << "Darknet | Detect end | LEN : " << frame.msg_len << std::endl; 

      // push to processed frame_queue
      processed_frame_queue.push_back(frame);
    }
  }

  zmq_close(sock_pull);
  zmq_close(sock_push);
  zmq_ctx_destroy(context);

  return 0;
}
