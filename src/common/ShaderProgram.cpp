#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <exception>

#include "ShaderProgram.hpp"
#include "OpenGL.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

ShaderProgram::ShaderProgram()
	: m_loaded(false)
{

}

ShaderProgram::~ShaderProgram()
{
	if(m_loaded) {
		glDeleteProgram(m_programId);
	}
}

int ShaderProgram::GetProgram() const {
	return m_programId;
}

void ShaderProgram::LoadFromFile(const std::string& vertexFile, const std::string& fragmentFile)
{
	if ( m_loaded ) {
		std::cerr << "Shader-program already loaded!" << std::endl;
		throw std::runtime_error("Shader-program already loaded!");
	}

	std::string vertexSource   = LoadShaderFromFile( vertexFile);
	std::string fragmentSource = LoadShaderFromFile( fragmentFile);

	LoadWithSource(vertexSource, fragmentSource);

	m_loaded = true;
}

void ShaderProgram::LoadWithSource(const std::string& vertexSource, const std::string& fragmentSource)
{
	if(m_loaded) {
		std::cerr << "Shader-program already loaded!" << std::endl;
		throw std::runtime_error("Shader-program already loaded!");
	}

	try {
		m_programId = CreateProgram(vertexSource, fragmentSource);
	}
	catch (std::runtime_error e) {
		std::cerr << "Error compiling shaders." << std::endl;
		throw;
	}

	m_loaded = true;
}

const std::string ShaderProgram::LoadShaderFromFile(const std::string& file)
{
	std::ifstream in(file);
	if(!in.is_open()) {
		in.open("../src/"+file);
		if(!in.is_open()) {
			std::cerr << "Couldn't load source from file " + file;
			throw std::runtime_error("Couldn't load source from file " + file);
		}
	}

	std::stringstream buffer;
	buffer << in.rdbuf();
	in.close();

	return buffer.str();
}

GLint ShaderProgram::BuildShader(GLenum eShaderType, const std::string &shaderText)
{
	GLuint shader = glCreateShader(eShaderType);
	const char *strFileData = shaderText.c_str();
	glShaderSource(shader, 1, &strFileData, NULL);

	glCompileShader(shader);

	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == GL_FALSE)
	{
		//With ARB_debug_output, we already get the info log on compile failure.
		if ( !GL_ARB_debug_output ) {
			GLint infoLogLength;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLogLength);

			GLchar *strInfoLog = new GLchar[infoLogLength + 1];
			glGetShaderInfoLog(shader, infoLogLength, NULL, strInfoLog);

			const char *strShaderType = NULL;
			switch(eShaderType)
			{
			case GL_VERTEX_SHADER: strShaderType = "vertex"; break;
			case GL_GEOMETRY_SHADER: strShaderType = "geometry"; break;
			case GL_FRAGMENT_SHADER: strShaderType = "fragment"; break;
			}

			fprintf(stderr, "Compile failure in %s shader:\n%s\n", strShaderType, strInfoLog);
			delete[] strInfoLog;
		}

		throw std::runtime_error("Compile failure in shader.");
	}

	return shader;
}

GLint ShaderProgram::CreateProgram(const std::string& vertexSource, const std::string& fragmentSource)
{
	try {
		// Create program
		GLint vertexShader   = BuildShader(GL_VERTEX_SHADER, vertexSource);
		GLint fragmentShader = BuildShader(GL_FRAGMENT_SHADER, fragmentSource);

		GLuint program = glCreateProgram();

		glAttachShader(program, vertexShader);
		glAttachShader(program, fragmentShader);	

		glLinkProgram(program);

		glDetachShader(program, vertexShader);
		glDetachShader(program, fragmentShader);
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		GLint status;
		glGetProgramiv (program, GL_LINK_STATUS, &status);
		if (status == GL_FALSE)
		{
			if(!GL_ARB_debug_output)
			{
				GLint infoLogLength;
				glGetProgramiv(program, GL_INFO_LOG_LENGTH, &infoLogLength);

				GLchar *strInfoLog = new GLchar[infoLogLength + 1];
				glGetProgramInfoLog(program, infoLogLength, NULL, strInfoLog);
				fprintf(stderr, "Linker failure: %s\n", strInfoLog);
				delete[] strInfoLog;
			}

			throw std::runtime_error("ShaderProgram could not be linked.");
		}

		return program;
	}
	catch (std::runtime_error e) {
		std::cerr << "Runtime-error: " << e.what() << std::endl;
		throw;
	}

	return -1;
}

void ShaderProgram::UpdateUniform(int location, float f) const
{
	glUniform1f( location, f );
}

void ShaderProgram::UpdateUniform(int location, const glm::mat4& m) const
{
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(m));
}

void ShaderProgram::UpdateUniform(const std::string& name, const glm::mat4& m) const
{
	GLint location = GetUniformLocation( name );
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(m));
}

void ShaderProgram::UpdateUniform(const std::string& name, const glm::vec2& v) const
{
	GLint location = GetUniformLocation( name );
	glUniform2fv(location, 1, glm::value_ptr(v));
}

void ShaderProgram::UpdateUniform(const std::string& name, const glm::vec3& v) const
{
	GLint location = GetUniformLocation( name );
	glUniform3fv(location, 1, glm::value_ptr(v));
}

void ShaderProgram::UpdateUniform(const std::string& name, const glm::vec4& v) const
{
	GLint location = GetUniformLocation( name );
	glUniform4fv(location, 1, glm::value_ptr(v));
}

void ShaderProgram::UpdateUniform(const std::string& name, float f) const
{
	GLint location = GetUniformLocation( name );
	glUniform1f(location, f);
}

void ShaderProgram::UpdateUniformi(const std::string& name, int i) const 
{
	GLint location = GetUniformLocation( name );
	glUniform1i(location, i);
}

void ShaderProgram::UseProgram() const
{
	glUseProgram( m_programId );
}

int ShaderProgram::GetAttribLocation( const std::string &name )
{
	GLint location = glGetAttribLocation( m_programId, name.c_str() );

	if (location < 0) {
		// Warn
		printf("Error getting attribute named %s from shader\n", name.c_str());
	}

	return location;
}

int ShaderProgram::GetUniformLocation( const std::string& name ) const
{
	GLint location = glGetUniformLocation(m_programId, name.c_str());

	if (location < 0) {
		// Warn
		printf("Error getting uniform named %s from shader\n", name.c_str());
	}

	return location;
}
