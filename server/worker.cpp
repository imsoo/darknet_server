#include <zmq.h>
#include <iostream>
#include <vector>
#include <memory>
#include <assert.h>
#include <pthread.h>
#include "yolo_v2_class.hpp"
// opencv
#ifdef OPENCV
#include <opencv2/opencv.hpp>			// C++
#pragma comment(lib, "opencv_core249.lib")
#pragma comment(lib, "opencv_imgproc249.lib")
#pragma comment(lib, "opencv_highgui249.lib")
void draw_boxes(cv::Mat mat_img, std::vector<bbox_t> result_vec, std::vector<std::string> obj_names) {
	for (auto &i : result_vec) {
		cv::Scalar color(60, 160, 260);
		cv::rectangle(mat_img, cv::Rect(i.x, i.y, i.w, i.h), color, 3);
		if(obj_names.size() > i.obj_id)
			putText(mat_img, obj_names[i.obj_id], cv::Point2f(i.x, i.y - 10), cv::FONT_HERSHEY_COMPLEX_SMALL, 1, color);
		if(i.track_id > 0)
			putText(mat_img, std::to_string(i.track_id), cv::Point2f(i.x+5, i.y + 15), cv::FONT_HERSHEY_COMPLEX_SMALL, 1, color);
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
#define MAX_BUF_LEN 128000
#define MAX_SEQ_LEN 20
int main()
{
  // opencv
  std::vector<int> param = {cv::IMWRITE_JPEG_QUALITY, 10};

  // ZMQ
  unsigned char recv_buffer[MAX_BUF_LEN];
  unsigned char send_buffer[MAX_BUF_LEN];
  unsigned char seq_buffer[MAX_SEQ_LEN];

  int recv_msg_len;
  int send_msg_len;
  int recv_seq_len;
  int ret;

  void *context = zmq_ctx_new();

  void *sock_pull = zmq_socket(context, ZMQ_PULL);
  ret = zmq_connect(sock_pull, "ipc://unprocessed");

  void *sock_push = zmq_socket(context, ZMQ_PUSH);
  ret = zmq_connect(sock_push, "ipc://processed");
	
	// darkent
  //Detector detector("./yolov3-tiny.cfg", "./yolov3-tiny.weights");
	//auto obj_names = objects_names_from_file("./coco.names");

  while(1) {
		// recv from sync
    recv_seq_len = zmq_recv(sock_pull, seq_buffer, MAX_SEQ_LEN, ZMQ_NOBLOCK);
    if (recv_seq_len > 0) {
      recv_msg_len = zmq_recv(sock_pull, recv_buffer, MAX_BUF_LEN, ZMQ_NOBLOCK);
      //std::cout << "Worker | Recv From Ventilator | SEQ : " << seq_buffer << " LEN : " << recv_msg_len << std::endl;

/*
      // darknet
			// unsigned char array -> vector
      std::vector<unsigned char> raw_vec(recv_buffer, recv_buffer + recv_msg_len);
	
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

			// vector -> array
      send_msg_len = res_vec.size();
      std::copy(res_vec.begin(), res_vec.end(), send_buffer);
*/
			// send to sync
      zmq_send(sock_push, seq_buffer, recv_seq_len, ZMQ_SNDMORE);
      //zmq_send(sock_push, send_buffer, send_msg_len, 0);
      zmq_send(sock_push, recv_buffer, recv_msg_len, 0);
      //std::cout << "Worker | Send To Sync | SEQ : " << seq_buffer << " LEN : " << send_msg_len << std::endl;
    }
  }

  zmq_close(sock_pull);
  zmq_close(sock_push);
  zmq_ctx_destroy(context);

  return 0;
}
