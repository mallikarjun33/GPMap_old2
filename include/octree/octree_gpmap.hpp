#ifndef _OCTREE_GPMAP_HPP_
#define _OCTREE_GPMAP_HPP_

// STL
#include <cmath>			// floor, ceil
#include <vector>

// PCL
#include <pcl/point_types.h>
#include <pcl/octree/octree.h>
#include <pcl/octree/octree_impl.h>

// Eigen
#include <Eigen/Dense>

//// Boost
//#include "boost/tuple/tuple.hpp"

// OpenGP
#include "GP.h"

// GPMap
#include "util/util.hpp"						// min max
#include "octree/octree_container.hpp"		// LeafNode
#include "data/test_data.hpp"					// meshGrid
#include "plsc/plsc.hpp"						// PLSC

namespace GPMap {

typedef LeafNode														LeafT;
typedef pcl::octree::OctreeContainerEmpty<int>				BranchT;
typedef pcl::octree::OctreeBase<int, LeafT, BranchT>		OctreeT;

//template<typename PointT, 
//			typename LeafT		= LeafNode,
//			typename BranchT	= pcl::octree::OctreeContainerEmpty<int>,
//			typename OctreeT	= pcl::octree::OctreeBase<int, LeafT, BranchT> >
//class OctreeGPMap : protected pcl::octree::OctreePointCloud<PointT, LeafNode, BranchT, OctreeT>

//template<typename PointT>
//class OctreeGPMap : protected pcl::octree::OctreePointCloud<PointT, LeafNode, BranchT, OctreeT>

template<typename PointT,
			template<typename> class MeanFunc, 
			template<typename> class CovFunc, 
			template<typename> class LikFunc,
			template <typename, 
						 template<typename> class,
						 template<typename> class,
						 template<typename> class> class InfMethod>
class OctreeGPMap : protected pcl::octree::OctreePointCloud<PointT, LeafNode, BranchT, OctreeT>
{
protected:
	// octree
	typedef pcl::octree::OctreePointCloud<PointT, LeafT, BranchT, OctreeT>	Parent;
	typedef OctreeGPMap<PointT, MeanFunc, CovFunc, LikFunc, InfMethod>		OctreeGPMapType;

	// Gaussian processes
	typedef float Scalar;
	typedef GP::GaussianProcess<Scalar, MeanFunc, CovFunc, LikFunc, InfMethod>	GPType;

public:
	typedef typename GPType::Hyp Hyp;

protected:
	// data
	typedef GP::DerivativeTrainingData<float>		DerivativeTrainingData;
	typedef GP::TestData<float>						TestData;

public:
	public:
	/** @brief Constructor
	*  @param resolution: octree resolution at lowest octree level
    */
   OctreeGPMap(const double			BLOCK_SIZE, 
					const size_t			NUM_CELLS_PER_AXIS, 
					const bool				FLAG_INDEPENDENT_BCM,
					const bool				FLAG_DUPLICATE_POINTS = false,
					const size_t			MIN_NUM_POINTS_TO_PREDICT = 10)
		: pcl::octree::OctreePointCloud<PointT, LeafT, BranchT, OctreeT>(BLOCK_SIZE),
		  BLOCK_SIZE_						(resolution_),
		  NUM_CELLS_PER_AXIS_			(max<size_t>(1, NUM_CELLS_PER_AXIS)),
		  NUM_CELLS_PER_BLOCK_			(NUM_CELLS_PER_AXIS_*NUM_CELLS_PER_AXIS_*NUM_CELLS_PER_AXIS_),
		  CELL_SIZE_						(BLOCK_SIZE_/static_cast<double>(NUM_CELLS_PER_AXIS_)),
		  FLAG_INDEPENDENT_BCM_			(FLAG_INDEPENDENT_BCM),
		  FLAG_DUPLICATE_POINTS_		(FLAG_DUPLICATE_POINTS),
		  MIN_NUM_POINTS_TO_PREDICT_	(max<size_t>(1, MIN_NUM_POINTS_TO_PREDICT)),
		  m_pXs(new Matrix(NUM_CELLS_PER_BLOCK_, 3))
   {
#ifdef _TEST_OCTREE_GPMAP
		PCL_WARN("Testing octree-based GPMap\n");
#endif

		// set the test positions at (0, 0, 0)
		meshGrid(Eigen::Vector3f(0.f, 0.f, 0.f), NUM_CELLS_PER_AXIS_, CELL_SIZE_, m_pXs);
   }

	/** @brief Empty class constructor */
	virtual ~OctreeGPMap()
	{
	}

	/** @brief Define bounding box for octree
	* @note Bounding box cannot be changed once the octree contains elements.
	* @param[in] min_pt lower bounding box corner point
	* @param[in] max_pt upper bounding box corner point
	*/
	template <typename GeneralPointT>
	void defineBoundingBox(const GeneralPointT &min_pt, const GeneralPointT &max_pt)
	{
		defineBoundingBox(static_cast<double>(min_pt.x), static_cast<double>(min_pt.y), static_cast<double>(min_pt.z), 
								static_cast<double>(max_pt.x), static_cast<double>(max_pt.y), static_cast<double>(max_pt.z));
	}

	/** @brief Define bounding box for octree
	* @note Bounding box cannot be changed once the octree contains elements.
	* @param[in] minX X coordinate of lower bounding box corner
	* @param[in] minY Y coordinate of lower bounding box corner
	* @param[in] minZ Z coordinate of lower bounding box corner
	* @param[in] maxX X coordinate of upper bounding box corner
	* @param[in] maxY Y coordinate of upper bounding box corner
	* @param[in] maxZ Z coordinate of upper bounding box corner
	*/
	void defineBoundingBox(double minX, double minY, double minZ,
								  double maxX, double maxY, double maxZ)
	{
		minX = floor(minX/BLOCK_SIZE_ - 1.f)*BLOCK_SIZE_;
		minY = floor(minY/BLOCK_SIZE_ - 1.f)*BLOCK_SIZE_;
		minZ = floor(minZ/BLOCK_SIZE_ - 1.f)*BLOCK_SIZE_;
		maxX = ceil (maxX/BLOCK_SIZE_ + 1.f)*BLOCK_SIZE_;
		maxY = ceil (maxY/BLOCK_SIZE_ + 1.f)*BLOCK_SIZE_;
		maxZ = ceil (maxZ/BLOCK_SIZE_ + 1.f)*BLOCK_SIZE_;
		Parent::defineBoundingBox(minX, minY, minZ, maxX, maxY, maxZ);

#ifdef _TEST_OCTREE_GPMAP
		std::cout << "min: (" << minX_ << ", " << minY_ << ", " << minZ_ << "), "
					 << "max: (" << maxX_ << ", " << maxY_ << ", " << maxZ_ << ")" << std::endl;
#endif

	}

   /** @brief Provide a pointer to the input data set.
	  * @details	Refer to pcl::octree::OctreePointCloud<PointT, LeafT, BranchT, OctreeT>
	  *				::setInputCloud(const PointCloudConstPtr &cloud_arg, const IndicesConstPtr &indices_arg = IndicesConstPtr ())
	  *				where assertion is activated when this->leafCount_!=0.
     * @param[in] pCloud the const boost shared pointer to a PointCloud message
     * @param[in] pIndices the point indices subset that is to be used from \a cloud - if 0 the whole point cloud is used
    */
   void setInputCloud(const PointCloudConstPtr	&pCloud,
							 const float					gap = 0.f,
							 //const pcl::PointXYZ			&sensorPosition = pcl::PointXYZ(),
							 const IndicesConstPtr		&pIndices = IndicesConstPtr())
   {
		//assert(this->leafCount_==0);

		// set the input cloud
		input_	= pCloud;
		indices_	= pIndices;

		// gap and sensor position for generating empty points
		m_gap = gap;
		//m_sensorPosition = sensorPosition;
	}

	/** @brief Add points from input point cloud to octree.
	  * @details	Refer to pcl::octree::OctreePointCloud<PointT, LeafT, BranchT, OctreeT>::addPointsFromInputCloud()
	  *				where assertion is activated when this->leafCount_!=0.
	  */
	void addPointsFromInputCloud()
	{
		// assert (this->leafCount_==0);

		// reset the previous point indices in each voxel
		resetPointIndexVectors();

#ifdef _TEST_OCTREE_GPMAP
		PCL_INFO("After reset, there should be no points danlged in the all voxels.\n");
		assert(totalNumOfPointsDangledInVoxels() == 0);
#endif

		// min/max of the new observations
		PointT min_pt, max_pt;
		pcl::getMinMax3D(*input_, min_pt, max_pt);

		// make sure bounding box is big enough
		if(!boundingBoxDefined_) 
			defineBoundingBox(min_pt, max_pt);
		else
		{
			//min_pt.x -= 2.f * static_cast<float>(BLOCK_SIZE_);
			//min_pt.y -= 2.f * static_cast<float>(BLOCK_SIZE_);
			//min_pt.z -= 2.f * static_cast<float>(BLOCK_SIZE_);
			//max_pt.x += 2.f * static_cast<float>(BLOCK_SIZE_);
			//max_pt.y += 2.f * static_cast<float>(BLOCK_SIZE_);
			//max_pt.z += 2.f * static_cast<float>(BLOCK_SIZE_);
			min_pt.x -= static_cast<float>(BLOCK_SIZE_);
			min_pt.y -= static_cast<float>(BLOCK_SIZE_);
			min_pt.z -= static_cast<float>(BLOCK_SIZE_);
			max_pt.x += static_cast<float>(BLOCK_SIZE_);
			max_pt.y += static_cast<float>(BLOCK_SIZE_);
			max_pt.z += static_cast<float>(BLOCK_SIZE_);

			// adopt the bounding box
			adoptBoundingBoxToPoint(min_pt);
			adoptBoundingBoxToPoint(max_pt);
		}

#ifdef _TEST_OCTREE_GPMAP
		PCL_INFO("After adopting the bounding box, all the shifted points should be in the range\n");
		// shifted point
		float shiftedPointX, shiftedPointY, shiftedPointZ;
#endif

		// add the new point cloud
		if(indices_)
		{
			for(std::vector<int>::const_iterator current = indices_->begin (); current != indices_->end (); ++current)
			{
				if(isFinite(input_->points[*current]))
				{
					assert( (*current>=0) && (*current < static_cast<int>(input_->points.size())));
	
#ifdef _TEST_OCTREE_GPMAP
					// current point
					const PointT &currPoint = input_->points[*current];

					// shifted points
					for(float deltaX = -BLOCK_SIZE_; deltaX <= static_cast<float>(BLOCK_SIZE_); deltaX += BLOCK_SIZE_)
					{
						shiftedPointX = currPoint.x + deltaX;
						for(float deltaY = -BLOCK_SIZE_; deltaY <= static_cast<float>(BLOCK_SIZE_); deltaY += BLOCK_SIZE_)
						{
							shiftedPointY = currPoint.y + deltaY;
							for(float deltaZ = -BLOCK_SIZE_; deltaZ <= static_cast<float>(BLOCK_SIZE_); deltaZ += BLOCK_SIZE_)
							{
								shiftedPointZ = currPoint.z + deltaZ;

								// shifted point should be in the range
								assert(shiftedPointX >= minX_ && shiftedPointX <= maxX_ && 
										 shiftedPointY >= minY_ && shiftedPointY <= maxY_ && 
										 shiftedPointZ >= minZ_ && shiftedPointZ <= maxZ_);
							}
						}
					}
#endif
					// add points to octree
					this->addPointIdx(*current);
				}
			}
		}
		else
		{
			for(size_t i = 0; i < input_->points.size (); i++)
			{
				if (isFinite(input_->points[i]))
				{
#ifdef _TEST_OCTREE_GPMAP
					// current point
					const PointT &currPoint = input_->points[i];

					// shifted points
					for(float deltaX = -BLOCK_SIZE_; deltaX <= static_cast<float>(BLOCK_SIZE_); deltaX += BLOCK_SIZE_)
					{
						shiftedPointX = currPoint.x + deltaX;
						for(float deltaY = -BLOCK_SIZE_; deltaY <= static_cast<float>(BLOCK_SIZE_); deltaY += BLOCK_SIZE_)
						{
							shiftedPointY = currPoint.y + deltaY;
							for(float deltaZ = -BLOCK_SIZE_; deltaZ <= static_cast<float>(BLOCK_SIZE_); deltaZ += BLOCK_SIZE_)
							{
								shiftedPointZ = currPoint.z + deltaZ;

								// shifted point should be in the range
								assert(shiftedPointX >= minX_ && shiftedPointX <= maxX_ && 
										 shiftedPointY >= minY_ && shiftedPointY <= maxY_ && 
										 shiftedPointZ >= minZ_ && shiftedPointZ <= maxZ_);
							}
						}
					}
#endif
					// add points to octree
					this->addPointIdx(static_cast<int>(i));
				}
			}
		}

#ifndef CONST_LEAF_NODE_ITERATOR_
		m_nonEmptyBlockCenterPointXYZList.clear();
		getOccupiedBlockCenters(m_nonEmptyBlockCenterPointXYZList, true);
#endif
	}

	/** @brief		Train hyperparameters 
	  * @return		Minimum sum of negative log marginalizations of all leaf nodes
	 */
	GP::DlibScalar train(Hyp &logHyp, const int maxIter, const size_t numRandomBlocks = 0, const GP::DlibScalar minValue = 1e-15)
	{
		// select random blocks
#ifndef CONST_LEAF_NODE_ITERATOR_
		if(numRandomBlocks > 0 && numRandomBlocks < m_nonEmptyBlockCenterPointXYZList.size())
		{
			random_unique(m_nonEmptyBlockCenterPointXYZList.begin(), m_nonEmptyBlockCenterPointXYZList.end(), numRandomBlocks);
			m_numRandomBlocks = numRandomBlocks;
		}
		else
			m_numRandomBlocks = m_nonEmptyBlockCenterPointXYZList.size();
#endif

		// conversion from GP hyperparameters to a Dlib vector
		GP::DlibVector logDlib;
		logDlib.set_size(logHyp.size());
		GP::Hyp2Dlib<Scalar, MeanFunc, CovFunc, LikFunc>(logHyp, logDlib);

		// trainer
#if EIGEN_VERSION_AT_LEAST(3,2,0)
		GP::DlibScalar minNlZ = GP::TrainerUsingApproxDerivatives<OctreeGPMapType>::train<GP::BOBYQA, GP::NoStopping>(logDlib,
																																						  *this, // Bug: const object
																																						  maxIter, minValue);
#else
	#error
#endif

		// conversion from a Dlib vector to GP hyperparameters
		GP::Dlib2Hyp<Scalar, MeanFunc, CovFunc, LikFunc>(logDlib, logHyp);

		return minNlZ;
	}

	/** @brief		Operator for optimizing hyperparameters 
	  * @return		Sum of negative log marginalizations of all leaf nodes
	  * @todo		Do not cover all of the leaf nodes, but select some(10,100) of them randomly
	 */
	GP::DlibScalar operator()(const GP::DlibVector &logDlib) const
	{
		// Sum of negative log marginalizations of all leaf nodes
		GP::DlibScalar sumNlZ(0);

		// convert a Dlib vector to GP hyperparameters
		Hyp logHyp;
		GP::Dlib2Hyp<Scalar, MeanFunc, CovFunc, LikFunc>(logDlib, logHyp);
		std::cout << "hyp.mean = " << std::endl << logHyp.mean.array().exp().matrix() << std::endl << std::endl;
		std::cout << "hyp.cov = "  << std::endl << logHyp.cov.array().exp().matrix()  << std::endl << std::endl;
		std::cout << "hyp.lik = "  << std::endl << logHyp.lik.array().exp().matrix()  << std::endl << std::endl;

		// for each leaf node
		LeafNode* pLeafNode;
		Indices indexList;
		Scalar nlZ;
		size_t blockCount(0);
#ifdef CONST_LEAF_NODE_ITERATOR_
		LeafNodeIterator iter(*this);
		while(*++iter)
#else
		std::cout << "Total: " << m_numRandomBlocks << " / " << m_nonEmptyBlockCenterPointXYZList.size() << " - ";
		pcl::octree::OctreeKey key;
		for(PointXYZVList::const_iterator iter = m_nonEmptyBlockCenterPointXYZList.begin();
			 (iter != m_nonEmptyBlockCenterPointXYZList.cend()) && (blockCount < m_numRandomBlocks);
			 iter++, blockCount++)
#endif
		{
			// leaf node corresponding the octree key
#ifdef CONST_LEAF_NODE_ITERATOR_
			pLeafNode = static_cast<LeafNode*>(iter.getCurrentOctreeNode())->getDataTVector();
#else
			// key
			genOctreeKeyforPointXYZ(*iter, key);
			pLeafNode = findLeaf(key);
#endif
			// get indices
			const Indices &indexList = pLeafNode->getDataTVector();

			// if there is no point in the node, ignore it
			if(indexList.size() < MIN_NUM_POINTS_TO_PREDICT_) continue;
			std::cout << blockCount << "(" << indexList.size() << "), ";

			// training data
			DerivativeTrainingData derivativeTrainingData;
			MatrixPtr pX, pXd; VectorPtr pYYd;
			//generateTraingData(input_, indexList, m_sensorPosition, m_gap, pX, pXd, pYYd);
			generateTraingData(input_, indexList, m_gap, pX, pXd, pYYd);
			derivativeTrainingData.set(pX, pXd, pYYd);

			//InfType::negativeLogMarginalLikelihood(logHyp, 
			GPType::negativeLogMarginalLikelihood(logHyp, 
															  derivativeTrainingData,
															  nlZ, 
															  VectorPtr(),
															  1);

			// sum
			sumNlZ += static_cast<GP::DlibScalar>(nlZ);
		}

		std::cout << ": " << sumNlZ << std::endl << std::endl;
		return sumNlZ;
	}

	/** @brief		Update the GPMap with new observations */
	void update(const Hyp		&logHyp,
					const int		maxIter = 0)
	{
#ifdef _TEST_OCTREE_GPMAP
		PCL_INFO("More than one points should be dangled in itself or neighbors.\n");
#endif
				
		// if a point index is duplicated to 
		if(FLAG_DUPLICATE_POINTS_)
		{
			// leaf node iterator
			LeafNodeIterator iter(*this);

			// for each leaf node
			Eigen::Vector3f min_pt;
			//Indices indexList;
			while(*++iter)
			{
				// key
				const pcl::octree::OctreeKey &key = iter.getCurrentOctreeKey();

				// min point
				genVoxelMinPoint(key, min_pt);

				// collect indices
				//indexList.clear();
				//getData(key, indexList);
				const Indices &indexList  = static_cast<LeafNode*>(iter.getCurrentOctreeNode())->getDataTVector();

#ifdef _TEST_OCTREE_GPMAP
				// more than one points should be dangled in itself or neighbors
				// assert(indexList.size() > 0);
#endif
				// if the total number of points are too small, ignore it.
				if(indexList.size() < MIN_NUM_POINTS_TO_PREDICT_) continue;

				// leaf node
				LeafNode *pLeafNode = static_cast<LeafNode *>(iter.getCurrentOctreeNode());

				// predict
				predict(logHyp, indexList, min_pt, pLeafNode, maxIter);
			}
		}
		else
		{
			// TODO
			// Present: for each leaf node, collect all point indices and if it is greater than minimum, update
			// Future: collect all non-empty block centers to a set, add its neighbors to the set, then update all nodes in the set

			// create empty neigboring blocks if necessary
			PointXYZVListPtr pNonEmptyBlockCenterPointXYZList = createEmptyNeigboringBlocks();
			size_t NUM_NON_EMPTY_BLOCKS = pNonEmptyBlockCenterPointXYZList->size();
			std::cout << "Total: " << NUM_NON_EMPTY_BLOCKS << ": ";

			// leaf node iterator
			LeafNodeIterator iter(*this);

			// for each leaf node
			Eigen::Vector3f min_pt;
			std::vector<int> indexList;
			size_t blockCount(0);
			while(*++iter)
			{
				// key
				const pcl::octree::OctreeKey &key = iter.getCurrentOctreeKey();

				// min point of the current block
				genVoxelMinPoint(key, min_pt);

				// collect point indices in neighboring blocks
				indexList.clear();
				int nextKeyX, nextKeyY, nextKeyZ;
				for(int deltaX = -1; deltaX <= 1; deltaX++)
				{
					for(int deltaY = -1; deltaY <= 1; deltaY++)
					{
						for(int deltaZ = -1; deltaZ <= 1; deltaZ++)
						{
							// neighboring block key
							nextKeyX = static_cast<int>(key.x) + deltaX;
							nextKeyY = static_cast<int>(key.y) + deltaY;
							nextKeyZ = static_cast<int>(key.z) + deltaZ;

							// if the neighboring block is out of range, ignore it
							if(nextKeyX < 0 || nextKeyY < 0 || nextKeyZ < 0 || 
								nextKeyX > static_cast<int>(maxKey_.x) ||
								nextKeyY > static_cast<int>(maxKey_.y) ||
								nextKeyZ > static_cast<int>(maxKey_.z)) continue;

							// get data
							getData(pcl::octree::OctreeKey(static_cast<unsigned int>(nextKeyX), 
																	 static_cast<unsigned int>(nextKeyY), 
																	 static_cast<unsigned int>(nextKeyZ)),
																	 indexList);
						}
					}
				}

#ifdef _TEST_OCTREE_GPMAP
				// more than one points should be dangled in itself or neighbors
				// assert(indexList.size() > 0);
#endif
				// if the total number of points are too small, ignore it.
				if(indexList.size() < MIN_NUM_POINTS_TO_PREDICT_) continue;
				std::cout << blockCount << "(" << indexList.size() << "), ";

				// leaf node
				LeafNode *pLeafNode = static_cast<LeafNode *>(iter.getCurrentOctreeNode());

				// predict
				predict(logHyp, indexList, min_pt, pLeafNode, maxIter);

				// next
				blockCount++;
			}
			std::cout << std::endl;
		}
	}

	/** @brief		Get occupied voxel center points
	  * @details	Refer to pcl::octree::OctreePointCloud<PointT, LeafT, BranchT, OctreeT>::getOccupiedVoxelCenters(AlignedPointTVector &voxelCenterList_arg) const
	  *				which only accept a vector of PointT points, not pcl::PointXYZ.
	  *				Thus, even if the octree has pcl::PointNormals, the center point should also be pcl::PointXYZ.
	  */
	size_t getOccupiedVoxelCenters(PointXYZVList	&voxelCenterPointXYZVector,
											 const bool			fWithoutIsolatedVoxels) const
	{
		// clear the vector
		voxelCenterPointXYZVector.clear();

		// shift key
		pcl::octree::OctreeKey key(0, 0, 0);

		// search for occupied voxels recursively
		return getOccupiedVoxelCentersRecursive(this->rootNode_, key, voxelCenterPointXYZVector, fWithoutIsolatedVoxels);
	}

	/** @brief		Get occupied block center points
	  * @details	Refer to pcl::octree::OctreePointCloud<PointT, LeafT, BranchT, OctreeT>::getOccupiedVoxelCenters(AlignedPointTVector &voxelCenterList_arg) const
	  *				which only accept a vector of PointT points, not pcl::PointXYZ.
	  *				Thus, even if the octree has pcl::PointNormals, the center point should also be pcl::PointXYZ.
	  */
	bool getOccupiedBlockCenters(PointXYZVList	&blockCenterPointXYZVector,
										  const bool		fWithoutEmptyBlocks)
	{
		// clear the vector
		blockCenterPointXYZVector.clear();

		// center point
		pcl::PointXYZ center;

		// leaf node iterator
		LeafNodeIterator iter(*this);

		// check whether it is empty or not
		if(fWithoutEmptyBlocks)
		{
			// for each leaf node
			while(*++iter)
			{
				// if the leaf node has no points, ignore it.
				if(static_cast<LeafNode*>(iter.getCurrentOctreeNode())->getSize() == 0) continue;

				// key
				const pcl::octree::OctreeKey &key = iter.getCurrentOctreeKey();

				// generate the center point
			   genLeafNodeCenterXYZFromOctreeKey(key, center);

				// add the center point
				blockCenterPointXYZVector.push_back(center);
			}
		}
		else
		{
			// for each leaf node
			while(*++iter)
			{
				// key
				const pcl::octree::OctreeKey &key = iter.getCurrentOctreeKey();

				// generate the center point
			   genLeafNodeCenterXYZFromOctreeKey(key, center);

				// add the center point
				blockCenterPointXYZVector.push_back(center);
			}
		}

		return blockCenterPointXYZVector.size() > 0;
	}

	/** @brief Get occupied cell centers */
	size_t getOccupiedCellCenters(PointXYZVList		&cellCenterPointXYZVector,
											const float			threshold,
											const bool			fRemoveIsolatedCells)
	{
		// clear the vector
		cellCenterPointXYZVector.clear();

		// leaf node iterator
		LeafNodeIterator iter(*this);

		// for each leaf node
		Eigen::Vector3f min_pt;
		VectorPtr pMean;
		MatrixPtr pVariance;
		while(*++iter)
		{
			// key
			const pcl::octree::OctreeKey &key = iter.getCurrentOctreeKey();

			// min point
			genVoxelMinPoint(key, min_pt);

			// leaf node
			LeafNode *pLeafNode = static_cast<LeafNode *>(iter.getCurrentOctreeNode());

			// mean, variance
			pLeafNode->get(pMean, pVariance);
			if(!pMean || !pVariance) continue;
			if(pVariance->cols() != 1)
			{
				MatrixPtr pTempVariance(new Matrix(pVariance->rows(), 1));
				pTempVariance->noalias() = pVariance->diagonal();
				pVariance = pTempVariance;
			}

			// check if each cell is occupied
			size_t row;
			for(size_t ix = 0; ix < NUM_CELLS_PER_AXIS_; ix++)
				for(size_t iy = 0; iy < NUM_CELLS_PER_AXIS_; iy++)
					for(size_t iz = 0; iz < NUM_CELLS_PER_AXIS_; iz++)
						if(isNotIsolatedCell(pMean, pVariance, ix, iy, iz, threshold, fRemoveIsolatedCells, row))
							cellCenterPointXYZVector.push_back(pcl::PointXYZ((*m_pXs)(row, 0) + min_pt.x(), 
																							 (*m_pXs)(row, 1) + min_pt.y(),
																							 (*m_pXs)(row, 2) + min_pt.z()));
		}

		return cellCenterPointXYZVector.size();
	}

	/** @brief Get the total number of point indices stored in each voxel */
	size_t totalNumOfPointsDangledInVoxels()
	{
		// size
		size_t n(0);

		// leaf node iterator
		LeafNodeIterator iter(*this);

		// for each leaf node
		LeafNode *pLeafNode;
		while(*++iter)
		{
			// get size
			pLeafNode = static_cast<LeafNode*>(iter.getCurrentOctreeNode());
			n += pLeafNode->getSize();
		}

		return n;
	}

	bool isThereEmptyLeafNode() const
	{
		// leaf node iterator
		LeafNodeIterator iter(*this);

		// for each leaf node
		while(*++iter)
		{
			// get size
			if(static_cast<LeafNode*>(iter.getCurrentOctreeNode())->getSize() <= 0) return false;
		}

		return true;
	}

   /** @brief		Get a pointer to the input point cloud dataset.
	  * @details	This function overrides the parent's corresponding function
	  *				to cover the protected inheritance.
     * @return pointer to pointcloud input class.
     */
   inline PointCloudConstPtr getInputCloud() const
   {
      return Parent::getInputCloud();
   }
	
	/** @brief		Get octree voxel resolution
	  * @details	This function overrides the parent's corresponding function
	  *				to cover the protected inheritance.
     * @return voxel resolution at lowest tree level
     */
   double getResolution() const
   {
		//return Parent::getResolution();
		return CELL_SIZE_;
   }

	double getCellSize() const
	{
		return CELL_SIZE_;
	}

protected:

	/** @brief Reset the points in each voxel */
	void resetPointIndexVectors()
	{
		// leaf node iterator
		LeafNodeIterator iter(*this);

		// for each leaf node
		while(*++iter)
		{
			// reset
			LeafNode *pLeafNode = static_cast<LeafNode*>(iter.getCurrentOctreeNode());
			pLeafNode->reset();
		}
	}

	/** @brief		Add a point from input cloud to the corresponding voxel and neighboring ones
	  * @details	Refer to pcl::octree::OctreePointCloud<PointT, LeafT, BranchT, OctreeT>::addPointIdx (const int pointIdx_arg)
	  *				which add the point to the corresponding voxel only.
	  */
	void addPointIdx(const int pointIdx)
	{
		// check the index range
		assert(pointIdx < static_cast<int>(input_->points.size()));
	
		// point
		const PointT& point = input_->points[pointIdx];
		
		// make sure bounding box is big enough
		if(FLAG_DUPLICATE_POINTS_)
		{
			PointT min_pt(point), max_pt(point);
			min_pt.x -= BLOCK_SIZE_;
			min_pt.y -= BLOCK_SIZE_;
			min_pt.z -= BLOCK_SIZE_;
			max_pt.x += BLOCK_SIZE_;
			max_pt.y += BLOCK_SIZE_;
			max_pt.z += BLOCK_SIZE_;
			adoptBoundingBoxToPoint(min_pt);
			adoptBoundingBoxToPoint(max_pt);
		}
		else
		{
			adoptBoundingBoxToPoint(point);
		}
		
		// key
		pcl::octree::OctreeKey key;
		genOctreeKeyforPoint(point, key);
		
		// add point to octree at key
		if(FLAG_DUPLICATE_POINTS_)
		{
			for(int deltaX = -1; deltaX <= 1; deltaX++)
			{
				for(int deltaY = -1; deltaY <= 1; deltaY++)
				{
					for(int deltaZ = -1; deltaZ <= 1; deltaZ++)
					{
#ifdef _TEST_OCTREE_GPMAP
						assert(static_cast<int>(key.x) + deltaX >= 0);
						assert(static_cast<int>(key.y) + deltaY >= 0);
						assert(static_cast<int>(key.z) + deltaZ >= 0);
						assert(static_cast<int>(key.x) + deltaX <= static_cast<int>(maxKey_.x));
						assert(static_cast<int>(key.y) + deltaY >= static_cast<int>(maxKey_.y));
						assert(static_cast<int>(key.z) + deltaZ >= static_cast<int>(maxKey_.z));
#endif
						this->addData(pcl::octree::OctreeKey(static_cast<unsigned int>(key.x+deltaX), 
																		 static_cast<unsigned int>(key.y+deltaY),
																		 static_cast<unsigned int>(key.z+deltaZ)),
																		 pointIdx);
					}
				}
			}
		}
		else
			this->addData(key, pointIdx);
	}

	/** @brief Create empty neighboring blocks for each occupied block if necessary */
	PointXYZVListPtr createEmptyNeigboringBlocks()
	{
#ifdef _TEST_OCTREE_GPMAP
		// max key before create empty neigboring blocks
		const pcl::octree::OctreeKey key_before(maxKey_);
#endif

		// non-empty block center points
		PointXYZVListPtr	pNonEmptyBlockCenterPointXYZList(new PointXYZVList());
		if(!getOccupiedBlockCenters(*pNonEmptyBlockCenterPointXYZList, true)) return pNonEmptyBlockCenterPointXYZList;

		// for each center point
		PointT nextCenter;
		pcl::octree::OctreeKey nextKey;
		for(PointXYZVList::const_iterator iter = pNonEmptyBlockCenterPointXYZList->begin();
			 iter != pNonEmptyBlockCenterPointXYZList->cend();
			 iter++)
		{
			// center point
			const pcl::PointXYZ &currCenter = (*iter);

			// add -1 index to the neighboring leaf nodes
			for(float deltaX = -BLOCK_SIZE_; deltaX <= static_cast<float>(BLOCK_SIZE_); deltaX += BLOCK_SIZE_)
			{
				for(float deltaY = -BLOCK_SIZE_; deltaY <= static_cast<float>(BLOCK_SIZE_); deltaY += BLOCK_SIZE_)
				{
					for(float deltaZ = -BLOCK_SIZE_; deltaZ <= static_cast<float>(BLOCK_SIZE_); deltaZ += BLOCK_SIZE_)
					{
						// except the current leaf node
						if(deltaX == 0.f && deltaY == 0.f && deltaZ == 0.f) continue;

						// neighbor's center point
						nextCenter.x = currCenter.x + deltaX;
						nextCenter.y = currCenter.y + deltaY;
						nextCenter.z = currCenter.z + deltaZ;

#ifdef _TEST_OCTREE_GPMAP
						// min/max range should be adopted 
						// in addPointsFromInputCloud() with adoptBoundingBoxToPoint()
						assert(nextCenter.x > minX_ && nextCenter.y > minY_ && nextCenter.z > minZ_);
						assert(nextCenter.x < maxX_ && nextCenter.y < maxY_ && nextCenter.z < maxZ_);
#endif
						// make sure bounding box is big enough
						adoptBoundingBoxToPoint(nextCenter);

#ifdef _TEST_OCTREE_GPMAP
						// min/max range should be adopted 
						// in addPointsFromInputCloud() with adoptBoundingBoxToPoint()
						assert(nextCenter.x > minX_ && nextCenter.y > minY_ && nextCenter.z > minZ_);
						assert(nextCenter.x < maxX_ && nextCenter.y < maxY_ && nextCenter.z < maxZ_);
#endif

						// neighbor's octree key
						genOctreeKeyforPoint(nextCenter, nextKey);

						// add dummy index (-1) to create an empty leaf node
						addData(nextKey, -1);
					}
				}
			}
		}

#ifdef _TEST_OCTREE_GPMAP
		// max key after create empty neigboring blocks
		const pcl::octree::OctreeKey key_after(maxKey_);

		// max key should not be changed
		assert(key_before == key_after);
#endif

		return pNonEmptyBlockCenterPointXYZList;
	}


	/** @brief Get the maximum octree key */
	void getMaxKey(pcl::octree::OctreeKey &key) const
	{
		// calculate unsigned integer octree key
		//key.x = static_cast<unsigned int>((this->maxX_ - this->minX_) / this->resolution_);
		//key.y = static_cast<unsigned int>((this->maxY_ - this->minY_) / this->resolution_);
		//key.z = static_cast<unsigned int>((this->maxZ_ - this->minZ_) / this->resolution_);
		key = maxKey_;
	}

	/** @brief Get the point indices in the leaf node corresponding the octree key */
	bool getData(const pcl::octree::OctreeKey &key, std::vector<int> &indexList) const
	{
		// leaf node corresponding the octree key
		LeafNode* pLeafNode = findLeaf(key);

		// if the leaf node exists, add point indices to the vector
		if(pLeafNode)
		{
			pLeafNode->getData(indexList);
			return true;
		}
		return false;
	}

	/** @brief Get the min max points of the voxel corresponding the octree key */
	void genVoxelBounds(const pcl::octree::OctreeKey &key, Eigen::Vector3f &min_pt, Eigen::Vector3f &max_pt) const 
	{
		// calculate voxel bounds
		genVoxelMinPoint(key, min_pt);
		genVoxelMaxPoint(key, max_pt);
	}

	/** @brief Get the min points of the voxel corresponding the octree key */
	inline void genVoxelMinPoint(const pcl::octree::OctreeKey &key, Eigen::Vector3f &min_pt) const 
	{
		// calculate voxel bounds
		min_pt(0) = static_cast<float>(static_cast<double>(key.x) * this->resolution_ + this->minX_);
		min_pt(1) = static_cast<float>(static_cast<double>(key.y) * this->resolution_ + this->minY_);
		min_pt(2) = static_cast<float>(static_cast<double>(key.z) * this->resolution_ + this->minZ_);
	}

	/** @brief Get the max points of the voxel corresponding the octree key */
	inline void genVoxelMaxPoint(const pcl::octree::OctreeKey &key, Eigen::Vector3f &max_pt) const 
	{
		// calculate voxel bounds
		max_pt(0) = static_cast<float>(static_cast<double>(key.x + 1) * this->resolution_ + this->minX_);
		max_pt(1) = static_cast<float>(static_cast<double>(key.y + 1) * this->resolution_ + this->minY_);
		max_pt(2) = static_cast<float>(static_cast<double>(key.z + 1) * this->resolution_ + this->minZ_);
	}

	/** @brief		Get the center point of the voxel corresponding the octree key
	  * @details	Refer to pcl::octree::OctreePointCloud<PointT, LeafT, BranchT, OctreeT>::genLeafNodeCenterFromOctreeKey(const OctreeKey & key, PointT & point) const
	  *				which only accept PointT point, not pcl::PointXYZ.
	  *				Thus, even if the octree has pcl::PointNormals, the center point should also be pcl::PointXYZ.
	  */
	void genLeafNodeCenterXYZFromOctreeKey(const pcl::octree::OctreeKey &key, pcl::PointXYZ &point) const
	{
		// define point to leaf node voxel center
		point.x = static_cast<float>((static_cast<double>(key.x) + 0.5) * this->resolution_ + this->minX_);
		point.y = static_cast<float>((static_cast<double>(key.y) + 0.5) * this->resolution_ + this->minY_);
		point.z = static_cast<float>((static_cast<double>(key.z) + 0.5) * this->resolution_ + this->minZ_);
	}

	/** @brief		Generate an octree key for point
	  * @details	Refer to pcl::octree::OctreePointCloud<PointT, LeafT, BranchT, OctreeT>::genOctreeKeyforPoint (const PointT& point_arg, OctreeKey & key_arg)
	  *				which only accept PointT point, not pcl::PointXYZ.
	  *				Thus, even if the octree has pcl::PointNormals, the center point should also be pcl::PointXYZ.
	  */
	void genOctreeKeyforPointXYZ(const pcl::PointXYZ &point, pcl::octree::OctreeKey &key) const
	{
		// calculate integer key for point coordinates
		key.x = static_cast<unsigned int> ((point.x - this->minX_) / this->resolution_);
		key.y = static_cast<unsigned int> ((point.y - this->minY_) / this->resolution_);
		key.z = static_cast<unsigned int> ((point.z - this->minZ_) / this->resolution_);
	}

	/** @brief		Get the occupied voxel center points
	  * @details	Refer to pcl::octree::OctreePointCloud<PointT, LeafT, BranchT, OctreeT>::getOccupiedVoxelCentersRecursive(const BranchNode* node_arg, const OctreeKey& key_arg, AlignedPointXYZVector &voxelCenterList_arg) const
	  *				which only accept a vector of PointT points, not pcl::PointXYZ.
	  *				Thus, even if the octree has pcl::PointNormals, the center point should also be pcl::PointXYZ.
	  */
	size_t getOccupiedVoxelCentersRecursive(const BranchNode	*node,
														 const pcl::octree::OctreeKey		&key,
														 PointXYZVList						&voxelCenterPointXYZVector,
														 const bool								fWithoutIsolatedVoxels) const
	{
		// voxel count
		size_t voxelCount = 0;
		
		// iterate over all children
		for(unsigned char childIdx = 0; childIdx < 8; childIdx++)
		{
			if (!this->branchHasChild(*node, childIdx)) continue;
			
			const pcl::octree::OctreeNode *childNode;
			childNode = this->getBranchChildPtr(*node, childIdx);
			
			// generate new key for current branch voxel
			pcl::octree::OctreeKey newKey;
			newKey.x = (key.x << 1) | (!!(childIdx & (1 << 2)));
			newKey.y = (key.y << 1) | (!!(childIdx & (1 << 1)));
			newKey.z = (key.z << 1) | (!!(childIdx & (1 << 0)));
			
			// for each node type
			switch(childNode->getNodeType())
			{
				// if this node is a branch node, go deeper recursively
				case pcl::octree::BRANCH_NODE:
				{
					// recursively proceed with indexed child branch
					voxelCount += getOccupiedVoxelCentersRecursive(static_cast<const BranchNode*>(childNode), newKey, voxelCenterPointXYZVector, fWithoutIsolatedVoxels);
					break;
				}
				
				// if this node is a leaf node, check if it is not isolated and add the center point
				case pcl::octree::LEAF_NODE:
				{
					// if it is an isolated voxel, do not add its center point
					if(fWithoutIsolatedVoxels && !isNotIsolatedVoxel(newKey)) break;

					// calculate the center point and add it to the vector
					pcl::PointXYZ newPoint;
					genLeafNodeCenterXYZFromOctreeKey(newKey, newPoint);
					voxelCenterPointXYZVector.push_back(newPoint);
					voxelCount++;
					break;
				}
				
				default:
					break;
			}
		}
		
		return voxelCount;
	}

	bool isNotIsolatedVoxel(const pcl::octree::OctreeKey &key) const
	{
		// if it is on the boundary
		// TODO: maxKey_
		if(key.x == 0 || key.y == 0 || key.z == 0 ||
			key.x >= maxKey_.x || key.y >= maxKey_.y || key.z >= maxKey_.z) return true;

		// check if the node is surrounded with occupied nodes
		if(!existLeaf(key.x+1, key.y,   key.z  ))		return true;
		if(!existLeaf(key.x-1, key.y,   key.z  ))		return true;
		if(!existLeaf(key.x,   key.y+1, key.z  ))		return true;
		if(!existLeaf(key.x,   key.y-1, key.z  ))		return true;
		if(!existLeaf(key.x,   key.y,   key.z+1))		return true;
		if(!existLeaf(key.x,   key.y,   key.z-1))		return true;

		return false;
	}

	inline bool isCellNotOccupied(const VectorPtr &pMean, const MatrixPtr &pVariance, const size_t ix, const size_t iy, const size_t iz, const float threshold) const
	{
		const size_t row(xyz2row(NUM_CELLS_PER_AXIS_, ix, iy, iz));
		return PLSC((*pMean)(row), (*pVariance)(row, 0)) < threshold;
	}

	inline bool isNotIsolatedCell(const VectorPtr &pMean, const MatrixPtr &pVariance, const size_t ix, const size_t iy, const size_t iz, const float threshold, const bool fRemoveIsolatedCells, size_t &row) const
	{
		// current index
		row = xyz2row(NUM_CELLS_PER_AXIS_, ix, iy, iz);
		
		// check neighboring cells
		if(fRemoveIsolatedCells)
		{
			// last index
			const size_t lastIdx(NUM_CELLS_PER_AXIS_-1);

			if(ix == 0 || iy == 0 || iz == 0 ||
				ix >= lastIdx || iy >= lastIdx || iz >= lastIdx) return true;

			// check if the node is surrounded with occupied nodes
			if(isCellNotOccupied(pMean, pVariance, ix+1, iy,   iz  , threshold))		return true;
			if(isCellNotOccupied(pMean, pVariance, ix-1, iy,   iz  , threshold))		return true;
			if(isCellNotOccupied(pMean, pVariance, ix,   iy+1, iz  , threshold))		return true;
			if(isCellNotOccupied(pMean, pVariance, ix,   iy-1, iz  , threshold))		return true;
			if(isCellNotOccupied(pMean, pVariance, ix,   iy,   iz+1, threshold))		return true;
			if(isCellNotOccupied(pMean, pVariance, ix,   iy,   iz-1, threshold))		return true;
		}

		return isCellNotOccupied(pMean, pVariance, ix, iy, iz, threshold);
	}

	/** @details	The leaf node has only index vector, 
	  *				no information about the point cloud or min/max boundary of the voxel.
	  *				Thus, prediction is done in OctreeGPMap not in LeafNode.
	  *				But the result will be dangled to LeafNode for further BCM update.
	  */
	void predict(const Hyp					&logHyp,
					 const Indices				&indexList, 
					 Eigen::Vector3f			&min_pt,
					 LeafNode					*pLeafNode,
					 const int					maxIter)
	{
		// training data
		DerivativeTrainingData derivativeTrainingData;
		MatrixPtr pX, pXd; VectorPtr pYYd;
		//generateTraingData(input_, indexList, m_sensorPosition, m_gap, pX, pXd, pYYd);
		generateTraingData(input_, indexList, m_gap, pX, pXd, pYYd);
		derivativeTrainingData.set(pX, pXd, pYYd);

		// test data
		TestData testData;
		MatrixPtr pXs(new Matrix(NUM_CELLS_PER_BLOCK_, 3));
		Matrix minValue(1, 3); 
		minValue << min_pt.x(), min_pt.y(), min_pt.z();
		pXs->noalias() = (*m_pXs) + minValue.replicate(NUM_CELLS_PER_BLOCK_, 1);
		testData.set(pXs);

		// train
		//Hyp localLogHyp(logHyp);
		Hyp localLogHyp;
		localLogHyp.mean = logHyp.mean;
		localLogHyp.cov = logHyp.cov;
		localLogHyp.lik = logHyp.lik;

		if(maxIter > 0)
			GPType::train<GP::BOBYQA, GP::NoStopping>(localLogHyp, derivativeTrainingData, maxIter);

		// predict
		try
		{
			// predict
			GPType::predict(localLogHyp, derivativeTrainingData, testData, FLAG_INDEPENDENT_BCM_); // 5000
			//GPType::predict(logHyp, derivativeTrainingData, testData, FLAG_INDEPENDENT_BCM_, pXs->rows());

			// update BCM
			pLeafNode->update(testData.pMu(), testData.pSigma());
		}
		catch(GP::Exception &e)
		{
			std::cerr << e.what() << std::endl;
		}

	}

protected:
	/** @brief		Flag for duplicating a point index to neighboring voxels 
	  * @details	If it is duplicated, prediction will be easy without considering neighboring voxels,
	  *				but the total memory size for indices will be 27 times bigger. */
	const bool		FLAG_DUPLICATE_POINTS_;

	/** @brief		Independent BCM: mean vector and variance vector,
	  *				Dependent BCM: mean vector and covariance matrix
	  */
	const bool		FLAG_INDEPENDENT_BCM_;

	/** @brief Size of each block (voxel) */
	double			&BLOCK_SIZE_;
	const double	CELL_SIZE_;
	
	/** @brief		Number of cells per a block
	  * @details	Note that each block(voxel) has a number of cells.
	  *				The block size corresponds to the resolution of voxels in pcl::octree::OctreePointCloud
	  */
	const size_t	NUM_CELLS_PER_AXIS_;
	const size_t	NUM_CELLS_PER_BLOCK_;


	/** @brief		Minimum number of points to predict signed distances with GPR */
	const size_t	MIN_NUM_POINTS_TO_PREDICT_;

	/** @brief For generating empty points */
	float				m_gap;
	//pcl::PointXYZ	m_sensorPosition;

#ifndef CONST_LEAF_NODE_ITERATOR_
	/** @brief Non empty block center points for training hyperparameters */
	PointXYZVList	m_nonEmptyBlockCenterPointXYZList;
	size_t			m_numRandomBlocks;
#endif

	/** @brief		Test inputs of a block whose minimum point is (0, 0, 0) */
	MatrixPtr	m_pXs;
};
 
}

#endif