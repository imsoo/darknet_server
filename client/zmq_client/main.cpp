#define FD_SETSIZE 4096;
#include <zmq.h>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <thread>
#include <queue>
#include <chrono>
#include <thread>
#include <concurrent_priority_queue.h>
#include "share_queue.h"


using namespace cv;
using namespace std;


// thread
void fetch_thread(void);
void capture_thread(void);
void recv_thread(void);
void output_show_thread(void);
void input_show_thread(void);
volatile bool exit_flag = false;
volatile bool fetch_flag = false;

// ZMQ
void *context;
void *sock_push_server;
void *sock_pull_server;

// pair
class ComparePair
{
public:
	bool operator()(pair<long, void *> n1, pair<long, void *> n2) {
		return n1.first > n2.first;
	}
};

// Queue
//SharedQueue<Mat> recv_queue;
SharedQueue<Mat> cap_queue;
SharedQueue<Mat> fetch_queue;
concurrency::concurrent_priority_queue<pair<long, void*>, ComparePair> recv_queue;


// opencv
static void onMouse(int event, int x, int y, int, void*);
VideoCapture cap;
static Mat mat_show_output;
static Mat mat_show_input;
static Mat mat_recv;
static Mat mat_cap;
static Mat mat_fetch;

const int cap_width = 640;
const int cap_height = 480;
double delay;

int main()
{
	// ZMQ
	context = zmq_ctx_new();

	sock_push_server = zmq_socket(context, ZMQ_PUSH);
	zmq_connect(sock_push_server, "tcp://104.199.144.166:5575");

	sock_pull_server = zmq_socket(context, ZMQ_PULL);
	zmq_connect(sock_pull_server, "tcp://104.199.144.166:5570");


	//비디오 캡쳐 초기화
	//cap = VideoCapture("C:\\Users\\COMSE\\source\\repos\\zmq_test\\x64\\Release\\road.mp4");

	cap = VideoCapture(0);

	if (!cap.isOpened()) {
		cerr << "Erro VideoCapture.\n";
		return -1;
	}

	double fps = cap.get(CAP_PROP_FPS);
	delay = (1000.0 / fps) / 2.0;

	
	// 동영상 프레임 읽어오기
	cap.read(mat_fetch);

	if (mat_fetch.empty()) {
		cerr << "빈 영상이 캡쳐되었습니다.\n";
		return 0;
	}

	mat_show_output = mat_fetch.clone();
	mat_show_input = mat_fetch.clone();
	
	// thread 설정
	thread thread_fetch(fetch_thread);
	thread_fetch.detach();

	while (!fetch_flag);

	thread thread_show_input(output_show_thread);
	thread thread_show_output(input_show_thread);
	thread thread_recv(recv_thread);
	thread thread_capture(capture_thread);

	thread_show_input.detach();
	thread_show_output.detach();
	thread_recv.detach();
	thread_capture.detach();

	while (!exit_flag)
	{
		cout << recv_queue.size() << " " << delay <<  endl;
	}

	cap.release();

	zmq_close(sock_pull_server);
	zmq_close(sock_push_server);
	zmq_ctx_destroy(context);

	return 0;
}

#define BUF_LEN 256000
#define FETCH_THRESH 0
void fetch_thread(void) {
	while (!exit_flag) {
		// 동영상 프레임 읽어오기
		if (cap.grab()) {
			cap.retrieve(mat_fetch);

			// fetch 큐에 삽입
			fetch_queue.push_back(mat_fetch.clone());

			if (fetch_queue.size() > FETCH_THRESH)
				fetch_flag = true;
		}
		// 
		else {
			fetch_flag = true;
			return;
		}
	}
}

void capture_thread(void) {
	static vector<int> param = { IMWRITE_JPEG_QUALITY, 75 };
	static vector<uchar> encode_buf(BUF_LEN);
	int frame_seq_num = 1;
	string frame_seq;

	while (!exit_flag) {
		// 동영상 프레임 읽어오기
		mat_cap = fetch_queue.front();
		fetch_queue.pop_front();

		if (mat_cap.empty()) {
			cerr << "빈 영상이 캡쳐되었습니다.\n";
			return;
		}

		// 동영상 프레임 크기 조정
		resize(mat_cap, mat_cap, Size(cap_width, cap_height));

		// 캡처 큐에 삽입
		cap_queue.push_back(mat_cap.clone());

		// jpg 인코딩
		imencode(".jpg", mat_cap, encode_buf, param);

		// 서버로 전송
		frame_seq = to_string(frame_seq_num);
		zmq_send(sock_push_server, frame_seq.c_str(), frame_seq.length(), ZMQ_SNDMORE);
		zmq_send(sock_push_server, &encode_buf[0], encode_buf.size(), 0);
		frame_seq_num++;
	}
}

#define MAX_SEQ_NUM 20
void recv_thread(void) {
	static vector<uchar> decode_buf(BUF_LEN);
	unsigned char seq_buf[MAX_SEQ_NUM] = { 0 };
	int recv_seq_len;
	int recv_msg_len;
	int frame_seq_num = 1;

	while (!exit_flag) {

		recv_seq_len = zmq_recv(sock_pull_server, seq_buf, MAX_SEQ_NUM, ZMQ_NOBLOCK);

		if (recv_seq_len > 0) {
			frame_seq_num = atoi((const char *)seq_buf);
			recv_msg_len = zmq_recv(sock_pull_server, &decode_buf[0], BUF_LEN, ZMQ_NOBLOCK);
			// 디코딩
			mat_recv = imdecode(decode_buf, IMREAD_COLOR);

			// 정상 프레임 인 경우
			if (mat_recv.rows > 0) {

				// 동영상 프레임 크기 조정
				resize(mat_recv, mat_recv, Size(cap_width, cap_height));

				// 수신 큐(우선순위 큐) 에 삽입
				// Mat 동적 할당함.
				pair<long, void *> p = make_pair(frame_seq_num, new Mat(mat_recv));
				recv_queue.push(p);
			}
		}
	}
}

#define DONT_SHOW 0
#define DONT_SHOW_THRESH 1
#define SHOW_START 1
#define SHOW_START_THRESH 2

int volatile show_state = DONT_SHOW;
int volatile show_frame = 1;
void input_show_thread(void) {
	cvNamedWindow("INPUT");
	moveWindow("INPUT", 0, 0);
	cv::imshow("INPUT", mat_show_input);
	setMouseCallback("INPUT", onMouse, 0);

	while (!exit_flag) {

		switch (show_state) {
		case DONT_SHOW:
			break;
		case SHOW_START:
			if (cap_queue.size() >= DONT_SHOW_THRESH) {
				mat_show_input = cap_queue.front();
				cap_queue.pop_front();
			}
			break;
		}

		// INPUT (CAM VIDEO) 영상을 화면에 출력
		putText(mat_show_input, "INPUT", Point(10, 25),
			FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 0, 255), 2);

		cv::imshow("INPUT", mat_show_input);

		// ESC 키를 입력하면 루프가 종료됩니다. 
		if (waitKey(1) >= 0)
			exit_flag = true;

		// 잠깐 대기
		std::chrono::duration<double, std::milli> timespan(delay);
		std::this_thread::sleep_for(timespan);
	}
}

void output_show_thread(void) {
	cvNamedWindow("OUTPUT");
	moveWindow("OUTPUT", 730, 0);
	cv::imshow("OUTPUT", mat_show_output);
	setMouseCallback("OUTPUT", onMouse, 0);

	while (!exit_flag) {

		switch (show_state) {
		case DONT_SHOW:
			if (recv_queue.size() >= SHOW_START_THRESH) {
				show_state = SHOW_START;
			}
			break;
		case SHOW_START:
			if (recv_queue.size() >= DONT_SHOW_THRESH) {

				pair<long, void *> p;
				// pop 성공
				if (recv_queue.try_pop(p)) {
					// 순서에 맞는 프레임인 경우 꺼내서 출력
					if (p.first == show_frame) {
						mat_show_output = ((Mat *)p.second)->clone();
						delete p.second;
						show_frame++;
					}
					// 아닌 경우 다시 삽입
					else {
						show_state = DONT_SHOW;
						recv_queue.push(p);
					}
				}
			}
			else {
				show_state = DONT_SHOW;
			}
			break;
		}
		// OUTPUT 영상을 화면에 출력
		putText(mat_show_output, "OUTPUT", Point(10, 25),
			FONT_HERSHEY_SIMPLEX, 0.5, Scalar(0, 255, 0), 2);

		cv::imshow("OUTPUT", mat_show_output);

		// ESC 키를 입력하면 루프가 종료됩니다. 
		if (waitKey(1) >= 0)
			exit_flag = true;

		// 잠깐 대기
		std::chrono::duration<double, std::milli> timespan(delay);
		std::this_thread::sleep_for(timespan);

	}
}

static void onMouse(int event, int x, int y, int, void*)
{
	if (event == EVENT_LBUTTONDOWN) {
		cout << "onMouse" << endl;
		show_state = DONT_SHOW;
	}

	return;
}