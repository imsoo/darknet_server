#include <zmq.h>
#include <iostream>
#include <vector>
#include <memory>
#include <chrono>
#include <assert.h>
#include <pthread.h>
#include "share_queue.h"
#include "frame.hpp"
#include "args.hpp"
#include "yolo_v2_class.hpp"
// opencv
#ifdef OPENCV
#include <opencv2/opencv.hpp>			// C++

void draw_boxes(cv::Mat mat_img, std::vector<bbox_t> result_vec, std::vector<std::string> obj_names)
{
  int const colors[6][3] = { { 1,0,1 },{ 0,0,1 },{ 0,1,1 },{ 0,1,0 },{ 1,1,0 },{ 1,0,0 } };

  for (auto &i : result_vec) {
    cv::Scalar color = obj_id_to_color(i.obj_id);
    cv::rectangle(mat_img, cv::Rect(i.x, i.y, i.w, i.h), color, 3);
    if (obj_names.size() > i.obj_id) {
      std::string obj_name = obj_names[i.obj_id];
      cv::Size const text_size = getTextSize(obj_name, cv::FONT_HERSHEY_DUPLEX, 1.0, 2, 0);
      int max_width = (text_size.width > i.w + 2) ? text_size.width : (i.w + 2);
      max_width = std::max(max_width, (int)i.w + 2);
      max_width = text_size.width + 2;
      //max_width = std::max(max_width, 283);

      cv::rectangle(mat_img, cv::Point2f(std::max((int)i.x - 1, 0), std::max((int)i.y - 30, 0)),
          cv::Point2f(std::min((int)i.x + max_width, mat_img.cols - 1), std::min((int)i.y, mat_img.rows - 1)),
          color, CV_FILLED, 8, 0);
      putText(mat_img, obj_name, cv::Point2f(i.x + 10, i.y - 10), cv::FONT_HERSHEY_DUPLEX, 0.8, cv::Scalar(0, 0, 0), 2);
    }
  }
}

std::string make_json(std::vector<bbox_t> cur_bbox_vec, std::vector<std::string> obj_names, int frame_id)
{
  int cnt = cur_bbox_vec.size() - 1;
  std::string out_str;
  char *tmp_buf = (char *)calloc(1024, sizeof(char));
  sprintf(tmp_buf, "{\n \"frame_id\":%d, \n \"objects\": [ \n", frame_id);
  out_str = tmp_buf;
  free(tmp_buf);

  for (auto & i : cur_bbox_vec) {
    char *buf = (char *)calloc(2048, sizeof(char));

    sprintf(buf, "  {\"class_id\":%d, \"name\":\"%s\", \"absolute_coordinates\":{\"center_x\":%d, \"center_y\":%d, \"width\":%d, \"height\":%d}, \"confidence\":%f",
        i.obj_id, obj_names[i.obj_id].c_str(), i.x, i.y, i.w, i.h, i.prob);

    //sprintf(buf, "  {\"class_id\":%d, \"name\":\"%s\", \"relative_coordinates\":{\"center_x\":%f, \"center_y\":%f, \"width\":%f, \"height\":%f}, \"confidence\":%f",
    //    i.obj_id, obj_names[i.obj_id], i.x, i.y, i.w, i.h, i.prob);

    out_str += buf;
    out_str += "}";
    if (cnt != 0)
      out_str += ",\n";

    free(buf);
    cnt--;
  }
  out_str += "\n ] \n}";
  return out_str;
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
SharedQueue<Frame> unprocessed_frame_queue;
SharedQueue<Frame> processed_frame_queue;;

// pool
Frame_pool *frame_pool;

void *recv_in_thread(void *ptr)
{
  int recv_json_len;
  unsigned char json_buf[JSON_BUF_LEN];
  Frame frame;

  while(1) {
    recv_json_len = zmq_recv(sock_pull, json_buf, JSON_BUF_LEN, ZMQ_NOBLOCK);

    if (recv_json_len > 0) {
      frame = frame_pool->alloc_frame();
      json_buf[recv_json_len] = '\0';
      json_to_frame(json_buf, frame);

#ifdef DEBUG
      std::cout << "Worker | Recv From Ventilator | SEQ : " << frame.seq_buf 
        << " LEN : " << frame.msg_len << std::endl;
#endif
      unprocessed_frame_queue.push_back(frame);
    }
  }
}

void *send_in_thread(void *ptr)
{
  int send_json_len;
  unsigned char json_buf[JSON_BUF_LEN];
  Frame frame;

  while(1) {
    if (processed_frame_queue.size() > 0) {
      frame = processed_frame_queue.front();
      processed_frame_queue.pop_front();

#ifdef DEBUG
      std::cout << "Worker | Send To Sink | SEQ : " << frame.seq_buf
        << " LEN : " << frame.msg_len << std::endl;
#endif
      send_json_len = frame_to_json(json_buf, frame);
      zmq_send(sock_push, json_buf, send_json_len, 0);

      frame_pool->free_frame(frame);
    }
  }
}

int main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(stderr, "usage: %s <cfg> <weights> <names> [-gpu GPU_ID] [-thresh THRESH]\n", argv[0]);
    return 0;
  }

  const char *cfg_path = argv[1];
  const char *weights_path = argv[2];
  const char *names_path = argv[3];
  int gpu_id = find_int_arg(argc, argv, "-gpu", 0);
  float thresh = find_float_arg(argc, argv, "-thresh", 0.2);
  fprintf(stdout, "cfg : %s, weights : %s, names : %s, gpu-id : %d, thresh : %f\n", 
      cfg_path, weights_path, names_path, gpu_id, thresh);

  // opencv
  std::vector<int> param = {cv::IMWRITE_JPEG_QUALITY, 60 };

  // ZMQ
  int ret;

  void *context = zmq_ctx_new();

  sock_pull = zmq_socket(context, ZMQ_PULL);
  ret = zmq_connect(sock_pull, "ipc://unprocessed");
  assert(ret != -1);

  sock_push = zmq_socket(context, ZMQ_PUSH);
  ret = zmq_connect(sock_push, "ipc://processed");
  assert(ret != -1);

  // frame__pool
  frame_pool = new Frame_pool(5000);

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
  Detector detector(cfg_path, weights_path, gpu_id);
  auto obj_names = objects_names_from_file(names_path);

  // frame
  Frame frame;
  int frame_len;
  int frame_seq;
  unsigned char *frame_buf_ptr;

  auto time_begin = std::chrono::steady_clock::now();
  auto time_end = std::chrono::steady_clock::now();
  double det_time;

  while(1) {
    // recv from ven
    if (unprocessed_frame_queue.size() > 0) {
      frame = unprocessed_frame_queue.front();
      unprocessed_frame_queue.pop_front();

      frame_seq = atoi((const char*)frame.seq_buf);
      frame_len = frame.msg_len;
      frame_buf_ptr = frame.msg_buf;

      // darknet
      // unsigned char array -> vector
      std::vector<unsigned char> raw_vec(frame_buf_ptr, frame_buf_ptr + frame_len);

      // vector -> mat
      cv::Mat raw_mat = cv::imdecode(cv::Mat(raw_vec), 1);

      // fight
      // mat -> image_t
      std::shared_ptr<image_t> raw_img = detector.mat_to_image(raw_mat);

      // detect
      time_begin = std::chrono::steady_clock::now();
      std::vector<bbox_t> det_vec = detector.detect(*raw_img, thresh);
      time_end = std::chrono::steady_clock::now();
      det_time = std::chrono::duration <double, std::milli> (time_end - time_begin).count();

#ifdef DEBUG
      std::cout << "Darknet | Detect | SEQ : " << frame.seq_buf << " Time : " << det_time << "ms" << std::endl;
#endif

      // detect result to json
      std::string det_json = make_json(det_vec, obj_names, frame_seq);
      frame.det_len = det_json.size();
      memcpy(frame.det_buf, det_json.c_str(), frame.det_len);
      frame.det_buf[frame.det_len] = '\0';

      // bounding box draw
      draw_boxes(raw_mat, det_vec, obj_names);

      // mat -> vector
      std::vector<unsigned char> res_vec;
      cv::imencode(".jpg", raw_mat, res_vec, param);

      // vector -> frame array
      frame.msg_len = res_vec.size();
      std::copy(res_vec.begin(), res_vec.end(), frame.msg_buf);

      // push to processed frame_queue
      processed_frame_queue.push_back(frame);
    }
  }

  delete frame_pool;
  zmq_close(sock_pull);
  zmq_close(sock_push);
  zmq_ctx_destroy(context);

  return 0;
}
