/******************************************************************************
 * Copyright 2017-2018 Baidu Robotic Vision Authors. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

#ifndef NAVIGATION_INCLUDE_NAVIGATION_NAVIGATION_TYPE_H_
#define NAVIGATION_INCLUDE_NAVIGATION_NAVIGATION_TYPE_H_

#include <glog/logging.h>
#include <Eigen/Core>
#include <algorithm>
#include <string>
#include <vector>
#include <chrono>

namespace Navigation {
struct WayPoint {
  float timestamp_sec;
  Eigen::Vector3f xyz;
  Eigen::Vector3f direction;
  char tag;

  WayPoint() :
    timestamp_sec(-1.f),
    direction(Eigen::Vector3f(0, 0, 0)),
    tag(0x00) {}

  bool operator <(const WayPoint& rhs) const {
    return timestamp_sec < rhs.timestamp_sec;
  }
};

typedef std::vector<WayPoint> VecWayPoint;

enum class NaviStatus {
  NORMAL = 0,
  LOST = 1,
  OBSTACLE_AVOID = 2,
  STOP = 3,
  STANDBY = 4,
  MANUAL = 5,
  LOST_RECOVERY = 6,
};

inline std::string NaviStatusToString(const NaviStatus navi_status) {
  if (navi_status == NaviStatus::NORMAL) {
    return "NORNAL";
  } else if (navi_status == NaviStatus::LOST) {
    return "LOST";
  } else if (navi_status == NaviStatus::LOST_RECOVERY) {
    return "LOST_RECOVERY";
  } else if (navi_status == NaviStatus::OBSTACLE_AVOID) {
    return "OBSTACLE_AVOID";
  } else if (navi_status == NaviStatus::STOP) {
    return "STOP";
  } else if (navi_status == NaviStatus::STANDBY) {
    return "STANDBY";
  } else {
    return "unknown";
  }
}

/**
 * message for lidar
 */
// No auto-initialization.
struct radius_theta{
  float radius;
  float theta;
};

struct x_y{
  float x;
  float y;
};

class ScanMessage {
  // [Note]: This class do not set scan_xy & scan_rt in the same time.
  // This class only does the conversion in the getters.
  // If scan_rt is set by setScanRT(...), the range of theta is determined by the source data.
  // If scan_rt is changed or generated by other member functions, the range of theta is [-pi, pi].
  // The result of sort function is only in theta ascending order.
 public:
  ScanMessage() {}
  explicit ScanMessage(const std::chrono::steady_clock::time_point& input_tp)
    : sorted_by_theta_(false), tp_(input_tp) {}
  explicit ScanMessage(const std::vector<x_y>& input_scan_xy)
    : sorted_by_theta_(false), scan_xy_(input_scan_xy) {
  }
  explicit ScanMessage(const std::vector<radius_theta>& input_scan_rt)
    : sorted_by_theta_(false), scan_rt_(input_scan_rt) {
  }
  ScanMessage(const std::vector<x_y>& input_scan_xy,
              const std::chrono::steady_clock::time_point& input_tp)
    : sorted_by_theta_(false), scan_xy_(input_scan_xy), tp_(input_tp) {}
  ScanMessage(const std::vector<radius_theta>& input_scan_rt,
              const std::chrono::steady_clock::time_point& input_tp)
    : sorted_by_theta_(false), scan_rt_(input_scan_rt), tp_(input_tp) {}

  std::vector<x_y> getScanXY() {
    if (scan_xy_.empty() && !scan_rt_.empty()) {
      scanRTtoXY(scan_rt_, &scan_xy_);
    }
    return scan_xy_;
  }

  void compute_smallest_r_within_delta_theta(std::vector<radius_theta>* scan_rt) {
    // TODO(meng): do not hard code
    const float delta_theta = 0.5 * M_PI / 512;
    std::vector<radius_theta> scan_rt_copy = *scan_rt;
    for (int i = 0; i < scan_rt_copy.size(); i++) {
      radius_theta& rt = (*scan_rt)[i];
      int j = i;
      while (j >= 0) {
        if (fabs(scan_rt_copy[j].theta - rt.theta) > delta_theta) {
          break;
        }
        rt.radius = fmin(rt.radius, scan_rt_copy[j].radius);
        j--;
      }
      j = i;
      while (j < scan_rt_copy.size()) {
        if (fabs(scan_rt_copy[j].theta - rt.theta) > delta_theta) {
          break;
        }
        rt.radius = fmin(rt.radius, scan_rt_copy[j].radius);
        j++;
      }
    }
  }

  std::vector<radius_theta> getScanRT(const bool need_sort = true) {
    if (scan_rt_.empty() && !scan_xy_.empty()) {
      scanXYtoRT(scan_xy_, &scan_rt_);
      sorted_by_theta_ = false;
    }
    if (need_sort && !sorted_by_theta_) {
      thetaAscendingSortScanRT(&scan_rt_);
      sorted_by_theta_ = true;
    }
    compute_smallest_r_within_delta_theta(&scan_rt_);
    return scan_rt_;
  }
  std::chrono::steady_clock::time_point getTimePoint() const {
    return tp_;
  }
  void setScanXY(const std::vector<x_y>& input_scan_xy) {
    scan_xy_ = input_scan_xy;
    if (!scan_rt_.empty()) {
      scan_rt_.clear();
    }
  }
  void setScanRT(const std::vector<radius_theta>& input_scan_rt, const bool is_sorted = false) {
    scan_rt_ = input_scan_rt;
    if (!scan_xy_.empty()) {
      scan_xy_.clear();
    }
    sorted_by_theta_ = is_sorted;
  }
  void setTimePoint(const std::chrono::steady_clock::time_point& input_tp) {
    tp_ = input_tp;
  }
  size_t scan_size() {
    return scan_xy_.empty() ? scan_rt_.size() : scan_xy_.size();
  }
  void scan_clear() {
    scan_xy_.clear();
    scan_rt_.clear();
    sorted_by_theta_ = false;
  }
  void scan_reserve(const size_t size) {
    scan_xy_.reserve(size);
    scan_rt_.reserve(size);
  }
  void scan_xy_push_back(const x_y &xy) {
    // this function will cause scan_rt_ empty
    scan_xy_.push_back(xy);
    if (!scan_rt_.empty()) {
      scan_rt_.clear();
    }
    sorted_by_theta_ = false;
  }
  void scan_rt_push_back(const radius_theta &rt, const bool in_order = false) {
    // this funtion will cause scan_xy_ empty
    scan_rt_.push_back(rt);
    if (!scan_xy_.empty()) {
      scan_xy_.clear();
    }
    sorted_by_theta_ = in_order;
  }

  void scanXYtoRT(const std::vector<x_y>& input_scan_xy,
                  std::vector<radius_theta>* output_scan_rt) const {
    output_scan_rt->clear();
    if (input_scan_xy.empty()) {
      return;
    }
    output_scan_rt->reserve(input_scan_xy.size());
    for (auto xy : input_scan_xy) {
      radius_theta rt;
      XYtoRT(xy, &rt);
      output_scan_rt->push_back(rt);
    }
  }
  void scanRTtoXY(const std::vector<radius_theta>& input_scan_rt,
                  std::vector<x_y>* output_scan_xy) const {
    output_scan_xy->clear();
    if (input_scan_rt.empty()) {
      return;
    }
    output_scan_xy->reserve(input_scan_rt.size());
    for (auto rt : input_scan_rt) {
      x_y xy;
      RTtoXY(rt, &xy);
      output_scan_xy->push_back(xy);
    }
  }
  void XYtoRT(const x_y& xy, radius_theta* rt) const {
    rt->radius = std::hypotf(xy.x, xy.y);
    rt->theta = atan2f(xy.y, xy.x);  // [-pi, pi]
  }
  void RTtoXY(const radius_theta& rt, x_y* xy) const {
    xy->x = rt.radius * cosf(rt.theta);
    xy->y = rt.radius * sinf(rt.theta);
  }

  void thetaAscendingSortScanRT(std::vector<radius_theta> *scan_rt) const {
    std::sort(scan_rt->begin(), scan_rt->end(),
              [](radius_theta first,  radius_theta second) { return first.theta < second.theta; });
  }

 protected:
  std::chrono::steady_clock::time_point tp_;
  std::vector<x_y> scan_xy_;
  std::vector<radius_theta> scan_rt_;
  bool sorted_by_theta_;  // Only in ascending order.
};

}  // namespace Navigation

#endif  // NAVIGATION_INCLUDE_NAVIGATION_NAVIGATION_TYPE_H_
