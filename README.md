## Darknet server

This is for the use of the **Darknet (Open source neural networks)** in cloud computing. using this project, You can send video or Webcam stream to server, and get result from Server in real time.

* #### Examples of results
| YOLO (Object Detection) | OpenPose (Pose Estimation) |
|:---:|:---:|
| <img src="https://user-images.githubusercontent.com/11255376/70911582-8c842180-2055-11ea-9fa1-897e6fae626b.gif" width="100%" height="35%"> | <img src="https://user-images.githubusercontent.com/11255376/71064948-91f77e00-21b3-11ea-98b8-c397bba0e406.gif" width="100%" height="35%"> |


### Overview
In this project, Server and client communicate based on **ZeroMQ** message library. Client read frame from video or Webcam using by **OpenCV** and send to server by json message format. Server receive message and do work something. (Object detection or Pose Estimation) and send result (processed frame and detection result) back to client by json message format.

| Client - Server | Server Parallel Pipeline |
|:---:|:---:|
| <img src="https://user-images.githubusercontent.com/11255376/71083332-1c50d980-21d6-11ea-932c-9ad691cce55a.png" width="100%" height="40%"> | <img src="https://user-images.githubusercontent.com/11255376/68307966-6a5ec180-00ef-11ea-92a5-f2328b46e9cd.png" width="100%" height="30%"> |

### Requirements 

* Windows or Linux
* **OpenCV** : https://opencv.org/releases/
* **ZeroMQ (libzmq)** : https://github.com/zeromq/libzmq
* **json-c** : https://github.com/json-c/json-c
* on Linux **g++**, on Windows **MSVC 2015/2017/2019** https://visualstudio.microsoft.com/thank-you-downloading-visual-studio/?sku=Community


### Pre-trained models

* `yolov3.cfg` (236 MB COCO **Yolo v3**) - requires 4 GB GPU-RAM: https://pjreddie.com/media/files/yolov3.weights
* `yolov3-tiny.cfg` (34 MB COCO **Yolo v3 tiny**) - requires 1 GB GPU-RAM:  https://pjreddie.com/media/files/yolov3-tiny.weights
* `openpose.cfg` (200 MB **OpenPose**) - requires 4 GB GPU-RAM: https://github.com/lincolnhard/openpose-darknet
* `fight.cfg` (235 MB **Yolo v3 custom train**) - requires 4 GB GPU-RAM: https://drive.google.com/open?id=1wqLMNwWGdkxPiFpeXJSLfnKZp8ZD99PS

* #### Examples of results (fight)

|<img src="https://user-images.githubusercontent.com/11255376/71348509-06655f00-25b0-11ea-9351-2be699084517.gif" width="150%" height="30%">|<img src="https://user-images.githubusercontent.com/11255376/71348563-272db480-25b0-11ea-8530-67fff677f551.gif" width="150%" height="30%">|
|:---:|:---:|

### How to Build 
* #### Server (Linux)
``` sh
## get darknet
git clone https://github.com/AlexeyAB/darknet
cd darknet

## add some code in yolo_v2_class.cpp
vi src/yolo_v2_class.cpp
## add below Detector::get_net_height() member function definition
LIB_API int Detector::get_net_out_width() const {
    detector_gpu_t &detector_gpu = *static_cast<detector_gpu_t *>(detector_gpu_ptr.get());
    return detector_gpu.net.layers[detector_gpu.net.n - 2].out_w;
}
LIB_API int Detector::get_net_out_height() const {
    detector_gpu_t &detector_gpu = *static_cast<detector_gpu_t *>(detector_gpu_ptr.get());
    return detector_gpu.net.layers[detector_gpu.net.n - 2].out_h;
}
LIB_API float *Detector::predict(float *input) const {
    detector_gpu_t &detector_gpu = *static_cast<detector_gpu_t *>(detector_gpu_ptr.get());
    return network_predict(detector_gpu.net, input);
}

## add some code in yolo_v2_class.hpp
vi include/yolo_v2_class.hpp
## add below Detector::get_net_height() member function declaration
LIB_API int get_net_out_width() const;
LIB_API int get_net_out_height() const;
LIB_API float *predict(float *input) const;


## Build a library darknet.so
vi Makefile   ## set option LIBSO = 1
make          ## build a library darknet.so
sudo cp libdarknet.so /usr/local/lib/

## Build darknet_server
git clone https://github.com/imsoo/darknet_server
cd darknet_server/server
make

## Run Sink & Ventilator 
./sink
./ventilator

## Run worker (if GPU shows low utilization and has enough memory, run more worker)
./worker <CFG_PATH> <WEIGHTS_PATH> <NAMES_PATH> [-pose] [-gpu GPU_ID] [-thresh THRESH] 
```

* #### Client (Linux)
``` sh
## Build darknet_client
git clone https://github.com/imsoo/darknet_server
cd darknet_server/client/darknet_client
make

## Run darknet client
./darknet_client <-addr ADDR> <-vid VIDEO_PATH | -cam CAM_NUM> [-out_vid] [-out_json] [-dont_show]
```


* #### Client (Windows)
**Visual Studio Setting Up and Build**
  * **OpenCV** : https://stackoverflow.com/questions/35537226/setting-up-opencv-3-1-in-visual-studio-2015
  * **ZeroMQ** :https://joshuaburkholder.com/wordpress/2018/05/25/build-and-static-link-zeromq-on-windows/
  * **json-c** : https://github.com/json-c/json-c#building-on-unix-and-windows-with-vcpkg-gccg-curl-unzip-and-tar

### How to use on the command line
*  #### Server (Linux)
``` 
YOLOv3 : ./worker cfg/yolov3.cfg weights/yolov3.weights names/cooc.names -gpu 0 -thresh 0.2
OpenPose : ./worker cfg/openpose.cfg weights/openpose.weights -gpu 0 -pose
```

* #### Client (Windows | Linux)
``` 
Cam : ./darknet_client -addr x.x.x.x -cam 0
Video : ./darknet_client -addr x.x.x.x -vid test.mp4 
Save result video file : ./darknet_client -addr x.x.x.x -vid test.mp4 -out_vid  # save to test_output.mp4
Save result json file : ./darknet_client -addr x.x.x.x -vid test.mp4 -out_json  # save to test_output.json
Save result only (don't show window) : ./darknet_client -addr x.x.x.x -cam 0 -out_vid -out_json -dont_show
```

### JSON Output
* #### YOLO :
``` jsonc
{
  "det": [
    {
      "frame_id": 1,
      "objects": [
        {
          "class_id": 27,
          "name": "giraffe",
          "absolute_coordinates": {
            "center_x": 275,
            "center_y": 194,
            "width": 8,
            "height": 28
          },
          "confidence": 0.20249
        }
      ]
    }
  ]
}
```
* #### OpenPose : 
``` jsonc
{
  "det": [
    {
      "frame_id": 1,
      "people": [
        {
          "0": [351.977112,175.938629],   // NOSE
          "1": [384.007935,207.964188],   // NECK
          "2": [339.164185,208.048325],   // RRShoulder
          "3": [336.007416,275.159973],   // RElbow
          "4": [0,0],                     // RWrist
          "5": [425.677979,204.863556],   // LShoulder
          "6": [454.353577,268.810059],   // LElbow
          "7": [444.86261,326.342651],    // LWrist
          "8": [361.499115,335.980316],   // RHip
          "9": [361.627502,428.830963],   // RKnee
          "10": [0,0],                    // RAnkle
          "11": [416.036926,335.951752],  // LHip
          "12": [419.235413,428.756653],  // LKnee
          "13": [0,0],                    // LAnkle
          "14": [345.621887,166.345505],  // REye
          "15": [364.737091,166.310699],  // LEye
          "16": [0,0],                    // REar
          "17": [387.237091,163.059067]   // LEar
        }
      ]
    }
  ]
}
```

### References
* #### Darknet : https://github.com/AlexeyAB/darknet
* #### OpenCV : https://github.com/opencv/opencv
* #### ZeroMQ : https://github.com/zeromq/libzmq
* #### json-c : https://github.com/json-c/json-c 
* #### openpose-darknet : https://github.com/lincolnhard/openpose-darknet
* #### cpp-base64 : https://github.com/ReneNyffenegger/cpp-base64
* #### mem_pool : https://www.codeproject.com/Articles/27487/Why-to-use-memory-pool-and-how-to-implement-it
* #### share_queue : https://stackoverflow.com/questions/36762248/why-is-stdqueue-not-thread-safe