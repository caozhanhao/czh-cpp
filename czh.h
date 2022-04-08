#pragma once
#include "node.h"
#include "parser.h"
#include <fstream>
#include <sstream>
#include <memory>
using czh::parser::Parser;
using czh::node::Node;
using czh::lexer::Lexer;
using czh::error::Err;
namespace czh
{
	std::shared_ptr<std::string> get_string_from_file(const std::string& path)
	{
		std::ifstream file{ path, std::ios::binary };
		std::stringstream ss;
		ss << file.rdbuf();
		return std::move(std::make_shared<std::string>(ss.str()));
	}
	class Czh
	{
		friend std::ostream& operator<<(std::ostream& os, Czh& czh);
	private:
		Lexer lexer;
		Parser parser;
	public:
		Czh(const std::string& czh_path)
			:lexer(get_string_from_file(czh_path), std::make_shared<std::string>(czh_path)) {}
		std::shared_ptr<Node> parse()
		{
			parser.set_tokens(lexer.get_all_token());
			return  parser.parse();
		}
	};
}