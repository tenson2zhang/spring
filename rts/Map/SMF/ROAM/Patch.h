/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef PATCH_H
#define PATCH_H

#include "Rendering/GL/myGL.h"
#include "Game/Camera.h"
#include "System/Rectangle.h"
#include "System/type2.h"

#include <array>
#include <vector>


class CSMFGroundDrawer;
class CCamera;


// how many heightmap pixels a patch consists of
#define PATCH_SIZE 128

// depth of variance tree; should be near SQRT(PATCH_SIZE) + 1
#define VARIANCE_DEPTH (12)

// how many TriTreeNodes should be reserved per pool
// this is a reasonable baseline for *most* maps but
// not guaranteed to suffice under all possible user
// detail levels on every map in existence
#define NEW_POOL_SIZE (1 << 20)
// debug (simulates fast pool exhaustion)
// #define NEW_POOL_SIZE (1 << 2)


// stores the triangle-tree structure, but no coordinates
struct TriTreeNode
{
	TriTreeNode()
		: LeftChild(nullptr)
		, RightChild(nullptr)
		, BaseNeighbor(nullptr)
		, LeftNeighbor(nullptr)
		, RightNeighbor(nullptr)
	{}

	// all non-leaf nodes have both children, so just check for one
	bool IsLeaf() const { assert(RightChild == nullptr); return (LeftChild == nullptr); }
	bool IsBranch() const { assert(LeftChild != nullptr); return (RightChild != nullptr); }

	TriTreeNode* LeftChild;
	TriTreeNode* RightChild;

	TriTreeNode* BaseNeighbor;
	TriTreeNode* LeftNeighbor;
	TriTreeNode* RightNeighbor;
};



// maintains a pool of TriTreeNodes, so we can (re)construct triangle-trees
// without dynamically (de)allocating nodes (note that InitPools() actually
// creates a pool for each worker thread to avoid locking)
class CTriNodePool
{
public:
	static void InitPools(bool shadowPass, size_t newPoolSize = NEW_POOL_SIZE);
	static void ResetAll(bool shadowPass);
	inline static CTriNodePool* GetPool(bool shadowPass);

public:
	CTriNodePool(const size_t poolSize);

	void Reset();
	bool Allocate(TriTreeNode*& left, TriTreeNode*& right);

	bool OutOfNodes() const { return (nextTriNodeIdx >= pool.size()); }

private:
	std::vector<TriTreeNode> pool;

	// index of next free TriTreeNode
	size_t nextTriNodeIdx;
};



// stores information needed at the Patch level
class Patch
{
public:
	enum RenderMode {
		VBO = 1,
		DL  = 2,
		VA  = 3
	};

public:
	friend class CRoamMeshDrawer;
	friend class CPatchInViewChecker;

	Patch();
	~Patch();

	void Init(CSMFGroundDrawer* drawer, int worldX, int worldZ); //FIXME move this into the ctor
	void Reset();

	TriTreeNode* GetBaseLeft()  { return &baseLeft;  }
	TriTreeNode* GetBaseRight() { return &baseRight; }

	bool IsVisible(const CCamera*) const;
	char IsDirty() const { return isDirty; }
	int GetTriCount() const { return (indices.size() / 3); }

	void UpdateHeightMap(const SRectangle& rect = SRectangle(0, 0, PATCH_SIZE, PATCH_SIZE));

	bool Tessellate(const float3& camPos, int viewRadius, bool shadowPass);
	void ComputeVariance();

	void GenerateIndices();
	void Upload();
	void Draw();
	void DrawBorder();
	void SetSquareTexture() const;

public:
	static void SwitchRenderMode(int mode = -1);
	static int GetRenderMode() { return renderMode; }

	#if 0
	void UpdateVisibility(CCamera* cam);
	#endif
	static void UpdateVisibility(CCamera* cam, std::vector<Patch>& patches, const int numPatchesX);

protected:
	void VBOUploadVertices();

private:
	// recursive functions
	bool Split(TriTreeNode* tri);
	void RecursTessellate(TriTreeNode* tri, const int2 left, const int2 right, const int2 apex, const int node);
	void RecursRender(const TriTreeNode* tri, const int2 left, const int2 right, const int2 apex);

	float RecursComputeVariance(
		const   int2 left,
		const   int2 rght,
		const   int2 apex,
		const float3 hgts,
		const    int node
	);

	void RecursBorderRender(
		CVertexArray* va,
		const TriTreeNode* tri,
		const int2 left,
		const int2 rght,
		const int2 apex,
		int depth,
		bool leftChild
	);

	float GetHeight(int2 pos);

	void GenerateBorderIndices(CVertexArray* va);

private:
	static RenderMode renderMode;

	CSMFGroundDrawer* smfGroundDrawer;

	// pool used during Tessellate; each invoked Split allocates from this
	CTriNodePool* curTriPool;

	// which variance we are currently using [only valid during the Tessellate and ComputeVariance passes]
	float* currentVariance;

	// does the variance-tree need to be recalculated for this Patch?
	bool isDirty;
	bool vboVerticesUploaded;

	float varianceMaxLimit;
	float camDistLODFactor; // defines the LOD falloff in camera distance

	// world-coordinate offsets of this patch
	int2 coors;


	TriTreeNode baseLeft;  // left base-triangle tree node
	TriTreeNode baseRight; // right base-triangle tree node

	std::vector<float> varianceLeft;  // left variance tree
	std::vector<float> varianceRight; // right variance tree

	// TODO: remove for both the Displaylist and the VBO implementations (only really needed for VA's)
	std::vector<float> vertices;
	std::vector<unsigned int> indices;

	// frame on which this patch was last visible, per pass
	// NOTE:
	//   shadow-mesh patches are only ever viewed by one camera
	//   normal-mesh patches can be viewed by *multiple* types!
	std::array<unsigned int, CCamera::CAMTYPE_VISCUL> lastDrawFrames;


	GLuint triList;
	GLuint vertexBuffer;
	GLuint vertexIndexBuffer;
};

#endif
