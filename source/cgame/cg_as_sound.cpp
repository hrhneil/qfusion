/*
Copyright (C) 2020 Victor Luchits

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "cg_as_local.h"

static void asFunc_S_SetEntitySpatilization( int entNum, asvec3_t *origin, asvec3_t *velocity ) {
	return trap_S_SetEntitySpatilization( entNum, origin->v, velocity->v );
}

const gs_asglobfuncs_t asCGameSoundGlobalFuncs[] = {
	{ "void AddLoopSound( SoundHandle @, int entnum, float fvol, float attenuation )",
		asFUNCTION( trap_S_AddLoopSound ), NULL },
	{ "void StartRelativeSound( SoundHandle @, int entnum, int channel, float fvol, float attenuation )",
		asFUNCTION( trap_S_StartRelativeSound ), NULL },
	{ "void StartGlobalSound( SoundHandle @, int channel, float fvol )",
		asFUNCTION( trap_S_StartGlobalSound ), NULL },
	{ "void StartLocalSound( SoundHandle @, int channel, float fvol )",
		asFUNCTION( trap_S_StartLocalSound ), NULL },
	{ "void SetEntitySpatilization( int entnum, const Vec3 &in origin, const Vec3 &in velocity )",
		asFUNCTION( asFunc_S_SetEntitySpatilization ), NULL },

	{ NULL },
};

//======================================================================
