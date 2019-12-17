#include "people.hpp"

std::string People::get_output(void) {
	std::string out_str = "\"people\": [\n";
	int person_num = keyshape[0];
	int part_num = keyshape[1];
	for (int person = 0; person < person_num; person++) {
		if (person != 0)
			out_str += ",\n";
		out_str += " {\n";
		for (int part = 0; part < part_num; part++) {
			if (part != 0) 
				out_str += ",\n ";
			int index = (person * part_num + part) * keyshape[2];
			char *buf = (char*)calloc(2048, sizeof(char));

			if (keypoints[index + 2] >  thresh) {
				sprintf(buf, " \"%d\":[%f, %f]", part, keypoints[index] * scale, keypoints[index + 1] * scale);
			}
			else {
				sprintf(buf, " \"%d\":[%f, %f]", part, 0.0, 0.0);
			}
      out_str += buf;
			free(buf);
		}
		out_str += "\n }";
	}
  out_str += "\n ]";
	return out_str;
}


void People::render_pose_keypoints(cv::Mat& frame)
{
  const int num_keypoints = keyshape[1];
  unsigned int pairs[] =
  {
    1, 2, 1, 5, 2, 3, 3, 4, 5, 6, 6, 7, 1, 8, 8, 9, 9, 10,
    1, 11, 11, 12, 12, 13, 1, 0, 0, 14, 14, 16, 0, 15, 15, 17
  };
  float colors[] =
  {
    255.f, 0.f, 85.f, 255.f, 0.f, 0.f, 255.f, 85.f, 0.f, 255.f, 170.f, 0.f,
    255.f, 255.f, 0.f, 170.f, 255.f, 0.f, 85.f, 255.f, 0.f, 0.f, 255.f, 0.f,
    0.f, 255.f, 85.f, 0.f, 255.f, 170.f, 0.f, 255.f, 255.f, 0.f, 170.f, 255.f,
    0.f, 85.f, 255.f, 0.f, 0.f, 255.f, 255.f, 0.f, 170.f, 170.f, 0.f, 255.f,
    255.f, 0.f, 255.f, 85.f, 0.f, 255.f
  };
  const int pairs_size = sizeof(pairs) / sizeof(unsigned int);
  const int number_colors = sizeof(colors) / sizeof(float);

  for (int person = 0; person < keyshape[0]; ++person)
  {
    // Draw lines
    for (int pair = 0u; pair < pairs_size; pair += 2)
    {
      const int index1 = (person * num_keypoints + pairs[pair]) * keyshape[2];
      const int index2 = (person * num_keypoints + pairs[pair + 1]) * keyshape[2];
      if (keypoints[index1 + 2] > thresh && keypoints[index2 + 2] > thresh)
      {
        const int color_index = pairs[pair + 1] * 3;
        cv::Scalar color { colors[(color_index + 2) % number_colors],
          colors[(color_index + 1) % number_colors],
          colors[(color_index + 0) % number_colors]};
        cv::Point keypoint1{ intRoundUp(keypoints[index1] * scale), intRoundUp(keypoints[index1 + 1] * scale) };
        cv::Point keypoint2{ intRoundUp(keypoints[index2] * scale), intRoundUp(keypoints[index2 + 1] * scale) };
        cv::line(frame, keypoint1, keypoint2, color, 2);
      }
    }
    // Draw circles
    for (int part = 0; part < num_keypoints; ++part)
    {
      const int index = (person * num_keypoints + part) * keyshape[2];
      if (keypoints[index + 2] > thresh)
      {
        const int color_index = part * 3;
        cv::Scalar color { colors[(color_index + 2) % number_colors],
          colors[(color_index + 1) % number_colors],
          colors[(color_index + 0) % number_colors]};
        cv::Point center{ intRoundUp(keypoints[index] * scale), intRoundUp(keypoints[index + 1] * scale) };
        cv::circle(frame, center, 3, color, -1);
      }
    }
  }
}
