#include <string_view>
#include <array>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <span>
#include <filesystem>
#include <algorithm>
#include <optional>
#include <cassert>

namespace inc_analizer
{
template<typename T>
using optional_ref = std::optional<std::reference_wrapper<T>>;

using std::string_view_literals::operator""sv;
namespace cmake_processed
{
  struct target_t
  {
    std::string_view target_name;
    constexpr auto operator <=>( target_t const & ) const noexcept = default;
  };
  struct source_file_t
  {
    std::string_view
       target_name,
       source_path;
  };
  struct target_include_t
  {
    std::string_view
       target_name,
       include_path;
  };
  struct target_dependency_t
  {
    std::string_view
       target_name,
       link_target;
  };
}

constexpr auto trim(std::string_view v) noexcept
{
  v.remove_prefix(std::min(v.find_first_not_of(" "), v.size()));
  v.remove_prefix(std::min(v.find_first_not_of("<\""), v.size()));
  using size_type = std::string_view::size_type;
  v.remove_suffix(v.size() - v.find_last_not_of(" \n\r\t") -1 );
  v.remove_suffix(v.size() - v.find_last_not_of(">\"") -1 );
  return v;
}

struct source_t
{
  std::string_view source_name;
  std::unordered_map<std::string_view, std::vector<source_t>> source_includes;
  constexpr auto operator <=>( source_t const & rh ) const noexcept 
    { return source_name <=> rh.source_name; }
};
struct file_node_t;

struct include_ref_t
  {
  file_node_t * ref;
  std::string include_path; //path with include can be found relative to include search path for target
  };
  
struct file_node_t
  {
  std::string full_path;
  file_node_t() noexcept = default;
  
  explicit file_node_t( std::string const& p) noexcept : full_path{p}{}
  ///\brief references with #include to other headers
  std::vector<include_ref_t> references;
  };
struct target_t
{
  std::string_view target_name;
  
  std::vector<std::string_view> include_paths;
  std::vector<std::string_view> interface_include_paths;
  
  std::vector<include_ref_t> private_includes, interface_includes;
  std::vector<file_node_t *> sources;
  std::vector<target_t *> references;
  
  constexpr auto operator <=>( target_t const & rh ) const noexcept
    { return target_name <=> rh.target_name; }
};

struct target_include_ref_t
  {
  file_node_t const * include;
  std::string include_relative_path;
  target_t const * target;
  };
  
auto find_include(std::vector<include_ref_t> const & includes, std::string_view include_with_subpath ) noexcept
  -> file_node_t *
  {
  auto it { std::find_if(std::begin(includes), std::end(includes), 
               [&include_with_subpath](include_ref_t const& ref)
               { return ref.include_path == include_with_subpath; }) };
  if( it != std::end(includes))
    return it->ref;
  return {};
  }
  
using target_map_type = std::unordered_map<std::string_view, target_t>;

namespace fs = std::filesystem;
struct include_collection_t
  {
  std::unordered_map<std::string, std::shared_ptr<file_node_t>> include_map;
  
  std::shared_ptr<file_node_t> insert_or_create(std::string const & absolute_path)
    {
    auto it_fn{ include_map.find(absolute_path) };
    if( include_map.end() == it_fn )
      std::tie(it_fn, std::ignore) = include_map.emplace(absolute_path,std::make_shared<file_node_t>(absolute_path));
    return it_fn->second;
    }
  };
static void scan_emplace_includes_for_target( std::ostream & out,
                                              include_collection_t & includes,
                                              std::string_view include_dir_absoulte_path,
                                              std::vector<include_ref_t> & target_relative_includes )
{
  for (fs::directory_entry const & dir_entry : fs::recursive_directory_iterator(include_dir_absoulte_path))
    {
    if(dir_entry.is_regular_file())
      {
      fs::path file_path{dir_entry.path()};
      auto ext{ file_path.extension().string() };
      if(ext.empty() || ext == ".h" || ext == ".hpp" || ext == ".inc")
        {
        std::string absoulte_path{file_path.string()};
        auto include { includes.insert_or_create(absoulte_path) };
        std::string inc_relative_path{absoulte_path.substr(include_dir_absoulte_path.size()+1)};
        out << " found include :" << inc_relative_path << '\n';
        target_relative_includes.emplace_back( include_ref_t{ include.get(), inc_relative_path});
        }
      }
    }
}
using header_guard_t = std::unordered_set<file_node_t const *>;
using header_stack_t = std::vector<target_include_ref_t>;
enum struct is_circular : bool { no, yes };

static is_circular check_if_is_circular(header_stack_t & header_stack, file_node_t const * include)
  {
  if( std::end(header_stack) != std::find_if( std::begin(header_stack), std::end(header_stack),
                                                [include](target_include_ref_t const & obj )
                                                { return include == obj.include; } ))
    return is_circular::yes;
  return is_circular::no;
  }

static void report_circular_dependency(std::ostream & out,
                                       header_stack_t & header_stack, target_t const & target,
                                       file_node_t const * include, std::string_view rel_path )
  {
  out << "\t\tcircular dependency for :" << target.target_name << ":" << rel_path/*include->full_path*/ << std::endl;
  for( auto it = std::rbegin(header_stack); std::rend(header_stack) != it; ++it )
    {
    out << "\t\t\t <-" << it->target->target_name << ":" << it->include_relative_path << std::endl;
    if( it->include == include )
      break;
    }
  }

enum struct processing_result_e : bool { failed, succeed };

static processing_result_e process_source(std::ostream & out,
                                          header_guard_t & header_guard,
                                          header_stack_t & header_stack,
                                          file_node_t const * include,
                                          std::string_view rel_path,
                                          target_t const & target)
  {
  auto processing_result { processing_result_e::succeed };
  header_stack.emplace_back( target_include_ref_t{ include, std::string{rel_path}, &target } );
  header_guard.emplace( include );
  
  std::fstream s(include->full_path, std::fstream::in );
  out << target.target_name << " :" << include->full_path << s.is_open() << std::endl;
  std::string line;
  while (std::getline(s, line))
    {
    std::string_view linev{ line };
//     out << "\t" << line << std::endl;
    auto pos{ linev.find("#include ") };
    if( pos != std::string::npos )
      {
      auto include{ trim(linev.substr(pos+9)) };
      out << "\t look for " << target.target_name << " -> " << include << std::endl;
      
      //search in own includes in private
      auto fn{ find_include(target.private_includes,include) };
      if( fn )
        {
        out << "\t\tinclude found in private :" << fn->full_path << std::endl;
        if(header_guard.end() == header_guard.find(fn))
          process_source(out, header_guard,header_stack,fn, include, target );
        else
          {
          //check for circular dependency
          if( is_circular::yes == check_if_is_circular(header_stack, fn ))
            {
            processing_result = processing_result_e::failed;
            report_circular_dependency(out, header_stack, target, fn, rel_path);
            report_circular_dependency(std::cout, header_stack, target, fn, rel_path);
            }
          }
        }
      else
        {
        //search in all targets 
        for( target_t const * ref : target.references )
          {
          fn = find_include(ref->interface_includes,include);
          if(fn)
            {
            out << "\t\tinclude found in referenced :" << ref->target_name << " :" << fn->full_path << std::endl;
            if(header_guard.end() == header_guard.find(fn))
              process_source(out, header_guard,header_stack,fn, include, *ref );
            else
              {
              //check for circular dependency
              if( is_circular::yes == check_if_is_circular(header_stack, fn ))
                {
                processing_result = processing_result_e::failed;
                report_circular_dependency(out, header_stack, target, fn, rel_path);
                report_circular_dependency(std::cout, header_stack, target, fn, rel_path);
                }
              }
            break;
            }
          }
        }
      if(fn)
        {}
      else
        {
        out << "\t\tinclude not found" << std::endl;
        }
      }
    }
  assert(header_stack.back().include == include);
  header_stack.pop_back();
  return processing_result;
  }
  
static processing_result_e process_data( 
                          std::ostream & out,
                          std::span<cmake_processed::source_file_t const> targets_sources,
                          std::span<cmake_processed::target_include_t const> targets_include_paths,
                          std::span<cmake_processed::target_include_t const> targets_interface_include_paths,
                          std::span<cmake_processed::target_dependency_t const> target_dependencies )
{
  target_map_type targets;
  include_collection_t includes;
  auto result{ processing_result_e::succeed };
  for( auto const & src :  targets_include_paths )
    {
    target_t & target{ targets[src.target_name] };
    target.include_paths.emplace_back(src.include_path);
    scan_emplace_includes_for_target(out, includes,src.include_path,target.private_includes);
    }
    
  for( auto const & src :  targets_interface_include_paths )
    {
    target_t & target{ targets[src.target_name] };
    target.interface_include_paths.emplace_back(src.include_path);
    scan_emplace_includes_for_target(out, includes,src.include_path,target.interface_includes);
    }
    
  for( auto const build_dep : target_dependencies)
    {
    target_t & target{ targets[build_dep.target_name] };
    if( target.target_name.empty())
      target.target_name = build_dep.target_name;
    target_t & dep{ targets[build_dep.link_target] };
    if( dep.target_name.empty())
      dep.target_name = build_dep.link_target;
    target.references.emplace_back( &dep );
    }
    
  for( auto const & src :  targets_sources )
    {
    target_t & target{ targets[src.target_name] };
    std::shared_ptr<file_node_t> source{includes.insert_or_create(std::string(src.source_path))};
    target.sources.emplace_back( source.get() );
  
    header_guard_t header_guard;
    header_stack_t header_stack;
  
    //Building analysis for include and target
    auto include { includes.insert_or_create(std::string{src.source_path}) };
    processing_result_e processing_result
      { process_source(out,header_guard,header_stack,include.get(), "", target) };
    if( processing_result == processing_result_e::failed)
      result = processing_result_e::failed;
    
    }
    
//     "node0" [ label = "exec", shape = box ];
//     "node1" [ label = "libb\n(ia::libb)", shape = box ];
//     "node1" -> "node2" [ style = dotted ] // libb -> liba
  return result;
}

namespace cmake_processed
{
static constexpr std::array targets 
  {
@ANALYZER_TARGETS_DATA@
  };
static constexpr std::array targets_sources 
  {
@ANALYZER_SOURCES_DATA@
  };
static constexpr std::array targets_include_paths 
  {
@ANALYZER_INCLUDES_DATA@
  };
static constexpr std::array targets_interface_include_paths 
  {
@ANALYZER_INTERFACE_INCLUDES_DATA@
  };
static constexpr std::array targets_link_dependency 
  {
@ANALYZER_LINK_TARGETS_DATA@
  };
}

}

int main()
  {
  std::ofstream fout("ia_log.txt");
  using inc_analizer::target_t;
  using inc_analizer::source_t;
  using inc_analizer::target_map_type;
  using inc_analizer::processing_result_e;
  processing_result_e result {
  inc_analizer::process_data(fout,
                             inc_analizer::cmake_processed::targets_sources,
                             inc_analizer::cmake_processed::targets_include_paths,
                             inc_analizer::cmake_processed::targets_interface_include_paths,
                             inc_analizer::cmake_processed::targets_link_dependency) };
  return processing_result_e::succeed == result ? 0 : 1;
  }
