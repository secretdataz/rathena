// Copyright (c) rAthena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#ifndef LUA_SCRIPT_HOST_HPP
#define LUA_SCRIPT_HOST_HPP

#include <memory>
#include <string>

#include "pc.hpp"

namespace lua {
	class executor;

	/**
	 * Compiled lua code in bytecode form. I decided to wrap
	 * the vector around a struct in case more data is needed
	 * in the future.
	 */
	struct bytecode
	{
		std::vector<char> bytes;
	};

	/**
	 * Relevant info about a script. The map server will make
	 * use of this data to define a NPC.
	 *
	 * More data could be added as it only supports NPCs for now
	 * but adding support for configurables is also possible I think
	 */
	struct script_metadata
	{
		std::string path;
		std::string map;
		int x;
		int y;
		int facing;
		std::string name;
		int sprite;
		std::unique_ptr<lua::bytecode> code = nullptr;
	};

	/**
	 * A wrapper for the lua function calls. Its job is to
	 * serve as an interface for the more complex lua C api
	 * while also taking advantange of RAII to handle the
	 * underlying pointer.
	 */
	class interpreter
	{
	public:
		~interpreter() noexcept;
		interpreter(const interpreter&) = delete;
		interpreter(interpreter&&) = delete;
		interpreter& operator=(const interpreter&) = delete;
		interpreter& operator=(interpreter&&) noexcept = delete;

	public:
		interpreter();

		/**
		 * A function which takes care of a script initialization.
		 * It will scan a file for metadata and determine wheter
		 * the data is valid or not. nullptr is returned in case
		 * of error.
		 */
		std::unique_ptr<script_metadata> extract_metadata(const std::string& path);

	private:
		struct impl;
		std::unique_ptr<impl> self;
	};

	/**
	 * This class will execute previously compiled bytecode.
	 * 
	 * It differs from the interpretor because the contexts
	 * in which the scripts run are different.
	 * 
	 * It's also expected to perform less (if any) checks when
	 * running the code given that the interpretor should be in
	 * charge of checking for errors before compiling.
	 */
	class executor
	{
	public:
		~executor() noexcept;
		executor(const executor&) = delete;
		executor(executor&&) = delete;
		executor& operator=(const executor&) = delete;
		executor& operator=(executor&&) noexcept = delete;

	public:
		executor(map_session_data* sd, npc_data* nd);

	public:
		bool run(const char* code, size_t size);
		void resume();

	private:
		struct impl;
		std::unique_ptr<impl> self;
	};
}

#endif
