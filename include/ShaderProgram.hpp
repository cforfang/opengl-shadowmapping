#pragma once
#ifndef SHADERPROGRAM_HPP
#define SHADERPROGRAM_HPP

#include <string>
#include <vector>
#include <map>

#include "OpenGL.hpp"

struct ShaderInfo
{
	static ShaderInfo VSFS(const std::string& vs, const std::string& fs)
	{
		ShaderInfo si;
		si.setVertexShaderFile(vs);
		si.setFragmentShaderFile(fs);
		return si;
	}

	void setVertexShaderFile(const std::string& str)
	{
		vsFile = str;
	}

	void setFragmentShaderFile(const std::string& str)
	{
		fsFile = str;
	}

	void setGeometryShaderFile(const std::string& str)
	{
		gsFile = str;
	}

	size_t GetHash() const
	{
		std::string str = vsFile + fsFile + gsFile;
		return std::hash<std::string>()(str);
	}

	std::string vsFile{ "" };
	std::string gsFile{ "" };
	std::string fsFile{ "" };
};

class ShaderProgram
{
public:
	ShaderProgram();
	virtual ~ShaderProgram();

	bool Load(const ShaderInfo& shaderInfo, const std::string& includeDir = "");
	void DeleteProgram();

	int GetAttribLocation(const std::string &s);

	static int GetUniformLocation(GLuint program, const std::string& name);
	static bool UpdateUniform(int programId, int location, const glm::vec4& m);
	static bool UpdateUniform(int programId, int location, const glm::vec3& m);
	static bool UpdateUniform(int programId, int location, const glm::mat4& m);
	static bool UpdateUniform(int programId, int location, float f);

	int GetUniformLocation(const std::string& name);
	bool UpdateUniform(const std::string&, const glm::mat4&);
	bool UpdateUniform(const std::string&, const glm::vec2&);
	bool UpdateUniform(const std::string&, const glm::vec3&);
	bool UpdateUniform(const std::string&, const glm::vec4&);
	bool UpdateUniform(const std::string&, float);
	bool UpdateUniformi(const std::string&, int);

	int  GetProgram() const;
	void UseProgram() const;

	bool Reload();
	bool IsLoaded() const;

private:
	// Noncopyable
	ShaderProgram(const ShaderProgram& other);
	ShaderProgram& operator=(const ShaderProgram& other);

	// Disallows automatic type conversions (since 
	// there are different glUniform*-functions for ints, floats, etc.)
	// (Alternativly could add suffixes to methods)
	template<typename T> void UpdateUniform(std::string, T arg);
	template<typename T> void UpdateUniform(int programId, int location, T arg);

	std::map<std::string, GLint> m_uniformLocations;

	unsigned int m_programId;
	ShaderInfo   m_shaderInfo;
	std::string  m_includeDir;
	bool         m_loadedFromFile;
};

#endif // SHADERPROGRAM_HPP
