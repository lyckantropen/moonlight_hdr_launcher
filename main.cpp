#pragma warning(push)
#pragma warning(disable : 4244)
#include <boost/process.hpp>
#pragma warning(pop)
#include <boost/property_tree/ini_parser.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace bp = boost::process;
namespace fs = std::filesystem;
namespace pt = boost::property_tree;

const static std::string default_launcher = "C:/Program Files (x86)/GOG Galaxy/GalaxyClient.exe";

int main(int argc, char **argv)
{
  auto pwd = fs::path(argv[0]).parent_path();
  auto inifile = pwd / fs::path("moonlight_hdr_launcher.ini");

  std::string launcher_exe = default_launcher;
  bool wait_on_process = false;

  if (fs::exists(inifile))
  {
    std::cout << "Found config file: " << inifile.string() << std::endl;
    std::ifstream ini_f{inifile.c_str()};
    pt::ptree ini;
    pt::read_ini(ini_f, ini);

    launcher_exe = ini.get_optional<std::string>("options.launcher_exe").get_value_or(default_launcher);
    wait_on_process = ini.get_optional<bool>("options.wait_on_process").get_value_or(false);
  }

  if (wait_on_process)
  {
    std::cout << "Launching '" << launcher_exe << "' and waiting for it to complete." << std::endl;
    auto c = bp::child{launcher_exe, bp::std_out > stdout, bp::std_err > stderr};
    c.wait();
    return c.exit_code();
  }
  else
  {
    std::cout << "Launching '" << launcher_exe << "' and detaching immediately." << std::endl;
    bp::spawn(launcher_exe);
  }
  return 0;
}