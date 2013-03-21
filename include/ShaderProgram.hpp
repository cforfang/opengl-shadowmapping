#pragma once
#ifndef SHADERPROGRAM_HPP
#define SHADERPROGRAM_HPP

#include <string>
#include "OpenGL.hpp"

class ShaderProgram
{
public:
	ShaderProgram();
	virtual ~ShaderProgram();

	void LoadFromFile(const std::string& vertexFile, const std::string& fragmentFile);
	void LoadWithSource(const std::string& vertexSource, const std::string& fragmentSource);

	void UpdateUniform(int location, const glm::mat4& m) const;
	void UpdateUniform(int location, float f) const;

	void UpdateUniform(const std::string&, const glm::mat4&) const;
	void UpdateUniform(const std::string&, const glm::vec2&) const;
	void UpdateUniform(const std::string&, const glm::vec3&) const;
	void UpdateUniform(const std::string&, const glm::vec4&) const;
	void UpdateUniform(const std::string&, float)      const;
	void UpdateUniformi(const std::string&, int)        const;

	int  GetProgram() const;
	void UseProgram() const;

	int GetAttribLocation( const std::string &s );
	int GetUniformLocation( const std::string& name ) const;

private:
	ShaderProgram(const ShaderProgram& other);
	const ShaderProgram& operator=(const ShaderProgram& other);

	int  m_programId;
	bool m_loaded;

	const std::string LoadShaderFromFile(const std::string& file);
	GLint BuildShader(const GLenum eShaderType, const std::string &shaderText);
	GLint CreateProgram(const std::string& vertexSource, const std::string& fragmentSource) ;
};

#endif // SHADERPROGRAM_HPP