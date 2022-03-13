#pragma warning(push)
#pragma warning(disable : 4244)
#include <boost/process.hpp>
#pragma warning(pop)
#include "windows.h"
#include <boost/property_tree/ini_parser.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>

#include "WinReg.hpp"
#include "hdr_toggle.hpp"

#ifdef SENTRY_DEBUG
#define SENTRY_BUILD_STATIC 1
#include <sentry.h>
#endif

#ifndef MHDRL_VERSION
#define MHDRL_VERSION "develop"
#endif

namespace bp = boost::process;
namespace fs = std::filesystem;
namespace pt = boost::property_tree;
using namespace std::string_literals;

const static std::string default_launcher = "C:/Program Files (x86)/GOG Galaxy/GalaxyClient.exe";

DEVMODE const get_primary_display_registry_settings() {
  DEVMODE devmode{};
  devmode.dmSize = sizeof(devmode);
  auto rval = EnumDisplaySettings(nullptr, ENUM_REGISTRY_SETTINGS, &devmode);
  return devmode;
}

DWORD get_max_refresh_rate(uint16_t width, uint16_t height) {
  DWORD max_refresh_rate = 0;
  for (DWORD graphics_mode_index = 0;; ++graphics_mode_index) {
    DEVMODE devmode{};
    devmode.dmSize = sizeof(devmode);
    auto rval = EnumDisplaySettings(0, graphics_mode_index, &devmode);
    if (rval == 0) {
      break;
    }
    if (devmode.dmPelsWidth == width && devmode.dmPelsHeight == height) {
      max_refresh_rate = std::max(max_refresh_rate, devmode.dmDisplayFrequency);
    }
  }

  return max_refresh_rate;
}

LONG _ChangeDisplaySettings(DEVMODE *devmode, DWORD dwFlags) {
  DEVMODE current_settings{};
  current_settings.dmSize = sizeof(current_settings);
  EnumDisplaySettings(0, ENUM_CURRENT_SETTINGS, &current_settings);
  if (current_settings.dmPelsWidth == devmode->dmPelsWidth && current_settings.dmPelsHeight == devmode->dmPelsHeight &&
      ((devmode->dmFields & DM_DISPLAYFREQUENCY == 0) || devmode->dmDisplayFrequency == current_settings.dmDisplayFrequency)) {
    return DISP_CHANGE_SUCCESSFUL;
  }

  return ChangeDisplaySettings(devmode, dwFlags);
}

template <typename log_function>
void set_resolution(uint16_t width, uint16_t height, uint16_t refresh_rate, bool wait_on_process, bool refresh_rate_use_max, log_function log_func) {
  DEVMODE devmode{};
  devmode.dmSize = sizeof(devmode);
  devmode.dmPelsWidth = width;
  devmode.dmPelsHeight = height;
  devmode.dmFields = DM_PELSHEIGHT | DM_PELSWIDTH;
  if (refresh_rate == 0 && refresh_rate_use_max) {
    log_func("setting max refresh_rate"s);
    refresh_rate = get_max_refresh_rate(width, height);
  } else {
    if (refresh_rate_use_max) {
      log_func("refresh_rate and refresh_rate_use_max specified, defaulting to specified rate"s);
    }
  }
  if (refresh_rate != 0) {
    devmode.dmDisplayFrequency = refresh_rate;
    devmode.dmFields |= DM_DISPLAYFREQUENCY;
  }
  DWORD result;
  if (wait_on_process) {
    // if we are waiting on the process we can clean up at the end
    result = _ChangeDisplaySettings(&devmode, CDS_UPDATEREGISTRY);
  } else {
    // otherwise we do the best we can
    result = _ChangeDisplaySettings(&devmode, 0);
  }
  if (result != DISP_CHANGE_SUCCESSFUL) {
    log_func("ChangeDisplaySettings failed error:" + std::to_string(result));
  }
}

void log(const std::string &message, std::ofstream &file, bool log_to_stdout = true) {
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  auto time = std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
  file << time << ": " << message << std::endl;
  if (log_to_stdout) {
    std::cout << time << ": " << message << std::endl;
  }
  file.flush();
}

std::optional<fs::path> get_destination_folder_path() {
  winreg::RegKey key;
  auto result = key.TryOpen(HKEY_CURRENT_USER, L"SOFTWARE\\lyckantropen\\moonlight_hdr_launcher");
  if (result) {
    if (auto dest_path = key.TryGetStringValue(L"destination_folder")) {
      return fs::path(*dest_path);
    } else {
      return {};
    }
  } else {
    return {};
  }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
  case WM_COMMAND: {
    int wmId = LOWORD(wParam);
    // Parse the menu selections:
    switch (wmId) {
    case 105: // IDM_EXIT
      PostQuitMessage(0);
      break;
    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
    }
  } break;
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
  default:
    return DefWindowProc(hWnd, message, wParam, lParam);
  }
  return 0;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
  auto argc = __argc;
  auto argv = __argv;

  auto pwd = fs::path(argv[0]).parent_path();
  auto reg_dest_path = get_destination_folder_path();

  if (reg_dest_path) {
    pwd = *reg_dest_path;
  }

  auto log_path = pwd / fs::path("moonlight_hdr_launcher_log.txt");
  auto logfile = std::ofstream{log_path.string()};
  auto inifile = pwd / fs::path("moonlight_hdr_launcher.ini");

#ifdef SENTRY_DEBUG
  sentry_options_t *options = sentry_options_new();
  sentry_options_set_dsn(options, "https://31576029989d413dbf37509b44ef4ce1@o498001.ingest.sentry.io/5574936");
  sentry_options_set_release(options, (std::string("moonlight_hdr_launcher@") + MHDRL_VERSION).c_str());
  auto sentry_temp = fs::temp_directory_path() / "moonlight_hdr_launcher-sentry";
  sentry_options_set_database_pathw(options, sentry_temp.c_str());
  sentry_options_add_attachment(options, log_path.string().c_str());
  sentry_options_add_attachment(options, inifile.string().c_str());
  sentry_init(options);

  sentry_value_t pwd_c = sentry_value_new_breadcrumb("default", "pwd");
  sentry_value_set_by_key(pwd_c, "message", sentry_value_new_string(pwd.string().c_str()));
  sentry_add_breadcrumb(pwd_c);
  sentry_value_t inifile_c = sentry_value_new_breadcrumb("default", "inifile");
  sentry_value_set_by_key(inifile_c, "message", sentry_value_new_string(inifile.string().c_str()));
  sentry_add_breadcrumb(inifile_c);
#endif

  log(std::string("Moonlight HDR Launcher Version ") + std::string(MHDRL_VERSION), logfile);

  if (reg_dest_path) {
    log(std::string("Working folder read from registry: ") + pwd.string(), logfile);
  }

  for (int i = 0; i < argc; ++i) {
    log(std::string("argv[") + std::to_string(i) + std::string("]: ") + std::string(argv[i]), logfile);
  }

  int retcode = 1;
#ifndef SENTRY_DEBUG
  try
#endif
  {
    log(std::string("Setting current working directory to ") + pwd.string(), logfile);
    fs::current_path(pwd);

    std::string launcher_exe = default_launcher;
    bool wait_on_process = true;
    bool toggle_hdr = false;
    uint16_t resX = 0;
    uint16_t resY = 0;
    uint16_t refresh_rate = 0;
    bool refresh_rate_use_max = true;
    bool disable_reset_display_mode = false;
    bool remote_desktop = false;

    if (fs::exists(inifile)) {
      log(std::string("Found config file: ") + inifile.string(), logfile);

      std::ifstream ini_f{inifile.c_str()};
      pt::ptree ini;
      pt::read_ini(ini_f, ini);

      launcher_exe = ini.get_optional<std::string>("options.launcher_exe").get_value_or(""s);
      wait_on_process = ini.get_optional<bool>("options.wait_on_process").get_value_or(wait_on_process);
      toggle_hdr = ini.get_optional<bool>("options.toggle_hdr").get_value_or(toggle_hdr);
      resX = ini.get_optional<uint16_t>("options.resX").get_value_or(resX);
      resY = ini.get_optional<uint16_t>("options.resY").get_value_or(resY);
      refresh_rate = ini.get_optional<uint16_t>("options.refresh_rate").get_value_or(refresh_rate);
      refresh_rate_use_max = ini.get_optional<bool>("options.refresh_rate_use_max").get_value_or(true);
      disable_reset_display_mode = ini.get_optional<bool>("options.disable_reset_display_mode").get_value_or(false);
      remote_desktop = ini.get_optional<bool>("options.remote_desktop").get_value_or(false);
      if (launcher_exe != ""s) {
        if (remote_desktop) {
          log("remote_desktop and launcher_exe both specified defaulting to launcher_exe"s, logfile);
        }
        remote_desktop = false;
      }
      if (!remote_desktop && launcher_exe == ""s) {
        launcher_exe = default_launcher;
      }

      log(std::string("options.launcher_exe=") + launcher_exe, logfile);
      log(std::string("options.wait_on_process=") + std::to_string(wait_on_process), logfile);
      log(std::string("options.toggle_hdr=") + std::to_string(toggle_hdr), logfile);

#ifdef SENTRY_DEBUG
      sentry_value_t le_c = sentry_value_new_breadcrumb("default", "launcher_exe");
      sentry_value_set_by_key(le_c, "message", sentry_value_new_string(launcher_exe.c_str()));
      sentry_add_breadcrumb(le_c);
#endif
    }
    DEVMODE original_display_mode{};
    if (resX != 0 && resY != 0) {
      original_display_mode = get_primary_display_registry_settings();
      set_resolution(resX, resY, refresh_rate, wait_on_process, refresh_rate_use_max, [&logfile](std::string a) -> void { log(a, logfile); });
    }

    if (wait_on_process) {
      WNDCLASS wc = {};
      wc.lpfnWndProc = WndProc;
      wc.hInstance = hInstance;
      wc.lpszClassName = L"DummyWindow";
      RegisterClass(&wc);
      auto launcher_window =
          CreateWindowW(L"DummyWindow", L"MoonLight HDR Launcher Do Not Close", WS_OVERLAPPEDWINDOW, 0, 0, 1, 1, nullptr, nullptr, hInstance, nullptr);
      if (launcher_window) {
        ShowWindow(launcher_window, nCmdShow);
      }

      std::optional<HdrToggle> hdr_toggle;
      if (toggle_hdr) {
        log("Attempting to set HDR mode", logfile);
#ifndef SENTRY_DEBUG
        try
#endif
        {
          hdr_toggle = HdrToggle{};
          if (!hdr_toggle->set_hdr_mode(true)) {
            log("Failed to set HDR mode", logfile);
          }
        }
#ifndef SENTRY_DEBUG
        catch (NvapiException &e) {
          log(std::string("Failed to set HDR mode: ") + e.what(), logfile);
        }
#endif
      }

      if (!remote_desktop) {
        log(std::string("Launching '") + launcher_exe + std::string("' and waiting for it to complete."), logfile);
        bp::ipstream is; // reading pipe-stream
        try {
          auto c = bp::child{launcher_exe, bp::std_out > is, bp::std_err > is};
          while (c.running()) {
            std::string line;
            if (std::getline(is, line) && !line.empty()) {
              log(std::string("SUBPROCESS: ") + line, logfile);
            }
          }
          c.wait();
          if (!disable_reset_display_mode && original_display_mode.dmSize > 0)
            _ChangeDisplaySettings(&original_display_mode, CDS_UPDATEREGISTRY);
          if (c.exit_code() != 0) {
            log(std::string("The command \"") + launcher_exe + std::string("\" has terminated with exit code ") + std::to_string(c.exit_code()), logfile);
          }
        } catch (bp::process_error const &e) {
          log(std::string("Error executing command. Error code: ") + std::to_string(e.code().value()) + ", message: " + e.code().message(), logfile);
#ifdef SENTRY_DEBUG
          sentry_value_t exc = sentry_value_new_object();
          sentry_value_set_by_key(exc, "type", sentry_value_new_string("bp::process_error"));
          sentry_value_set_by_key(exc, "code", sentry_value_new_int32(e.code().value()));

          sentry_value_t event = sentry_value_new_event();
          sentry_value_set_by_key(event, "exception", exc);
          sentry_capture_event(event);
#endif
        }
      } else {
        MSG msg;
        while (GetMessage(&msg, launcher_window, 0, 0)) {
          if (msg.hwnd == 0) {
            break;
          }
          DispatchMessage(&msg);
        }
      }

      if (toggle_hdr) {
        log("Attempting to disable HDR mode", logfile);
        try {
          hdr_toggle->set_hdr_mode(false);
        } catch (NvapiException &e) {
          log(std::string("Failed to disable HDR mode: ") + e.what(), logfile);
        }
      }
      if (launcher_window) {
        DestroyWindow(launcher_window);
      }
    } else {
      log(std::string("Launching '") + launcher_exe + std::string("' and detaching immediately."), logfile);
      bp::spawn(launcher_exe);
    }
    retcode = 0;
  }
#ifndef SENTRY_DEBUG
  catch (std::runtime_error &e) {
    log(std::string("Error: ") + std::string(e.what()), logfile);
  } catch (std::exception &e) {
    log(std::string("Error: ") + std::string(e.what()), logfile);
  } catch (...) {
    log("Unknown error", logfile);
  }
#endif

#ifdef SENTRY_DEBUG
  sentry_shutdown();
#endif

  return retcode;
}
