#include "ChannelMap.h"

namespace riposte {

ChannelMap::Mapped ChannelMap::wide_to_narrow(float wide_cx, float wide_cy) const {
    const float wide_vfov = p_.wide_hfov_rad / p_.aspect;
    const float narrow_vfov = p_.narrow_hfov_rad / p_.aspect;
    // Wide normalized -> bearing, remove the mounting-offset residual, then
    // re-normalize against the narrow FOV.
    const float az = ((wide_cx - 0.5F) * p_.wide_hfov_rad) - p_.offset_az_rad;
    const float el = ((wide_cy - 0.5F) * wide_vfov) - p_.offset_el_rad;
    Mapped m;
    m.cx = 0.5F + (az / p_.narrow_hfov_rad);
    m.cy = 0.5F + (el / narrow_vfov);
    m.in_fov = inside(m.cx, m.cy);
    return m;
}

ChannelMap::Mapped ChannelMap::narrow_to_wide(float narrow_cx, float narrow_cy) const {
    const float wide_vfov = p_.wide_hfov_rad / p_.aspect;
    const float narrow_vfov = p_.narrow_hfov_rad / p_.aspect;
    // Inverse of wide_to_narrow: narrow bearing + offset -> wide normalization.
    const float az = ((narrow_cx - 0.5F) * p_.narrow_hfov_rad) + p_.offset_az_rad;
    const float el = ((narrow_cy - 0.5F) * narrow_vfov) + p_.offset_el_rad;
    Mapped m;
    m.cx = 0.5F + (az / p_.wide_hfov_rad);
    m.cy = 0.5F + (el / wide_vfov);
    m.in_fov = inside(m.cx, m.cy);
    return m;
}

} // namespace riposte
