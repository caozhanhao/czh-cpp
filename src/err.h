//   Copyright 2022 caozhanhao
//
//   Licensed under the Apache License, Version 2.0 (the "License");
//   you may not use this file except in compliance with the License.
//   You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
//   Unless required by applicable law or agreed to in writing, software
//   distributed under the License is distributed on an "AS IS" BASIS,
//   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//   See the License for the specific language governing permissions and
//   limitations under the License.
#pragma once
#include <stdexcept>
#include <string>

#define _CZH_STRINGFY(x) #x
#define CZH_STRINGFY(x) _CZH_STRINGFY(x)
#define CZH_ERROR_LOCATION  __FILE__ ":line " CZH_STRINGFY(__LINE__)

namespace czh::error
{
  class CzhError : public std::runtime_error
  {
  private:
    std::string location;
    std::string detail;
  public:
    CzhError(std::string location_, const std::string &detail_)
        : runtime_error(detail_), location(std::move(location_)),
          detail(detail_)
    {}
    
    [[nodiscard]] std::string get_content() const
    {
      return {"\033[1;37m" + location + ":"
              + "\033[0;32;31m error : \033[m" + detail};
    }
  };
  
  class Error : public std::logic_error
  {
  private:
    std::string location;
    std::string detail;
  public:
    Error(std::string location_, std::string func_name_, const std::string &detail_)
        : logic_error(detail_), location(std::move(location_) + ":" + func_name_),
          detail(detail_)
    {}
    
    [[nodiscard]] std::string get_detail() const
    {
      return detail;
    }
    
    [[nodiscard]] std::string get_content() const
    {
      return {"\033[1;37m" + location + ":"
              + "\033[0;32;31m error : \033[m" + detail};
    }
  };
}