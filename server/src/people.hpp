#ifndef __PEOPLE
#define __PEOPLE
#include <opencv2/opencv.hpp>
#include <iostream>
#include <string>

template<typename T>
inline int intRoundUp(const T a)
{
      return int(a+0.5f);
}

class People
{
  public:
  const float thresh = 0.05;
  std::vector<float> keypoints;
  std::vector<int> keyshape;
  float scale;

  People() {}

  People(std::vector<float> _keypoints, std::vector<int> _keyshape, float _scale) :
    keypoints(_keypoints), keyshape(_keyshape), scale(_scale) {}

  inline int get_person_num(void) const { return keyshape[0]; };
  std::string get_output(void);
	void render_pose_keypoints(cv::Mat& frame);
};

#endif
