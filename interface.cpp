#include <vector>
#include <string>
#include <stdexcept>
#include <memory>


#include <boost/dll.hpp>
#include <re2/re2.h>

#include <rostrum/api.hpp>

class regex_adapter {
public:
  regex_adapter() = default;

  explicit regex_adapter(const char* const pattern) {
    compile(pattern);
  }

  void compile(const char* const pattern) {
    RE2::Options opts = RE2::DefaultOptions;
    opts.set_utf8(false);
    opts.set_log_errors(false);

    re2_ = std::make_unique<RE2>(pattern, opts);

    if (!re2_->ok()) {
      throw std::runtime_error{"invalid regexp"};
    }

    groups_num_ = re2_->NumberOfCapturingGroups();
    strings_.resize(groups_num_);
    args_.resize(groups_num_);
    args_pointers_.resize(groups_num_);
    for (auto i = 0; i != groups_num_; ++i) {
      args_[i] = &strings_[i];
      args_pointers_[i] = &args_[i];
    }
  }

  [[nodiscard]]
  sol::lua_value partial_match(const char* const string) {
    const auto success = RE2::PartialMatchN(string, *re2_, std::data(args_pointers_), groups_num_);
    if (success) {
      return sol::as_table(strings_);
    }
    
    return sol::nil;
  }

  [[nodiscard]]
   sol::lua_value find_all(const char* const string) {
    std::vector<sol::lua_value> result;

    re2::StringPiece string_piece = string;
    bool success;
    do {
      success = RE2::FindAndConsumeN(&string_piece, *re2_, std::data(args_pointers_), groups_num_);
      if (success) {
        result.push_back(sol::as_table(strings_));
      }
    }
    while (success);

    if (!result.empty()) {
      return sol::as_table(std::move(result));
    }
    
    return sol::nil;
  }

private:
  std::unique_ptr<RE2> re2_;
  std::vector<std::string> strings_;
  std::vector<RE2::Arg> args_;
  std::vector<RE2::Arg*> args_pointers_;
  decltype(re2_->NumberOfCapturingGroups()) groups_num_{0};
};

namespace {
  sol::table imbue_lua(sol::state_view& lua) {
    // as long as we do not use multiples states this makes things easier
    sol::set_default_state(lua);

    auto table = lua.create_table();
    static_cast<void>(table.new_usertype<regex_adapter>(
      "expr", sol::constructors<regex_adapter(), regex_adapter(const char*)>(),
      "compile", &regex_adapter::compile,
      "partial_match", &regex_adapter::partial_match,
      "find_all", &regex_adapter::find_all));
    return table;
  }

  void query_info(rostrum::api::module_info& module_info_ptr) noexcept {
    module_info_ptr = rostrum::api::module_info{{"regex"}, {"regex"}, rostrum::api::module_version{1, 0}, imbue_lua};
  }

  BOOST_DLL_ALIAS(query_info, __rostrum_query_info);
}
