// ----------------------------------------------------------------------------------------------------

#include <string>

#define SOKOL_IMPL
#include "sokol_gfx.h"
#undef SOKOL_IMPL

#include "renderer.h"

// ----------------------------------------------------------------------------------------------------

constexpr int32_t INITIAL_NUMBER_OF_COMMANDS = 512;

// ----------------------------------------------------------------------------------------------------

RENDERER::RENDERER(const sg_desc& desc) : m_update_semaphore(1), m_render_semaphore(1)
{
	// setup sokol graphics
	sg_setup(desc);

	// loop through commands
	for (int32_t i = 0; i < 2; i ++)
	{
		// reserve commands
		m_commands[i].reserve(INITIAL_NUMBER_OF_COMMANDS);
	}

	// release render semaphore
	m_render_semaphore.release();
}

// ----------------------------------------------------------------------------------------------------

RENDERER::~RENDERER()
{
	// loop through commands
	for (int32_t i = 0; i < 2; i ++)
	{
		// clear commands
		clear_commands(i);
	}
	
	// shutdown sokol graphics
	sg_shutdown();
}

// ----------------------------------------------------------------------------------------------------

bool RENDERER::execute_commands(bool terminating)
{
	// not flushing?
	if (!m_flushing)
	{
		// acquire update semaphore
		m_update_semaphore.acquire();
	}
	
	// loop through commands
	for (RENDER_COMMAND_ARRAY::iterator i = m_commands[m_commit_commands_index].begin(); i != m_commands[m_commit_commands_index].end(); i ++)
	{
		// set command
		const RENDER_COMMAND& command = *i;

		// ignore command?
		if (terminating && !(command.type >= RENDER_COMMAND::TYPE::DESTROY_BUFFER && command.type <= RENDER_COMMAND::TYPE::DESTROY_PASS))
		{
			continue;
		}

		// execute command
		switch (command.type)
		{
		case RENDER_COMMAND::TYPE::PUSH_DEBUG_GROUP:
			sg_push_debug_group(command.push_debug_group.name);
			break;
		case RENDER_COMMAND::TYPE::POP_DEBUG_GROUP:
			sg_pop_debug_group();
			break;
		case RENDER_COMMAND::TYPE::MAKE_BUFFER:
			sg_init_buffer(command.make_buffer.buffer, command.make_buffer.desc);
			break;
		case RENDER_COMMAND::TYPE::MAKE_IMAGE:
			sg_init_image(command.make_image.image, command.make_image.desc);
			break;
		case RENDER_COMMAND::TYPE::MAKE_SHADER:
			sg_init_shader(command.make_shader.shader, command.make_shader.desc);
			break;
		case RENDER_COMMAND::TYPE::MAKE_PIPELINE:
			sg_init_pipeline(command.make_pipeline.pipeline, command.make_pipeline.desc);
			break;
		case RENDER_COMMAND::TYPE::MAKE_PASS:
			sg_init_pass(command.make_pass.pass, command.make_pass.desc);
			break;
		case RENDER_COMMAND::TYPE::DESTROY_BUFFER:
			sg_destroy_buffer(command.destroy_buffer.buffer);
			break;
		case RENDER_COMMAND::TYPE::DESTROY_IMAGE:
			sg_destroy_image(command.destroy_image.image);
			break;
		case RENDER_COMMAND::TYPE::DESTROY_SHADER:
			sg_destroy_shader(command.destroy_shader.shader);
			break;
		case RENDER_COMMAND::TYPE::DESTROY_PIPELINE:
			sg_destroy_pipeline(command.destroy_pipeline.pipeline);
			break;
		case RENDER_COMMAND::TYPE::DESTROY_PASS:
			sg_destroy_pass(command.destroy_pass.pass);
			break;
		case RENDER_COMMAND::TYPE::UPDATE_BUFFER:
			sg_update_buffer(command.update_buffer.buffer, command.update_buffer.data, command.update_buffer.data_size);
			break;
		case RENDER_COMMAND::TYPE::APPEND_BUFFER:
			sg_append_buffer(command.append_buffer.buffer, command.append_buffer.data, command.append_buffer.data_size);
			break;
		case RENDER_COMMAND::TYPE::UPDATE_IMAGE:
			sg_update_image(command.update_image.image, command.update_image.cont);
			break;
		case RENDER_COMMAND::TYPE::BEGIN_DEFAULT_PASS:
			sg_begin_default_pass(command.begin_default_pass.pass_action, m_default_pass_width, m_default_pass_height);
			break;
		case RENDER_COMMAND::TYPE::BEGIN_PASS:
			sg_begin_pass(command.begin_pass.pass, command.begin_pass.pass_action);
			break;
		case RENDER_COMMAND::TYPE::APPLY_VIEWPORT:
			sg_apply_viewport(command.apply_viewport.x, command.apply_viewport.y, command.apply_viewport.width, command.apply_viewport.height, command.apply_viewport.origin_top_left);
			break;
		case RENDER_COMMAND::TYPE::APPLY_SCISSOR_RECT:
			sg_apply_scissor_rect(command.apply_scissor_rect.x, command.apply_scissor_rect.y, command.apply_scissor_rect.width, command.apply_scissor_rect.height, command.apply_scissor_rect.origin_top_left);
			break;
		case RENDER_COMMAND::TYPE::APPLY_PIPELINE:
			sg_apply_pipeline(command.apply_pipeline.pipeline);
			break;
		case RENDER_COMMAND::TYPE::APPLY_BINDINGS:
			sg_apply_bindings(command.apply_bindings.bindings);
			break;
		case RENDER_COMMAND::TYPE::APPLY_UNIFORMS:
			sg_apply_uniforms(command.apply_uniforms.stage, command.apply_uniforms.ub_index, command.apply_uniforms.data, command.apply_uniforms.data_size);
			break;
		case RENDER_COMMAND::TYPE::DRAW:
			sg_draw(command.draw.base_element, command.draw.number_of_elements, command.draw.number_of_instances);
			break;
		case RENDER_COMMAND::TYPE::END_PASS:
			sg_end_pass();
			break;
		case RENDER_COMMAND::TYPE::COMMIT:
			sg_commit();
			break;
		case RENDER_COMMAND::TYPE::CUSTOM:
			command.custom.custom_cb(command.custom.custom_data);
			break;
		case RENDER_COMMAND::TYPE::NOT_SET:
			break;
		}
	}

	// release render semaphore
	m_render_semaphore.release();
	
	return !m_flushing;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_push_debug_group(const char* name)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::PUSH_DEBUG_GROUP;
	
	// copy args
	command.push_debug_group.name = name;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_pop_debug_group()
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::POP_DEBUG_GROUP;
}

// ----------------------------------------------------------------------------------------------------

sg_buffer RENDERER::add_command_make_buffer(const sg_buffer_desc& desc, void (*completion_cb)(void* completion_data), void* completion_data)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::MAKE_BUFFER;
	
	// copy args
	command.make_buffer.desc = desc;
	command.make_buffer.completion_cb = completion_cb;
	command.make_buffer.completion_data = completion_data;
	
	// alloc buffer
	command.make_buffer.buffer = sg_alloc_buffer();
	
	// return buffer
	return command.make_buffer.buffer;
}

// ----------------------------------------------------------------------------------------------------

sg_image RENDERER::add_command_make_image(const sg_image_desc& desc, void (*completion_cb)(void* completion_data), void* completion_data)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::MAKE_IMAGE;
	
	// copy args
	command.make_image.desc = desc;
	command.make_image.completion_cb = completion_cb;
	command.make_image.completion_data = completion_data;

	// alloc image
	command.make_image.image = sg_alloc_image();
	
	// return image
	return command.make_image.image;
}

// ----------------------------------------------------------------------------------------------------

sg_shader RENDERER::add_command_make_shader(const sg_shader_desc& desc, void (*completion_cb)(void* completion_data), void* completion_data)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::MAKE_SHADER;
	
	// copy args
	command.make_shader.desc = desc;
	command.make_shader.completion_cb = completion_cb;
	command.make_shader.completion_data = completion_data;

	// alloc shader
	command.make_shader.shader = sg_alloc_shader();
	
	// return shader
	return command.make_shader.shader;
}

// ----------------------------------------------------------------------------------------------------

sg_pipeline RENDERER::add_command_make_pipeline(const sg_pipeline_desc& desc, void (*completion_cb)(void* completion_data), void* completion_data)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::MAKE_PIPELINE;
	
	// copy args
	command.make_pipeline.desc = desc;
	command.make_pipeline.completion_cb = completion_cb;
	command.make_pipeline.completion_data = completion_data;

	// alloc pipeline
	command.make_pipeline.pipeline = sg_alloc_pipeline();
	
	// return pipeline
	return command.make_pipeline.pipeline;
}

// ----------------------------------------------------------------------------------------------------

sg_pass RENDERER::add_command_make_pass(const sg_pass_desc& desc, void (*completion_cb)(void* completion_data), void* completion_data)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::MAKE_PASS;
	
	// copy args
	command.make_pass.desc = desc;
	command.make_pass.completion_cb = completion_cb;
	command.make_pass.completion_data = completion_data;

	// alloc pass
	command.make_pass.pass = sg_alloc_pass();
	
	// return pass
	return command.make_pass.pass;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_destroy_buffer(sg_buffer buffer)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::DESTROY_BUFFER;
	
	// copy args
	command.destroy_buffer.buffer = buffer;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_destroy_image(sg_image image)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::DESTROY_IMAGE;
	
	// copy args
	command.destroy_image.image = image;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_destroy_shader(sg_shader shader)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::DESTROY_SHADER;
	
	// copy args
	command.destroy_shader.shader = shader;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_destroy_pipeline(sg_pipeline pipeline)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::DESTROY_PIPELINE;
	
	// copy args
	command.destroy_pipeline.pipeline = pipeline;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_destroy_pass(sg_pass pass)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::DESTROY_PASS;
	
	// copy args
	command.destroy_pass.pass = pass;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_update_buffer(sg_buffer buffer, const void* data, int data_size, void (*completion_cb)(void* completion_data), void* completion_data)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::UPDATE_BUFFER;
	
	// copy args
	command.update_buffer.buffer = buffer;
	command.update_buffer.data = data;
	command.update_buffer.data_size = data_size;
	command.update_buffer.completion_cb = completion_cb;
	command.update_buffer.completion_data = completion_data;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_append_buffer(sg_buffer buffer, const void* data, int data_size, void (*completion_cb)(void* completion_data), void* completion_data)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::APPEND_BUFFER;
	
	// copy args
	command.append_buffer.buffer = buffer;
	command.append_buffer.data = data;
	command.append_buffer.data_size = data_size;
	command.append_buffer.completion_cb = completion_cb;
	command.append_buffer.completion_data = completion_data;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_update_image(sg_image image, const sg_image_content& cont, void (*completion_cb)(void* completion_data), void* completion_data)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::UPDATE_IMAGE;
	
	// copy args
	command.update_image.image = image;
	command.update_image.cont = cont;
	command.update_image.completion_cb = completion_cb;
	command.update_image.completion_data = completion_data;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_begin_default_pass(const sg_pass_action& pass_action)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::BEGIN_DEFAULT_PASS;
	
	// copy args
	command.begin_default_pass.pass_action = pass_action;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_begin_pass(sg_pass pass, const sg_pass_action& pass_action)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::BEGIN_PASS;
	
	// copy args
	command.begin_pass.pass = pass;
	command.begin_pass.pass_action = pass_action;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_apply_viewport(int x, int y, int width, int height, bool origin_top_left)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::APPLY_VIEWPORT;
	
	// copy args
	command.apply_viewport.x = x;
	command.apply_viewport.y = y;
	command.apply_viewport.width = width;
	command.apply_viewport.height = height;
	command.apply_viewport.origin_top_left = origin_top_left;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_apply_scissor_rect(int x, int y, int width, int height, bool origin_top_left)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::APPLY_SCISSOR_RECT;
	
	// copy args
	command.apply_scissor_rect.x = x;
	command.apply_scissor_rect.y = y;
	command.apply_scissor_rect.width = width;
	command.apply_scissor_rect.height = height;
	command.apply_scissor_rect.origin_top_left = origin_top_left;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_apply_pipeline(sg_pipeline pipeline)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::APPLY_PIPELINE;
	
	// copy args
	command.apply_pipeline.pipeline = pipeline;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_apply_bindings(const sg_bindings& bindings)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::APPLY_BINDINGS;
	
	// copy args
	command.apply_bindings.bindings = bindings;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_apply_uniforms(sg_shader_stage stage, int ub_index, const void* data, int data_size)
{
	// data size too big?
	if ((size_t)data_size > sizeof(RENDER_COMMAND::apply_uniforms.data))
	{
		return;
	}

	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::APPLY_UNIFORMS;
	
	// copy args
	command.apply_uniforms.stage = stage;
	command.apply_uniforms.ub_index = ub_index;
	memcpy(command.apply_uniforms.data, data, data_size);
	command.apply_uniforms.data_size = data_size;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_draw(int base_element, int number_of_elements, int number_of_instances)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::DRAW;
	
	// copy args
	command.draw.base_element = base_element;
	command.draw.number_of_elements = number_of_elements;
	command.draw.number_of_instances = number_of_instances;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_end_pass()
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::END_PASS;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_commit()
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::COMMIT;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_custom(void (*custom_cb)(void* custom_data), void* custom_data)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back();

	// set type
	command.type = RENDER_COMMAND::TYPE::CUSTOM;
	
	// copy args
	command.custom.custom_cb = custom_cb;
	command.custom.custom_data = custom_data;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::commit_commands()
{
	// acquire render semaphore
	m_render_semaphore.acquire();
	
	// clear commands
	clear_commands(m_commit_commands_index);
	
	// swap commands indexes
	std::swap(m_pending_commands_index, m_commit_commands_index);

	// release update semaphore
	m_update_semaphore.release();
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::flush_commands()
{
	// acquire render semaphore
	m_render_semaphore.acquire();
	
	// clear commands
	clear_commands(m_commit_commands_index);
	
	// swap commands indexes
	std::swap(m_pending_commands_index, m_commit_commands_index);

	// set flushing
	m_flushing = true;

	// release update semaphore
	m_update_semaphore.release();
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::clear_commands(int32_t commands_index)
{
	// loop through commands
	for (RENDER_COMMAND_ARRAY::iterator i = m_commands[commands_index].begin(); i != m_commands[commands_index].end(); i ++)
	{
		// set command
		RENDER_COMMAND& command = *i;
		
		// call completion cb
		switch (command.type)
		{
		case RENDER_COMMAND::TYPE::MAKE_BUFFER:
			if (command.make_buffer.completion_cb)
			{
				command.make_buffer.completion_cb(command.make_buffer.completion_data);
				command.make_buffer.completion_cb = nullptr;
			}
			break;
		case RENDER_COMMAND::TYPE::MAKE_IMAGE:
			if (command.make_image.completion_cb)
			{
				command.make_image.completion_cb(command.make_image.completion_data);
				command.make_image.completion_cb = nullptr;
			}
			break;
		case RENDER_COMMAND::TYPE::MAKE_SHADER:
			if (command.make_shader.completion_cb)
			{
				command.make_shader.completion_cb(command.make_shader.completion_data);
				command.make_shader.completion_cb = nullptr;
			}
			break;
		case RENDER_COMMAND::TYPE::MAKE_PIPELINE:
			if (command.make_pipeline.completion_cb)
			{
				command.make_pipeline.completion_cb(command.make_pipeline.completion_data);
				command.make_pipeline.completion_cb = nullptr;
			}
			break;
		case RENDER_COMMAND::TYPE::MAKE_PASS:
			if (command.make_pass.completion_cb)
			{
				command.make_pass.completion_cb(command.make_pass.completion_data);
				command.make_pass.completion_cb = nullptr;
			}
			break;
		case RENDER_COMMAND::TYPE::UPDATE_BUFFER:
			if (command.update_buffer.completion_cb)
			{
				command.update_buffer.completion_cb(command.update_buffer.completion_data);
				command.update_buffer.completion_cb = nullptr;
			}
			break;
		case RENDER_COMMAND::TYPE::APPEND_BUFFER:
			if (command.append_buffer.completion_cb)
			{
				command.append_buffer.completion_cb(command.append_buffer.completion_data);
				command.append_buffer.completion_cb = nullptr;
			}
			break;
		case RENDER_COMMAND::TYPE::UPDATE_IMAGE:
			if (command.update_image.completion_cb)
			{
				command.update_image.completion_cb(command.update_image.completion_data);
				command.update_image.completion_cb = nullptr;
			}
			break;
		default:
			break;
		}
	}
	
	// clear commands
	m_commands[commands_index].resize(0);
}