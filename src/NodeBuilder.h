#pragma

#include <glm/mat4x4.hpp>

#include <CesiumGltf/Node.h>
#include <CesiumGltf/Mesh.h>

namespace CesiumGltf {
	struct Model;
}

namespace osg {
	class Node;
	class MatrixTransform;
	class Group;
	class Geometry;
	class StateSet;
	class Material;
	class Texture2D;
}

namespace czmosg
{

	class NodeBuilder {
	public:
		NodeBuilder(CesiumGltf::Model* model, const glm::dmat4& transform);

		osg::Node* build();

	private:
		osg::Node* createNode(const CesiumGltf::Node& node);

		osg::Node* createMesh(const CesiumGltf::Mesh& mesh);

	private:
		CesiumGltf::Model* m_model;
		glm::dmat4 m_transform;
	};

}	// namespace czmosg