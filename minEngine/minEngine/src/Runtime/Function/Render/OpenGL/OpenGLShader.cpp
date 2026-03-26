#include "OpenGLShader.h"

namespace minEngine
{
    OpenGLShader::~OpenGLShader()
    {
        if (m_ID != 0)
        {
            glDeleteProgram(m_ID);
            m_ID = 0;
        }
    }

    OpenGLShader::OpenGLShader(const char *vertexShaderPath, const char *fragmentShaderPath)
    {
        std::string vertexShaderCode;
        std::string fragmentShaderCode;
        std::ifstream vShaderFile;
        std::ifstream fShaderFile;

        // ensure ifstream objects can throw exceptions:
        vShaderFile.exceptions (std::ifstream::failbit | std::ifstream::badbit);
        fShaderFile.exceptions (std::ifstream::failbit | std::ifstream::badbit);
        try
        {
            // open files
            vShaderFile.open(vertexShaderPath);
            fShaderFile.open(fragmentShaderPath);

            std::stringstream vShaderStream, fShaderStream;

            // read file's buffer contents into streams
            vShaderStream << vShaderFile.rdbuf();
            fShaderStream << fShaderFile.rdbuf();

            // close file handlers
            vShaderFile.close();
            fShaderFile.close();

            // convert stream into string
            vertexShaderCode = vShaderStream.str();
            fragmentShaderCode = fShaderStream.str();
        }
        catch (std::ifstream::failure& e)
        {
            std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
        }
        const char* vShaderCode = vertexShaderCode.c_str();
        const char * fShaderCode = fragmentShaderCode.c_str();

        // compile shaders
        unsigned int vertexShader, fragmentShader;
        int success;
        char infoLog[512];

        // vertex shader
        vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vShaderCode, NULL);
        glCompileShader(vertexShader);

        // check for shader compile errors
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if(!success)
        {
            glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
            std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
        }

        // fragment shader
        fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fShaderCode, NULL);
        glCompileShader(fragmentShader);

        // check for shader compile errors
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if(!success)
        {
            glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
            std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
        }

        // shader program
        m_ID = glCreateProgram();
        glAttachShader(m_ID, vertexShader);
        glAttachShader(m_ID, fragmentShader);
        glLinkProgram(m_ID);

        // check for linking errors
        glGetProgramiv(m_ID, GL_LINK_STATUS, &success);
        if(!success)
        {
            glGetProgramInfoLog(m_ID, 512, NULL, infoLog);
            std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
        }

        // delete the shaders as they're linked into our program now and no longer necessary
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    void OpenGLShader::Use()
    {
        glUseProgram(m_ID);
    }

    void OpenGLShader::UploadUniformInt(const std::string &name, int value)
    {
        int uniformLocation = -1;
        if(IsValidUniform(name, uniformLocation))
        {
            glUniform1i(uniformLocation, value);
        }
    }

    void OpenGLShader::UploadUniformFloat(const std::string &name, float value)
    {
        int uniformLocation = -1;
        if(IsValidUniform(name, uniformLocation))
        {
            glUniform1f(uniformLocation, value);
        }
    }

    void OpenGLShader::UploadUniformFloat3(const std::string &name, Vector3 value)
    {
        int uniformLocation = -1;
        if(IsValidUniform(name, uniformLocation))
        {
            glUniform3f(uniformLocation, value.x,  value.y, value.z);
        }

    }

    void OpenGLShader::UploadUniformMat4(const std::string &name, const float *matrix)
    {
        int uniformLocation = -1;
        if(IsValidUniform(name, uniformLocation))
        {
            glUniformMatrix4fv(uniformLocation, 1, GL_FALSE, matrix);
        }
    }

    void OpenGLShader::UploadUniformMat4(const std::string &name, const Matrix4 &matrix)
    {
        const float* matPtr = glm::value_ptr(matrix);
        UploadUniformMat4(name, matPtr);
    }

    void OpenGLShader::BindUniformBlock(const std::string &blockName, uint32_t bindingPoint)
    {
        int blockIndex = -1;
        if(IsValidUniformBlock(blockName, blockIndex))
        {
            glUniformBlockBinding(m_ID, blockIndex, bindingPoint);
        }
    }

    bool OpenGLShader::IsValidUniform(const std::string &name, int &uniformLocation)
    {
        uniformLocation = glGetUniformLocation(m_ID, name.c_str());     // TODO: cache uniform locations later
        if(uniformLocation == -1)
        {
            // ME_CORE_ERROR("Uniform {} not found in shader!", name);
            return false;
        }
        return true;
    }

    bool OpenGLShader::IsValidUniformBlock(const std::string &blockName, int &blockIndex)
    {
        blockIndex = glGetUniformBlockIndex(m_ID, blockName.c_str());
        if(blockIndex == GL_INVALID_INDEX)
        {
            // ME_CORE_ERROR("Uniform block {} not found in shader!", blockName);
            return false;
        }
        return true;
    }
}
