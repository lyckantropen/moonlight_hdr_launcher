#include "hdr_toggle.hpp"
#include <algorithm>

HdrToggle::HdrToggle()
{
  NvAPI_Initialize();
}

HdrToggle::~HdrToggle()
{
  NvAPI_Unload();
}

void HdrToggle::calc_mastering_data(NV_HDR_COLOR_DATA *hdr_data)
{
  double rx = 0.64;
  double ry = 0.33;
  double gx = 0.30;
  double gy = 0.60;
  double bx = 0.15;
  double by = 0.06;
  double wx = 0.3127;
  double wy = 0.3290;
  double min_master = 1.0;
  double max_master = 1000;
  double max_cll = 1000;
  double max_fall = 100;

  hdr_data->mastering_display_data.displayPrimary_x0 = (NvU16)ceil(rx * 0xC350 + 0.5);
  hdr_data->mastering_display_data.displayPrimary_y0 = (NvU16)ceil(ry * 0xC350 + 0.5);
  hdr_data->mastering_display_data.displayPrimary_x1 = (NvU16)ceil(gx * 0xC350 + 0.5);
  hdr_data->mastering_display_data.displayPrimary_y1 = (NvU16)ceil(gy * 0xC350 + 0.5);
  hdr_data->mastering_display_data.displayPrimary_x2 = (NvU16)ceil(bx * 0xC350 + 0.5);
  hdr_data->mastering_display_data.displayPrimary_y2 = (NvU16)ceil(by * 0xC350 + 0.5);
  hdr_data->mastering_display_data.displayWhitePoint_x = (NvU16)ceil(wx * 0xC350 + 0.5);
  hdr_data->mastering_display_data.displayWhitePoint_y = (NvU16)ceil(wy * 0xC350 + 0.5);
  hdr_data->mastering_display_data.max_content_light_level = (NvU16)ceil(max_cll + 0.5);
  hdr_data->mastering_display_data.max_display_mastering_luminance = (NvU16)ceil(max_master + 0.5);
  hdr_data->mastering_display_data.max_frame_average_light_level = (NvU16)ceil(max_fall + 0.5);
  hdr_data->mastering_display_data.min_display_mastering_luminance = (NvU16)ceil(min_master * 10000.0 + 0.5);
}

NV_HDR_COLOR_DATA HdrToggle::set_hdr_data(bool enabled)
{
  NV_HDR_COLOR_DATA color = {0};

  color.version = NV_HDR_COLOR_DATA_VER;
  color.cmd = NV_HDR_CMD_SET;

  if (enabled)
  {
    color.hdrColorFormat = NV_COLOR_FORMAT_RGB;
    color.hdrBpc = NV_BPC_8;
  }

  color.hdrDynamicRange = NV_DYNAMIC_RANGE_AUTO;
  color.hdrMode = enabled ? NV_HDR_MODE_UHDA : NV_HDR_MODE_OFF;

  calc_mastering_data(&color);

  return color;
}

bool HdrToggle::set_hdr_mode(bool enabled)
{
  auto impl = (HdrToggle *)this;

  auto disp_ids_hdr = get_hdr_display_ids();

  std::vector<bool> statuses;
  for (const auto &disp_id : disp_ids_hdr)
  {
    auto color = set_hdr_data(enabled);
    auto status = NvAPI_Disp_HdrColorControl(disp_id.displayId, &color) == NVAPI_OK;
    statuses.push_back(status);
  }

  if (std::any_of(std::begin(statuses), std::end(statuses), [](auto &s) { return s; }))
  {
    return true;
  }
  else
  {
    return *std::find_if_not(std::begin(statuses), std::end(statuses), [](auto &s) { return s; });
  }
}

std::vector<NV_GPU_DISPLAYIDS> HdrToggle::get_hdr_display_ids()
{
  NvU32 disp_id_count = 0;

  auto gpu_handles = std::vector<NvPhysicalGpuHandle>(NVAPI_MAX_PHYSICAL_GPUS);
  NvU32 num_of_gp_us = 0;

  auto status = NvAPI_EnumPhysicalGPUs(gpu_handles.data(), &num_of_gp_us) == NVAPI_OK;
  if (!status)
  {
    return {};
  }

  NvU32 connected_displays = 0;

  status = NvAPI_GPU_GetConnectedDisplayIds(gpu_handles[0], NULL, &disp_id_count, NULL) == NVAPI_OK;
  if (!status)
  {
    return {};
  }

  auto disp_ids = std::vector<NV_GPU_DISPLAYIDS>(disp_id_count);
  for (auto &disp_id : disp_ids)
  {
    disp_id.version = NV_GPU_DISPLAYIDS_VER;
  }

  status = NvAPI_GPU_GetConnectedDisplayIds(gpu_handles[0], disp_ids.data(), &disp_id_count, NULL) == NVAPI_OK;
  if (!status)
  {
    return {};
  }

  std::vector<NV_GPU_DISPLAYIDS> display_ids_hdr;
  for (auto &disp_id : disp_ids)
  {
    NV_HDR_CAPABILITIES hdr_capabilities;
    hdr_capabilities.version = NV_HDR_CAPABILITIES_VER;
    status = NvAPI_Disp_GetHdrCapabilities(disp_id.displayId, &hdr_capabilities) == NVAPI_OK;
    if (!status)
    {
      return {};
    }
    if (hdr_capabilities.isST2084EotfSupported == 1)
    {
      display_ids_hdr.push_back(disp_id);
    }
  }

  return display_ids_hdr;
}