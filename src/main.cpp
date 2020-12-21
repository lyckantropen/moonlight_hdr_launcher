#pragma warning(push)
#pragma warning(disable : 4244)
#include <boost/process.hpp>
#pragma warning(pop)
#include <boost/property_tree/ini_parser.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <chrono>
#include <sstream>
#include <optional>
#include "windows.h"

#include "hdr_toggle.hpp"

namespace bp = boost::process;
namespace fs = std::filesystem;
namespace pt = boost::property_tree;

const static std::string default_launcher = "C:/Program Files (x86)/GOG Galaxy/GalaxyClient.exe";

void log(const std::string &message, std::ofstream &file, bool log_to_stdout = true)
{
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  auto time = std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
  file << time << ": " << message << std::endl;
  if (log_to_stdout)
  {
    std::cout << time << ": " << message << std::endl;
  }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
  auto argc = __argc;
  auto argv = __argv;

  auto pwd = fs::path(argv[0]).parent_path();
  auto logfile = std::ofstream{(pwd / fs::path("moonlight_hdr_launcher_log.txt")).string()};

  for (int i = 0; i < argc; ++i)
  {
    log(std::string("argv[") + std::to_string(i) + std::string("]: ") + std::string(argv[i]), logfile);
  }

  auto inifile = pwd / fs::path("moonlight_hdr_launcher.ini");

  std::string launcher_exe = default_launcher;
  bool wait_on_process = true;
  bool toggle_hdr = true;

  if (fs::exists(inifile))
  {
    log(std::string("Found config file: ") + inifile.string(), logfile);

    std::ifstream ini_f{inifile.c_str()};
    pt::ptree ini;
    pt::read_ini(ini_f, ini);

    launcher_exe = ini.get_optional<std::string>("options.launcher_exe").get_value_or(default_launcher);
    wait_on_process = ini.get_optional<bool>("options.wait_on_process").get_value_or(wait_on_process);
    toggle_hdr = ini.get_optional<bool>("options.toggle_hdr").get_value_or(toggle_hdr);
  }

  if (wait_on_process)
  {
    std::optional<HdrToggle> hdr_toggle;
    if(toggle_hdr) {
      hdr_toggle = HdrToggle{};
      hdr_toggle->set_hdr_mode(true);
    }

    log(std::string("Launching '") + launcher_exe + std::string("' and waiting for it to complete."), logfile);
    bp::ipstream is; //reading pipe-stream
    auto c = bp::child{launcher_exe, bp::std_out > is, bp::std_err > is};
    while (c.running())
    {
      std::string line;
      if (std::getline(is, line) && !line.empty())
      {
        log(std::string("SUBPROCESS: ") + line, logfile);
      }
    }
    c.wait();

    if(toggle_hdr) {
      hdr_toggle->set_hdr_mode(false);
    }
    return c.exit_code();
  }
  else
  {
    log(std::string("Launching '") + launcher_exe + std::string("' and detaching immediately."), logfile);
    bp::spawn(launcher_exe);
  }
  return 0;
}