#pragma once
#include <vector>
#include <nvapi.h>

class HdrToggle
{
public:
  HdrToggle();
  virtual ~HdrToggle();
  bool set_hdr_mode(bool enabled);
private:
  NV_HDR_COLOR_DATA set_hdr_data(bool enabled);
  std::vector<NV_GPU_DISPLAYIDS> get_hdr_display_ids();
  void calc_mastering_data(NV_HDR_COLOR_DATA *hdr_data);
};