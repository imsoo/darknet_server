#ifndef __YOLO_DETECTOR_H
#define __YOLO_DETECTOR_H

#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
#include "yolo_v2_class.hpp"
#include "DetectorInterface.hpp"

class YoloDetector : public Detector, public DetectorInterface
{
private:
  std::vector<bbox_t> det_vec;
  std::vector<std::string> obj_names;
public:
  YoloDetector(const char* cfg_path, const char* weight_path, const char* names_path, int gpu_id);
  ~YoloDetector();
  virtual void detect(cv::Mat mat, float thresh);
  virtual void draw(cv::Mat mat);
  std::vector<std::string> objects_names_from_file(std::string const filename);
  virtual std::string det_to_json(int frame_id);
};

#endif
