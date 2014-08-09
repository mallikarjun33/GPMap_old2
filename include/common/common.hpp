#ifndef _COMMON_HPP_
#define _COMMON_HPP_

// STL
#include <algorithm>		// min, max

// PCL
#include <pcl/point_types.h>		// pcl::PointXYZ, pcl::Normal, pcl::PointNormal
#include <pcl/point_cloud.h>		// pcl::PointCloud
#include <pcl/common/common.h>	// pcl::getMinMax3D

// GPMap
#include "util/util.hpp"			// minPointXYZ, maxPointXYZ
namespace GPMap {

template <typename PointT>
void getMinMaxPointXYZ(const pcl::PointCloud<PointT>	&pointCloud,
							  pcl::PointXYZ						&min_pt, 
							  pcl::PointXYZ						&max_pt)
{
	// min/max points with the same point type
	PointT min_pt_temp, max_pt_temp;
	
	// get min max
	pcl::getMinMax3D(pointCloud, min_pt_temp, max_pt_temp);

	// set
	min_pt.x = min_pt_temp.x;
	min_pt.y = min_pt_temp.y;
	min_pt.z = min_pt_temp.z;

	max_pt.x = max_pt_temp.x;
	max_pt.y = max_pt_temp.y;
	max_pt.z = max_pt_temp.z;
}

template <typename PointT>
void getMinMaxPointXYZ(const std::vector<typename pcl::PointCloud<PointT>::Ptr>		&pPointClouds,
							  pcl::PointXYZ &min_pt, pcl::PointXYZ &max_pt)
{
	pcl::PointXYZ min_pt_temp, max_pt_temp;

	// for each point cloud
	for(size_t i = 0; i < pPointClouds.size(); i++)
	{
		// get min max
		getMinMaxPointXYZ<PointT>(*pPointClouds[i], min_pt_temp, max_pt_temp);

		// compare
		if(i == 0)
		{
			min_pt = min_pt_temp;
			max_pt = max_pt_temp;
		}
		else
		{
			min_pt = minPointXYZ(min_pt, min_pt_temp);
			max_pt = maxPointXYZ(max_pt, max_pt_temp);
		}
	}
}


}

#endif