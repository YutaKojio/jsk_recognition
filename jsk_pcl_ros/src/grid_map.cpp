// -*- mode: C++ -*-
/*********************************************************************
 * Software License Agreement (BSD License)
 *
 *  Copyright (c) 2014, JSK Lab
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/o2r other materials provided
 *     with the distribution.
 *   * Neither the name of the Willow Garage nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *********************************************************************/

#include "jsk_pcl_ros/grid_map.h"
#include <boost/make_shared.hpp>
#include <Eigen/Core>
#include "jsk_pcl_ros/geo_util.h"
#include <eigen_conversions/eigen_msg.h>

//#define DEBUG_GRID_MAP

namespace jsk_pcl_ros
{
  GridMap::GridMap(double resolution, const std::vector<float>& coefficients):
    resolution_(resolution), vote_(0)
  {
    normal_[0] = coefficients[0];
    normal_[1] = coefficients[1];
    normal_[2] = coefficients[2];
    d_ = coefficients[3];
    if (normal_.norm() != 1.0) {
      d_ = d_ / normal_.norm();
      normal_.normalize();
    }
    O_ = - d_ * normal_;
    // decide ex_ and ey_
    Eigen::Vector3f u(1, 0, 0);
    if (normal_ == u) {
      u[0] = 0; u[1] = 1; u[2] = 0;
    }
    ey_ = normal_.cross(u).normalized();
    ex_ = ey_.cross(normal_).normalized();
  }
  
  GridMap::~GridMap()
  {
    
  }

  void GridMap::registerIndex(const int x, const int y)
  {
    ColumnIterator it = data_.find(x);
    if (it != data_.end()) {
      (it->second).insert(y);
    }
    else {
      RowIndices new_row;
      new_row.insert(y);
      data_[x] = new_row;
    }
  }
  
  void GridMap::registerIndex(const GridIndex::Ptr& index)
  {
    registerIndex(index->x, index->y);
  }
  
  void GridMap::registerPoint(const pcl::PointXYZRGB& point)
  {
    GridIndex::Ptr index (new GridIndex());
    pointToIndex(point, index);
    // check duplication
    registerIndex(index);
  }
  
  void GridMap::registerLine(const pcl::PointXYZRGB& from,
                             const pcl::PointXYZRGB& to)
  {
#ifdef DEBUG_GRID_MAP
    std::cout << "newline" << std::endl;
#endif
    //GridLine::Ptr new_line (new GridLine(from, to));
    //lines_.push_back(new_line);
    // count up all the grids which the line penetrates
    
    // 1. convert to y = ax + b style equation.
    // 2. move x from the start index to the end index and count up the y range
    // if it cannot be convert to y = ax + b style, it means the equation
    // is represented as x = c style.
    double from_x = from.getVector3fMap().dot(ex_) / resolution_;
    double from_y = from.getVector3fMap().dot(ey_) / resolution_;
    double to_x = to.getVector3fMap().dot(ex_) / resolution_;
    double to_y = to.getVector3fMap().dot(ey_) / resolution_;
#ifdef DEBUG_GRID_MAP
    std::cout << "registering (" << (int)from_x << ", " << (int)from_y << ")" << std::endl;
    std::cout << "registering (" << (int)to_x << ", " << (int)to_y << ")" << std::endl;
#endif
    registerIndex(from_x, from_y);
    registerIndex(to_x, to_y);
    if (from_x != to_x) {
      double a = (to_y - from_y) / (to_x - from_x);
      double b = - a * from_x + from_y;
#ifdef DEBUG_GRID_MAP
      std::cout << "a: " << a << std::endl;
#endif
      if (a == 0.0) {
#ifdef DEBUG_GRID_MAP
        std::cout << "parallel to x" << std::endl;
#endif
        int from_int_x = (int)from_x;
        int to_int_x = (int)to_x;
        int int_y = (int)from_y;
        if (from_int_x > to_int_x) {
          std::swap(from_int_x, to_int_x);
        }
        for (int ix = from_int_x; ix < to_int_x; ++ix) {
          registerIndex(ix, int_y);
#ifdef DEBUG_GRID_MAP
          std::cout << "registering (" << ix << ", " << int_y << ")" << std::endl;
#endif
        }
      }
      else if (fabs(a) < 1.0) {
#ifdef DEBUG_GRID_MAP
        std::cout << "based on x" << std::endl;
#endif
        int from_int_x = (int)from_x;
        int to_int_x = (int)to_x;
        if (from_int_x > to_int_x) {
          std::swap(from_int_x, to_int_x);
        }
        
        for (int ix = from_int_x; ix < to_int_x; ++ix) {
          double y = a * ix + b;
          registerIndex(ix, (int)y);
#ifdef DEBUG_GRID_MAP
          std::cout << "registering (" << ix << ", " << (int)y << ")" << std::endl;
#endif
        }
      }
      else {
#ifdef DEBUG_GRID_MAP
        std::cout << "based on y" << std::endl;
#endif
        int from_int_y = (int)from_y;
        int to_int_y = (int)to_y;
        if (from_int_y > to_int_y) {
          std::swap(from_int_y, to_int_y);
        }
        
        for (int iy = from_int_y; iy < to_int_y; ++iy) {
          double x = iy / a - b / a;
          registerIndex((int)x, iy);
#ifdef DEBUG_GRID_MAP
          std::cout << "registering (" << (int)x << ", " << iy << ")" << std::endl;
#endif
        }
      }
      
    }
    else {
#ifdef DEBUG_GRID_MAP
      std::cout << "parallel to y" << std::endl;
#endif
      // the line is parallel to y
      int from_int_y = (int)from_y;
      int to_int_y = (int)to_y;
      int int_x = (int)from_x;
      if (from_int_y > to_int_y) {
        std::swap(from_int_y, to_int_y);
      }
      for (int iy = from_int_y; iy < to_int_y; ++iy) {
        registerIndex(int_x, iy);
#ifdef DEBUG_GRID_MAP
        std::cout << "registering (" << int_x << ", " << (int)iy << ")" << std::endl;
#endif
      }
    }
  }

  void GridMap::registerPointCloud(pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud)
  {
    for (size_t i = 0; i < cloud->points.size(); i++) {
      registerPoint(cloud->points[i]);
    }
  }
  
  void GridMap::pointToIndex(const pcl::PointXYZRGB& point, GridIndex::Ptr index)
  {
    pointToIndex(point.getVector3fMap(), index);
  }

  void GridMap::pointToIndex(const Eigen::Vector3f& p, GridIndex::Ptr index)
  {
    index->x = (p - O_).dot(ex_) / resolution_;
    index->y = (p - O_).dot(ey_) / resolution_;
  }

  void GridMap::gridToPoint(GridIndex::Ptr index, Eigen::Vector3f& pos)
  {
    gridToPoint(*index, pos);
  }

  void GridMap::gridToPoint(const GridIndex& index, Eigen::Vector3f& pos)
  {
    //pos = resolution_ * (index.x * ex_ + index.y * ey_) + O_;
    pos = resolution_ * ((index.x + 0.5) * ex_ + (index.y + 0.5) * ey_) + O_;
  }

  void GridMap::gridToPoint2(const GridIndex& index, Eigen::Vector3f& pos)
  {
    //pos = resolution_ * ((index.x - 0.5) * ex_ + (index.y - 0.5) * ey_) + O_;
    pos = resolution_ * ((index.x - 0.0) * ex_ + (index.y - 0.0) * ey_) + O_;
  }

  
  bool GridMap::getValue(const int x, const int y)
  {
    // check line
    for (size_t i = 0; i < lines_.size(); i++) {
      GridLine::Ptr line = lines_[i];
      Eigen::Vector3f A, B, C, D;
      gridToPoint2(GridIndex(x, y), A);
      gridToPoint2(GridIndex(x + 1, y), B);
      gridToPoint2(GridIndex(x + 1, y + 1), C);
      gridToPoint2(GridIndex(x, y + 1), D);
      bool penetrate = line->penetrateGrid(A, B, C, D);
      if (penetrate) {
      //   // printf("(%lf, %lf, %lf) - (%lf, %lf, %lf) penetrate (%d, %d)\n",
      //   //        line->from[0],line->from[1],line->from[2],
      //   //        line->to[0],line->to[1],line->to[2],
      //   //        x, y);
      //   //std::cout << "penetrate"
        return true;
      }
    }

    ColumnIterator it = data_.find(x);
    if (it == data_.end()) {
      return false;
    }
    else {
      RowIndices c = it->second;
      if (c.find(y) == c.end()) {
        return false;
      }
      else {
        return true;
      }
    }
  }
  
  bool GridMap::getValue(const GridIndex& index)
  {
    return getValue(index.x, index.y);
  }
  
  bool GridMap::getValue(const GridIndex::Ptr& index)
  {
    return getValue(*index);
  }

  void GridMap::fillRegion(const GridIndex::Ptr start, std::vector<GridIndex::Ptr>& output)
  {
#ifdef DEBUG_GRID_MAP
    std::cout << "filling " << start->x << ", " << start->y << std::endl;
#endif
    output.push_back(start);
    registerIndex(start);
#ifdef DEBUG_GRID_MAP
    if (abs(start->x) > 100 || abs(start->y) > 100) {
      //exit(1);
      std::cout << "force to quit" << std::endl;
      for (size_t i = 0; i < lines_.size(); i++) {
        GridLine::Ptr line = lines_[i];
        Eigen::Vector3f from = line->from;
        Eigen::Vector3f to = line->to;
        std::cout << "line[" << i << "]: "
                  << "[" << from[0] << ", " << from[1] << ", " << from[2] << "] -- "
                  << "[" << to[0] << ", " << to[1] << ", " << to[2] << std::endl;
      }
      return;                   // force to quit
    }
#endif
    GridIndex U(start->x, start->y + 1),
              D(start->x, start->y - 1),
              R(start->x + 1, start->y),
              L(start->x - 1, start->y);
    
    if (!getValue(U)) {
      fillRegion(boost::make_shared<GridIndex>(U), output);
    }
    if (!getValue(L)) {
      fillRegion(boost::make_shared<GridIndex>(L), output);
    }
    if (!getValue(R)) {
      fillRegion(boost::make_shared<GridIndex>(R), output);
    }
    if (!getValue(D)) {
      fillRegion(boost::make_shared<GridIndex>(D), output);
    }
    
  }
  
  void GridMap::fillRegion(const Eigen::Vector3f& start, std::vector<GridIndex::Ptr>& output)
  {
    GridIndex::Ptr start_index (new GridIndex);
    pointToIndex(start, start_index);
    fillRegion(start_index, output);
  }
  
  void GridMap::indicesToPointCloud(const std::vector<GridIndex::Ptr>& indices,
                                    pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud)
  {
    for (size_t i = 0; i < indices.size(); i++) {
      GridIndex::Ptr index = indices[i];
      Eigen::Vector3f point;
      pcl::PointXYZRGB new_point;
      gridToPoint(index, point);
      new_point.x = point[0];
      new_point.y = point[1];
      new_point.z = point[2];
      cloud->points.push_back(new_point);
    }
  }

  void GridMap::originPose(Eigen::Affine3d& output)
  {
    Eigen::Matrix3d rot_mat;
    rot_mat.col(0) = Eigen::Vector3d(ex_[0], ex_[1], ex_[2]);
    rot_mat.col(1) = Eigen::Vector3d(ey_[0], ey_[1], ey_[2]);
    rot_mat.col(2) = Eigen::Vector3d(normal_[0], normal_[1], normal_[2]);
    output = Eigen::Translation3d(Eigen::Vector3d(O_[0], O_[1], O_[2]))
      * Eigen::Quaterniond(rot_mat);
  }
  
  void GridMap::toMsg(SparseOccupancyGrid& grid)
  {
    grid.resolution = resolution_;
    // compute origin POSE from O and normal_, d_
    Eigen::Affine3d plane_pose;
    originPose(plane_pose);
    tf::poseEigenToMsg(plane_pose, grid.origin_pose);
    for (ColumnIterator it = data_.begin();
         it != data_.end();
         it++) {
      int column_index = it->first;
      RowIndices row_indices = it->second;
      SparseOccupancyGridColumn ros_column;
      ros_column.column_index = column_index;
      for (RowIterator rit = row_indices.begin();
           rit != row_indices.end();
           rit++) {
        SparseOccupancyGridCell cell;
        cell.row_index = *rit;
        cell.value = 1.0;
        ros_column.cells.push_back(cell);
      }
      grid.columns.push_back(ros_column);
    }
  }

  Plane GridMap::toPlane()
  {
    return Plane(Eigen::Vector3d(normal_[0], normal_[1], normal_[2]), d_);
  }

  std::vector<float> GridMap::getCoefficients()
  {
    std::vector<float> output;
    output.push_back(normal_[0]);
    output.push_back(normal_[1]);
    output.push_back(normal_[2]);
    output.push_back(d_);
    return output;
  }

  void GridMap::vote()
  {
    ++vote_;
  }

  unsigned int GridMap::getVoteNum()
  {
    return vote_;
  }

  void GridMap::setGeneration(unsigned int generation) {
    generation_ = generation;
  }

  unsigned int GridMap::getGeneration()
  {
    return generation_;
  }
  
}
