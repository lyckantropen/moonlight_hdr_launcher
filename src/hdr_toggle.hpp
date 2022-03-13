#pragma once
#include <stdexcept>
#include <vector>
#include <nvapi.h>

bool _check_status(const NvAPI_Status &s, const std::string &message, bool should_raise = true);
#define check_status(s) _check_status(s, std::string(__func__) + "," + std::to_string(__LINE__) + ": ");
#define check_status_nothrow(s) _check_status(s, std::string(__func__) + "," + std::to_string(__LINE__) + ": ", false);

struct NvapiException : public std::runtime_error {
  explicit NvapiException(const std::string &what) : std::runtime_error(what) {}
  explicit NvapiException(const char *what) : std::runtime_error(what) {}
};

class HdrToggle {
public:
  HdrToggle();
  virtual ~HdrToggle();
  bool set_hdr_mode(bool enabled);

private:
  NV_HDR_COLOR_DATA set_hdr_data(bool enabled);
  std::vector<NV_GPU_DISPLAYIDS> get_hdr_display_ids();
  void calc_mastering_data(NV_HDR_COLOR_DATA *hdr_data);
};