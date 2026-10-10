#ifndef __PATCH_OPTIONS_H__
#define __PATCH_OPTIONS_H__

#include "patch_helpers.h"

DECLARE_FUNC(int, recomp_get_story_skip);
DECLARE_FUNC(int, recomp_get_camera_type);
DECLARE_FUNC(int, recomp_get_lightning_frequency);
DECLARE_FUNC(int, recomp_get_cutscene_bordering);
DECLARE_FUNC(int, recomp_get_mp_enabled);
DECLARE_FUNC(int, recomp_get_coop_enabled);
DECLARE_FUNC(int, recomp_get_coop_debug);

#endif