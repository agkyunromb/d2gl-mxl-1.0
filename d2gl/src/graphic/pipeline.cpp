#include "pch.h"
#include "pipeline.h"
#include "frame_buffer.h"
#include "texture.h"
#include "uniform_buffer.h"

namespace d2gl {

GLuint current_program = 0;
uint32_t current_blend_index = 10;

Pipeline::Pipeline(const PipelineCreateInfo& info)
	: m_attachment_blends(info.attachment_blends)
{
	m_id = glCreateProgram();

	GLuint vs = 0, fs = 0, cs = 0;
	if (info.shader->vert) {
		vs = createShader(info.shader->vert, GL_VERTEX_SHADER);
		glAttachShader(m_id, vs);
	}
	if (info.shader->frag) {
		fs = createShader(info.shader->frag, GL_FRAGMENT_SHADER);
		glAttachShader(m_id, fs);
	}
	if (info.shader->comp) {
		cs = createShader(info.shader->comp, GL_COMPUTE_SHADER);
		glAttachShader(m_id, cs);
	}

	glLinkProgram(m_id);
	glValidateProgram(m_id);
	glDeleteShader(vs);
	glDeleteShader(fs);
	glDeleteShader(cs);

	glUseProgram(m_id);

	for (auto& binding : info.bindings) {
		switch (binding.type) {
			case BindingType::UniformBuffer: {
				GLuint ubo_index = glGetUniformBlockIndex(m_id, binding.name.c_str());
				glUniformBlockBinding(m_id, ubo_index, binding.value);
				break;
			}
			case BindingType::Texture: {
				setUniform1i(binding.name, binding.value);
				break;
			}
		};
	}

	glUseProgram(0);
	current_program = 0;
	current_blend_index = 10;
}

Pipeline::~Pipeline()
{
	glDeleteProgram(m_id);
}

void Pipeline::bind(uint32_t index)
{
	if (current_program == m_id && current_blend_index == index)
		return;

	App.context->flushVertices();

	if (current_program != m_id) {
		glUseProgram(m_id);
		current_program = m_id;
		current_blend_index = 10;
	}

	if (current_blend_index != index) {
		setBlendState(index);
		current_blend_index = index;
	}
}

void Pipeline::setBlendState(uint32_t index)
{
	const auto& blends = m_attachment_blends[index];
	if (blends[0] == BlendType::NoBlend) {
		glDisable(GL_BLEND);
		return;
	}

	glEnable(GL_BLEND);
	if (App.gl_caps.independent_blending) {
		for (size_t i = 0; i < blends.size(); i++) {
			const auto factor = blendFactor(blends[i]);
			glBlendFuncSeparatei(i, factor.src_color, factor.dst_color, factor.src_alpha, factor.dst_alpha);
		}
	} else {
		const auto factor = blendFactor(blends[0]);
		glBlendFuncSeparate(factor.src_color, factor.dst_color, factor.src_alpha, factor.dst_alpha);
	}
}

BlendFactors Pipeline::blendFactor(BlendType type)
{
	switch (type) {
		case BlendType::One_Zero: return { GL_ONE, GL_ZERO, GL_ONE, GL_ZERO };
		case BlendType::Zero_SColor: return { GL_ZERO, GL_SRC_COLOR, GL_ZERO, GL_SRC_COLOR };
		case BlendType::One_One: return { GL_ONE, GL_ONE, GL_ZERO, GL_ONE };
		case BlendType::SAlpha_OneMinusSAlpha: return { GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA };
	}
	return { GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA };
}

void Pipeline::setUniform1i(const std::string& name, int value)
{
	bind();
	glUniform1i(getUniformLocation(name), value);
}

void Pipeline::setUniformMat4f(const std::string& name, const glm::mat4& matrix)
{
	bind();
	glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, &matrix[0][0]);
}

GLint Pipeline::getUniformLocation(const std::string& name)
{
	if (m_uniform_cache.find(name) != m_uniform_cache.end())
		return m_uniform_cache[name];

	int location = glGetUniformLocation(m_id, name.c_str());
	if (location == -1) {
		error("Uniform (%s) not found!", name.c_str());
		return -1;
	}

	m_uniform_cache.insert({ name, location });
	return location;
}

GLuint Pipeline::createShader(const char* source, int type)
{
	GLuint id = glCreateShader(type);
	GLint length = strlen(source);

	glShaderSource(id, 1, &source, &length);
	glCompileShader(id);

	int result;
	glGetShaderiv(id, GL_COMPILE_STATUS, &result);
	if (result == GL_FALSE) {
		int lenght;
		glGetShaderiv(id, GL_INFO_LOG_LENGTH, &lenght);
		char* message = (char*)_malloca(lenght * sizeof(char));
		glGetShaderInfoLog(id, lenght, &lenght, message);
		error("Shader compile failed! (%s) | %s", (type == GL_VERTEX_SHADER ? "VERTEX" : (type == GL_FRAGMENT_SHADER ? "FRAGMENT" : "COMPUTE")), message);

		glDeleteShader(id);
		return 0;
	}

	return id;
}

const ShaderSource g_shader_movie = {
#include "shaders/movie.glsl.h"
};

const ShaderSource g_shader_postfx = {
#include "shaders/postfx.glsl.h"
};

// clang-format off
const std::vector<std::pair<std::string, ShaderSource>> g_shader_upscale = {
	{ "simple-sharp-bilinear", {
		#include "shaders/upscale/simple-sharp-bilinear.glsl.h"
	} },
	{ "xbr-lv2-noblend", {
		#include "shaders/upscale/xbr-lv2-noblend.glsl.h"
	} },
	{ "xbrz-freescale", {
		#include "shaders/upscale/xbrz-freescale.glsl.h"
	} },
	{ "2xbr-hybrid-v5-gamma", {
		#include "shaders/upscale/2xbr-hybrid-v5-gamma.glsl.h"
	} },
	{ "aa-shader-4.0", {
		#include "shaders/upscale/aa-shader-4.0.glsl.h"
	} },
	{ "advanced-aa", {
		#include "shaders/upscale/advanced-aa.glsl.h"
	} },
};
// clang-format on

}