#include "yolo_detector.hpp"

YoloDetector::YoloDetector(const char* cfg_path, const char* weight_path, const char* names_path, int gpu_id) : Detector(cfg_path, weight_path,
gpu_id) 
{
  obj_names = objects_names_from_file(names_path);
}

YoloDetector::~YoloDetector()
{

}

void YoloDetector::detect(cv::Mat mat, float thresh = 0.2)
{
  // mat -> image_t
  std::shared_ptr<image_t> img = mat_to_image(mat);

  // detect
  det_vec.clear();
  det_vec = Detector::detect(*img, thresh);
}

std::vector<std::string> YoloDetector::objects_names_from_file(std::string const filename) {
  std::ifstream file(filename);
  std::vector<std::string> file_lines;
  if (!file.is_open()) return file_lines;
  for(std::string line; file >> line;) file_lines.push_back(line);
  std::cout << "object names loaded \n";
  return file_lines;
}


void YoloDetector::draw(cv::Mat mat)
{
  int const colors[6][3] = { { 1,0,1 },{ 0,0,1 },{ 0,1,1 },{ 0,1,0 },{ 1,1,0 },{ 1,0,0 } };

  for (auto &i : det_vec) {
    cv::Scalar color = obj_id_to_color(i.obj_id);
    cv::rectangle(mat, cv::Rect(i.x, i.y, i.w, i.h), color, 3);
    if (obj_names.size() > i.obj_id) {
      std::string obj_name = obj_names[i.obj_id];
      cv::Size const text_size = getTextSize(obj_name, cv::FONT_HERSHEY_DUPLEX, 1.0, 2, 0);
      int max_width = (text_size.width > i.w + 2) ? text_size.width : (i.w + 2);
      max_width = std::max(max_width, (int)i.w + 2);
      max_width = text_size.width + 2;

      cv::rectangle(mat, 
          cv::Point2f(std::max((int)i.x - 1, 0), std::max((int)i.y - 30, 0)),
          cv::Point2f(std::min((int)i.x + max_width, mat.cols - 1), std::min((int)i.y, mat.rows - 1)), 
          color, CV_FILLED, 8, 0);
      putText(mat, obj_name, cv::Point2f(i.x + 10, i.y - 10), cv::FONT_HERSHEY_DUPLEX, 0.8, cv::Scalar(0, 0, 0), 2);
    }
  }
}

std::string YoloDetector::det_to_json(int frame_id)
{
  int cnt = det_vec.size() - 1;
  std::string out_str;
  char *tmp_buf = (char *)calloc(1024, sizeof(char));
  sprintf(tmp_buf, "{\n \"frame_id\":%d, \n \"objects\": [ \n", frame_id);
  out_str = tmp_buf;
  free(tmp_buf);

  for (auto & i : det_vec) {
    char *buf = (char *)calloc(2048, sizeof(char));

    sprintf(buf, "  {\"class_id\":%d, \"name\":\"%s\", \"absolute_coordinates\":{\"center_x\":%d, \"center_y\":%d, \"width\":%d, \"height\":%d}, \"confidence\":%f", i.obj_id, obj_names[i.obj_id].c_str(), i.x, i.y, i.w, i.h, i.prob);

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

