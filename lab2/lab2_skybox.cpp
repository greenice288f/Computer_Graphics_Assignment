#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <render/shader.h>
#include <random>
#include <ctime> // Add this for time()
#include <fstream>   // For file streams like std::ifstream
#include <sstream>   // For std::istringstream
#include <vector>    // For std::vector
#include <string>    // For std::string
#include <glm/vec3.hpp> // For glm::vec3
#include <tiny_gltf.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#define _USE_MATH_DEFINES
#include <math.h>

static GLFWwindow *window;
static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);

// OpenGL camera view parameters
static glm::vec3 eye_center;
static glm::vec3 lookat(0, 0, 0);
static glm::vec3 up(0, 1, 0);
static glm::vec3 lastLookat = lookat; // Store the last known lookat value

// View control
static float viewAzimuth = 0.f;
static float viewPolar = 0.f;
static float viewDistance = 3000.0f;
void loadOBJfromTinkerCad(const char* filepath, std::vector<GLfloat>& vertices,
	std::vector<GLfloat>& uvs, std::vector<GLuint>& indices) {
	std::ifstream file(filepath);
	if (!file.is_open()) {
		std::cerr << "Error: Cannot open OBJ file " << filepath << std::endl;
		return;
	}

	std::string line;
	std::vector<glm::vec3> temp_vertices;
	std::vector<glm::vec2> temp_uvs;
	bool hasUVs = false;

	while (std::getline(file, line)) {

		std::istringstream ss(line);
		std::string prefix;
		ss >> prefix;

		if (prefix == "v") { // Vertex
			glm::vec3 vertex;
			ss >> vertex.x >> vertex.y >> vertex.z;
			temp_vertices.push_back(vertex);
		}
		else if (prefix == "vt") { // Texture coordinate
			glm::vec2 uv;
			ss >> uv.x >> uv.y;
			temp_uvs.push_back(uv);
			hasUVs = true; // Set the flag since we found a vt line
		}
		else if (prefix == "f") { // Face
			GLuint vertexIndex[3], uvIndex[3];
			char separator;
			if (hasUVs) {
				for (int i = 0; i < 3; ++i) {
					ss >> vertexIndex[i] >> separator >> uvIndex[i];
					indices.push_back(vertexIndex[i] - 1);
				}

				for (int i = 0; i < 3; ++i)
				{
					uvs.push_back(temp_uvs[uvIndex[i] - 1].x);
					uvs.push_back(temp_uvs[uvIndex[i] - 1].y);
				}
			}
			else {
				for (int i = 0; i < 3; ++i) {
					ss >> vertexIndex[i];
					indices.push_back(vertexIndex[i] - 1);
				}
				for (int i = 0; i < 3; i++) {
					uvs.push_back(0.0f);
					uvs.push_back(0.0f);
				}
			}
		}
	}

	for (const auto& v : temp_vertices) {
		vertices.push_back(v.x);
		vertices.push_back(v.y);
		vertices.push_back(v.z);
	}
}

static GLuint LoadTextureTileBox(const char *texture_file_path) {
    int w, h, channels;
    uint8_t* img = stbi_load(texture_file_path, &w, &h, &channels, 3);
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

	// Set texture wrapping parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// Set texture filtering parameters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    if (img) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, img);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cout << "Failed to load texture " << texture_file_path << std::endl;
    }
    stbi_image_free(img);

    return texture;
}

struct Island {
    glm::vec3 position;
    glm::vec3 scale;
    char* texture;
	std::vector<GLfloat> vertices;
	std::vector<GLfloat> uvs;
	std::vector<GLuint> indices;
    std::vector<GLfloat> colors; // Added color vector

    // OpenGL Buffers
    GLuint vertexArrayID;
    GLuint vertexBufferID;
	GLuint uvBufferID;
    GLuint indexBufferID;
	GLuint colorBufferID; // Added color buffer
    GLuint textureID;

    // Shader Variable IDs
    GLuint mvpMatrixID;
    GLuint textureSamplerID;
    GLuint programID;

    void initialize(glm::vec3 position, glm::vec3 scale, char* texturePath, const char* objPath) {
        this->position = position;
        this->scale = scale;
        this->texture = texturePath;
		loadOBJfromTinkerCad(objPath, vertices, uvs, indices);

		colors.resize(vertices.size());
		// Generate brownish colors with different lighting
		for (size_t i = 0; i < vertices.size() / 3; ++i) {
			float intensity = 0.5f + 0.5f * (static_cast<float>(i) / static_cast<float>(vertices.size() / 3)); // Vary intensity from 0.5 to 1.0
			float red = 0.6f * intensity;   // Base red with intensity adjustment
			float green = 0.4f * intensity; // Base green with intensity adjustment
			float blue = 0.2f * intensity;  // Base blue with intensity adjustment (kept low for brown tone)

			colors[i * 3] = red;   // Red
			colors[i * 3 + 1] = green; // Green
			colors[i * 3 + 2] = blue;  // Blue
		}



        // Create vertex array object
        glGenVertexArrays(1, &vertexArrayID);
        glBindVertexArray(vertexArrayID);

        // Create vertex buffer object
        glGenBuffers(1, &vertexBufferID);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);

		// Create color buffer object
		glGenBuffers(1, &colorBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(GLfloat), colors.data(), GL_STATIC_DRAW);

		// Create uv buffer object
		glGenBuffers(1, &uvBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(GLfloat), uvs.data(), GL_STATIC_DRAW);

        // Create index buffer object
        glGenBuffers(1, &indexBufferID);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
		
        // Load Texture
        textureID = LoadTextureTileBox(texturePath);


        // Load Shaders
        programID = LoadShadersFromFile("../../../lab2/shaders/island.vert", "../../../lab2/shaders/island.frag"); // Assuming you have these shaders
        mvpMatrixID = glGetUniformLocation(programID, "MVP");
        textureSamplerID = glGetUniformLocation(programID, "textureSampler");
    }

    void render(glm::mat4 cameraMatrix) {
        glUseProgram(programID);

        // Vertex attribute
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
		
		// Color attribute
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
		
		// UV attribute
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);


        // Index buffer
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);


        // Model matrix
        glm::mat4 modelMatrix = glm::mat4(1.0f);
        modelMatrix = glm::translate(modelMatrix, position);
        modelMatrix = glm::scale(modelMatrix, scale);

        // MVP Matrix
        glm::mat4 mvp = cameraMatrix * modelMatrix;
        glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

		// Texture
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textureID);
		glUniform1i(textureSamplerID, 0);

        // Draw
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
		
        glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);
    }

    void cleanup() {
        glDeleteBuffers(1, &vertexBufferID);
		glDeleteBuffers(1, &uvBufferID);
        glDeleteBuffers(1, &indexBufferID);
		glDeleteBuffers(1, &colorBufferID);
        glDeleteVertexArrays(1, &vertexArrayID);
        glDeleteTextures(1, &textureID);
        glDeleteProgram(programID);
    }
};
struct Cloud {
	glm::vec3 position;
	glm::vec3 scale;
	char* texture;
	std::vector<GLfloat> vertices;
	std::vector<GLfloat> uvs;
	std::vector<GLuint> indices;
	std::vector<GLfloat> colors; // Added color vector

	// OpenGL Buffers
	GLuint vertexArrayID;
	GLuint vertexBufferID;
	GLuint uvBufferID;
	GLuint indexBufferID;
	GLuint colorBufferID; // Added color buffer
	GLuint textureID;

	// Shader Variable IDs
	GLuint mvpMatrixID;
	GLuint textureSamplerID;
	GLuint programID;

	void initialize(glm::vec3 position, glm::vec3 scale, char* texturePath, const char* objPath) {
		this->position = position;
		this->scale = scale;
		this->texture = texturePath;
		loadOBJfromTinkerCad(objPath, vertices, uvs, indices);

		colors.resize(vertices.size());
		// Generate colors based on vertex index
		for (size_t i = 0; i < vertices.size() / 3; ++i) {
			colors[i * 3] = 0.8f + 0.2f * (static_cast<float>(i) / static_cast<float>(vertices.size() / 3)); // Red
			colors[i * 3 + 1] = 0.8f + 0.2f * (static_cast<float>(i * i) / static_cast<float>((vertices.size() / 3) * (vertices.size() / 3))); // Green
			colors[i * 3 + 2] = 0.8f + 0.2f * (static_cast<float>(i * i * i) / static_cast<float>((vertices.size() / 3) * (vertices.size() / 3) * (vertices.size() / 3))); // Blue

		}

		// Create vertex array object
		glGenVertexArrays(1, &vertexArrayID);
		glBindVertexArray(vertexArrayID);

		// Create vertex buffer object
		glGenBuffers(1, &vertexBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);

		// Create color buffer object
		glGenBuffers(1, &colorBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(GLfloat), colors.data(), GL_STATIC_DRAW);

		// Create uv buffer object
		glGenBuffers(1, &uvBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(GLfloat), uvs.data(), GL_STATIC_DRAW);

		// Create index buffer object
		glGenBuffers(1, &indexBufferID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

		// Load Texture
		textureID = LoadTextureTileBox(texturePath);


		// Load Shaders
		programID = LoadShadersFromFile("../../../lab2/shaders/island.vert", "../../../lab2/shaders/island.frag"); // Assuming you have these shaders
		mvpMatrixID = glGetUniformLocation(programID, "MVP");
		textureSamplerID = glGetUniformLocation(programID, "textureSampler");
	}

	void render(glm::mat4 cameraMatrix) {
		glUseProgram(programID);

		// Vertex attribute
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

		// Color attribute
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

		// UV attribute
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);


		// Index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);


		// Model matrix
		glm::mat4 modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::translate(modelMatrix, position);
		modelMatrix = glm::scale(modelMatrix, scale);

		// MVP Matrix
		glm::mat4 mvp = cameraMatrix * modelMatrix;
		glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

		// Texture
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textureID);
		glUniform1i(textureSamplerID, 0);

		// Draw
		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);
	}

	void cleanup() {
		glDeleteBuffers(1, &vertexBufferID);
		glDeleteBuffers(1, &uvBufferID);
		glDeleteBuffers(1, &indexBufferID);
		glDeleteBuffers(1, &colorBufferID);
		glDeleteVertexArrays(1, &vertexArrayID);
		glDeleteTextures(1, &textureID);
		glDeleteProgram(programID);
	}
};
struct skyBox {
	glm::vec3 position;		// Position of the box
	glm::vec3 scale;		// Size of the box in each axis
	char*  texture;
	int height;
	GLfloat vertex_buffer_data[72] = {
		-1.0f, 1.0f, 1.0f,
		 1.0f, 1.0f, 1.0f,
		 1.0f, -1.0f, 1.0f,
		-1.0f, -1.0f, 1.0f,

		 1.0f, 1.0f, -1.0f,
		-1.0f, 1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, 1.0f, -1.0f,
		-1.0f, 1.0f, 1.0f,
		-1.0f, -1.0f, 1.0f,
		-1.0f, -1.0f, -1.0f,

		 1.0f, 1.0f, 1.0f,
		 1.0f, 1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, 1.0f,

		-1.0f, 1.0f, -1.0f,
		 1.0f, 1.0f, -1.0f,
		 1.0f, 1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,

		-1.0f, -1.0f, 1.0f,
		 1.0f, -1.0f, 1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f
	};



	GLfloat color_buffer_data[72] =

	{
		// Front, red
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,

		// Back, yellow
		1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,

		// Left, green
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,

		// Right, cyan
		0.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f,

		// Top, blue
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,

		// Bottom, magenta
		1.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 1.0f,
	};

	GLuint index_buffer_data[36] = {		// 12 triangle faces of a box
		0, 1, 2,
		0, 2, 3,

		4, 5, 6,
		4, 6, 7,

		8, 9, 10,
		8, 10, 11,

		12, 13, 14,
		12, 14, 15,

		16, 17, 18,
		16, 18, 19,

		20, 21, 22,
		20, 22, 23,
	};

    // TODO: Define UV buffer data
    // ---------------------------
    // ---------------------------

    GLfloat uv_buffer_data[48] = {
        // Front face (+Z)
        0.25f, 0.33f, // Bottom-left
        0.50f, 0.33f, // Bottom-right
        0.50f, 0.66f, // Top-right
        0.25f, 0.66f, // Top-left

        // Back face (-Z)
        0.75f, 0.33f, // Bottom-left
        1.0f, 0.33f,  // Bottom-right
        1.0f, 0.66f,  // Top-right
        0.75f, 0.66f, // Top-left

        // Left face (-X)
        0.00f, 0.33f, // Bottom-left
        0.25f, 0.33f, // Bottom-right
        0.25f, 0.66f, // Top-right
        0.00f, 0.66f, // Top-left

        // Right face (+X)
        0.50f, 0.33f, // Bottom-left
        0.75f, 0.33f, // Bottom-right
        0.75f, 0.66f, // Top-right
        0.50f, 0.66f, // Top-left

        // Top face (+Y)
        0.25f, 0.00f, // Bottom-left
        0.50f, 0.00f, // Bottom-right
        0.50f, 0.30f, // Top-right
        0.25f, 0.30f, // Top-left

        // Bottom face (-Y)
        0.25f, 0.68f, // Bottom-left
        0.50f, 0.68f, // Bottom-right
        0.50f, 1.0f,  // Top-right
        0.25f, 1.0f   // Top-left
    };


	// OpenGL buffers
	GLuint vertexArrayID;
	GLuint vertexBufferID;
	GLuint indexBufferID;
	GLuint colorBufferID;
	GLuint uvBufferID;
	GLuint textureID;

	// Shader variable IDs
	GLuint mvpMatrixID;
	GLuint textureSamplerID;
	GLuint programID;

	void initialize(glm::vec3 position, glm::vec3 scale, char* texture,int height) {
		// Define scale of the building geometry
		this->position = position;
		this->scale = scale;
		this->texture=texture;
		this->height = height;
		for (int i = 0; i < 72; ++i) color_buffer_data[i] = 1.0f;

		// Create a vertex array object
		glGenVertexArrays(1, &vertexArrayID);
		glBindVertexArray(vertexArrayID);

		// Create a vertex buffer object to store the vertex data
		glGenBuffers(1, &vertexBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

		// Create a vertex buffer object to store the color data
        // TODO:
		glGenBuffers(1, &colorBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(color_buffer_data), color_buffer_data, GL_STATIC_DRAW);

		// TODO: Create a vertex buffer object to store the UV data
		// --------------------------------------------------------
        // --------------------------------------------------------
		for (int i = 0; i < 24; ++i) uv_buffer_data[2*i+1] *= height; //In this loop, the texture coordinates at odd indices (xyz --> y) are scaled by a factor of 5.

		glGenBuffers(1, &uvBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(uv_buffer_data), uv_buffer_data,
GL_STATIC_DRAW);
		// Create an index buffer object to store the index data that defines triangle faces
		glGenBuffers(1, &indexBufferID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buffer_data), index_buffer_data, GL_STATIC_DRAW);

		// Create and compile our GLSL program from the shaders
		programID = LoadShadersFromFile("../../../lab2/shaders/skybox.vert", "../../../lab2/shaders/skybox.frag");
		if (programID == 0)
		{
			std::cerr << "Failed to load shaders." << std::endl;
		}

		// Get a handle for our "MVP" uniform
		mvpMatrixID = glGetUniformLocation(programID, "MVP");

        // TODO: Load a texture
        // --------------------
        // --------------------
		textureID = LoadTextureTileBox(texture);

        // TODO: Get a handle to texture sampler
        // -------------------------------------
        // -------------------------------------
		textureSamplerID  = glGetUniformLocation(programID,"textureSampler");

	}

	void render(glm::mat4 cameraMatrix) {
		glUseProgram(programID);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);



		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);

		// TODO: Model transform
		// -----------------------

		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glm::mat4 modelMatrix = glm::mat4();
        // Scale the box along each axis to make it look like a building
        modelMatrix = glm::scale(modelMatrix, scale);
		modelMatrix = glm::translate(modelMatrix, position);

        // -----------------------

		// Set model-view-projection matrix
		glm::mat4 mvp = cameraMatrix * modelMatrix;
		glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

		// TODO: Enable UV buffer and texture sampler
		// ------------------------------------------
        // ------------------------------------------

		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textureID);
		glUniform1i(textureSamplerID, 0);

		// Draw the box
		glDrawElements(
			GL_TRIANGLES,      // mode
			36,    			   // number of indices
			GL_UNSIGNED_INT,   // type
			(void*)0           // element array buffer offset
		);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
        //glDisableVertexAttribArray(2);
	}

	void cleanup() {
		glDeleteBuffers(1, &vertexBufferID);
		glDeleteBuffers(1, &colorBufferID);
		glDeleteBuffers(1, &indexBufferID);
		glDeleteVertexArrays(1, &vertexArrayID);
		//glDeleteBuffers(1, &uvBufferID);
		//glDeleteTextures(1, &textureID);
		glDeleteProgram(programID);
	}
};

struct Surface {
	glm::vec3 position;
	glm::vec3 scale;
	char* texture;
	std::vector<GLfloat> vertices;
	std::vector<GLfloat> uvs;
	std::vector<GLuint> indices;
	std::vector<GLfloat> colors; // Added color vector

	// OpenGL Buffers
	GLuint vertexArrayID;
	GLuint vertexBufferID;
	GLuint uvBufferID;
	GLuint indexBufferID;
	GLuint colorBufferID; // Added color buffer
	GLuint textureID;

	// Shader Variable IDs
	GLuint mvpMatrixID;
	GLuint textureSamplerID;
	GLuint programID;

	void initialize(glm::vec3 position, glm::vec3 scale, char* texturePath, const char* objPath) {
		this->position = position;
		this->scale = scale;
		this->texture = texturePath;
		loadOBJfromTinkerCad(objPath, vertices, uvs, indices);

		colors.resize(vertices.size());
		// Generate colors based on vertex index
		for (size_t i = 0; i < vertices.size() / 3; ++i) {
			colors[i * 3] = 0.4f + 0.2f * (static_cast<float>(i) / static_cast<float>(vertices.size() / 3)); // Red (lower base value)
			colors[i * 3 + 1] = 0.8f + 0.2f * (static_cast<float>(i * i) / static_cast<float>((vertices.size() / 3) * (vertices.size() / 3))); // Green (higher base value, prioritized)
			colors[i * 3 + 2] = 0.3f + 0.1f * (static_cast<float>(i * i * i) / static_cast<float>((vertices.size() / 3) * (vertices.size() / 3) * (vertices.size() / 3))); // Blue (minimal)
		}


		// Create vertex array object
		glGenVertexArrays(1, &vertexArrayID);
		glBindVertexArray(vertexArrayID);

		// Create vertex buffer object
		glGenBuffers(1, &vertexBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);

		// Create color buffer object
		glGenBuffers(1, &colorBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glBufferData(GL_ARRAY_BUFFER, colors.size() * sizeof(GLfloat), colors.data(), GL_STATIC_DRAW);

		// Create uv buffer object
		glGenBuffers(1, &uvBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(GLfloat), uvs.data(), GL_STATIC_DRAW);

		// Create index buffer object
		glGenBuffers(1, &indexBufferID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

		// Load Texture
		textureID = LoadTextureTileBox(texturePath);


		// Load Shaders
		programID = LoadShadersFromFile("../../../lab2/shaders/island.vert", "../../../lab2/shaders/island.frag"); // Assuming you have these shaders
		mvpMatrixID = glGetUniformLocation(programID, "MVP");
		textureSamplerID = glGetUniformLocation(programID, "textureSampler");
	}

	void render(glm::mat4 cameraMatrix) {
		glUseProgram(programID);

		// Vertex attribute
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

		// Color attribute
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

		// UV attribute
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);


		// Index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);


		// Model matrix
		glm::mat4 modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::translate(modelMatrix, position);
		modelMatrix = glm::scale(modelMatrix, scale);

		// MVP Matrix
		glm::mat4 mvp = cameraMatrix * modelMatrix;
		glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

		// Texture
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textureID);
		glUniform1i(textureSamplerID, 0);

		// Draw
		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);
	}

	void cleanup() {
		glDeleteBuffers(1, &vertexBufferID);
		glDeleteBuffers(1, &uvBufferID);
		glDeleteBuffers(1, &indexBufferID);
		glDeleteBuffers(1, &colorBufferID);
		glDeleteVertexArrays(1, &vertexArrayID);
		glDeleteTextures(1, &textureID);
		glDeleteProgram(programID);
	}
};
struct Building {
	glm::vec3 position;		// Position of the box 
	glm::vec3 scale;		// Size of the box in each axis
	char* texture;
	int height;
	GLfloat vertex_buffer_data[72] = {	// Vertex definition for a canonical box
		// Front face
		-1.0f, -1.0f, 1.0f,
		1.0f, -1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,

		// Back face 
		1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f, 1.0f, -1.0f,
		1.0f, 1.0f, -1.0f,

		// Left face
		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f, 1.0f,
		-1.0f, 1.0f, 1.0f,
		-1.0f, 1.0f, -1.0f,

		// Right face 
		1.0f, -1.0f, 1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f, 1.0f, -1.0f,
		1.0f, 1.0f, 1.0f,

		// Top face
		-1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, 1.0f,
		1.0f, 1.0f, -1.0f,
		-1.0f, 1.0f, -1.0f,

		// Bottom face
		-1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, -1.0f,
		1.0f, -1.0f, 1.0f,
		-1.0f, -1.0f, 1.0f,
	};


	GLfloat color_buffer_data[72] =

	{
		// Front, red
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,
		1.0f, 0.0f, 0.0f,

		// Back, yellow
		1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,
		1.0f, 1.0f, 0.0f,

		// Left, green
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,
		0.0f, 1.0f, 0.0f,

		// Right, cyan
		0.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f,
		0.0f, 1.0f, 1.0f,

		// Top, blue
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,
		0.0f, 0.0f, 1.0f,

		// Bottom, magenta
		1.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 1.0f,
		1.0f, 0.0f, 1.0f,
	};

	GLuint index_buffer_data[36] = {		// 12 triangle faces of a box
		0, 1, 2,
		0, 2, 3,

		4, 5, 6,
		4, 6, 7,

		8, 9, 10,
		8, 10, 11,

		12, 13, 14,
		12, 14, 15,

		16, 17, 18,
		16, 18, 19,

		20, 21, 22,
		20, 22, 23,
	};

	// TODO: Define UV buffer data
	// ---------------------------
	// ---------------------------
	GLfloat uv_buffer_data[48] = {
		// Front
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,
		// Back
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,

		// Left
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,

		// Right
		0.0f, 1.0f,
		1.0f, 1.0f,
		1.0f, 0.0f,
		0.0f, 0.0f,

		// Top - we do not want texture the top
		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,

		// Bottom - we do not want texture the bottom
		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,
		0.0f, 0.0f,
	};
	// OpenGL buffers
	GLuint vertexArrayID;
	GLuint vertexBufferID;
	GLuint indexBufferID;
	GLuint colorBufferID;
	GLuint uvBufferID;
	GLuint textureID;

	// Shader variable IDs
	GLuint mvpMatrixID;
	GLuint textureSamplerID;
	GLuint programID;

	void initialize(glm::vec3 position, glm::vec3 scale, char* texture, int height) {
		// Define scale of the building geometry
		this->position = position;
		this->scale = scale;
		this->texture = texture;
		this->height = height;
		for (int i = 0; i < 72; ++i) color_buffer_data[i] = 1.0f;

		// Create a vertex array object
		glGenVertexArrays(1, &vertexArrayID);
		glBindVertexArray(vertexArrayID);

		// Create a vertex buffer object to store the vertex data		
		glGenBuffers(1, &vertexBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(vertex_buffer_data), vertex_buffer_data, GL_STATIC_DRAW);

		// Create a vertex buffer object to store the color data
		// TODO: 
		glGenBuffers(1, &colorBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(color_buffer_data), color_buffer_data, GL_STATIC_DRAW);

		// TODO: Create a vertex buffer object to store the UV data
		// --------------------------------------------------------
		// --------------------------------------------------------

		glGenBuffers(1, &uvBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glBufferData(GL_ARRAY_BUFFER, sizeof(uv_buffer_data), uv_buffer_data,
			GL_STATIC_DRAW);
		// Create an index buffer object to store the index data that defines triangle faces
		glGenBuffers(1, &indexBufferID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(index_buffer_data), index_buffer_data, GL_STATIC_DRAW);

		// Create and compile our GLSL program from the shaders
		programID = LoadShadersFromFile("../../../lab2/shaders/box.vert", "../../../lab2/shaders/box.frag");
		if (programID == 0)
		{
			std::cerr << "Failed to load shaders." << std::endl;
		}
		mvpMatrixID = glGetUniformLocation(programID, "MVP");
		textureID = LoadTextureTileBox(texture);
		textureSamplerID = glGetUniformLocation(programID, "textureSampler");
		// Bind the texture
		glBindTexture(GL_TEXTURE_2D, textureID);

		// Set texture wrapping parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // Wrap texture horizontally (U axis)
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // Wrap texture vertically (V axis)

		// Set texture filtering parameters
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // Minification filter
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // Magnification filter

		// Unbind the texture
		// Assuming you have a valid OpenGL context and a texture ID
		GLuint textureID;
		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_2D, textureID);

		// Set texture wrapping parameters to remove seams
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		// Set texture filtering parameters to ensure smooth transitions
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		// Generate mipmaps for the texture
		glGenerateMipmap(GL_TEXTURE_2D);

		// Unbind the texture
		glBindTexture(GL_TEXTURE_2D, 0);
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	void render(glm::mat4 cameraMatrix) {
		glUseProgram(programID);

		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, colorBufferID);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);



		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);

		// TODO: Model transform 
		// -----------------------

		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);
		glm::mat4 modelMatrix = glm::mat4();
		// Scale the box along each axis to make it look like a building
		modelMatrix = glm::translate(modelMatrix, position);
		modelMatrix = glm::scale(modelMatrix, scale);

		// -----------------------

		// Set model-view-projection matrix
		glm::mat4 mvp = cameraMatrix * modelMatrix;
		glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

		// TODO: Enable UV buffer and texture sampler
		// ------------------------------------------
		// ------------------------------------------

		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textureID);
		glUniform1i(textureSamplerID, 0);

		// Draw the box
		glDrawElements(
			GL_TRIANGLES,      // mode
			36,    			   // number of indices
			GL_UNSIGNED_INT,   // type
			(void*)0           // element array buffer offset
		);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		//glDisableVertexAttribArray(2);
	}

	void cleanup() {
		glDeleteBuffers(1, &vertexBufferID);
		glDeleteBuffers(1, &colorBufferID);
		glDeleteBuffers(1, &indexBufferID);
		glDeleteVertexArrays(1, &vertexArrayID);
		//glDeleteBuffers(1, &uvBufferID);
		//glDeleteTextures(1, &textureID);
		glDeleteProgram(programID);
	}
};
void loadOBJWithNormals(const char* filepath, std::vector<GLfloat>& vertices,
	std::vector<GLfloat>& uvs, std::vector<GLfloat>& normals,
	std::vector<GLuint>& indices) {
	std::ifstream file(filepath);
	if (!file.is_open()) {
		std::cerr << "Error: Cannot open OBJ file " << filepath << std::endl;
		return;
	}

	std::vector<glm::vec3> temp_vertices;
	std::vector<glm::vec2> temp_uvs;
	std::vector<glm::vec3> temp_normals;

	std::string line;
	while (std::getline(file, line)) {
		std::istringstream ss(line);
		std::string prefix;
		ss >> prefix;

		if (prefix == "v") {
			glm::vec3 vertex;
			ss >> vertex.x >> vertex.y >> vertex.z;
			temp_vertices.push_back(vertex);
		}
		else if (prefix == "vt") {
			glm::vec2 uv;
			ss >> uv.x >> uv.y;
			temp_uvs.push_back(uv);
		}
		else if (prefix == "vn") {
			glm::vec3 normal;
			ss >> normal.x >> normal.y >> normal.z;
			temp_normals.push_back(normal);
		}
		else if (prefix == "f") {
			GLuint vertexIndex[3], uvIndex[3], normalIndex[3];
			char separator;

			for (int i = 0; i < 3; ++i) {
				ss >> vertexIndex[i] >> separator >> uvIndex[i] >> separator >> normalIndex[i];
				indices.push_back(vertexIndex[i] - 1);

				glm::vec3 vertex = temp_vertices[vertexIndex[i] - 1];
				glm::vec2 uv = temp_uvs[uvIndex[i] - 1];
				glm::vec3 normal = temp_normals[normalIndex[i] - 1];

				vertices.push_back(vertex.x);
				vertices.push_back(vertex.y);
				vertices.push_back(vertex.z);

				uvs.push_back(uv.x);
				uvs.push_back(uv.y);

				normals.push_back(normal.x);
				normals.push_back(normal.y);
				normals.push_back(normal.z);
			}
		}
	}
}
class Model {
public:
	glm::vec3 position;
	glm::vec3 scale;
	std::string objPath;
	std::string mtlPath;

	std::vector<GLfloat> vertices;
	std::vector<GLfloat> uvs;
	std::vector<GLfloat> normals;
	std::vector<GLuint> indices;

	// OpenGL Buffers
	GLuint vertexArrayID;
	GLuint vertexBufferID;
	GLuint uvBufferID;
	GLuint normalBufferID;
	GLuint indexBufferID;

	// Shader Variable IDs
	GLuint mvpMatrixID;
	GLuint textureSamplerID;
	GLuint programID;

	std::map<std::string, GLuint> textureMap;

	Model() {}
	void loadOBJWithMTL(const char* objFile, const char* mtlFile, std::vector<GLfloat>& vertices, std::vector<GLfloat>& uvs,
		std::vector<GLfloat>& normals, std::vector<GLuint>& indices, std::map<std::string, GLuint>& textureMap) {
		std::ifstream objStream(objFile);
		if (!objStream.is_open()) {
			std::cerr << "Error: Cannot open OBJ file " << objFile << std::endl;
			return;
		}

		std::ifstream mtlStream(mtlFile);
		if (!mtlStream.is_open()) {
			std::cerr << "Error: Cannot open MTL file " << mtlFile << std::endl;
			return;
		}


		std::map<std::string, std::string> materials;
		std::string currentMaterial;
		std::string line;

		//Read the MTL file
		while (std::getline(mtlStream, line))
		{
			std::istringstream ss(line);
			std::string prefix;
			ss >> prefix;

			if (prefix == "newmtl")
			{
				ss >> currentMaterial;
			}
			else if (prefix == "map_Kd")
			{
				std::string texturePath;
				ss >> texturePath;
				materials[currentMaterial] = texturePath;
			}
		}
		std::map<std::string, GLuint> loadedTextures; // Texture cache

		std::vector<glm::vec3> temp_vertices;
		std::vector<glm::vec2> temp_uvs;
		std::vector<glm::vec3> temp_normals;
		std::map<std::string, int> materialMap; // Map material name to start index for faces

		// Parse obj file
		while (std::getline(objStream, line)) {
			std::istringstream ss(line);
			std::string prefix;
			ss >> prefix;

			if (prefix == "v") {
				glm::vec3 vertex;
				ss >> vertex.x >> vertex.y >> vertex.z;
				temp_vertices.push_back(vertex);
			}
			else if (prefix == "vt") {
				glm::vec2 uv;
				ss >> uv.x >> uv.y;
				temp_uvs.push_back(uv);
			}
			else if (prefix == "vn") {
				glm::vec3 normal;
				ss >> normal.x >> normal.y >> normal.z;
				temp_normals.push_back(normal);
			}
			else if (prefix == "usemtl")
			{
				ss >> currentMaterial;
				materialMap[currentMaterial] = indices.size();
			}
			else if (prefix == "f") {
				GLuint vertexIndex[3], uvIndex[3], normalIndex[3];
				char separator;

				for (int i = 0; i < 3; ++i) {
					ss >> vertexIndex[i] >> separator >> uvIndex[i] >> separator >> normalIndex[i];
					indices.push_back(vertexIndex[i] - 1);

					glm::vec3 vertex = temp_vertices[vertexIndex[i] - 1];
					glm::vec2 uv = temp_uvs[uvIndex[i] - 1];
					glm::vec3 normal = temp_normals[normalIndex[i] - 1];

					vertices.push_back(vertex.x);
					vertices.push_back(vertex.y);
					vertices.push_back(vertex.z);

					uvs.push_back(uv.x);
					uvs.push_back(uv.y);

					normals.push_back(normal.x);
					normals.push_back(normal.y);
					normals.push_back(normal.z);
				}
			}
		}


		// Load textures, if they have not been loaded before, cache them
		for (const auto& mat : materials) {
			const std::string& matName = mat.first;
			const std::string& texturePath = mat.second;

			// Ensure the texture path is valid
			if (texturePath.empty()) {
				std::cerr << "Error: No texture path provided for material " << matName << std::endl;
				continue; //Skip to the next material
			}
			if (loadedTextures.find(texturePath) == loadedTextures.end()) {
				GLuint texture = LoadTextureTileBox(texturePath.c_str());
				textureMap[matName] = texture;
				loadedTextures[texturePath] = texture;

			}
			else
			{
				textureMap[matName] = loadedTextures[texturePath];
			}
		}

	}

	void initialize(glm::vec3 position, glm::vec3 scale, std::string objPath, std::string mtlPath) {
		this->position = position;
		this->scale = scale;
		this->objPath = objPath;
		this->mtlPath = mtlPath;


		loadOBJWithMTL(objPath.c_str(), mtlPath.c_str(), vertices, uvs, normals, indices, textureMap);
		// Create vertex array object
		glGenVertexArrays(1, &vertexArrayID);
		glBindVertexArray(vertexArrayID);

		// Create vertex buffer object
		glGenBuffers(1, &vertexBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);


		// Create uv buffer object
		glGenBuffers(1, &uvBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(GLfloat), uvs.data(), GL_STATIC_DRAW);

		// Create normal buffer object
		glGenBuffers(1, &normalBufferID);
		glBindBuffer(GL_ARRAY_BUFFER, normalBufferID);
		glBufferData(GL_ARRAY_BUFFER, normals.size() * sizeof(GLfloat), normals.data(), GL_STATIC_DRAW);

		// Create index buffer object
		glGenBuffers(1, &indexBufferID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);


		// Load Shaders
		programID = LoadShadersFromFile("../../../lab2/shaders/model.vert", "../../../lab2/shaders/model.frag");
		if (programID == 0)
		{
			std::cerr << "Failed to load model shaders." << std::endl;
		}
		mvpMatrixID = glGetUniformLocation(programID, "MVP");
		textureSamplerID = glGetUniformLocation(programID, "textureSampler");
	}
	void render(glm::mat4 cameraMatrix) {
		glUseProgram(programID);

		// Vertex attribute
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexBufferID);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

		// UV attribute
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, uvBufferID);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

		// Normal attribute
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, normalBufferID);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, 0);

		// Index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBufferID);

		// Model matrix
		glm::mat4 modelMatrix = glm::mat4(1.0f);
		modelMatrix = glm::translate(modelMatrix, position);
		modelMatrix = glm::scale(modelMatrix, scale);

		// MVP Matrix
		glm::mat4 mvp = cameraMatrix * modelMatrix;
		glUniformMatrix4fv(mvpMatrixID, 1, GL_FALSE, &mvp[0][0]);

		// Iterate through the material mapping
		int currentMaterialStart = 0;
		std::string previousMaterial = "";
		for (const auto& mat : textureMap) {
			const std::string& matName = mat.first;
			GLuint texture = mat.second;
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, texture);
			glUniform1i(textureSamplerID, 0);

			// Draw elements
			size_t indexCount = indices.size();
			if (indices.size() > currentMaterialStart)
			{
				if (previousMaterial != "") {
					int start_index = 0;
					for (const auto& prevMat : textureMap)
					{
						if (previousMaterial == prevMat.first)
						{

							for (int i = 0; i < indices.size(); i++)
							{
								bool found = false;
								for (const auto& currMat : textureMap)
								{
									if (matName == currMat.first)
									{
										found = true;
										break;
									}
								}
								if (found) {

									glDrawElements(GL_TRIANGLES, indices.size() - start_index, GL_UNSIGNED_INT, (void*)(start_index * sizeof(GLuint)));
									break;
								}
							}

							break;
						}

						start_index += indices.size();
					}

				}
				else
					glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
			}
			previousMaterial = matName;
		}

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);
	}
	void cleanup() {
		glDeleteBuffers(1, &vertexBufferID);
		glDeleteBuffers(1, &uvBufferID);
		glDeleteBuffers(1, &normalBufferID);
		glDeleteBuffers(1, &indexBufferID);
		glDeleteVertexArrays(1, &vertexArrayID);
		glDeleteProgram(programID);
		for (const auto& texture : textureMap) {
			glDeleteTextures(1, &texture.second);
		}
	}
};
int main(void)
{
	// Initialise GLFW
	if (!glfwInit())
	{
		std::cerr << "Failed to initialize GLFW." << std::endl;
		return -1;
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // For MacOS
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow(1024, 768, "Lab 2", NULL, NULL);
	if (window == NULL)
	{
		std::cerr << "Failed to open a GLFW window." << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
	glfwSetKeyCallback(window, key_callback);

	// Load OpenGL functions, gladLoadGL returns the loaded version, 0 on error.
	int version = gladLoadGL(glfwGetProcAddress);
	if (version == 0)
	{
		std::cerr << "Failed to initialize OpenGL context." << std::endl;
		return -1;
	}

	// Background
	glClearColor(0.2f, 0.2f, 0.25f, 0.0f);

	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	// TODO: Create more buildings
    // ---------------------------

	std::vector<Building> buildings;

	int gridSize = 4;            // Number of buildings along X and Z axes
	int spacing = 80;              // Spacing between buildings to represent streets
	Building b1;
	glm::vec3 position;

	position = glm::vec3(-300, -273+(3 * 64), -400);
	glm::vec3 size = glm::vec3(64, 3* 64, 64);
	b1.initialize(position, size, "../../../lab2/textures/facade4.jpg", 3);
	buildings.push_back(b1);

	Building b2;
	position = glm::vec3(-300, -273 + (3 * 64), -272);
	b2.initialize(position, size, "../../../lab2/textures/facade4.jpg", 3);
	buildings.push_back(b2);

	size = glm::vec3(64, 4 * 64, 64);
	Building b3;
	position = glm::vec3(-300, -273 + (4 * 64), -100);
	b3.initialize(position, size, "../../../lab2/textures/facade1.jpg", 4);
	buildings.push_back(b3);
	
	Building b4;
	position = glm::vec3(-300, -273 + (4 * 64), 28);
	b4.initialize(position, size, "../../../lab2/textures/facade1.jpg", 4);
	buildings.push_back(b4);

	size = glm::vec3(64, 2 * 64, 64);
	Building b5;
	position = glm::vec3(-300, -273 + (2 * 64), 200);
	b5.initialize(position, size, "../../../lab2/textures/facade3.jpg", 2);
	buildings.push_back(b5);

	Building b6;
	position = glm::vec3(-300, -273 + (2 * 64), 328);
	b6.initialize(position, size, "../../../lab2/textures/facade3.jpg", 2);
	buildings.push_back(b6);
	Model loadedModel;
	loadedModel.initialize(glm::vec3(0, 0, 0), glm::vec3(100, 100, 100), "../../../lab2/untitled.obj", "../../../lab2/untitled.mtl");
	//skybox-------------------------------------
	glm::vec3 skyboxScale(30,30,30); // Add margin
	skyBox skybox;
	skybox.initialize(glm::vec3(0, 0, 0), glm::vec3(1, 1, 1), "../../../lab2/textures/sky.png", 1);
	//--------------------------------------------idk
	Island island;
	island.initialize(glm::vec3(0, 0.5, 0), glm::vec3(20, 20, 20), "../../../lab2/textures/facade1.jpg", "../../../lab2/island.obj");
	Cloud cloud;
	cloud.initialize(glm::vec3(200, 200, 200), glm::vec3(5, 5, 5), "../../../lab2/textures/facade1.jpg", "../../../lab2/cloud.obj");
	Surface surface;
	surface.initialize(glm::vec3(0, -20, 0), glm::vec3(20, 20, 20), "../../../lab2/textures/facade1.jpg", "../../../lab2/surface.obj");
	Cloud spire;
	spire.initialize(glm::vec3(250, -250, 800), glm::vec3(5, 10, 5), "../../../lab2/textures/facade1.jpg", "../../../lab2/spire.obj");

	// Camera setup
    eye_center.y = viewDistance * cos(viewPolar);
    eye_center.x = viewDistance * cos(viewAzimuth);
    eye_center.z = viewDistance * sin(viewAzimuth);

	glm::mat4 viewMatrix, projectionMatrix;
    glm::float32 FoV = 45;
	glm::float32 zNear = 0.1f;
	glm::float32 zFar = 5000.0f;
	projectionMatrix = glm::perspective(glm::radians(FoV), 4.0f / 3.0f, zNear, zFar);
	std::cout << "Initial lookat: (" << lookat.x << ", " << lookat.y << ", " << lookat.z << ")\n";

	do
	{
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// Print updated lookat coordinates only if it has changed
		if (lookat != lastLookat)
		{
			std::cout << "Updated lookat: (" << lookat.x << ", " << lookat.y << ", " << lookat.z << ")\n";
			lastLookat = lookat;
		}
		viewMatrix = glm::lookAt(eye_center, lookat, up);

		glm::mat4 vp = projectionMatrix * viewMatrix;

		// Render the building
		glm::mat4 viewMatrixSkybox = glm::mat4(glm::mat3(viewMatrix)); // Remove translation
		glm::mat4 vpSkybox = projectionMatrix * viewMatrixSkybox;
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);
		skybox.render(vpSkybox);
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LESS);
		//island.render(vp);
		//surface.render(vp);
		//spire.render(vp);
		//cloud.render(vp);
		//for (size_t i = 0; i < buildings.size(); ++i) {
		//	buildings[i].render(vp);
		//}
		loadedModel.render(vp);



		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while (!glfwWindowShouldClose(window));
	island.cleanup();
	for (size_t i = 0; i < buildings.size(); ++i) {
		buildings[i].cleanup();
	}
	cloud.cleanup();
	surface.cleanup();
	skybox.cleanup();
	spire.cleanup();
	loadedModel.cleanup();

	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
	static float movementSpeed = 20.0f; // Movement speed for WASD
	static float rotationSpeed = 0.05f; // Rotation speed for arrow keys

	if ((action == GLFW_REPEAT || action == GLFW_PRESS))
	{
		glm::vec3 forward = glm::normalize(lookat - eye_center); // Forward vector
		glm::vec3 right = glm::normalize(glm::cross(forward, up)); // Right vector
		bool lookatChanged = false; // Flag to check if `lookat` changed

		// WASD for camera movement
		if (key == GLFW_KEY_W) { eye_center += forward * movementSpeed; lookat += forward * movementSpeed; }
		if (key == GLFW_KEY_S) { eye_center -= forward * movementSpeed; lookat -= forward * movementSpeed; }
		if (key == GLFW_KEY_A) { eye_center -= right * movementSpeed; lookat -= right * movementSpeed; }
		if (key == GLFW_KEY_D) { eye_center += right * movementSpeed; lookat += right * movementSpeed; }
		if (key == GLFW_KEY_Q) { eye_center += up * movementSpeed; lookat += up * movementSpeed; }
		if (key == GLFW_KEY_E) { eye_center -= up * movementSpeed; lookat -= up * movementSpeed; }

		// Arrow keys for camera rotation
		if (key == GLFW_KEY_UP) { viewPolar += rotationSpeed; if (viewPolar > 1.5f) viewPolar = 1.5f; lookatChanged = true; }
		if (key == GLFW_KEY_DOWN) { viewPolar -= rotationSpeed; if (viewPolar < -1.5f) viewPolar = -1.5f; lookatChanged = true; }
		if (key == GLFW_KEY_LEFT) { viewAzimuth -= rotationSpeed; lookatChanged = true; }
		if (key == GLFW_KEY_RIGHT) { viewAzimuth += rotationSpeed; lookatChanged = true; }

		// Recalculate `lookat` based on updated orientation
		if (lookatChanged)
		{
			float x = viewDistance * cos(viewPolar) * cos(viewAzimuth);
			float y = viewDistance * sin(viewPolar);
			float z = viewDistance * cos(viewPolar) * sin(viewAzimuth);

			lookat = eye_center + glm::vec3(x, y, z);
		}

		// Reset camera
		if (key == GLFW_KEY_R && action == GLFW_PRESS)
		{
			viewAzimuth = 0.f;
			viewPolar = 0.f;
			eye_center = glm::vec3(0.0f, 0.0f, viewDistance);
			lookat = glm::vec3(0, 0, 0);
			std::cout << "Camera Reset.\n";
		}

		// Exit application
		if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window, GL_TRUE);
		}
	}
}
