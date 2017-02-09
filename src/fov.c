#include "cgame\cg_local.h"
#include "mppShared.h"

#ifdef MPP_LEGACY
typedef struct  {
	char			*name;
	char			*description;
	char			*string;
	char			*resetString;		// cvar_restart will reset to this value
	char			*latchedString;		// for CVAR_LATCH vars
	uint32_t		flags;
	qboolean		modified;			// set each time the cvar is changed
	int				modificationCount;	// incremented each time the cvar is changed
	float			value;				// atof( string )
	int				integer;			// atoi( string )
	qboolean		validate;
	qboolean		integral;
	float			min, max;

	struct cvar_s	*next, *prev;
	struct cvar_s	*hashNext, *hashPrev;
	int				hashIndex;
} mpp_cvar_t;
#else
	typedef cvar_t mpp_cvar_t;
#endif

static MultiPlugin_t	*sys;
static MultiSystem_t	*trap;

static mpp_cvar_t *hooked_fov;
static mpp_cvar_t *hooked_zoom;

static void CG_UseFov(int cgFov, float zoomFov) {
	float	x;
	float	phase;
	float	v;
	float	fov_x, fov_y;
	float	f;

	if (cgFov < 1)
	{
		cgFov = 1;
	}

	if (sys->cg->predictedPlayerState.pm_type == PM_INTERMISSION) {
		// if in intermission, use a fixed value
		fov_x = 80;//90;
	}
	else {
		// user selectable
		if (sys->cgs->dmflags & DF_FIXED_FOV) {
			// dmflag to prevent wide fov for all clients
			fov_x = 80;//90;
		}
		else {
			fov_x = cgFov;
			if (fov_x < 1) {
				fov_x = 1;
			}
			else if (fov_x > 160) {
				fov_x = 160;
			}
		}

		if (sys->cg->predictedPlayerState.zoomMode == 2)
		{ //binoculars
			if (zoomFov > 40.0f)
			{
				zoomFov -= sys->cg->frametime * 0.075f;

				if (zoomFov < 40.0f)
				{
					zoomFov = 40.0f;
				}
				else if (zoomFov > cgFov)
				{
					zoomFov = cgFov;
				}
			}

			fov_x = zoomFov;
		}
		else if (sys->cg->predictedPlayerState.zoomMode)
		{
			if (!sys->cg->predictedPlayerState.zoomLocked)
			{
				if (zoomFov > 50)
				{ //Now starting out at nearly half zoomed in
					zoomFov = 50;
				}
				zoomFov -= sys->cg->frametime * 0.035f;//0.075f;

				if (zoomFov < MAX_ZOOM_FOV)
				{
					zoomFov = MAX_ZOOM_FOV;
				}
				else if (zoomFov > cgFov)
				{
					zoomFov = cgFov;
				}
				else
				{	// Still zooming
					static int zoomSoundTime = 0;

					if (zoomSoundTime < sys->cg->time || zoomSoundTime > sys->cg->time + 10000)
					{
						trap->S.StartSound(sys->cg->refdef.vieworg, ENTITYNUM_WORLD, CHAN_LOCAL, sys->cgs->media.disruptorZoomLoop);
						zoomSoundTime = sys->cg->time + 300;
					}
				}
			}

			if (zoomFov < MAX_ZOOM_FOV)
			{
				zoomFov = 50;		// hack to fix zoom during vid restart
			}
			fov_x = zoomFov;
		}
		else
		{
			zoomFov = 80;

			f = (sys->cg->time - sys->cg->predictedPlayerState.zoomTime) / ZOOM_OUT_TIME;
			if (f > 1.0)
			{
				fov_x = fov_x;
			}
			else
			{
				fov_x = sys->cg->predictedPlayerState.zoomFov + f * (fov_x - sys->cg->predictedPlayerState.zoomFov);
			}
		}
	}

	x = sys->cg->refdef.width / tan(fov_x / 360 * M_PI);
	fov_y = atan2(sys->cg->refdef.height, x);
	fov_y = fov_y * 360 / M_PI;

	// warp if underwater
	if (sys->cg->refdef.viewContents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA)) {
		phase = sys->cg->time / 1000.0 * WAVE_FREQUENCY * M_PI * 2;
		v = WAVE_AMPLITUDE * sin(phase);
		fov_x += v;
		fov_y -= v;
	}

#ifdef _XBOX
	if (cg.widescreen)
		fov_x = fov_y * 1.77777f;
#endif


	// set it
	sys->cg->refdef.fov_x = fov_x;
	sys->cg->refdef.fov_y = fov_y;

	if (sys->cg->predictedPlayerState.zoomMode)
	{
		sys->cg->zoomSensitivity = zoomFov / cgFov;
	}
	else if (!sys->cg->zoomed) {
		sys->cg->zoomSensitivity = 1;
	}
	else {
		sys->cg->zoomSensitivity = sys->cg->refdef.fov_y / 75.0;
	}
}

__declspec(dllexport) void mpp(MultiPlugin_t *pPlugin)
{
	sys = pPlugin;
	trap = sys->System;

	hooked_fov = sys->mppLocateCvar("cg_fov");
	hooked_zoom = sys->mppLocateCvar("cg_zoomfov");
}

__declspec(dllexport) int mppPostSystem(int *args) {
	CG_UseFov(hooked_fov->integer, hooked_zoom->value);

	return sys->noBreakCode;
}