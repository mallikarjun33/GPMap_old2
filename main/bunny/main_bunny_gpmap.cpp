#if 1

// GPMap
#include "serialization/eigen_serialization.hpp" // Eigen
#include "io/io.hpp"								// loadPointClouds, savePointClouds, loadSensorPositionList
#include "visualization/cloud_viewer.hpp"	// show
#include "data/training_data.hpp"			// genEmptyPointList
#include "features/surface_normal.hpp"		// estimateSurfaceNormals
#include "common/common.hpp"					// getMinMaxPointXYZ
#include "octree/octree_gpmap.hpp"			// OctreeGPMap
#include "octree/octree_viewer.hpp"			// OctreeViewer
#include "util/log.hpp"							// Log
using namespace GPMap;

typedef OctreeGPMap<pcl::PointNormal, GP::MeanZeroDerObs, GP::CovSEisoDerObs, GP::LikGaussDerObs, GP::InfExactDerObs> OctreeGPMapType;

int main(int argc, char** argv)
{
	// files
	const size_t NUM_DATA = 4; 
	const std::string strInputDataFolder ("../../data/input/bunny/");
	const std::string strOutputDataFolder("../../data/output/bunny/");
	const std::string strFilenames_[] = {"bun000", "bun090", "bun180", "bun270"};
	StringList strFileNames(strFilenames_, strFilenames_ + NUM_DATA); 

	// log file
	std::string strLogFileName = strOutputDataFolder + "gpmap.log";
	Log logFile(strLogFileName);

	// [1] load/save hit points
	//PointXYZCloudPtrList hitPointCloudPtrList;
	//loadPointClouds<pcl::PointXYZ>(hitPointCloudPtrList, strFileNames, strInputDataFolder, ".ply");					// original ply files which are transformed in global coordinates
	//savePointClouds<pcl::PointXYZ>(hitPointCloudPtrList, strFileNames, strInputDataFolder, ".pcd");		// original pcd files which are transformed in global coordinates
	//loadPointClouds<pcl::PointXYZ>(hitPointCloudPtrList, strFileNames, strInputDataFolder, ".pcd");		// original pcd files which are transformed in global coordinates
	//show<pcl::PointXYZ>("Hit Points", hitPointCloudPtrList);

	// [2] load sensor positions
	//PointXYZVList sensorPositionList;
	//loadSensorPositionList(sensorPositionList, strFileNames, strInputDataFolder, "_camera_position.txt");
	//assert(NUM_DATA == hitPointCloudPtrList.size() && NUM_DATA == sensorPositionList.size());

	// [?] load/save empty points
	//const float GAP = 0.0001;
	//PointXYZCloudPtrList emptyPointCloudList;
	//genEmptyPointCloudList<pcl::PointXYZ>(hitPointCloudPtrList, sensorPositionList, GAP, emptyPointCloudList);
	//savePointClouds<pcl::PointXYZ>(emptyPointCloudList, strFileNames, strInputDataFolder, "_empty_points.pcd");		// original pcd files which are transformed in global coordinates
	//loadPointClouds<pcl::PointXYZ>(emptyPointCloudList, strFileNames, strInputDataFolder, "_empty_points.pcd");		// original pcd files which are transformed in global coordinates
	//PointXYZCloudPtrList hitEmptyPointCloudList;
	//combinePointCloudList(hitPointCloudPtrList, emptyPointCloudList, hitEmptyPointCloudList);
	//show<pcl::PointXYZ>("Hit/Empty Points", hitEmptyPointCloudList);

	// [4] load/save surface normals
	PointNormalCloudPtrList pointNormalCloudList;
	////estimateSurfaceNormals<ByNearestNeighbors>(hitPointCloudPtrList, sensorPositionList, false, 0.01, pointNormalCloudList);
	//estimateSurfaceNormals<ByMovingLeastSquares>(hitPointCloudPtrList, sensorPositionList, false, 0.01, pointNormalCloudList);
	//savePointClouds<pcl::PointNormal>(pointNormalCloudList, strFileNames, strInputDataFolder, "_normals.pcd");		// original pcd files which are transformed in global coordinates
	loadPointClouds<pcl::PointNormal>(pointNormalCloudList, strFileNames, strInputDataFolder, "_normals.pcd");		// original pcd files which are transformed in global coordinates
	//show<pcl::PointNormal>("Surface Normals", pointNormalCloudList, 0.005, true, 0.001);

	// [5] bounding box
	pcl::PointXYZ min_pt, max_pt;
	getMinMaxPointXYZ<pcl::PointNormal>(pointNormalCloudList, min_pt, max_pt);

	// [6] octree-based GPMap
	const double	BLOCK_SIZE = 0.003; // 0.01
	const size_t	NUM_CELLS_PER_AXIS = 3; // 10
	const bool		INDEPENDENT_BCM = true;
	const bool		POINT_DUPLICATION = false;
	const size_t	MIN_NUM_POINTS_TO_PREDICT = 10;
	OctreeGPMapType gpmap(BLOCK_SIZE, NUM_CELLS_PER_AXIS, INDEPENDENT_BCM, POINT_DUPLICATION, MIN_NUM_POINTS_TO_PREDICT);
	gpmap.defineBoundingBox(min_pt, max_pt);

	// hyperparameters
	//const float ell(0.1f), sigma_f(1.f), sigma_n(0.01f), sigma_nd(0.1f);
	const float ell			= 0.107467f;		// 0.107363f;
	const float sigma_f		= 0.99968f;			//0.99985f;
	const float sigma_n		= 0.00343017f;		// 0.0034282f;
	const float sigma_nd		= 0.0985929f;		// 0.0990157f;
	OctreeGPMapType::Hyp logHyp;
	logHyp.cov(0) = log(ell);
	logHyp.cov(1) = log(sigma_f);
	logHyp.lik(0) = log(sigma_n);
	logHyp.lik(1) = log(sigma_nd);

	// [7] update
	const float GAP = 0.001; // 0.0001
	//const float occupancyThreshold = 0.95;
	//float GAP; std::cout << "GAP: "; std::cin >> GAP; std::cout << GAP << std::endl;
	//float occupancyThreshold; std::cout << "Occupancy Threshold: "; std::cin >> occupancyThreshold; std::cout << occupancyThreshold << std::endl;
	boost::timer::cpu_times gpmap_add_point_elapsed, gpmap_update_elapsed;
	boost::timer::cpu_times gpmap_add_point_total_elapsed, gpmap_update_total_elapsed, gpmap_total_elapsed;
	gpmap_add_point_total_elapsed.clear();
	gpmap_update_total_elapsed.clear();
	gpmap_total_elapsed.clear();
	for(size_t i = 0; i < NUM_DATA; i++)
	{
		logFile << "==== Updating the GPMap with the point cloud #" << i << " ====" << std::endl;

		// [1] Set input cloud
		logFile << "[1] Set input cloud" << std::endl;
		//gpmap.setInputCloud(pointNormalCloudList[i], GAP, sensorPositionList[i]);
		gpmap.setInputCloud(pointNormalCloudList[i], GAP);

		// [2] Add points from the input cloud
		logFile << "[2] Add points from the input cloud" << std::endl;
		gpmap_add_point_elapsed = gpmap.addPointsFromInputCloud();
		logFile << gpmap_add_point_elapsed << std::endl << std::endl;

		// [3] Update using GPR
		logFile << "[3] Update using GPR" << std::endl;
		gpmap_update_elapsed		= gpmap.update(logHyp);
		logFile << gpmap_update_elapsed << std::endl << std::endl;

		// [4] Save
		//while(true)
		//{
			//float minMeanThreshold; std::cout << "Min Mean Threshold: "; std::cin >> minMeanThreshold; std::cout << minMeanThreshold << std::endl;
			//float maxVarThreshold; std::cout << "Max Var Threshold: "; std::cin >> maxVarThreshold; std::cout << maxVarThreshold << std::endl;
			float minMeanThreshold(0.f);
			float maxVarThreshold(10.f);
			if(minMeanThreshold < 0 && maxVarThreshold < 0) break;
			std::stringstream ss;
			ss << strOutputDataFolder << "gpmap_bunny_upto_" << i << "_min_mean_" << minMeanThreshold << "_max_var_" << maxVarThreshold;
			gpmap.saveAsOctomap(ss.str(), minMeanThreshold, maxVarThreshold);
		//}

		// accumulate cpu times
		gpmap_add_point_total_elapsed += gpmap_add_point_elapsed;
		gpmap_update_total_elapsed		+= gpmap_update_elapsed;
		gpmap_total_elapsed				+= gpmap_add_point_elapsed;
		gpmap_total_elapsed				+= gpmap_update_elapsed;
	}

	// total time
	logFile << "============= Total Time =============" << std::endl;
	logFile << "- Total: add point"	<< std::endl << gpmap_add_point_total_elapsed	<< std::endl << std::endl;
	logFile << "- Total: update"		<< std::endl << gpmap_update_total_elapsed		<< std::endl << std::endl;
	logFile << "- Total"					<< std::endl << gpmap_total_elapsed					<< std::endl << std::endl;

	// [8] evaluation
	//logFile << "============= Evaluation =============" << std::endl;
	//unsigned int num_points, num_voxels_correct, num_voxels_wrong, num_voxels_unknown;
	//octomap.evaluate<pcl::PointXYZ, pcl::PointXYZ>(hitPointCloudPtrList, sensorPositionList,
	//															  num_points, num_voxels_correct, num_voxels_wrong, num_voxels_unknown);
	//logFile << "Number of hit points: " << num_points << std::endl;
	//logFile << "Number of correct voxels: " << num_voxels_correct << std::endl;
	//logFile << "Number of wrong voxels: " << num_voxels_wrong << std::endl;
	//logFile << "Number of unknown voxels: " << num_voxels_unknown << std::endl;
	//logFile << "Correct rate (correct/(correct+wrong)): " << static_cast<float>(num_voxels_correct)/static_cast<float>(num_voxels_correct+num_voxels_wrong) << std::endl;

	//// save
	//while(true)
	//{
	//	float occupancyThreshold; std::cout << "Occupancy Threshold: "; std::cin >> occupancyThreshold; std::cout << occupancyThreshold << std::endl;
	//	std::stringstream ss;
	//	ss << strOutputDataFolder << "gpmap_bunny_upto_" << i << "_" << occupancyThreshold;
	//	gpmap.saveAsOctomap(ss.str(), occupancyThreshold, false);
	//}
	system("pause");
}

#endif