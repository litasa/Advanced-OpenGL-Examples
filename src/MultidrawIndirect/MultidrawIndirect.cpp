//// Based on: https://github.com/fsole/GLSamples/blob/master/src/MultidrawIndirect.cpp
//// Modified by: Jakob Törmä Ruhl

#include <GL\glew.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>

void processInput(GLFWwindow *window);
void cursor_position_callback(GLFWwindow* window, double xpos, double ypos);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
//
namespace
{
    struct Vertex2D
    {
        float x, y;  //Position
        float u, v;  //Uv
    };

    struct DrawElementsCommand
    {
        GLuint vertexCount;
        GLuint instanceCount;
        GLuint firstIndex;
        GLuint baseVertex;
        GLuint baseInstance;
    };

    struct Matrix
    {
        float a0, a1, a2, a3;
        float b0, b1, b2, b3;
        float c0, c1, c2, c3;
        float d0, d1, d2, d3;
    };

    void setMatrix(Matrix&matrix, float x, float y)
    {
        /*
        1 0 0 0
        0 1 0 0
        0 0 1 0
        x y 0 1
        */
        matrix.a0 = 1;
        matrix.a1 = matrix.a2 = matrix.a3 = 0;

        matrix.b1 = 1;
        matrix.b0 = matrix.b2 = matrix.b3 = 0;
        
        matrix.c2 = 1;
        matrix.c0 = matrix.c1 = matrix.c3 = 0;

        matrix.d0 = x;
        matrix.d1 = y;
        matrix.d2 = 0;
        matrix.d3 = 1;
    }

    struct SDrawElementsCommand
    {
        GLuint vertexCount;
        GLuint instanceCount;
        GLuint firstIndex;
        GLuint baseVertex;
        GLuint baseInstance;
    };

    const std::vector<Vertex2D> gQuad = {
        //xy			//uv
        { 0.0f,0.0f,	0.0f,0.0f },
        { 0.1f,0.0f,	1.0f,0.0f },
        { 0.0f,0.1f,	0.0f,1.0f },
        { 0.1f,0.1f,	1.0f,1.0f }
    };

    const std::vector<Vertex2D> gTriangle =
    {
        { 0.0f, 0.0f,	0.0f,0.0f},
        { 0.05f, 0.1f,	0.5f,1.0f},
        { 0.1f, 0.0f,	1.0f,0.0f}
    };

    const std::vector<unsigned int> gQuadIndex = {
        0,1,2,
        1,3,2 };
    const std::vector<unsigned int> gTriangleIndex =
    {
        0,1,2
    };

    const GLchar* gVertexShaderSource[] =
    {
        "#version 430 core\n"
        "layout (location = 0 ) in vec2 position;\n"
        "layout (location = 1 ) in vec2 texCoord;\n"
        "layout (location = 3 ) in mat4 instanceMatrix;\n"
        "layout (location = 2 ) in uint drawid;\n"
        "out vec2 uv;\n"
        "out vec4 pos;\n"
        "flat out uint drawID;\n"
        "void main(void)\n"
        "{\n"
        "  gl_Position = instanceMatrix * vec4(position,0.0,1.0);\n"
        "  uv = texCoord;\n"
        "  pos = instanceMatrix * vec4(position,0.0,1.0);\n"
        "  drawID = drawid;\n"
        "}\n"
    };

    const GLchar* gFragmentShaderSource[] =
    {
        "#version 430 core\n"
        "out vec4 color;\n"
        "in vec2 uv;\n"
        "in vec4 pos;\n"
        "flat in uint drawID;\n"
        "uniform vec2 light_pos;\n"
        "layout (binding=0) uniform sampler2DArray textureArray;\n"
        "void main(void)\n"
        "{\n"
        "  float intensity = 1.0 / length(pos.xy - light_pos);\n"
        "  color = intensity * texture(textureArray, vec3(uv.x,uv.y,drawID) );\n"
        "}\n"
    };

    GLuint gVAO(0);
    GLuint gArrayTexture(0);
    GLuint gVertexBuffer(0);
    GLuint gElementBuffer(0);
    GLuint gIndirectBuffer(0);
    GLuint gMatrixBuffer(0);
    GLuint gProgram(0);

    float gMouseX(0);
    float gMouseY(0);

}//Unnamed namespace

void GenerateGeometry()
{
    //Generate 50 quads, 50 triangles
    const unsigned num_vertices = gQuad.size() * 50 + gTriangle.size() * 50;
    std::vector<Vertex2D> vVertex(num_vertices);
    Matrix vMatrix[100];
    unsigned vertexIndex(0);
    unsigned matrixIndex(0);
    float xOffset(-0.95f);
    float yOffset(-0.95f);
    // populate geometry
    for (unsigned int i(0); i != 10; ++i)
    {
        for (unsigned int j(0); j != 10; ++j)
        {
            //quad
            if (j % 2 == 0)
            {
                for (unsigned int k(0); k != 4; ++k)
                {
                    vVertex[vertexIndex++] = gQuad[k];
                }
            }
            //triangle
            else
            {
                for (unsigned int k(0); k != 3; ++k)
                {
                    vVertex[vertexIndex++] = gTriangle[k];
                }
            }
            //set position in model matrix
            setMatrix(vMatrix[matrixIndex++], xOffset, yOffset);
            xOffset += 0.2f;
        }
        yOffset += 0.2f;
        xOffset = -0.95f;
    }

    glGenVertexArrays(1, &gVAO);
    glBindVertexArray(gVAO);
    //Create a vertex buffer object
    glGenBuffers(1, &gVertexBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, gVertexBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex2D)*vVertex.size(), vVertex.data(), GL_STATIC_DRAW);

    //Specify vertex attributes for the shader
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (GLvoid*)(offsetof(Vertex2D,x)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex2D), (GLvoid*)(offsetof(Vertex2D,u)));

    //Create an element buffer and populate it
    int triangle_bytes = sizeof(unsigned int) * gTriangleIndex.size();
    int quad_bytes = sizeof(unsigned int) * gQuadIndex.size();
    glGenBuffers(1, &gElementBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gElementBuffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, triangle_bytes + quad_bytes, NULL, GL_STATIC_DRAW);

    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, quad_bytes, gQuadIndex.data());
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, quad_bytes, triangle_bytes, gTriangleIndex.data());

    //Generate draw commands
    SDrawElementsCommand vDrawCommand[100];
    GLuint baseVert = 0;
    for (unsigned int i(0); i<100; ++i)
    {
        if (i % 2 == 0)
        {
            vDrawCommand[i].vertexCount = 6;		//2 triangles = 6 vertices
            vDrawCommand[i].instanceCount = 1;		//Draw 1 instance
            vDrawCommand[i].firstIndex = 0;			//Draw from index 0 for this instance
            vDrawCommand[i].baseVertex = baseVert;	//Starting from baseVert
            vDrawCommand[i].baseInstance = i;		//gl_InstanceID. 
            baseVert += 4;
        }
        //triangles
        else
        {
            vDrawCommand[i].vertexCount = 3;		//1 triangle = 3 vertices
            vDrawCommand[i].instanceCount = 1;		//Draw 1 instance
            vDrawCommand[i].firstIndex = 0;			//Draw from index 0 for this instance
            vDrawCommand[i].baseVertex = baseVert;	//Starting from baseVert
            vDrawCommand[i].baseInstance = i;		//gl_InstanceID
            baseVert += 3;
        }
    }

    //feed the draw command data to the gpu
    glGenBuffers(1, &gIndirectBuffer);
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, gIndirectBuffer);
    glBufferData(GL_DRAW_INDIRECT_BUFFER, sizeof(vDrawCommand), vDrawCommand, GL_STATIC_DRAW);

    //feed the instance id to the shader
    glBindBuffer(GL_ARRAY_BUFFER, gIndirectBuffer);
    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(SDrawElementsCommand), (void*)(offsetof(DrawElementsCommand, baseInstance)));
    glVertexAttribDivisor(2, 1); //only once per instance

    //Setup per instance matrices
    //Method 1. Use Vertex attributes and the vertex attrib divisor
    glGenBuffers(1, &gMatrixBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, gMatrixBuffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vMatrix), vMatrix, GL_STATIC_DRAW);
    //A matrix is 4 vec4s
    glEnableVertexAttribArray(3 + 0);
    glEnableVertexAttribArray(3 + 1);
    glEnableVertexAttribArray(3 + 2);
    glEnableVertexAttribArray(3 + 3);

    glVertexAttribPointer(3 + 0, 4, GL_FLOAT, GL_FALSE, sizeof(Matrix), (GLvoid*)(offsetof(Matrix, a0)));
    glVertexAttribPointer(3 + 1, 4, GL_FLOAT, GL_FALSE, sizeof(Matrix), (GLvoid*)(offsetof(Matrix, b0)));
    glVertexAttribPointer(3 + 2, 4, GL_FLOAT, GL_FALSE, sizeof(Matrix), (GLvoid*)(offsetof(Matrix, c0)));
    glVertexAttribPointer(3 + 3, 4, GL_FLOAT, GL_FALSE, sizeof(Matrix), (GLvoid*)(offsetof(Matrix, d0)));
    //Only apply one per instance
    glVertexAttribDivisor(3 + 0, 1);
    glVertexAttribDivisor(3 + 1, 1);
    glVertexAttribDivisor(3 + 2, 1);
    glVertexAttribDivisor(3 + 3, 1);

    //Method 2. Use Uniform Buffers. Not shown here
}

void GenerateArrayTexture()
{
    //Generate an array texture
    glGenTextures(1, &gArrayTexture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D_ARRAY, gArrayTexture);

    //Create storage for the texture. (100 layers of 1x1 texels)
    glTexStorage3D(GL_TEXTURE_2D_ARRAY,
        1,                    //No mipmaps as textures are 1x1
        GL_RGB8,              //Internal format
        1, 1,                 //width,height
        100                   //Number of layers
    );

    for (unsigned int i(0); i != 100; ++i)
    {
        //Choose a random color for the i-essim image
        GLubyte color[3] = { GLubyte(rand() % 255),GLubyte(rand() % 255),GLubyte(rand() % 255) };

        //Specify i-essim image
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY,
            0,                     //Mipmap number
            0, 0, i,               //xoffset, yoffset, zoffset
            1, 1, 1,               //width, height, depth
            GL_RGB,                //format
            GL_UNSIGNED_BYTE,      //type
            color);                //pointer to data
    }

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

GLuint CompileShaders(const GLchar** vertexShaderSource, const GLchar** fragmentShaderSource)
{
    //Compile vertex shader
    GLuint vertexShader(glCreateShader(GL_VERTEX_SHADER));
    glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
    glCompileShader(vertexShader);

    //Compile fragment shader
    GLuint fragmentShader(glCreateShader(GL_FRAGMENT_SHADER));
    glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
    glCompileShader(fragmentShader);

    //Link vertex and fragment shader together
    GLuint program(glCreateProgram());
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);

    //Delete shaders objects
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return program;
}

int main()
{

    // glfw: initialize and configure
    // ------------------------------
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // glfw window creation
    // --------------------
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
    if (window == NULL)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSetCursorPosCallback(window, cursor_position_callback);

    glewInit();

    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // uncomment this statement to fix compilation on OS X

    //Set clear color
    glClearColor(1.0, 1.0, 1.0, 0.0);

    //Create and bind the shader program
    gProgram = CompileShaders(gVertexShaderSource, gFragmentShaderSource);
    glUseProgram(gProgram);

    glUniform1i(0, 0); //Sampler refers to texture unit 0

    GenerateGeometry();
    GenerateArrayTexture();


    // render loop
    while (!glfwWindowShouldClose(window))
    {
        // input
        processInput(window);

        glClear(GL_COLOR_BUFFER_BIT);

        // Use program. Not needed in this example since we only have one that
        // we already use
        //glUseProgram(gProgram);

        //populate uniform
        glUniform2f(glGetUniformLocation(gProgram, "light_pos"), gMouseX, gMouseY);
        
        // Bind the vertex array we want to draw from. Not needed in this example
        // since we only have one that is already bounded
        //glBindVertexArray(gVAO);

        //draw
        glMultiDrawElementsIndirect(GL_TRIANGLES, //type
            GL_UNSIGNED_INT, //indices represented as unsigned ints
            (GLvoid*)0, //start with the first draw command
            100, //draw 100 objects
            0); //no stride, the draw commands are tightly packed



        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

     // glfw: terminate, clearing all previously allocated GLFW resources.
    glfwTerminate();
    //Clean-up
    glDeleteProgram(gProgram);
    glDeleteVertexArrays(1, &gVAO);
    glDeleteBuffers(1, &gVertexBuffer);
    glDeleteBuffers(1, &gElementBuffer);
    glDeleteBuffers(1, &gMatrixBuffer);
    glDeleteBuffers(1, &gIndirectBuffer);
    glDeleteTextures(1, &gArrayTexture);
    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    gMouseX = -0.5f + float(xpos) / float(SCR_WIDTH);
    gMouseY = 0.5f - float(ypos) / float(SCR_HEIGHT);
}