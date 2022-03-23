#pragma warning(push)
#pragma warning(disable : 4244)
#include <boost/process.hpp>
#pragma warning(pop)
#include "windows.h"
#include <atomic>
#include <boost/property_tree/ini_parser.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>

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

const static std::string default_launcher = "C:\Program Files (x86)\Steam\steam.exe steam://open/bigpicture";

DEVMODE const get_primary_display_registry_settings() {
  DEVMODE devmode{};
  devmode.dmSize = sizeof(devmode);
  EnumDisplaySettings(nullptr, ENUM_REGISTRY_SETTINGS, &devmode);
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
    log_func("Setting max refresh_rate"s);
    refresh_rate = get_max_refresh_rate(width, height);
    log_func("Detected max refresh rate: "s + std::to_string(refresh_rate));
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

class DummyWindow {
public:
  DummyWindow(HINSTANCE hInstance, INT nCmdShow) : m_wc{} {
    m_wc.lpfnWndProc = WndProc;
    m_wc.hInstance = hInstance;
    m_wc.lpszClassName = L"DummyWindow";
    RegisterClass(&m_wc);
    m_window = CreateWindowW(L"DummyWindow", L"MoonLight HDR Launcher Do Not Close", WS_OVERLAPPEDWINDOW, 0, 0, 1, 1, nullptr, nullptr, hInstance, nullptr);
    if (m_window) {
      ShowWindow(m_window, nCmdShow);
    }
  }
  virtual ~DummyWindow() {
    if (m_window) {
      DestroyWindow(m_window);
    }
  }
  void message_loop() {
    MSG msg;
    while (GetMessage(&msg, m_window, 0, 0)) {
      if (msg.hwnd == 0) {
        break;
      }
      DispatchMessage(&msg);
    }
  }
  void close() {
    DestroyWindow(m_window);
    m_window = 0UL;
  }
  static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND: {
      int wmId = LOWORD(wParam);
      // Parse the menu selections:
      switch (wmId) {
      case 105: // IDM_EXIT
        break;
      default:
        return DefWindowProc(hWnd, message, wParam, lParam);
      }
    } break;
    case WM_DESTROY:
      break;
    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
  };

private:
  HWND m_window;
  WNDCLASS m_wc;
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

  log("Moonlight HDR Launcher Version "s + std::string(MHDRL_VERSION), logfile);

  if (reg_dest_path) {
    log("Working folder read from registry: "s + pwd.string(), logfile);
  }

  for (int i = 0; i < argc; ++i) {
    log(std::string("argv[") + std::to_string(i) + std::string("]: ") + std::string(argv[i]), logfile);
  }

  int retcode = 1;
#ifndef SENTRY_DEBUG
  try
#endif
  {
    log("Setting current working directory to "s + pwd.string(), logfile);
    fs::current_path(pwd);

    std::string launcher_exe;
    bool wait_on_process = true;
    bool toggle_hdr = false;
    uint16_t res_x = 0;
    uint16_t res_y = 0;
    uint16_t refresh_rate = 0;
    bool refresh_rate_use_max = true;
    bool remote_desktop = false;
    bool compatibility_window = true;

    if (fs::exists(inifile)) {
      log("Found config file: "s + inifile.string(), logfile);

      std::ifstream ini_f{inifile.c_str()};
      pt::ptree ini;
      pt::read_ini(ini_f, ini);

      launcher_exe = ini.get_optional<std::string>("options.launcher_exe").get_value_or(launcher_exe);
      wait_on_process = ini.get_optional<bool>("options.wait_on_process").get_value_or(wait_on_process);
      toggle_hdr = ini.get_optional<bool>("options.toggle_hdr").get_value_or(toggle_hdr);
      res_x = ini.get_optional<uint16_t>("options.res_x").get_value_or(res_x);
      res_y = ini.get_optional<uint16_t>("options.res_y").get_value_or(res_y);
      refresh_rate = ini.get_optional<uint16_t>("options.refresh_rate").get_value_or(refresh_rate);
      refresh_rate_use_max = ini.get_optional<bool>("options.refresh_rate_use_max").get_value_or(refresh_rate_use_max);
      remote_desktop = ini.get_optional<bool>("options.remote_desktop").get_value_or(remote_desktop);
      compatibility_window = ini.get_optional<bool>("options.compatibility_window").get_value_or(compatibility_window);
      if (launcher_exe != ""s) {
        if (remote_desktop) {
          log("remote_desktop and launcher_exe both specified, defaulting to launcher_exe"s, logfile);
        }
        remote_desktop = false;
      }
      if (!remote_desktop && launcher_exe == ""s) {
        launcher_exe = default_launcher;
      }
      if (remote_desktop && !compatibility_window) {
        log("compatibility_window required for remote_desktop, setting to true"s, logfile);
        compatibility_window = true;
      }

      log("options.launcher_exe="s + launcher_exe, logfile);
      log("options.wait_on_process="s + std::to_string(wait_on_process), logfile);
      log("options.toggle_hdr="s + std::to_string(toggle_hdr), logfile);

#ifdef SENTRY_DEBUG
      sentry_value_t le_c = sentry_value_new_breadcrumb("default", "launcher_exe");
      sentry_value_set_by_key(le_c, "message", sentry_value_new_string(launcher_exe.c_str()));
      sentry_add_breadcrumb(le_c);
#endif
    }

    // set display mode
    std::optional<DEVMODE> original_display_mode;
    if (res_x != 0 && res_y != 0) {
      original_display_mode = get_primary_display_registry_settings();
      log("Original display mode: "s + std::to_string(original_display_mode->dmPelsWidth) + "x"s + std::to_string(original_display_mode->dmPelsHeight) + "@"s +
              std::to_string(original_display_mode->dmDisplayFrequency),
          logfile);
      log("Setting display mode: "s + std::to_string(res_x) + "x"s + std::to_string(res_y) + "@"s + std::to_string(refresh_rate), logfile);
      set_resolution(res_x, res_y, refresh_rate, wait_on_process, refresh_rate_use_max, [&logfile](std::string a) -> void { log(a, logfile); });
    }

    if (wait_on_process) {
      std::atomic_bool dummy_window_ready = {false};
      std::unique_ptr<DummyWindow> dummy_window;
      std::thread dummy_window_thread = std::thread([&]() {
        if (compatibility_window) {
          dummy_window = std::make_unique<DummyWindow>(hInstance, nCmdShow);
          dummy_window_ready.store(true);
          if (dummy_window) {
            dummy_window->message_loop();
          }
        } else {
          dummy_window_ready.store(true);
        }
      });

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
          log("Failed to set HDR mode: "s + e.what(), logfile);
        }
#endif
      }

      if (dummy_window_ready == false) {
        dummy_window_ready.wait(true);
      }
      if (remote_desktop) {
        log("Running in remote desktop mode"s, logfile);
      } else {
        log("Launching '"s + launcher_exe + "' and waiting for it to complete."s, logfile);
        bp::ipstream is; // reading pipe-stream
        try {
          auto c = bp::child{launcher_exe, bp::std_out > is, bp::std_err > is};
          while (c.running()) {
            std::string line;
            if (std::getline(is, line) && !line.empty()) {
              log("SUBPROCESS: "s + line, logfile);
            }
          }
          c.wait();
          if (c.exit_code() != 0) {
            log(std::string("The command \"") + launcher_exe + std::string("\" has terminated with exit code ") + std::to_string(c.exit_code()), logfile);
          }
        } catch (bp::process_error const &e) {
          log("Error executing command. Error code: "s + std::to_string(e.code().value()) + ", message: " + e.code().message(), logfile);
#ifdef SENTRY_DEBUG
          sentry_value_t exc = sentry_value_new_object();
          sentry_value_set_by_key(exc, "type", sentry_value_new_string("bp::process_error"));
          sentry_value_set_by_key(exc, "code", sentry_value_new_int32(e.code().value()));

          sentry_value_t event = sentry_value_new_event();
          sentry_value_set_by_key(event, "exception", exc);
          sentry_capture_event(event);
#endif
        }
      }
      if (dummy_window) {
        dummy_window->close();
      }
      dummy_window_thread.join();

      if (toggle_hdr) {
        log("Attempting to disable HDR mode", logfile);
        try {
          hdr_toggle->set_hdr_mode(false);
        } catch (NvapiException &e) {
          log("Failed to disable HDR mode: "s + e.what(), logfile);
        }
      }
      if (original_display_mode) {
        log("Resetting to original display mode", logfile);
        _ChangeDisplaySettings(&original_display_mode.value(), CDS_UPDATEREGISTRY);
      }
    } else {
      log("Launching '"s + launcher_exe + "' and detaching immediately."s, logfile);
      bp::spawn(launcher_exe);
    }
    retcode = 0;
  }
#ifndef SENTRY_DEBUG
  catch (std::runtime_error &e) {
    log("Error: "s + std::string(e.what()), logfile);
  } catch (std::exception &e) {
    log("Error: "s + std::string(e.what()), logfile);
  } catch (...) {
    log("Unknown error", logfile);
  }
#endif

#ifdef SENTRY_DEBUG
  sentry_shutdown();
#endif

  return retcode;
}
