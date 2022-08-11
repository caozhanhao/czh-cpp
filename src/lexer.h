﻿#pragma once

#include "value.h"
#include "err.h"
#include "utils.h"

#include <memory>
#include <limits>
#include <fstream>
#include <charconv>
#include <vector>
#include <sstream>
#include <queue>
#include <string>
#include <set>
#include <map>
#include <algorithm>
#include <cstdio>
using czh::error::Error;
using czh::value::Value;
namespace czh::lexer
{
  enum class TokenType
  {
    ID,
    INT, LONGLONG, DOUBLE, STRING, BOOL,
    EQUAL,//=
    ARR_LP, ARR_RP,//[]
    COMMA, COLON,
    BPATH,//-
    FEND, SEND, SCEND,
    NOTE,
    UNEXPECTED
  };
  
  const std::map<char, TokenType> marks =
      {
          {'=', TokenType::EQUAL},
          {'[', TokenType::ARR_LP},
          {']', TokenType::ARR_RP},
          {':', TokenType::COLON},
          {'-', TokenType::BPATH},
          {';', TokenType::SEND},
          {',', TokenType::COMMA}
      };
  
  bool is_newline_and_next(const std::string &str, std::size_t &pos, int delta = 1)
  {
    if (str[pos] == '\r')
    {
      if (pos + 1 < str.size() && str[pos + 1] == '\n')
      {
        pos += 2 * delta;
        return true;
      }
      else
        pos += 1 * delta;
      return true;
    }
    else if (str[pos] == '\n')
    {
      pos += 1 * delta;
      return true;
    }
    return false;
  }
  
  class File
  {
  public:
    std::string filename;
    
    explicit File(std::string name)
        : filename(std::move(name))
    {}
    
    [[nodiscard]] virtual std::string
    get_spec_line(std::size_t beg, std::size_t end, std::size_t linenosize) const = 0;
    
    [[nodiscard]] virtual std::size_t get_lineno(std::size_t pos) const = 0;
    
    [[nodiscard]] virtual std::size_t get_arrowpos(std::size_t pos) const = 0;
    
    [[nodiscard]] virtual std::string get_name() const = 0;
    
    [[nodiscard]] virtual std::size_t size() const = 0;
    
    [[nodiscard]] virtual char view(int s) = 0;
    
    virtual void ignore(std::size_t s) = 0;
    
    [[nodiscard]] virtual bool check(std::size_t s) = 0;
  };
  
  class StreamFile : public File
  {
  public:
    std::unique_ptr<std::ifstream> file;
    std::size_t file_size;
    std::deque<char> buffer;
    std::size_t bufferpos;
  public:
    StreamFile(std::string name_, std::unique_ptr<std::ifstream> fs_)
        : File(std::move(name_)), file(std::move(fs_)), bufferpos(0)
    {
      if(!file->good())
        throw error::Error(CZH_ERROR_LOCATION, __func__, "Error File.");
      file->ignore(std::numeric_limits<std::streamsize>::max());
      file_size = file->gcount();
      file->clear();
      file->seekg(std::ios_base::beg);
    }
    
    [[nodiscard]] std::string get_spec_line(std::size_t beg, std::size_t end, std::size_t linenosize) const override
    {
      std::string ret;
      if (linenosize == 0)
        linenosize = utils::to_str(end).size();
  
      std::string tmp;
      
      std::string buffer;
      file->clear();
      file->seekg(std::ios::beg);
      for (int a = 1; std::getline(*file, tmp); a++)
      {
        if (beg <= a && a < end)
        {
          std::string addition = utils::to_str(a);
          if (addition.size() < linenosize)
            buffer += std::string(linenosize - addition.size(), '0');
          buffer += addition + " ";
          buffer += tmp;
          buffer += "\n";
        }
      }
      buffer.pop_back();
      return buffer;
    }
    
    [[nodiscard]] std::size_t get_lineno(std::size_t pos) const override
    {
      std::size_t lineno = 1;
      std::size_t postmp = 0;
      std::string tmp;
      file->clear();
      file->seekg(std::ios::beg);
      for (; std::getline(*file, tmp); lineno++)
      {
        postmp += tmp.size() + 1;
        if (postmp >= pos) break;
      }
      return lineno;
    }
    
    [[nodiscard]] std::size_t get_arrowpos(std::size_t pos) const override
    {
      std::size_t postmp = 0;
      std::string tmp;
      file->clear();
      file->seekg(std::ios::beg);
      while (std::getline(*file, tmp))
      {
        if (postmp + tmp.size() >= pos) return pos - postmp;
        postmp += tmp.size() + 1;
      }
      return 0;
    }
    
    [[nodiscard]] std::string get_name() const override
    {
      return filename;
    }
    
    [[nodiscard]] std::size_t size() const override
    {
      return file_size;
    }
    
    void ignore(std::size_t s) override
    {
      bufferpos += s;
    }
    
    [[nodiscard]] char view(int s) override
    {
      if(buffer.size() <= s)
        write_buffer();
      return buffer[bufferpos + s];
    }
    
    [[nodiscard]] bool check(std::size_t s) override
    {
      if(buffer.size() <= bufferpos + s)
        write_buffer();
      return (bufferpos + s < buffer.size());
    }
  
  private:
    void write_buffer()
    {
      while(bufferpos >= 10)
      {
        buffer.pop_front();
        --bufferpos;
      }
      int i = 0;
      while(i < 1024 && !file->eof())
      {
        buffer.emplace_back(file->get());
        ++i;
      }
      
      if(buffer.back() == -1)
        buffer.pop_back();
    }
  };
  
  class NonStreamFile : public File
  {
  public:
    std::string code;
    std::size_t codepos;
  public:
    NonStreamFile(std::string name, std::string code_)
        : File(std::move(name)), code(std::move(code_)), codepos(0)
    {}
    
    [[nodiscard]] std::string get_spec_line(std::size_t beg, std::size_t end, std::size_t linenosize) const override
    {
      std::string ret;
      if (linenosize == 0)
        linenosize = utils::to_str(end).size();
      std::size_t lineno = 1;
      bool first_line_flag = false;
      auto add = [&]()
      {
        std::string addition = utils::to_str(lineno);
        if (addition.size() < linenosize)
          ret += std::string(linenosize - addition.size(), '0');
        ret += addition + " ";
      };
      for (std::size_t i = 0; i < code.size();)
      {
        if (lineno >= beg && lineno < end)
        {
          if (!first_line_flag && lineno == 1)
          {
            add();
            first_line_flag = true;
          }
          if (code[i] != '\r' && code[i] != '\n')
            ret += code[i];
        }
        if (is_newline_and_next(code, i))
        {
          lineno++;
          if (lineno >= beg && lineno < end)
          {
            ret += '\n';
            add();
          }
        }
        else
          i++;
      }
      ret.erase(ret.begin());
      return ret;
    }
    
    [[nodiscard]] std::size_t get_lineno(std::size_t pos) const override
    {
      std::size_t lineno = 1;
      for (std::size_t i = 0; i < pos;)
      {
        if (is_newline_and_next(code, i))
          lineno++;
        else
          i++;
      }
      return lineno;
    }
    
    [[nodiscard]] std::size_t get_arrowpos(std::size_t pos) const override
    {
      std::size_t i = pos;
      while (i > 0)
      {
        if (is_newline_and_next(code, i, -1))
          break;
        else i--;
      }
      if (code[i] == '\r' && i + 1 < code.size() && code[i + 1] == '\n')
        return pos - i - 2;
      return pos - i - 1;
    }
    
    [[nodiscard]] std::string get_name() const override
    {
      return filename;
    }
    
    [[nodiscard]] std::size_t size() const override
    {
      return code.size();
    }
    
    void ignore(std::size_t s) override
    {
      codepos += s;
    }
    
    [[nodiscard]] char view(int s) override
    {
      return code[codepos + s];
    }
    
    [[nodiscard]] bool check(std::size_t s) override
    {
      return (codepos + s) < code.size();
    }
  };
  
  class Pos
  {
  public:
    std::size_t pos;
    std::size_t size;
    std::shared_ptr<File> code;
  public:
    explicit Pos(std::shared_ptr<File> code_)
        : pos(0), size(0), code(std::move(code_))
    {}
    
    Pos &operator+=(const std::size_t &p)
    {
      pos += p;
      return *this;
    }
    
    Pos &operator-=(const std::size_t &p)
    {
      pos -= p;
      return *this;
    }
    
    [[nodiscard]] std::string location() const
    {
      return (code->get_name() + ":line " + utils::to_str(code->get_lineno(pos)));
    }
    
    [[nodiscard]] std::size_t get() const
    {
      return pos;
    }
  
  public:
    Pos &set_size(std::size_t s)
    {
      size = s;
      return *this;
    }
    
    [[nodiscard]] std::unique_ptr<std::string> get_details_from_code() const
    {
      constexpr std::size_t last = 3;
      constexpr std::size_t next = 3;
      std::size_t lineno = code->get_lineno(pos);
      std::size_t linenosize = utils::to_str(lineno + next).size();
      std::size_t actual_last = last;
      std::size_t actual_next = next;
      std::size_t total_line = code->get_lineno(code->size() - 1);
      
      while(lineno - actual_last <= 0 && actual_last > 0)
        --actual_last;
      while(lineno + actual_next >= total_line && actual_next > 0)
        --actual_next;
      
      std::string temp1,temp2;
      if(actual_last != 0)
        temp1 += code->get_spec_line(lineno - actual_last, lineno + 1, linenosize);//[beg, end)
      if(actual_next != 0)
        temp2 = code->get_spec_line(lineno + 1, lineno + actual_next + 1, linenosize);
      
      std::string arrow("\n");
      arrow += std::string(code->get_arrowpos(pos) - size + linenosize + 1, ' ');
      arrow += "\033[0;32;32m";
      arrow.insert(arrow.end(), size, '^');
      arrow += "\033[m\n";
      
      std::string errorstring = temp1 + arrow + temp2;
      return std::move(std::make_unique<std::string>(errorstring));
    }
  };
  template<typename T>
  std::string to_token_str(const T&v) {return utils::to_str(v);}
  template <>
  std::string to_token_str(const std::string& v) { return v; }
  //not use
  template <>
  std::string to_token_str(const value::Note& v) { return ""; }
  template <>
  std::string to_token_str(node::Node* const& v) { return ""; }
  template<typename Ty>
  std::string to_token_str(const std::vector<Ty> &v){ return ""; }
  //end
  class Token
  {
  public:
    TokenType type;
    Value what;
    Pos pos;
  public:
    template<typename T>
    Token(TokenType type_, T what_, Pos pos_)
        :type(type_), what(std::move(what_)), pos(std::move(pos_))
    {}
    
    void error(const std::string &details) const
    {
      throw Error(pos.location(), __func__, details + ": \n"
                                            + *(pos.get_details_from_code()));
    }
    
    [[nodiscard]] std::string get_string() const
    {
      return std::visit(
          utils::overloaded{
            [](auto&& i)->auto{return czh::lexer::to_token_str(i);},
            [](char i)->auto{return std::string(1, i);}
          }
      , what.get_variant());
    }
  };
  
  enum class State
  {
    INIT,
    ID, VALUE,
    ARR_VALUE, EQUAL, ARR_LP, ARR_RP,
    COMMA, SC_COLON, PATH_COLON, BPATH, PATH_ID_TARGET, PATH_ID,
    UNEXPECTED, END
  };
  
  class Match
  {
  private:
    State state;
    State last_state;
  public:
    Match() : state(State::INIT), last_state(State::UNEXPECTED)
    {}
    
    std::string error_correct()
    {
      switch (last_state)
      {
        case State::INIT:
        case State::PATH_COLON:
        case State::BPATH:
          return "identifier";
        case State::ID:
          return "'=' or ':'";
        case State::EQUAL:
          return "value or '['";
        case State::ARR_LP:
          return "value or ']'";
        case State::ARR_VALUE:
          return "']' or ','";
        case State::COMMA:
          return "value";
        case State::PATH_ID:
          return "'-' or ':'";
        default:
          throw error::Error(CZH_ERROR_LOCATION, __func__, "Unexpected state.");
      }
      return "";
    }
    State get_state() const
    {
      return state;
    }
    void match(const TokenType &token)
    {
      if (token == TokenType::NOTE) return;
      switch (state)
      {
        case State::INIT:
          switch (token)
          {
            case TokenType::ID:
              state = State::ID;
              break;
            case TokenType::SCEND:
              state = State::END;
              break;
            default:
              last_state = state;
              state = State::UNEXPECTED;
              break;
          }
          break;
        case State::ID:
          switch (token)
          {
            case TokenType::EQUAL:
              state = State::EQUAL;
              break;
            case TokenType::COLON:
              state = State::END;
              break;
            default:
              last_state = state;
              state = State::UNEXPECTED;
              break;
          }
          break;
        case State::EQUAL:
          switch (token)
          {
            case TokenType::INT:
            case TokenType::LONGLONG:
            case TokenType::DOUBLE:
            case TokenType::STRING:
            case TokenType::BOOL:
              state = State::END;
              break;
            case TokenType::ARR_LP:
              state = State::ARR_LP;
              break;
            case TokenType::BPATH:
              state = State::BPATH;
              break;
            default:
              last_state = state;
              state = State::UNEXPECTED;
              break;
          }
          break;
        case State::ARR_LP:
          switch (token)
          {
            case TokenType::INT:
            case TokenType::LONGLONG:
            case TokenType::DOUBLE:
            case TokenType::STRING:
            case TokenType::BOOL:
              state = State::ARR_VALUE;
              break;
            case TokenType::ARR_RP:
              state = State::END;
              break;
            default:
              last_state = state;
              state = State::UNEXPECTED;
              break;
          }
          break;
        case State::ARR_VALUE:
          switch (token)
          {
            case TokenType::COMMA:
              state = State::COMMA;
              break;
            case TokenType::ARR_RP:
              state = State::END;
              break;
            default:
              last_state = state;
              state = State::UNEXPECTED;
              break;
          }
          break;
        case State::COMMA:
          switch (token)
          {
            case TokenType::INT:
            case TokenType::LONGLONG:
            case TokenType::DOUBLE:
            case TokenType::STRING:
            case TokenType::BOOL:
              state = State::ARR_VALUE;
              break;
            default:
              last_state = state;
              state = State::UNEXPECTED;
              break;
          }
          break;
        case State::BPATH:
          switch (token)
          {
            case TokenType::ID:
              state = State::PATH_ID;
              break;
            default:
              last_state = state;
              state = State::UNEXPECTED;
              break;
          }
          break;
        case State::PATH_ID:
          switch (token)
          {
            case TokenType::BPATH:
              state = State::BPATH;
              break;
            case TokenType::COLON:
              state = State::PATH_COLON;
              break;
            default:
              last_state = state;
              state = State::UNEXPECTED;
              break;
          }
          break;
        case State::PATH_COLON:
          switch (token)
          {
            case TokenType::ID:
              state = State::END;
              break;
            default:
              last_state = state;
              state = State::UNEXPECTED;
              break;
          }
          break;
        case State::VALUE:
        case State::ARR_RP:
        case State::SC_COLON:
        case State::PATH_ID_TARGET:
          break;
        case State::UNEXPECTED:
          throw error::Error(CZH_ERROR_LOCATION, __func__,
                             "Unexpected state can not match.");
        case State::END:
          if(token != TokenType::SEND && token != TokenType::FEND)
            throw error::Error(CZH_ERROR_LOCATION, __func__, "Unexpected end.");
          else
            reset();
          break;
        default:
          throw error::Error(CZH_ERROR_LOCATION, __func__, "Unexpected state.");
      }
    }
    
    bool good()
    {
      return state != State::UNEXPECTED;
    }
    bool end()
    {
      return state == State::END;
    }
    void reset()
    {
      state = State::INIT;
      last_state = State::UNEXPECTED;
    }
  };
  
  std::string get_string_from_file(const std::string &path)
  {
    std::ifstream file{path, std::ios::binary};
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
  }
  
  class NumberMatch
  {
  private:
    enum class State
    {
      INIT, INT, INT_DOT, SIGN, DOT, DOT_NO_INT, EXP, EXP_SIGN, EXP_INT, END, UNEXPECTED
    };
    enum class Token
    {
      INT, DOT, SIGN, EXP, UNEXPECTED, END
    };
    State state;
    bool _is_double;
  public:
    NumberMatch() : state(State::INIT), _is_double(false)
    {}
    
    bool match(const std::string &s)
    {
      for (auto &ch: s)
      {
        auto token = get_token(ch);
        if (token == Token::UNEXPECTED) return false;
        next(token);
        if (state == State::UNEXPECTED) return false;
      }
      next(get_token());
      if (state != State::END) return false;
      return true;
    }
    
    [[nodiscard]]bool is_double() const
    {
      return _is_double;
    }
    
    void reset()
    {
      state = State::INIT;
      _is_double = false;
    }
  
  private:
    Token get_token(int ch = -1)
    {
      if (std::isdigit(ch))
        return Token::INT;
      else if (ch == '.')
      {
        _is_double = true;
        return Token::DOT;
      }
      else if (ch == 'e' || ch == 'E')
        return Token::EXP;
      else if (ch == '+' || ch == '-')
        return Token::SIGN;
      else if (ch == -1)
        return Token::END;
      return Token::UNEXPECTED;
    }
    
    void next(const Token &token)
    {
      switch (state)
      {
        case State::INIT:
          switch (token)
          {
            case Token::INT:
              state = State::INT;
              break;
            case Token::DOT:
              state = State::DOT_NO_INT;
              break;
            case Token::SIGN:
              state = State::SIGN;
              break;
            default:
              state = State::UNEXPECTED;
              break;
          }
          break;
        case State::SIGN:
          switch (token)
          {
            case Token::INT:
              state = State::INT;
              break;
            case Token::DOT:
              state = State::DOT_NO_INT;
              break;
            default:
              state = State::UNEXPECTED;
              break;
          }
          break;
        case State::INT:
          switch (token)
          {
            case Token::INT:
              break;
            case Token::DOT:
              state = State::DOT;
              break;
            case Token::EXP:
              state = State::EXP;
              break;
            case Token::END:
              state = State::END;
              break;
            default:
              state = State::UNEXPECTED;
              break;
          }
          break;
        case State::INT_DOT:
          switch (token)
          {
            case Token::INT:
              break;
            case Token::EXP:
              state = State::EXP;
              break;
            case Token::END:
              state = State::END;
              break;
            default:
              state = State::UNEXPECTED;
              break;
          }
          break;
        case State::DOT_NO_INT:
          switch (token)
          {
            case Token::INT:
              state = State::INT_DOT;
              break;
            default:
              state = State::UNEXPECTED;
              break;
          }
          break;
        case State::DOT:
          switch (token)
          {
            case Token::INT:
              state = State::INT_DOT;
              break;
            case Token::EXP:
              state = State::EXP;
              break;
            case Token::END:
              state = State::END;
              break;
            default:
              state = State::UNEXPECTED;
              break;
          }
          break;
        case State::EXP:
          switch (token)
          {
            case Token::INT:
              state = State::EXP_INT;
              break;
            case Token::SIGN:
              state = State::EXP_SIGN;
              break;
            default:
              state = State::UNEXPECTED;
              break;
          }
          break;
        case State::EXP_SIGN:
          switch (token)
          {
            case Token::INT:
              state = State::EXP_INT;
              break;
            default:
              state = State::UNEXPECTED;
              break;
          }
          break;
        case State::EXP_INT:
          switch (token)
          {
            case Token::INT:
              break;
            case Token::END:
              state = State::END;
              break;
            default:
              state = State::UNEXPECTED;
              break;
          }
          break;
        default:
          state = State::UNEXPECTED;
          break;
      }
    }
  };
  
  inline bool isnumber(const char &ch)
  {
    return std::isdigit(ch) || ch == '.' || ch == 'e' || ch == 'E' || ch == '+' || ch == '-';
  }
  
  class Lexer
  {
  private:
    std::shared_ptr<File> code;
    std::deque<Token> tokenstream;
    Match match;
    NumberMatch nmatch;
    Pos codepos;
    bool parsing_path;
    bool is_eof;
  
  public:
    Lexer(const std::string &path, const std::string &_filename)
        : code(std::make_shared<NonStreamFile>(_filename, get_string_from_file(path))),
          codepos(code),
          parsing_path(false),
          is_eof(false)
    {}
    
    explicit Lexer(std::string code_str)
        : code(std::make_shared<NonStreamFile>("nonstream_temp", std::move(code_str))),
          codepos(code),
          parsing_path(false),
          is_eof(false)
    {}
    
    explicit Lexer(std::unique_ptr<std::ifstream> fs)
        : code(std::make_shared<StreamFile>("stream_temp", std::move(fs))),
          codepos(code),
          parsing_path(false),
          is_eof(false)
    {}
    
    Token view(std::size_t s)
    {
      if (tokenstream.size() <= s)
      {
        while (tokenstream.size() < 1024)
        {
          auto t = get_tok();
          check_token(t);
          tokenstream.emplace_back(t);
          if (t.type == TokenType::FEND)
          {
            is_eof = true;
            break;
          }
        }
      }
      return tokenstream[s];
    }
    
    void next(const std::size_t &s = 1)
    {
      tokenstream.pop_front();
    }
    
    [[nodiscard]]bool eof() const
    {
      return is_eof && tokenstream.empty();
    }
  
  private:
    void check_token(const Token &token)
    {
      if(token.type == TokenType::FEND)
      {
        if(match.get_state() == State::END || match.get_state() == State::INIT)
          return;
        else
          token.error("Unexpected end of file.");
      }
      if(match.end() && token.type != TokenType::SEND)
        match.match(TokenType::SEND);
      match.match(token.type);
      if (!match.good())
      {
        token.error("Unexpected token '" + token.get_string() + "'.Do you mean '"
                    + match.error_correct() + "'?");
      }
    }
    
    Pos get_pos()
    {
      return codepos;
    }
    
    Token get_tok()
    {
      while (check_char() && isspace(view_char()))
        next_char();
      
      bool is_num = false;
      if (!parsing_path && check_char() && (std::isdigit(view_char()) || view_char() == '.' || view_char() == '+' ||
                                            view_char() == '-'))
      {
        is_num = true;
        if (view_char() == '-')
        {
          if (check_char(1) && !(std::isdigit(view_char(1)) || view_char(1) == '.'))
            is_num = false; //-id
          if (check_char(2) && view_char(1) == '.' && view_char(2) == '.')
            is_num = false; //-..
          if (check_char(2) && view_char(1) == '.' && view_char(2) == '-')
            is_num = false;//-.-
          if (check_char(2) && view_char(1) == '.' && view_char(2) == ':')
            is_num = false;//-.:
        }
      }
      
      if (is_num)
      {
        std::string temp;
        do
        {
          temp += view_char();
          next_char();
        } while (check_char() && isnumber(view_char()));
        if (nmatch.match(temp))
        {
          if (nmatch.is_double())
          {
            nmatch.reset();
            return {TokenType::DOUBLE, utils::str_to<double>(temp), get_pos().set_size(temp.size())};
          }
          else
          {
            nmatch.reset();
            auto t = utils::str_to<double>(temp);
            if (t < std::numeric_limits<int>().max())
              return {TokenType::INT, (int) t, get_pos().set_size(temp.size())};
            else
              return {TokenType::LONGLONG, (long long) t, get_pos().set_size(temp.size())};
          }
        }
        else
        {
          Token tmp(TokenType::UNEXPECTED, 0, get_pos().set_size(temp.size()));
          tmp.error("Unexpected token '" + temp + "'.Is this a number?");
        }
      }
      else if (check_char() && view_char() == '"')//str
      {
        std::string temp;
        next_char();//eat '"'.
        while (check_char() && view_char() != '"')
        {
          temp += view_char();
          next_char();
        }
        next_char();//eat '"'
        return {TokenType::STRING, temp, get_pos().set_size(temp.size())};
      }
      else if ((check_char() && (isalpha(view_char()) || view_char() == '_')) || (check_char() && parsing_path &&
                                                                                  view_char() == '.'))//id
      {
        std::string temp;
        if (parsing_path && view_char() == '.')
        {
          temp = ".";
          next_char();
          if (check_char() && view_char() == '.')
          {
            temp += ".";
            next_char();
          }
        }
        while (check_char() && (isalnum(view_char()) || view_char() == '_') && view_char() != '-')
        {
          temp += view_char();
          next_char();
        }
        
        if (temp == "end")
          return {TokenType::SCEND, temp, get_pos().set_size(3)};
        else if (temp == "true")
          return {TokenType::BOOL, true, get_pos().set_size(4)};
        else if (temp == "false")
          return {TokenType::BOOL, false, get_pos().set_size(5)};
        else
          return {TokenType::ID, temp, get_pos().set_size(temp.size())};
      }
      else if (check_char() && marks.find(view_char()) != marks.end())//mark
      {
        next_char();
        if (marks.at(view_char(-1)) == TokenType::BPATH) parsing_path = true;
        if (parsing_path && marks.at(view_char(-1)) == TokenType::COLON) parsing_path = false;
        return {marks.at(view_char(-1)), view_char(-1), get_pos().set_size(1)};
      }
      else if (check_char(2) && view_char() == '/' && view_char(1) == 'b' && view_char(2) == '/')//note
      {
        std::string temp;
        next_char(3);//eat '/b/'
        while (!(check_char(2) && view_char() == '/' && view_char(1) == 'e' && view_char(2) == '/'))
        {
          temp += view_char();
          next_char();
        }
        next_char(3);//eat '/e/'
        return {TokenType::NOTE, value::Note(temp), get_pos().set_size(temp.size())};
      }
      else if (!check_char()) return {TokenType::FEND, 0, get_pos().set_size(0)};
      else
      {
        Token(TokenType::UNEXPECTED, 0, get_pos().set_size(0))
            .error(std::string("Unexpected token '" + std::string(1, view_char()) + "'."));
      }
      return {TokenType::UNEXPECTED, 0, get_pos().set_size(0)};
    }
    
    bool check_char(std::size_t s = 0)
    {
      return code->check(s);
    }
    
    char view_char(int s = 0)
    {
      return code->view(s);
    }
    
    void next_char(std::size_t s = 1)
    {
      code->ignore(s);
      codepos.pos += s;
    }
  };
}