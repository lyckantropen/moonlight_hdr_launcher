#pragma warning(push)
#pragma warning(disable:4244)
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

int main(int argc, char** argv) {
  auto pwd = fs::path(argv[0]).parent_path();
  auto inifile = pwd / fs::path("moonlight_hdr_launcher.ini");

  std::string launcher = default_launcher;

  if(fs::exists(inifile)) {
    std::ifstream ini_f {inifile.c_str()};
    pt::ptree ini;
    pt::read_ini(ini_f, ini);

    launcher = ini.get_optional<std::string>("options.launcher_exe").get_value_or(default_launcher);
  }

  bp::spawn(launcher);
}