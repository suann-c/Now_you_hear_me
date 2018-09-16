
#include "WalkMesh.hpp"
#include "read_chunk.hpp"
#include <fstream>


WalkMesh::WalkMesh(std::vector<glm::vec3> const &vertices_, std::vector< glm::uvec3 > const &triangles_)
	: vertices(vertices_), triangles(triangles_) {
	//this vertex struct will only have positions and normals
	struct Vertex {
		glm::vec3 position;
		glm::vec3 normal;
	};
	//Make sure vertex is the size expected as declared in struct
	static_assert(sizeof(Vertex) == 3*4+3*4, "Vertex is packed.");
	std::vector< Vertex > rawData;
	std::ifstream inputFile;
	inputFile.open("meshtest.pn", std::ifstream::in);
	//reading inputFile as a .pn
	read_chunk(inputFile, "pn..", &rawData);
	inputFile.close();
	uint32_t len = rawData.size();
	uint32_t loadMap = 0;
	//loop through newly loaded rawData and extract normal and position info out of it
	for (uint32_t i=0; i<len; i++) {
		//fill in vertex position
		glm::vec3 vertPos = rawData[i].position;
		vertices.emplace_back(vertPos);
		//also load each vertex's normals to walk on uneven ground
		glm::vec3 vertNorm = rawData[i].normal;
		vertex_normals.emplace_back(vertNorm);
		loadMap++;
		//load the map and triangle indices in all at once
		if (loadMap == 3) {
			uint32_t vert1Ind = i;
			uint32_t vert2Ind = i-1;
			uint32_t vert3Ind = i-2; //safe b/c won't run til at least 3 indices in vertex list
			triangles.emplace_back(glm::uvec3(vert1Ind, vert2Ind, vert3Ind));
			std::pair<glm::uvec2, uint32_t> pair1 (glm::uvec2(vert1Ind, vert2Ind), vert3Ind);
			next_vertex.insert(pair1);
			std::pair<glm::uvec2, uint32_t> pair2 (glm::uvec2(vert2Ind, vert3Ind), vert1Ind);
			next_vertex.insert(pair2);
			std::pair<glm::uvec2, uint32_t> pair3 (glm::uvec2(vert3Ind, vert1Ind), vert2Ind);
			next_vertex.insert(pair3);
			/*
			glm::uvec2 vert1_2 = glm::uvec2(vert1Ind, vert2Ind);
			glm::uvec2 vert2_3 = glm::uvec2(vert2Ind, vert3Ind);
			glm::uvec2 vert3_1 = glm::uvec2(vert3Ind, vert1Ind);
			next_vertex.insert(std::make_pair<glm::uvec2, uint32_t>(vert1_2, vert3Ind));
			next_vertex.insert(std::make_pair<glm::uvec2, uint32_t>(vert2_3, vert1Ind));
			next_vertex.insert(std::make_pair<glm::uvec2, uint32_t>(vert3_1, vert2Ind));
			*/
			loadMap = 0;
		}
	}
}

//using heron's formula
float WalkMesh::triangleArea(glm::vec3 A, glm::vec3 B, glm::vec3 C) const {
	float a = glm::distance(A, B);
	float b = glm::distance(B, C);
	float c = glm::distance(C, A);
	float semiperim = a+b+c;
	float area = glm::sqrt(semiperim*(semiperim-a)*(semiperim-b)*(semiperim-c));
	return area;
}

glm::vec3 WalkMesh::barycentric(glm::vec3 A, glm::vec3 B, glm::vec3 C, glm::vec3 pt) const {
	float triArea = triangleArea(A, B, C); //total bestTriangle's area
	float CAP = triangleArea(C, A, pt); //area of u's subtriangle
	float ABP = triangleArea(B, A, pt); //area of v's subtriangle
	float BCP = triangleArea(C, B, pt); //area of w's subtriangle
	float u = CAP/triArea;
	float v = ABP/triArea;
	float w = BCP/triArea;
	assert(u+v+w == 1.0f); //sanity check
	assert(u>=0 and v>=0 and w>=0); //make sure inside triangle
	glm::vec3 baryCoords = glm::vec3(u, v, w);
	return baryCoords;
}

WalkMesh::WalkPoint WalkMesh::start(glm::vec3 const &world_point) const {
	WalkPoint closest;
	glm::uvec3 bestTriangle;
	glm::vec3 closestPt;
	for (uint32_t t=0; t<triangles.size(); t++) {
		//POINT IN TRIANGLE TEST
		//for each triangle, find closest point on triangle to world_point
		glm::uvec3 currTriangle = triangles[t]; //use this as an index into vertices
		glm::vec3 currNormal = vertex_normals[t];
		glm::vec3 a = vertices[currTriangle[0]]; //pick a random pt in plane (a triangle vertex)
		glm::vec3 currPt = (world_point + a*glm::cross(currNormal,currNormal))/(glm::dot(currNormal,currNormal) + 1); //q
		//?? BARYCENTRIC-IFY AND SEE IF IT IS IN TRIANGLE. IF NOT THEN FIND CLOSEST ON TRIANGLE EDGE
		//new smallest distance!
		if (glm::distance(currPt, world_point)<glm::distance(closestPt, world_point)) {
			bestTriangle = currTriangle; //use this as an index into vertices
			closestPt = currPt;
		}
	}
	//if point is closest, closest.triangle gets the current triangle, closest.weights gets the barycentric coordinates
	glm::vec3 barycent = barycentric(vertices[bestTriangle[0]], vertices[bestTriangle[1]], vertices[bestTriangle[2]], closestPt);
	closest.triangle = bestTriangle;
	closest.weights = barycent;
	return closest;
}

void WalkMesh::walk(WalkPoint &wp, glm::vec3 const &step) const {
	//wp comes in filled out.
	//project step to barycentric coordinates to get weights_step want in terms of wp.triangle
	glm::uvec3 startTri = wp.triangle;
	glm::vec3 weights_step = barycentric(vertices[startTri[0]], vertices[startTri[1]], vertices[startTri[2]], step);

	float t = 1.0f;
	//keep updating walk 1 triangle at a time until no more time to walk
	while (t>0) {
		glm::uvec3 currTri = wp.triangle;
		//stepping over edge (one of barycentric subtriangles is zero in area)
		glm::vec3 crossEdge = wp.weights+t*weights_step; //has already been scaled by t.
		if (crossEdge[0]<=0 or crossEdge[1]<=0 or crossEdge[2]<=0) {
			//check in xyz coords divide each by weights_step to get which edge hit first (most negative)
			float tX = (-wp.weights[0])/weights_step[0];
			float tY = (-wp.weights[1])/weights_step[1];
			float tZ = (-wp.weights[2])/weights_step[2];
			float tMin1 = std::min(tX, tY);
			float tMin = std::min(tMin1, tZ);
			//figure out which two vertices it crosses and if it crosses a vertex reroute to edge
			uint32_t vertex1; //(?? DRAW A PICTURE)
			uint32_t vertex2;
			if (tMin == tX) {
				vertex1 = currTri[0];
				vertex2 = currTri[1];
			}
			else if (tMin == tY) {
				vertex1 = currTri[1];
				vertex2 = currTri[2];
			}
			else { //tMin == tZ
				vertex1 = currTri[2];
				vertex2 = currTri[0];
			}
			//wp.weights gets moved to triangle edge (barycentric calculation), and step gets reduced
			wp.weights = barycentric(vertices[currTri[0]], vertices[currTri[1]], vertices[currTri[2]], wp.weights*tMin);
			//if there is another triangle over the edge: (input vertices in opposite order)
			if (next_vertex.find(glm::uvec2(vertex2, vertex1)) != next_vertex.end()) {
				auto itr = next_vertex.find(glm::uvec2(vertex2, vertex1));
				uint32_t newTriVertex = itr->second;
				//wp.triangle gets updated to adjacent triangle (put in terms of barycentric for new triangle)
				glm::vec3 oldNormal = vertex_normals[wp.triangle[0]];
				for (uint32_t i=0; i<triangles.size(); i++) {
					glm::uvec3 aTriangle = triangles[i];
					bool x = false;
					bool y = false;
					bool z = false;
					for (uint32_t j=0; j<3; j++) { //loop through subtriangle
						if (vertices[aTriangle[0]]==vertices[vertex1] or 
							vertices[aTriangle[0]]==vertices[vertex2] or
							vertices[aTriangle[0]]==vertices[newTriVertex]) {
							x = true;
						}
						else if (vertices[aTriangle[1]]==vertices[vertex1] or 
							vertices[aTriangle[1]]==vertices[vertex2] or
							vertices[aTriangle[1]]==vertices[newTriVertex]) {
							y = true;
						}
						else {
							z = true;
						}
					}
					if (x and y and z) {
						wp.triangle = aTriangle;
						break;
					}
				}
				//step gets rotated over the edge (use rotation matrix from two triangles normals)
				//algorithm for rotation matrices from:
				//https://math.stackexchange.com/questions/180418/calculate-rotation-matrix-to-align-vector-a-to-vector-b-in-3d
				glm::vec3 newNormal = vertex_normals[wp.triangle[0]];
				glm::vec3 v = glm::cross(oldNormal, newNormal);
				//float s = std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]); //angle sine is norm
				float c = glm::dot(oldNormal, newNormal); //angle cosine
				glm::mat3 vx = glm::mat3(0, -v[2], v[1], v[2], 0, -v[0], -v[1], v[0], 0);
				glm::mat3 I = glm::mat3(1, 0, 0, 0, 1, 0, 0, 0, 1);
				glm::mat3 rotate = I+vx+(1/(1+c));
				weights_step = rotate*weights_step;
			}
			//else if there is no other triangle over the edge:
			else {
				//wp.triangle stays the same.
				//step gets updated to slide along the edge
				//??
				wp.weights = wp.weights+weights_step[0];
			}
			//case on tMin positive / negative if zero should be on next triangle 
			//?? CHEK
			t = t+tMin;
			std::cout<<"t is "<<t<<std::endl;
		}
		else {
			//wp.weights gets moved by weights_step, nothing else needs to be done.
			wp.weights += weights_step;
			t = 0.0f;
		}
	}

/*
	if (t >= 1.0f) { //if a triangle edge is not crossed
		//wp.weights gets moved by weights_step, nothing else needs to be done.
	}
	else { //if a triangle edge is crossed
		//wp.weights gets moved to triangle edge, and step gets reduced
		//if there is another triangle over the edge:
			//wp.triangle gets updated to adjacent triangle
			//step gets rotated over the edge
		//else if there is no other triangle over the edge:
			//wp.triangle stays the same.
			//step gets updated to slide along the edge
	}
*/
}
