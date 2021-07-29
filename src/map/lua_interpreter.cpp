// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include <fstream>
#include <sstream>
#include <lua.hpp>

#include "lua_bridge.hpp"
#include "lua_interpreter.hpp"

namespace {

	/**
	 * A structure which is used to keep track of the lua reader state
	 */
	struct lua_reader_state
	{
		const std::string* chunk = nullptr;
		bool read = false;
	};

	/**
	 * The reader function used by lua_load.
	 * Every time lua_load needs another piece of the chunk,
	 * it calls the reader, passing along its data parameter.
	 * 
	 * The reader must return a pointer to a block of memory with
	 * a new piece of the chunk and set size to the block size.
	 * The block must exist until the reader function is called again.
	 * To signal the end of the chunk, the reader must return NULL or
	 * set size to zero.
	 * 
	 * The reader function may return pieces of any size greater than zero.
	 */
	const char* lua_reader(lua_State* lua, void* data, size_t* size)
	{
		lua_reader_state* state = (lua_reader_state*)data;

		if (state->read) {
			return nullptr;
		}

		state->read = true;
		*size = state->chunk->length();

		return state->chunk->c_str();
	}

	/**
	 * A structure which is used to keep track of the lua writer state
	 */
	struct lua_writer_state
	{
		std::stringstream* stream = nullptr;
		size_t total_size = 0;
	};

	/**
	 * The type of the writer function used by lua_dump.
	 * Every time lua_dump produces another piece of chunk,
	 * it calls the writer, passing along the buffer to be written (buffer),
	 * its size (size), and the user_data parameter supplied to lua_dump.
	 * 
     * The writer returns an error code: 0 means no errors;
	 * any other value means an error and stops lua_dump
	 * from calling the writer again.
	 */
	int lua_writer(lua_State* lua, const void* buffer, size_t size, void* user_data)
	{
		lua_writer_state* state = (lua_writer_state*)user_data;
		state->stream->write((const char*)buffer, size);
		state->total_size += size;

		return 0;
	}

	/**
	 * A function that is called when reading a lua code chunk.
	 * It tests the code and ensures there are no errors
	 */
	int lua_error_handler(lua_State* lua)
	{
		const char* message = lua_tostring(lua, 1);

		if (message) {
			luaL_traceback(lua, lua, message, 1);
		} else {
			if (!lua_isnoneornil(lua, 1)) {
				if (!luaL_callmeta(lua, 1, "__tostring")) {
					lua_pushliteral(lua, "(no error message)");
				}
			}
		}

		return 1;
	}
}

namespace lua {

	//****************************************************************************/
	//* interpreter implementation
	//****************************************************************************/

	struct interpreter::impl
	{
		lua_State* lua = nullptr;

		impl()
		{
			lua = luaL_newstate();
			luaL_openlibs(lua);
		}

		std::unique_ptr<script_metadata> fetch_metadata()
		{
			if (!lua_istable(lua, -1)) {
				return nullptr;
			}

			std::unique_ptr<script_metadata> metadata = std::make_unique<script_metadata>();
			metadata->x = fetch_number("x");
			metadata->y = fetch_number("y");
			metadata->facing = fetch_number("facing");
			metadata->sprite = fetch_number("sprite");
			metadata->map = std::move(fetch_string("map"));
			metadata->name = std::move(fetch_string("name"));
			lua_settop(lua, 0);

			// search for the script function.
			lua_getglobal(lua, "script");

			if (!lua_isfunction(lua, -1)) {
				// will let the other end handle what happens when a script has no code.
				return metadata;
			}

			std::stringstream ss;
			lua_writer_state writer_state;
			writer_state.stream = &ss;

			// dump into bytecode
			int strip_debug_info = 1;
			lua_dump(lua, lua_writer, &writer_state, strip_debug_info);

			// store bytecode
			std::unique_ptr<lua::bytecode> code = std::make_unique<lua::bytecode>();
			// kinda hacky but i don't really know if ss.str() would
			// work provided that bytecode can contain null characters
			// maybe it does? who knows.
			code->bytes.resize(writer_state.total_size);
			ss.read(code->bytes.data(), writer_state.total_size);

			metadata->code = std::move(code);

			return metadata;
		}

		int fetch_number(const char* name)
		{
			lua_getfield(lua, -1, name);
			if (!lua_isnumber(lua, -1))
				return 0;

			int number = (int)lua_tointeger(lua, -1);
			lua_pop(lua, 1);

			return number;
		}

		std::string fetch_string(const char* name)
		{
			lua_getfield(lua, -1, name);
			if (!lua_isstring(lua, -1))
				return "";

			const char* str = lua_tostring(lua, -1);
			lua_pop(lua, 1);

			return str;
		}

		~impl()
		{
			if (lua != nullptr) {
				lua_close(lua);
			}
		}
	};

	interpreter::interpreter()
		: self(std::make_unique<impl>())
	{}

	std::unique_ptr<script_metadata> interpreter::extract_metadata(const std::string& path)
	{
		std::ifstream file;
		file.open(path);
		std::stringstream ss;
		ss << file.rdbuf();
		const std::string& script = ss.str();

		// clean the stack
		lua_settop(self->lua, 0);
		// push the error handler into the stack
		lua_pushcfunction(self->lua, lua_error_handler);
		lua_reader_state reader_state;
		reader_state.chunk = &script;

		std::string error_message;

		switch (const int lua_load_result = lua_load(self->lua, lua_reader, &reader_state, ("=" + path).c_str(), "t")) {
			case LUA_OK: {
				// call the error handler
				if (lua_pcall(self->lua, 0, 1, 1) != LUA_OK) {
					if (!lua_isnil(self->lua, -1)) {
						error_message = lua_tostring(self->lua, -1);
					}
				}
			}
			break;

			case LUA_ERRSYNTAX: {
				error_message = lua_tostring(self->lua, -1);
			}
			break;

			default: {
				std::stringstream ss;
				ss << "Unexpected lua_load_result: " << lua_load_result;
				error_message = ss.str();
			}
			break;
		}

		// validate the script
		if (!error_message.empty()) {
			return nullptr;
		}

		// fetch metadata
		return self->fetch_metadata();
	}

	interpreter::~interpreter() noexcept = default;

	//****************************************************************************/
	//*  executor implementation
	//****************************************************************************/

	struct executor::impl
	{
		lua_State* lua;
		lua_State* thread = nullptr;
		map_session_data* sd;
		npc_data* nd;

		impl(map_session_data* sd, npc_data* nd)
		{
			this->sd = sd;
			this->nd = nd;
			lua = luaL_newstate();
			luaL_openlibs(lua);
			register_functions();
			register_globals();
		}

		/**
		 * A function which registers global variables such as
		 * the session and npc data into the state.
		 */
		void register_globals()
		{
			lua_pushlightuserdata(lua, this);
			lua_setglobal(lua, "__executor__");
			lua_pushlightuserdata(lua, sd);
			lua_setglobal(lua, lua_helpers::SD_LUA_VARIABLE);
			lua_pushlightuserdata(lua, nd);
			lua_setglobal(lua, lua_helpers::ND_LUA_VARIABLE);
		}

		// i don't know if i like this but hey it works
		#define LUA_EXECUTOR_REGISTER(x) (lua_register(lua, #x, lua_bridge:: ## x))

		/**
		 * This function registers the native C functions into
		 * the main state so they can be called from lua. This
		 * is done every single time a script is run which would
		 * probably result in a decrease of performance. I don't
		 * know if it would be possible to register these functions
		 * when compiling though in that case the scripts would take
		 * more memory anyway.
		 */
		void register_functions()
		{
			LUA_EXECUTOR_REGISTER(mes);
			LUA_EXECUTOR_REGISTER(next);
			LUA_EXECUTOR_REGISTER(close);
		}

		~impl()
		{
			if (lua != nullptr) {
				lua_close(lua);
			}
		}
	};

	executor::executor(map_session_data* sd, npc_data* nd)
		: self(std::make_unique<executor::impl>(sd, nd))
	{
	}

	/**
	 * Runs a lua script
	 */
	bool executor::run(const char* code, size_t size)
	{
		// load bytecode into the lua state, in binary mode
		luaL_loadbuffer(self->lua, code, size, "b");

		// Create a lua coroutine so it can be paused/resumed
		// this was made this way because of the need of having
		// to wait for user input in some commands such as 'next'.
		// I don't know if it's possible to pause the main lua
		// 'thread' so I took this route.
		self->thread = lua_newthread(self->lua);

		// push the main function into the stack. lua_newthread()
		// pushes the coroutine into the stack so the function is
		// at index -2
		lua_pushvalue(self->lua, -2);

		// move the function into the new thread
		lua_xmove(self->lua, self->thread, 1);

		resume();
		return true;
	}

	void executor::resume()
	{
		int results;
		lua_resume(self->thread, nullptr, 0, &results);
	}

	executor::~executor() = default;
}
