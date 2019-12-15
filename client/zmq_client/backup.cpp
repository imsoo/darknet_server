#define FD_SETSIZE 4096;
#include <zmq.h>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <thread>

using namespace cv;
using namespace std;

template <typename T>
class SharedQueue
{
public:
	SharedQueue();
	~SharedQueue();

	T& front();
	void pop_front();

	void push_back(const T& item);
	void push_back(T&& item);

	int size();
	bool empty();

private:
	std::deque<T> queue_;
	std::mutex mutex_;
	std::condition_variable cond_;
};

template <typename T>
SharedQueue<T>::SharedQueue() {}

template <typename T>
SharedQueue<T>::~SharedQueue() {}

template <typename T>
T& SharedQueue<T>::front()
{
	std::unique_lock<std::mutex> mlock(mutex_);
	while (queue_.empty())
	{
		cond_.wait(mlock);
	}
	return queue_.front();
}

template <typename T>
void SharedQueue<T>::pop_front()
{
	std::unique_lock<std::mutex> mlock(mutex_);
	while (queue_.empty())
	{
		cond_.wait(mlock);
	}
	queue_.pop_front();
}

template <typename T>
void SharedQueue<T>::push_back(const T& item)
{
	std::unique_lock<std::mutex> mlock(mutex_);
	queue_.push_back(item);
	mlock.unlock();     // unlock before notificiation to minimize mutex con
	cond_.notify_one(); // notify one waiting thread

}

template <typename T>
void SharedQueue<T>::push_back(T&& item)
{
	std::unique_lock<std::mutex> mlock(mutex_);
	queue_.push_back(std::move(item));
	mlock.unlock();     // unlock before notificiation to minimize mutex con
	cond_.notify_one(); // notify one waiting thread

}

template <typename T>
int SharedQueue<T>::size()
{
	std::unique_lock<std::mutex> mlock(mutex_);
	int size = queue_.size();
	mlock.unlock();
	return size;
}

void capture_thread(void);
void recv_thread(void);
void output_show_thread(void);
void input_show_thread(void);
static void onMouse(int event, int x, int y, int, void*);


void *context;
void *requester;
volatile int requester_idx = 0;
zmq_pollitem_t items[1];
const int REQUESTER_NUM = 1;
SharedQueue<Mat> recv_queue;
SharedQueue<Mat> cap_queue;
VideoCapture cap;
volatile bool exit_flag = false;

static Mat mat_show_output;
static Mat mat_show_input;
static Mat mat_recv;
static Mat mat_cap;

const int cap_width = 640;
const int cap_height = 480;

int main()
{

	//  Socket to talk to clients
	context = zmq_ctx_new();
	requester = zmq_socket(context, ZMQ_DEALER);
	zmq_setsockopt(requester, ZMQ_IDENTITY, "ims", 3);
	zmq_connect(requester, "tcp://104.199.171.37:5570");

	items[0] = { static_cast<void*>(requester), 0, ZMQ_POLLIN, 0 };
	long long count = 0;
	//비디오 캡쳐 초기화
	cap = VideoCapture("C:\\Users\\COMSE\\source\\repos\\zmq_test\\x64\\Release\\road.mp4");
	cap.set(CV_CAP_PROP_BUFFERSIZE, 3);
	cap = VideoCapture(0);

	if (!cap.isOpened()) {
		cerr << "Erro VideoCapture.\n";
		return -1;
	}

	// 동영상 프레임 읽어오기
	cap.read(mat_cap);

	if (mat_cap.empty()) {
		cerr << "빈 영상이 캡쳐되었습니다.\n";
		return 0;
	}

	mat_show_output = mat_cap.clone();
	mat_show_input = mat_cap.clone();

	// 잠깐 대기
	std::chrono::duration<int, std::milli> timespan(1000);
	std::this_thread::sleep_for(timespan);

	thread thread_capture(capture_thread);
	thread thread_recv(recv_thread);
	thread thread_show_input(output_show_thread);
	thread thread_show_output(input_show_thread);

	thread_capture.detach();
	thread_recv.detach();
	thread_show_input.detach();
	thread_show_output.detach();

	while (!exit_flag)
	{
		cout << recv_queue.size() << endl;
	}

	zmq_close(requester);
	zmq_ctx_destroy(context);

	return 0;
}

#define BUF_LEN 256000

void capture_thread(void) {
	static vector<int> param = { IMWRITE_JPEG_QUALITY, 30 };
	static vector<uchar> encode_buf(BUF_LEN);

	while (!exit_flag) {
		// 동영상 프레임 읽어오기
		cap.read(mat_cap);

		if (mat_cap.empty()) {
			cerr << "빈 영상이 캡쳐되었습니다.\n";
			return;
		}

		// 동영상 프레임 크기 조정
		resize(mat_cap, mat_cap, Size(cap_width, cap_height));

		// 캡처 큐에 삽입
		cap_queue.push_back(mat_cap);

		// jpg 인코딩
		imencode(".jpg", mat_cap, encode_buf, param);

		// 서버로 전송
		//zmq_send(requester, "ims", 3, ZMQ_SNDMORE);
		zmq_send(requester, "", 0, ZMQ_SNDMORE);
		zmq_send(requester, &encode_buf[0], encode_buf.size(), ZMQ_NOBLOCK);
	}
}

void recv_thread(void) {
	static vector<uchar> decode_buf(BUF_LEN);
	while (!exit_flag) {
		zmq_poll(&items[0], 1, -1);
		// 데이터 수신
		if (items[0].revents & ZMQ_POLLIN) {
			zmq_recv(requester, &decode_buf[0], BUF_LEN, ZMQ_NOBLOCK);

			// 디코딩
			mat_recv = imdecode(decode_buf, IMREAD_COLOR);

			// 정상 프레임 인 경우
			if (mat_recv.rows > 0) {

				// 동영상 프레임 크기 조정
				resize(mat_recv, mat_recv, Size(cap_width, cap_height));

				// 수신 큐에 삽입
				recv_queue.push_back(mat_recv);
			}
		}
	}
}

#define DONT_SHOW 0
#define DONT_SHOW_THRESH 1
#define SHOW_START 1
#define SHOW_START_THRESH 10
int volatile show_state = DONT_SHOW;
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
		if (waitKey(10) >= 0)
			exit_flag = true;
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
				mat_show_output = recv_queue.front();
				recv_queue.pop_front();
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
		if (waitKey(10) >= 0)
			exit_flag = true;
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