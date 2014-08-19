#ifndef _GPMAP_TO_OCTOMAP_HPP_
#define _GPMAP_TO_OCTOMAP_HPP_

// STL
#include <string>
#include <vector>

// PCL
#include <pcl/point_types.h>		// pcl::PointXYZ, pcl::Normal, pcl::PointNormal
#include <pcl/point_cloud.h>		// pcl::PointCloud

// Octomap
#include <octomap/octomap.h>
#include <octomap/octomap_timing.h>
#include <octomap/ColorOcTree.h>

// GPMap
#include "util/data_types.hpp"	// PointXYZCloud
#include "util/timer.hpp"			// boost::timer

namespace GPMap {

//void LeafNode2PointNormal
//
//void GPMap2Octomap
//{
//		octomap::OcTree tree(mapResolution);
//
//}

class Octomap
{
public:
	/** @brief Constructor */
	Octomap(const double resolution,
			  const bool	FLAG_SIMPLE_UPDATE = false)
		:	m_pOctree(new octomap::OcTree(resolution)),
			FLAG_SIMPLE_UPDATE_(FLAG_SIMPLE_UPDATE)
	{
	}

	/** @brief Constructor */
	Octomap(const double				resolution,
			  const std::string		strFileName,
			  const bool				FLAG_SIMPLE_UPDATE = false)
		:	m_pOctree(new octomap::OcTree(resolution)),
			FLAG_SIMPLE_UPDATE_(FLAG_SIMPLE_UPDATE)
	{
		// load octomap
		m_pOctree->readBinary(strFileName);
	}

	/** @brief	Update a node of the octomap
	  * @return	Elapsed time (user/system/wall cpu times)
	  */
	inline void updateNode(const double x, const double y, const double z, const float log_odds_update, bool lazy_eval = false)
	{
		m_pOctree->updateNode(x, y, z, log_odds_update, lazy_eval);
	}

	/** @brief	Update a node of the octomap
	  * @return	Elapsed time (user/system/wall cpu times)
	  */
	inline void updateNode(const double x, const double y, const double z, bool occupied, bool lazy_eval = false)
	{
		m_pOctree->updateNode(x, y, z, occupied, lazy_eval);
	}

	/** @brief	Update the octomap with a point cloud
	  * @return	Elapsed time (user/system/wall cpu times)
	  */
	template <typename PointT1, typename PointT2>
	boost::timer::cpu_times
	update(const typename pcl::PointCloud<PointT1>	&pointCloud,
			 const PointT2										&sensorPosition,
			 const double										maxrange = -1)
	{
		// robot position
		octomap::point3d robotPosition(sensorPosition.x, sensorPosition.y, sensorPosition.z);

		// point cloud
		octomap::Pointcloud pc;
		for(size_t i = 0; i < pointCloud.size(); i++)
			pc.push_back(pointCloud.points[i].x, pointCloud.points[i].y, pointCloud.points[i].z);

		// timer - start
		boost::timer::cpu_timer timer;

		// update
		if (FLAG_SIMPLE_UPDATE_)	m_pOctree->insertPointCloudRays(pc, robotPosition, maxrange);
		else								m_pOctree->insertPointCloud(pc, robotPosition, maxrange);

		// timer - end
		boost::timer::cpu_times elapsed = timer.elapsed();

		// memory
		std::cout << "memory usage: "		<< m_pOctree->memoryUsage()		<< std::endl;
		std::cout << "leaf node count: " << m_pOctree->getNumLeafNodes() << std::endl;
		//if(tree->memoryUsage() > 900000000)
		if(m_pOctree->memoryUsage() > 500000000)
		{
			m_pOctree->toMaxLikelihood();
			m_pOctree->prune();
			std::cout << "after pruned - memory usage: "		<< m_pOctree->memoryUsage()		<< std::endl;
			std::cout << "after pruned - leaf node count: " << m_pOctree->getNumLeafNodes() << std::endl;
		}

		// return the elapsed time
		return elapsed;
	}

	/** @brief	Save the octomap as a binary file */
	bool save(const std::string &strFileNameWithoutExtension)
	{
		// check
		if(!m_pOctree) return false;

		// file name
		std::string strFileName;

		// *.ot
		if(FLAG_SIMPLE_UPDATE_)		strFileName = strFileNameWithoutExtension + "_simple.ot";
		else								strFileName = strFileNameWithoutExtension + ".ot";
		m_pOctree->write(strFileName);

		// *.bt
		if(FLAG_SIMPLE_UPDATE_)		strFileName = strFileNameWithoutExtension + "_simple_ml.bt";
		else								strFileName = strFileNameWithoutExtension + "_ml.bt";
		m_pOctree->toMaxLikelihood();
		m_pOctree->prune();
		m_pOctree->writeBinary(strFileName);

		std::cout << std::endl;
		return true;
	}

	/** @brief	Evaluate the octomap */
	template <typename PointT1, typename PointT2>
	bool evaluate(const std::vector<typename pcl::PointCloud<PointT1>::Ptr>			&pPointCloudPtrList,
					  const std::vector<PointT2, Eigen::aligned_allocator<PointT2> >	&sensorPositionList,
					  unsigned int																&num_points,
					  unsigned int																&num_voxels_correct,
					  unsigned int																&num_voxels_wrong,
					  unsigned int																&num_voxels_unknown,
					  const double																maxrange = -1)
	{
		// check size
		assert(pPointCloudPtrList.size() == sensorPositionList.size());

		// check memory
		if(!m_pOctree) return false;

		// initialization
		num_points = 0;
		num_voxels_correct = 0;
		num_voxels_wrong = 0;
		num_voxels_unknown = 0;

		// for each observation
		for(size_t i = 0; i < pPointCloudPtrList.size(); i++)
		{
			// robot position
			octomap::point3d robotPosition(sensorPositionList[i].x, sensorPositionList[i].y, sensorPositionList[i].z);

			// point cloud
			num_points += pPointCloudPtrList[i]->size();
			octomap::Pointcloud pc;
			for(size_t j = 0; j < pPointCloudPtrList[i]->size(); j++)
				pc.push_back(pPointCloudPtrList[i]->points[j].x, pPointCloudPtrList[i]->points[i].y, pPointCloudPtrList[i]->points[j].z);

			// free/occupied cells
			octomap::KeySet free_cells, occupied_cells;
			m_pOctree->computeUpdate(pc, robotPosition, free_cells, occupied_cells, maxrange);
			
			// count free cells
			for(octomap::KeySet::iterator it = free_cells.begin(); it != free_cells.end(); ++it)
			{
				octomap::OcTreeNode* n = m_pOctree->search(*it);
				if(n)
				{
					if(m_pOctree->isNodeOccupied(n))	num_voxels_wrong++;
					else										num_voxels_correct++;
				}
				else											num_voxels_unknown++;
			}
			
			// count occupied cells
			for(octomap::KeySet::iterator it = occupied_cells.begin(); it != occupied_cells.end(); ++it)
			{
				octomap::OcTreeNode* n = m_pOctree->search(*it);
				if(n)
				{
					if(m_pOctree->isNodeOccupied(n))	num_voxels_correct++;
					else										num_voxels_wrong++;
				}
				else											num_voxels_unknown++;
			}
		}

		return true;
	}

protected:
	/** @brief	Octomap */
	boost::shared_ptr<octomap::OcTree> m_pOctree;

	/** @brief	Flag for simple update */
	const bool FLAG_SIMPLE_UPDATE_;
};

//template <typename PointT>
//void PCD2Octomap(const double								resolution,
//					  typename pcl::PointCloud<PointT>	&cloud,
//					  PointXYZCloud							&sensorPositions)
//{
//	// octomap
//	octomap::OcTree* tree = new octomap::OcTree(resolution);
//
//}

}

#endif