// ----------------------------------------------------------------------------------------------------

#define SOKOL_IMPL
#include "sokol_gfx.h"
#undef SOKOL_IMPL

#include "renderer.h"

// ----------------------------------------------------------------------------------------------------

constexpr int32_t INITIAL_NUMBER_OF_COMMANDS = 512;
constexpr int32_t INITIAL_NUMBER_OF_CLEANUPS = 128;

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

	// reserve cleamups
	m_cleanups.reserve(INITIAL_NUMBER_OF_CLEANUPS);
	
	// release render semaphore
	m_render_semaphore.release();
}

// ----------------------------------------------------------------------------------------------------

RENDERER::~RENDERER()
{
	// process cleanups
	process_cleanups(-1);
	
	// shutdown sokol graphics
	sg_shutdown();
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::execute_commands()
{
	// not flushing?
	if (!m_flushing)
	{
		// acquire update semaphore
		m_update_semaphore.acquire();
	}
	
	{
		// lock execute mutex
		std::scoped_lock<std::mutex> lock(m_execute_mutex);

		// loop through commands
		for (const auto& command : m_commands[m_commit_commands_index])
		{
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
				sg_uninit_buffer(command.destroy_buffer.buffer);
				break;
			case RENDER_COMMAND::TYPE::DESTROY_IMAGE:
				sg_uninit_image(command.destroy_image.image);
				break;
			case RENDER_COMMAND::TYPE::DESTROY_SHADER:
				sg_uninit_shader(command.destroy_shader.shader);
				break;
			case RENDER_COMMAND::TYPE::DESTROY_PIPELINE:
				sg_uninit_pipeline(command.destroy_pipeline.pipeline);
				break;
			case RENDER_COMMAND::TYPE::DESTROY_PASS:
				sg_uninit_pass(command.destroy_pass.pass);
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
	}

	// release render semaphore
	m_render_semaphore.release();
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::wait_for_flush()
{
	// initialise finished flushing
	bool finished_flushing = false;

	// wait for flush
	while (!finished_flushing)
	{
		// not flushing?
		if (!m_flushing)
		{
			// acquire update semaphore
			m_update_semaphore.acquire();
		}
		
		{
			// lock execute mutex
			std::scoped_lock<std::mutex> lock(m_execute_mutex);
			
			// loop through commands
			for (const auto& command : m_commands[m_commit_commands_index])
			{
				// execute command
				switch (command.type)
				{
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
				default:
					break;
				}
			}
		}
		
		// udpate finished flushing
		finished_flushing = m_flushing;

		// release render semaphore
		m_render_semaphore.release();
	}
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_push_debug_group(const char* name)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::PUSH_DEBUG_GROUP);

	// copy args
	command.push_debug_group.name = name;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_pop_debug_group()
{
	// add command
	/*RENDER_COMMAND& command = */m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::POP_DEBUG_GROUP);
}

// ----------------------------------------------------------------------------------------------------

sg_buffer RENDERER::add_command_make_buffer(const sg_buffer_desc& desc)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::MAKE_BUFFER);

	// copy args
	command.make_buffer.desc = desc;
	
	// alloc buffer
	command.make_buffer.buffer = sg_alloc_buffer();
	
	// return buffer
	return command.make_buffer.buffer;
}

// ----------------------------------------------------------------------------------------------------

sg_image RENDERER::add_command_make_image(const sg_image_desc& desc)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::MAKE_IMAGE);

	// copy args
	command.make_image.desc = desc;

	// alloc image
	command.make_image.image = sg_alloc_image();
	
	// return image
	return command.make_image.image;
}

// ----------------------------------------------------------------------------------------------------

sg_shader RENDERER::add_command_make_shader(const sg_shader_desc& desc)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::MAKE_SHADER);

	// copy args
	command.make_shader.desc = desc;

	// alloc shader
	command.make_shader.shader = sg_alloc_shader();
	
	// return shader
	return command.make_shader.shader;
}

// ----------------------------------------------------------------------------------------------------

sg_pipeline RENDERER::add_command_make_pipeline(const sg_pipeline_desc& desc)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::MAKE_PIPELINE);

	// copy args
	command.make_pipeline.desc = desc;

	// alloc pipeline
	command.make_pipeline.pipeline = sg_alloc_pipeline();
	
	// return pipeline
	return command.make_pipeline.pipeline;
}

// ----------------------------------------------------------------------------------------------------

sg_pass RENDERER::add_command_make_pass(const sg_pass_desc& desc)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::MAKE_PASS);

	// copy args
	command.make_pass.desc = desc;

	// alloc pass
	command.make_pass.pass = sg_alloc_pass();
	
	// return pass
	return command.make_pass.pass;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_destroy_buffer(sg_buffer buffer)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::DESTROY_BUFFER);

	// copy args
	command.destroy_buffer.buffer = buffer;

	// schedule cleanup
	schedule_cleanup(dealloc_buffer_cb, (void*)(uintptr_t)command.destroy_buffer.buffer.id);
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_destroy_image(sg_image image)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::DESTROY_IMAGE);

	// copy args
	command.destroy_image.image = image;

	// schedule cleanup
	schedule_cleanup(dealloc_image_cb, (void*)(uintptr_t)command.destroy_image.image.id);
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_destroy_shader(sg_shader shader)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::DESTROY_SHADER);

	// copy args
	command.destroy_shader.shader = shader;

	// schedule cleanup
	schedule_cleanup(dealloc_shader_cb, (void*)(uintptr_t)command.destroy_shader.shader.id);
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_destroy_pipeline(sg_pipeline pipeline)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::DESTROY_PIPELINE);

	// copy args
	command.destroy_pipeline.pipeline = pipeline;

	// schedule cleanup
	schedule_cleanup(dealloc_pipeline_cb, (void*)(uintptr_t)command.destroy_pipeline.pipeline.id);
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_destroy_pass(sg_pass pass)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::DESTROY_PASS);

	// copy args
	command.destroy_pass.pass = pass;

	// schedule cleanup
	schedule_cleanup(dealloc_pass_cb, (void*)(uintptr_t)command.destroy_pass.pass.id);
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_update_buffer(sg_buffer buffer, const void* data, int data_size)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::UPDATE_BUFFER);

	// copy args
	command.update_buffer.buffer = buffer;
	command.update_buffer.data = data;
	command.update_buffer.data_size = data_size;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_append_buffer(sg_buffer buffer, const void* data, int data_size)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::APPEND_BUFFER);

	// copy args
	command.append_buffer.buffer = buffer;
	command.append_buffer.data = data;
	command.append_buffer.data_size = data_size;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_update_image(sg_image image, const sg_image_content& cont)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::UPDATE_IMAGE);

	// copy args
	command.update_image.image = image;
	command.update_image.cont = cont;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_begin_default_pass(const sg_pass_action& pass_action)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::BEGIN_DEFAULT_PASS);

	// copy args
	command.begin_default_pass.pass_action = pass_action;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_begin_pass(sg_pass pass, const sg_pass_action& pass_action)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::BEGIN_PASS);

	// copy args
	command.begin_pass.pass = pass;
	command.begin_pass.pass_action = pass_action;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_apply_viewport(int x, int y, int width, int height, bool origin_top_left)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::APPLY_VIEWPORT);

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
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::APPLY_SCISSOR_RECT);

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
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::APPLY_PIPELINE);

	// copy args
	command.apply_pipeline.pipeline = pipeline;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_apply_bindings(const sg_bindings& bindings)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::APPLY_BINDINGS);

	// copy args
	command.apply_bindings.bindings = bindings;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_apply_uniforms(sg_shader_stage stage, int ub_index, const void* data, int data_size)
{
	// validate data size
	assert ((size_t)data_size <= sizeof(RENDER_COMMAND::apply_uniforms.data))

	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::APPLY_UNIFORMS);

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
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::DRAW);

	// copy args
	command.draw.base_element = base_element;
	command.draw.number_of_elements = number_of_elements;
	command.draw.number_of_instances = number_of_instances;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_end_pass()
{
	// add command
	/*RENDER_COMMAND& command = */m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::END_PASS);
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_commit()
{
	// add command
	/*RENDER_COMMAND& command = */m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::COMMIT);
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::add_command_custom(void (*custom_cb)(void* custom_data), void* custom_data)
{
	// add command
	RENDER_COMMAND& command = m_commands[m_pending_commands_index].emplace_back(RENDER_COMMAND::TYPE::CUSTOM);

	// copy args
	command.custom.custom_cb = custom_cb;
	command.custom.custom_data = custom_data;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::schedule_cleanup(void (*cleanup_cb)(void* cleanup_data), void* cleanup_data, int32_t number_of_frames_to_defer)
{
	// add cleanup
	RENDER_CLEANUP& cleanup = m_cleanups.emplace_back(cleanup_cb, cleanup_data);
	
	// set frame index
	cleanup.frame_index = m_frame_index + 1 + number_of_frames_to_defer;
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::commit_commands()
{
	// acquire render semaphore
	m_render_semaphore.acquire();
	
	// clear commands
	m_commands[m_commit_commands_index].resize(0);
	
	// process cleanups
	process_cleanups(m_frame_index);
	
	// swap commands indexes
	std::swap(m_pending_commands_index, m_commit_commands_index);

	// increase frame index
	m_frame_index ++;

	// release update semaphore
	m_update_semaphore.release();
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::flush_commands()
{
	// acquire render semaphore
	m_render_semaphore.acquire();
	
	// clear commands
	m_commands[m_commit_commands_index].resize(0);
	
	// swap commands indexes
	std::swap(m_pending_commands_index, m_commit_commands_index);

	// set flushing
	m_flushing = true;

	// release update semaphore
	m_update_semaphore.release();
}

// ----------------------------------------------------------------------------------------------------

void RENDERER::process_cleanups(int32_t frame_index)
{
	// loop through cleanups
	for (auto& cleanup : m_cleanups)
	{
		// call cleanup cb?
		if ((cleanup.frame_index <= frame_index || frame_index < 0) && cleanup.cleanup_cb)
		{
			// call cleanup cb
			cleanup.cleanup_cb(cleanup.cleanup_data);
			
			// reset cleanup cb
			cleanup.cleanup_cb = nullptr;
		}
	}
	
	// erase invalid cleanups
	m_cleanups.erase(std::remove_if(m_cleanups.begin(), m_cleanups.end(), [](const auto& cleanup) { return !cleanup.cleanup_cb; }), m_cleanups.end());
}
