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

#include "WinReg.hpp"
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

std::optional<fs::path> get_destination_folder_path()
{
  winreg::RegKey key;
  auto result = key.TryOpen(HKEY_CURRENT_USER, L"SOFTWARE\\lyckantropen\\moonlight_hdr_launcher");
  if (result)
  {
    if (auto dest_path = key.TryGetStringValue(L"destination_folder"))
    {
      return fs::path(*dest_path);
    }
    else
    {
      return {};
    }
  }
  else
  {
    return {};
  }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow)
{
  auto argc = __argc;
  auto argv = __argv;

  auto pwd = fs::path(argv[0]).parent_path();
  auto reg_dest_path = get_destination_folder_path();

  if (reg_dest_path)
  {
    pwd = *reg_dest_path;
  }

  auto logfile = std::ofstream{(pwd / fs::path("moonlight_hdr_launcher_log.txt")).string()};

  if (reg_dest_path)
  {
    log(std::string("Working folder read from registry: ") + pwd.string(), logfile);
  }

  for (int i = 0; i < argc; ++i)
  {
    log(std::string("argv[") + std::to_string(i) + std::string("]: ") + std::string(argv[i]), logfile);
  }

  try
  {
    log(std::string("Setting current working directory to ") + pwd.string(), logfile);
    fs::current_path(pwd);

    auto inifile = pwd / fs::path("moonlight_hdr_launcher.ini");

    std::string launcher_exe = default_launcher;
    bool wait_on_process = true;
    bool toggle_hdr = false;

    if (fs::exists(inifile))
    {
      log(std::string("Found config file: ") + inifile.string(), logfile);

      std::ifstream ini_f{inifile.c_str()};
      pt::ptree ini;
      pt::read_ini(ini_f, ini);

      launcher_exe = ini.get_optional<std::string>("options.launcher_exe").get_value_or(default_launcher);
      wait_on_process = ini.get_optional<bool>("options.wait_on_process").get_value_or(wait_on_process);
      toggle_hdr = ini.get_optional<bool>("options.toggle_hdr").get_value_or(toggle_hdr);

      log(std::string("options.launcher_exe=") + launcher_exe, logfile);
      log(std::string("options.wait_on_process=") + std::to_string(wait_on_process), logfile);
      log(std::string("options.toggle_hdr=") + std::to_string(toggle_hdr), logfile);
    }

    if (wait_on_process)
    {
      std::optional<HdrToggle> hdr_toggle;
      if (toggle_hdr)
      {
        log("Attempting to set HDR mode", logfile);
        try
        {
          hdr_toggle = HdrToggle{};
          if (!hdr_toggle->set_hdr_mode(true))
          {
            log("Failed to set HDR mode", logfile);
          }
        }
        catch (NvapiException &e)
        {
          log(std::string("Failed to set HDR mode: ") + e.what(), logfile);
        }
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
      if (c.exit_code() != 0)
      {
        log(std::string("The command \"") + launcher_exe + std::string("\" has terminated with exit code ") + std::to_string(c.exit_code()), logfile);
      }

      if (toggle_hdr)
      {
        log("Attempting to disable HDR mode", logfile);
        try
        {
          hdr_toggle->set_hdr_mode(false);
        }
        catch (NvapiException &e)
        {
          log(std::string("Failed to disable HDR mode: ") + e.what(), logfile);
        }
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
  catch (std::runtime_error &e)
  {
    log(std::string("Error: ") + std::string(e.what()), logfile);
    return 1;
  }
  catch (std::exception &e)
  {
    log(std::string("Error: ") + std::string(e.what()), logfile);
    return 1;
  }
  catch (...)
  {
    log("Unknown error", logfile);
    return 1;
  }
}