#pragma warning(push)
#pragma warning(disable : 4244)
#include <boost/process.hpp>
#pragma warning(pop)
#define VC_EXTRALEAN 1
#include "windows.h"
#include <boost/property_tree/ini_parser.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

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

static const std::string default_launcher = "C:/Program Files (x86)/GOG Galaxy/GalaxyClient.exe"s;


std::optional<DEVMODE> const get_primary_display_registry_settings() {
    DEVMODE devmode{};
    devmode.dmSize = sizeof(devmode);
    if (EnumDisplaySettings(nullptr, ENUM_REGISTRY_SETTINGS, &devmode)) {
        return devmode;
    }
    return {};
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
void set_resolution(uint16_t width, uint16_t height, uint16_t refresh_rate, bool wait_on_process, bool refresh_rate_use_max, log_function log) {
  DEVMODE devmode{};
  devmode.dmSize = sizeof(devmode);
  devmode.dmPelsWidth = width;
  devmode.dmPelsHeight = height;
  devmode.dmFields = DM_PELSHEIGHT | DM_PELSWIDTH;
  if (refresh_rate == 0 && refresh_rate_use_max) {
    log("setting max refresh_rate"s);
    refresh_rate = get_max_refresh_rate(width, height);
    log("Detected max refresh rate: "s + std::to_string(refresh_rate));
  } else {
    if (refresh_rate_use_max) {
      log("refresh_rate and refresh_rate_use_max specified, defaulting to specified rate"s);
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
    log("ChangeDisplaySettings failed error:"s + std::to_string(result));
  }
}

void log_func(const std::string &message, std::ofstream &file,
              bool log_to_stdout = true) {
  auto now = std::chrono::system_clock::now();
  auto in_time_t = std::chrono::system_clock::to_time_t(now);
  auto time = std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
  file << time << ": " << message << std::endl;
  if (log_to_stdout) {
    std::cout << time << ": " << message << std::endl;
  }
  file.flush();
}

template <typename log_function>
std::optional<fs::path> get_destination_folder_path(log_function log) {
  HKEY hkey;
  static constexpr size_t buf_size = 4096;//we are getting a path, I believe windows doesn't allow paths past 255 chars anyway
  wchar_t recv_buffer[buf_size];
  DWORD recv_buffer_size = buf_size * sizeof(wchar_t);
  auto status = RegOpenKeyEx(HKEY_CURRENT_USER,
                             L"SOFTWARE\\lyckantropen\\moonlight_hdr_launcher",
                             REG_NONE, KEY_READ | KEY_QUERY_VALUE, &hkey);
  if (status == ERROR_SUCCESS) {
    status = RegGetValue(hkey, nullptr, L"destination_folder", RRF_RT_REG_SZ,
                         nullptr, &recv_buffer, &recv_buffer_size);
    if (status == ERROR_SUCCESS)
      return fs::path(recv_buffer);
    if (recv_buffer_size > buf_size * sizeof(wchar_t))
      log("insallation path in registry is too long");//we could dynamically allocate a large enough buffer and try again
	//but it seems like a waste of code as paths should never be too long to fit in our buffer.
  }
  log("could not find installation path in registry");
  return {};
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
  virtual ~DummyWindow() { DestroyWindow(m_window); }
  void message_loop() {
    MSG msg;
    while (GetMessage(&msg, m_window, 0, 0)) {
      if (msg.hwnd == 0) {
        break;
      }
      DispatchMessage(&msg);
    }
  }
  static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
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

private:
  HWND m_window;
  WNDCLASS m_wc;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, INT nCmdShow) {
  auto argc = __argc;
  auto argv = __argv;

  auto pwd = fs::path(argv[0]).parent_path();
  auto log_path = pwd / fs::path("moonlight_hdr_launcher_log.txt");
  auto logfile = std::ofstream{log_path.string()};
  auto log = [&logfile](std::string message,
                        bool log_to_stdout = true) -> void {
    log_func(message, logfile, log_to_stdout);
  };

  auto reg_dest_path = get_destination_folder_path(log);

  if (reg_dest_path) {
    pwd = *reg_dest_path;
  }

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

  log("Moonlight HDR Launcher Version "s + std::string(MHDRL_VERSION));

  if (reg_dest_path) {
    log("Working folder read from registry: "s + pwd.string());
  }

  for (int i = 0; i < argc; ++i) {
    log("argv["s + std::to_string(i) + "]: "s + std::string(argv[i]));
  }

  int retcode = 1;
#ifndef SENTRY_DEBUG
  try
#endif
  {
    log("Setting current working directory to "s + pwd.string());
    fs::current_path(pwd);

    std::string launcher_exe = default_launcher;
    bool wait_on_process = true;
    bool toggle_hdr = false;
    bool enable_hdr = false;//toggle_hdr but don't bother to turn it off at the end
    uint16_t res_x = 0;
    uint16_t res_y = 0;
    uint16_t refresh_rate = 0;
    bool refresh_rate_use_max = true;
    bool disable_reset_display_mode = false;
    bool remote_desktop = false;
    bool compatibility_window = true;
    uint8_t hdr_bpc = 0;

    if (fs::exists(inifile)) {
      log("Found config file: "s + inifile.string());

      std::ifstream ini_f{inifile.c_str()};
      pt::ptree ini;
      pt::read_ini(ini_f, ini);

      launcher_exe = ini.get_optional<std::string>("options.launcher_exe").get_value_or(""s);
      wait_on_process = ini.get_optional<bool>("options.wait_on_process").get_value_or(wait_on_process);
      toggle_hdr = ini.get_optional<bool>("options.toggle_hdr").get_value_or(toggle_hdr);
      enable_hdr = ini.get_optional<bool>("options.enable_hdr").get_value_or(enable_hdr);
      res_x = ini.get_optional<uint16_t>("options.res_x").get_value_or(res_x);
      res_y = ini.get_optional<uint16_t>("options.res_y").get_value_or(res_y);
      refresh_rate = ini.get_optional<uint16_t>("options.refresh_rate").get_value_or(refresh_rate);
      refresh_rate_use_max = ini.get_optional<bool>("options.refresh_rate_use_max").get_value_or(refresh_rate_use_max);
      disable_reset_display_mode = ini.get_optional<bool>("options.disable_reset_display_mode").get_value_or(false);
      remote_desktop = ini.get_optional<bool>("options.remote_desktop").get_value_or(remote_desktop);
      hdr_bpc = ini.get_optional<uint8_t>("options.hdr_bpc").get_value_or(0);

      switch (hdr_bpc) {//This switch is uglier than a constexpr container and an algorithm check
			//but with winRel compiler options this produces smaller code 
      case 0:
      case 6:
      case 8:
      case 10:
      case 12:
      case 16:
        break;
      default:
        log("unsupported bits per colour channel setting. Possible settings (manually confirm what your graphics card / display support): 0 (default), 6,8,10,12,16\nsetting to default"s);
        hdr_bpc = 0;
      }

      log("hdr bits per colour channel set to:"s + std::to_string(hdr_bpc));

      if (enable_hdr && toggle_hdr) {
        log("enable_hdr and toggle_hdr specified defaulting to toggle_hdr"s);
        enable_hdr = false;
      }

      compatibility_window = ini.get_optional<bool>("options.compatibility_window").get_value_or(compatibility_window);
      if (launcher_exe != ""s) {
        if (remote_desktop)
          log("remote_desktop and launcher_exe both specified defaulting to launcher_exe"s);
        remote_desktop = false;
      }
      if (!remote_desktop && launcher_exe == ""s) {
        launcher_exe = default_launcher;
      }
      if (remote_desktop && !compatibility_window) {
        log("compatibility_window required for remote_desktop, setting to true"s);
        compatibility_window = true;
      }

      log("options.launcher_exe="s + launcher_exe);
      log("options.wait_on_process="s + std::to_string(wait_on_process));
      log("options.toggle_hdr="s + std::to_string(toggle_hdr));

#ifdef SENTRY_DEBUG
      sentry_value_t le_c = sentry_value_new_breadcrumb("default", "launcher_exe");
      sentry_value_set_by_key(le_c, "message", sentry_value_new_string(launcher_exe.c_str()));
      sentry_add_breadcrumb(le_c);
#endif
    }
    std::optional<HdrToggle> hdr_toggle;
    if (enable_hdr) {
      log("Attempting to set HDR mode"s);
#ifndef SENTRY_DEBUG
      try
#endif
      {
        hdr_toggle = HdrToggle{};
        if (!hdr_toggle->set_hdr_mode(true, hdr_bpc)) {
          log("Failed to set HDR mode"s);
        }
      }
#ifndef SENTRY_DEBUG
      catch (NvapiException &e) {
        log("Failed to set HDR mode: "s + e.what());
      }
#endif
    }

    // set display mode
    std::optional<DEVMODE> original_display_mode;
    if (res_x != 0 && res_y != 0) {
      original_display_mode = get_primary_display_registry_settings();
      if (original_display_mode) {
          log("Original display mode: "s + std::to_string(original_display_mode->dmPelsWidth) + "x"s + std::to_string(original_display_mode->dmPelsHeight) + "@"s +
              std::to_string(original_display_mode->dmDisplayFrequency));
      }
      else {
          log("Error getting original display mode"s);
      }
      log("Setting display mode: "s + std::to_string(res_x) + "x"s + std::to_string(res_y) + "@"s + std::to_string(refresh_rate));
      set_resolution(res_x, res_y, refresh_rate, wait_on_process,
                     refresh_rate_use_max, log);
    }

    if (wait_on_process) {
      std::unique_ptr<DummyWindow> launcher_window;
      if (compatibility_window) {
        launcher_window = std::make_unique<DummyWindow>(hInstance, nCmdShow);
      }

      std::optional<HdrToggle> hdr_toggle;
      if (toggle_hdr) {
        log("Attempting to set HDR mode"s);
#ifndef SENTRY_DEBUG
        try
#endif
        {
          hdr_toggle = HdrToggle{};
          if (!hdr_toggle->set_hdr_mode(true, hdr_bpc)) {
            log("Failed to set HDR mode"s);
          }
        }
#ifndef SENTRY_DEBUG
        catch (NvapiException &e) {
          log("Failed to set HDR mode: "s + e.what());
        }
#endif
      }

      if (remote_desktop) {
        launcher_window->message_loop();
      } else {
        log("Launching '"s + launcher_exe + "' and waiting for it to complete."s);
        bp::ipstream is; // reading pipe-stream
        try {
          auto c = bp::child{launcher_exe, bp::std_out > is, bp::std_err > is};
          while (c.running()) {
            std::string line;
            if (std::getline(is, line) && !line.empty()) {
              log("SUBPROCESS: "s + line);
            }
          }
          c.wait();
          if (c.exit_code() != 0) {
            log("The command \""s + launcher_exe +
                "\" has terminated with exit code "s +
                std::to_string(c.exit_code()));
          }
        } catch (bp::process_error const &e) {
          log("Error executing command. Error code: "s +
              std::to_string(e.code().value()) + ", message: "s +
              e.code().message());
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

      if (toggle_hdr) {
        log("Attempting to disable HDR mode"s);
        try {
          hdr_toggle->set_hdr_mode(false, hdr_bpc);
        } catch (NvapiException &e) {
          log("Failed to disable HDR mode: "s + e.what());
        }
      }
      if (original_display_mode && !disable_reset_display_mode) {
        log("Resetting to original display mode");
        _ChangeDisplaySettings(&original_display_mode.value(), CDS_UPDATEREGISTRY);
      }

    } else {
      log("Launching '"s + launcher_exe + "' and detaching immediately."s);
      bp::spawn(launcher_exe);
    }
    retcode = 0;
  }
#ifndef SENTRY_DEBUG
  catch (std::runtime_error &e) {
    log("Error: "s + std::string(e.what()));
  } catch (std::exception &e) {
    log("Error: "s + std::string(e.what()));
  } catch (...) {
    log("Unknown error"s);
  }
#endif

#ifdef SENTRY_DEBUG
  sentry_shutdown();
#endif

  return retcode;
}
