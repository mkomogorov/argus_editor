
#include "argus_editor_cmake.h"

#include <Magick++.h>
#include <string>
#include <cstring>
#include <iostream>

using namespace std;
using namespace Magick;

namespace argus
{

	bool space_(const char*& start_end)
	{
		auto start = start_end;
		while (isspace(*start_end)) start_end++;

		return start_end != start;
	}

	bool lexema_(const char*& start_end, string& lex)  // find lexem letters+digit+{}+-[:]_ are possible
	{
		auto isid = [](char ch)
		{
			return isalnum(ch) || strchr("{}+-[]_", ch) != NULL;
		};

		auto start = start_end;
		for (; isid(*start_end); start_end++);
		lex.append(start, start_end);

		return start != start_end;
	}

	bool next_lexema_(const char*& start_end, string& lex)  // skip_space and find lexem 
	{
		space_(start_end);
		return lexema_(start_end, lex);
	}

	bool number_(const char*& start_end, int& value)  // find positive integer value
	{
		auto src = start_end;
		value = 0;
		if (isdigit(*src))
		{
			value = int(*src - '0');
			for (*src++; isdigit(*src); src++)
				value = value * 10 + int(*src - '0');

			start_end = src;
			return true;
		}
		return false;
	}

	bool file_(const char*& start_end, string& lex)  // find file name. The following character can be used in file_name ".,%@#$&^+-\\/_:{}" as well
	{
		auto isfile = [](char ch)
		{
			return isalnum(ch) || strchr(".,%@#$&^+-\\/_:", ch) != NULL;
		};

		auto start = start_end;
		for (; isfile(*start_end); start_end++);
		lex.append(start, start_end);

		return start != start_end;
	}

	class Runner
	{
	public:
		class Command  // Base class for command implementation
		{
		public:
			Command() : tags(0), description(0) {}

			virtual int   run(Runner& runner, const char*& src) = 0;

			Command& set(const char** t, const char* d)
			{
				tags = t;
				description = d;

				return *this;
			}
			const char** tags;          // list of lexemas for this command for instance "quit", "exit", "q"
			const char* description;
		};

		struct Data_item // struct for building list of named data
		{
			Data_item(const string& item_name) : next(0), id(item_name) {}

			std::string   id;
			Image         image;
			Data_item* next;
		};

		Runner() :first_item(0) {}
		~Runner()
		{
			for (auto it = first_item; it; )
			{
				auto next = it->next;
				delete it;
				it = next;
			}
		}

		void register_command(Command* command)  // add new command to common pool
		{
			commands.push_back(command);
		}

		Command* find_command(const string& tag)
		{
			//for (auto cmd : commands)
			for (auto it = commands.begin(); it != commands.end(); it++)
			{
				for (auto list = (*it)->tags; *list; list++)
				{
					if (strcmp(tag.c_str(), *list) == 0)
						return (*it);
				}
			}
			return 0;
		}

		Data_item* find_data(const string& id)
		{
			for (auto it = first_item; it; it = it->next)
			{
				if (strcmp(id.c_str(), it->id.c_str()) == 0)
					return it;
			}
			return 0;
		}


		void replace_or_insert(const string& id, Data_item* data_item)
		{
			if (data_item)
			{
				if (first_item == 0)
				{
					data_item->next = 0;
					first_item = data_item;
					return;
				}
				if (strcmp(first_item->id.c_str(), id.c_str()) == 0)
				{
					data_item->next = first_item->next;
					delete first_item;
					first_item = data_item;
					return;
				}
				for (auto it = first_item; it; it = it->next)
				{
					if (it->next)
					{
						if (strcmp(id.c_str(), it->next->id.c_str()) == 0)
						{
							data_item->next = it->next->next;
							delete it->next;
							it->next = data_item;
							return;
						}
					}
				}
				data_item->next = first_item;
				first_item = data_item;
			}
		}

		void run()
		{
			char line[1024];
			while (1)
			{
				line[0] = 0;
				if (fgets(line, 1023, stdin))
				{
					const char* ptr = line;
					space_(ptr);

					string command_name;
					lexema_(ptr, command_name);

					if (auto cmd = find_command(command_name))
					{
						if (cmd->run(*this, ptr) < 0)
							return;

						continue;
					}

					// come here if error or command not found
					printf("Undefined command: %s\nUse help to get info, or press 'q' to exit\n", line);
				}
			}
		}

		std::vector<Command*> commands;
		Data_item* first_item;
	};


	Runner::Command* load_cmd()      // команда загузки файла 
	{
		class Cmd : public Runner::Command
		{
		public:
			virtual int run(Runner& runner, const char*& src)
			{
				Runner::Data_item* data = 0;
				string id;
				try
				{
					string file_name;

					next_lexema_(src, id);
					space_(src);
					file_(src, file_name);

					if (id.empty() || file_name.empty())
					{
						printf("Bad syntax use:\n%s\n", description);
						return 0;
					}
					if (data = new Runner::Data_item(id.c_str()))
						data->image.read(file_name);
				}
				catch (exception& e)
				{
					delete data;
					data = 0;
					printf("%s\n", e.what());
				}
				runner.replace_or_insert(id.c_str(), data);

				return 0;
			}
		};

		static const char* tags[] = { "load", "ld", 0 };
		static Cmd cmd;
		cmd.set(tags,
			"load  <name>  <filename>\n"
			"   name     - id of named object.  <name> is created if it does note exist\n"
			"   filename - file name to load image\n\n");

		return &cmd;
	}

	Runner::Command* store_cmd()      // команда сохранения файла 
	{
		class Cmd : public Runner::Command
		{
		public:
			virtual int run(Runner& runner, const char*& src)
			{
				try
				{
					string  id;
					string  file_name;

					space_(src);
					lexema_(src, id);
					space_(src);
					file_(src, file_name);

					if (id.empty() || file_name.empty())
					{
						printf("Bad syntax use:\n%s\n", description);
						return 0;
					}

					if (auto data = runner.find_data(id))
						data->image.write(file_name);
					else
					{
						printf("Named object %s not found\n", id.c_str());
						return 0;
					}


				}
				catch (std::exception& e)
				{
					printf("%s\n", e.what());
				}
				return 0;
			}
		};

		static const char* tags[] = { "store", "s", 0 };
		static Cmd cmd;
		cmd.set(tags,
			"store  <name>  <filename>\n"
			"   name   - id of named object\n"
			"   filename - file name to save image\n\n");

		return &cmd;
	}

	Runner::Command* resize_cmd()      // команда изменения размера
	{
		class Cmd : public Runner::Command
		{
		public:
			virtual int run(Runner& runner, const char*& src)
			{
				string   dst_id;
				Runner::Data_item* dst_data = 0;
				try
				{
					string   src_id;
					next_lexema_(src, src_id);
					next_lexema_(src, dst_id);
					space_(src);

					int      width;
					bool wret = number_(src, width);
					space_(src);

					int      height;
					bool hret = number_(src, height);
					if (!hret || src_id.empty() || dst_id.empty() || !wret)
					{
						printf("Bad syntax, use:\n%s\n", description);
						return 0;
					}

					auto src_data = runner.find_data(src_id);
					if (!src_data)
					{
						printf("Name %s not found\n", src_id.c_str());
						return 0;
					}
					if (dst_data = new Runner::Data_item(dst_id.c_str()))
					{
						Geometry geometry(width, height);
						Magick::FilterType filter(GaussianFilter);

						dst_data->image = src_data->image;
						dst_data->image.filterType(filter);
						dst_data->image.zoom(geometry);
						//    data->image.scale(geometry);
					}
				}
				catch (std::exception& e)
				{
					delete dst_data;
					dst_data = 0;
					printf("%s\n", e.what());
				}

				runner.replace_or_insert(dst_id, dst_data);
				return 0;
			}
		};

		static const char* tags[] = { "resize", 0 };
		static Cmd cmd;
		cmd.set(tags,
			"resize  <from_name>  <to_name> <width> <height>\n"
			"   <from_name>  - id of named source object\n"
			"   <to_name>    - id of named destination object. <to_name> is created if it does note exist\n"
			"   <width> <height> - must be positive integer value\n\n");
		return &cmd;
	}

	Runner::Command* blur_cmd()      // команда изменения размера
	{
		class Cmd : public Runner::Command
		{
		public:
			virtual int run(Runner& runner, const char*& src)
			{
				string   dst_id;
				Runner::Data_item* dst_data = 0;
				try
				{
					string   src_id;
					next_lexema_(src, src_id);
					next_lexema_(src, dst_id);
					space_(src);

					int      radius;
					if (!number_(src, radius) || src_id.empty() || dst_id.empty())
					{
						printf("Bad syntax, use:\n%s\n", description);
						return 0;
					}

					auto src_data = runner.find_data(src_id);
					if (!src_data)
					{
						printf("Name %s not found\n", src_id.c_str());
						return 0;
					}
					if (dst_data = new Runner::Data_item(dst_id.c_str()))
					{
						dst_data->image = src_data->image;
						dst_data->image.blur(double(radius));
					}
				}
				catch (std::exception& e)
				{
					delete dst_data;
					dst_data = 0;
					printf("%s\n", e.what());
				}

				runner.replace_or_insert(dst_id, dst_data);
				return 0;
			}
		};

		static const char* tags[] = { "blur", 0 };
		static Cmd cmd;
		cmd.set(tags,
			"blur  <from_name>  <to_name> <radius> \n"
			"   <from_name> - id of named source object\n"
			"   <to_name>   - id of named destination object. <to_name> is created if it does note exist\n"
			"   <radius>    - specifies the radius of the Gaussian, in pixels, not counting the center pixel. Must be positive integer value\n\n");
		return &cmd;
	}

	Runner::Command* help_cmd()  // help command     
	{
		class Cmd : public Runner::Command
		{
		public:
			virtual int run(Runner& runner, const char*& src)
			{
				string id;
				if (space_(src))
				{
					if (lexema_(src, id))
					{
						if (auto cmd = runner.find_command(id))
						{
							printf("%s\n", cmd->description);
							return 0;
						}
					}
				}
				//            for(auto cmd:runner.commands)
				for (auto it = runner.commands.begin(); it != runner.commands.end(); it++)
					printf("%s\n", (*it)->description);

				return 0;
			}
		};

		static const char* tags[] = { "help", "h", 0 };
		static Cmd cmd;
		cmd.set(tags,
			"help  [command_name]\n"
			"  command_name\n"
			"  if <command_name> not set full list will be present\n\n");
		return &cmd;
	}


	Runner::Command* exit_cmd()    // команда завершить работу 
	{
		class Cmd : public Runner::Command
		{
		public:
			virtual int run(Runner& runner, const char*& src)
			{
				return -1;
			}
		};

		static const char* tags[] = { "exit", "quit", "q", 0 };
		static Cmd cmd;
		cmd.set(tags, "exit|quit|q   - exit\n");
		return &cmd;
	}

}

int main(int /*argc*/, char** argv)
{

	// Initialize ImageMagick install location for Windows
	InitializeMagick(*argv);

	printf("Welcome to argus image editor\n\n"
		"enter help - to show available commands list\n"
		"or q - to exit\n");
	try
	{
		// string srcdir("");
		// if(getenv("SRCDIR") != 0)
		//   srcdir = getenv("SRCDIR");

		argus::Runner runner;
		runner.register_command(argus::load_cmd());
		runner.register_command(argus::store_cmd());
		runner.register_command(argus::resize_cmd());
		runner.register_command(argus::blur_cmd());
		runner.register_command(argus::help_cmd());
		runner.register_command(argus::exit_cmd());

		runner.run();

	}
	catch (exception& error_)
	{
		printf("Caught exception: %s\n", error_.what());
		return 1;
	}

	return 0;
}

