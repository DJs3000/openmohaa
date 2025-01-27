/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Foobar; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "tr_local.h"
#include "tr_vis.h"
#include "tiki.h"

int			r_firstSceneDrawSurf;
int			r_firstSceneSpriteSurf;

int			r_numdlights;
int			r_firstSceneDlight;

int			r_numentities;
int			r_firstSceneEntity;

int			r_numsprites;
int			r_firstSceneSprite;

int			r_numtermarks;
int			r_firstSceneTerMark;

int			r_numpolys;
int			r_firstScenePoly;

int			r_numpolyverts;


/*
====================
R_ToggleSmpFrame

====================
*/
void R_ToggleSmpFrame( void ) {
	if ( r_smp->integer ) {
		// use the other buffers next frame, because another CPU
		// may still be rendering into the current ones
		tr.smpFrame ^= 1;
	} else {
		tr.smpFrame = 0;
	}

	backEndData[tr.smpFrame]->commands.used = 0;

	r_firstSceneDrawSurf = 0;
	r_firstSceneSpriteSurf = 0;

	r_numdlights = 0;
	r_firstSceneDlight = 0;

	r_numentities = 0;
	r_firstSceneEntity = 0;

	r_numsprites = 0;
	r_firstSceneSprite = 0;

	r_numtermarks = 0;
	r_firstSceneTerMark = 0;

	r_numpolys = 0;
	r_firstScenePoly = 0;

	r_numpolyverts = 0;
}


/*
====================
RE_ClearScene

====================
*/
void RE_ClearScene( void ) {
	r_firstSceneDlight = r_numdlights;
	r_firstSceneEntity = r_numentities;
	r_firstSceneSprite = r_numsprites;
	r_firstSceneTerMark = r_numtermarks;
	r_firstScenePoly = r_numpolys;
}

/*
===========================================================================

DISCRETE POLYS

===========================================================================
*/

/*
=====================
R_AddPolygonSurfaces

Adds all the scene's polys into this view's drawsurf list
=====================
*/
void R_AddPolygonSurfaces( void ) {
	int			i;
	shader_t	*sh;
	srfPoly_t	*poly;

	tr.currentEntityNum = ENTITYNUM_WORLD;
	tr.shiftedEntityNum = tr.currentEntityNum << QSORT_ENTITYNUM_SHIFT;

	for ( i = 0, poly = tr.refdef.polys; i < tr.refdef.numPolys ; i++, poly++ ) {
		sh = R_GetShaderByHandle( poly->hShader );
		R_AddDrawSurf( ( void * )poly, sh, qfalse );
	}
}

/*
=====================
RE_AddPolyToScene

=====================
*/
qboolean RE_AddPolyToScene(qhandle_t hShader, int numVerts, const polyVert_t* verts, int renderfx) {
	srfPoly_t	*poly;

	if ( !tr.registered ) {
		return qfalse;
	}

	if (numVerts + r_numpolyverts > max_polyverts || r_numpolys >= max_polys) {
		ri.Printf(PRINT_WARNING, "Exceeded MAX POLYS\n");
		return qfalse;
	}

	poly = &backEndData[tr.smpFrame]->polys[r_numpolys];
	poly->surfaceType = SF_POLY;
	poly->hShader = hShader;
	poly->numVerts = numVerts;
	poly->verts = &backEndData[tr.smpFrame]->polyVerts[r_numpolyverts];
	poly->renderfx = renderfx;

	Com_Memcpy(poly->verts, verts, sizeof(polyVert_t) * numVerts);
	++r_numpolys;
	r_numpolyverts += numVerts;

	return qtrue;
}

/*
=====================
R_AddTerrainMarkSurfaces
=====================
*/
void R_AddTerrainMarkSurfaces(void) {
    // FIXME: unimplemented
}

/*
=====================
RE_AddTerrainMarkToScene
=====================
*/
void RE_AddTerrainMarkToScene(int iTerrainIndex, qhandle_t hShader, int numVerts, const polyVert_t* verts, int renderfx) {
    // FIXME: unimplemented
}

//=================================================================================

/*
=====================
RE_GetRenderEntity
=====================
*/
refEntity_t* RE_GetRenderEntity(int entityNumber) {
    int i;

    for (i = 0; i < r_numentities; i++) {
        if (backEndData[0]->entities[i].e.entityNumber == entityNumber) {
            return &backEndData[0]->entities[i].e;
        }
    }

    return NULL;
}

/*
=====================
RE_AddRefEntityToScene

=====================
*/
void RE_AddRefEntityToScene( const refEntity_t *ent, int parentEntityNumber) {
	if ( !tr.registered ) {
		return;
	}
  // https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=402
	if ( r_numentities >= ENTITYNUM_WORLD ) {
		return;
	}
	if ( ent->reType < 0 || ent->reType >= RT_MAX_REF_ENTITY_TYPE ) {
		ri.Error( ERR_DROP, "RE_AddRefEntityToScene: bad reType %i", ent->reType );
	}

	backEndData[tr.smpFrame]->entities[r_numentities].e = *ent;
	backEndData[tr.smpFrame]->entities[r_numentities].bLightGridCalculated = qfalse;
	backEndData[tr.smpFrame]->entities[r_numentities].sphereCalculated = qfalse;

	if (parentEntityNumber != ENTITYNUM_NONE)
	{
		int i;

		//
		// Find the parent entity to attach to
		//
		for (i = r_firstSceneEntity; i < r_numentities; i++)
		{
			if (backEndData[tr.smpFrame]->entities[i].e.entityNumber == parentEntityNumber)
			{
				backEndData[tr.smpFrame]->entities[r_numentities].e.parentEntity = i - r_firstSceneEntity;
				break;
			}
		}

		if (i == r_numentities) {
			backEndData[tr.smpFrame]->entities[i].e.parentEntity = ENTITYNUM_NONE;
		}
	}
	else
	{
		backEndData[tr.smpFrame]->entities[r_numentities].e.parentEntity = ENTITYNUM_NONE;
	}

	r_numentities++;
}

void RE_AddRefSpriteToScene(const refEntity_t* ent) {
	refSprite_t* spr;
	int i;

	if (!tr.registered) {
		return;
	}

	if (r_numsprites >= MAX_SPRITES) {
		return;
	}

	spr = &backEndData[tr.smpFrame]->sprites[r_numsprites];
	VectorCopy(ent->origin, spr->origin);
	spr->surftype = SF_SPRITE;
    spr->hModel = ent->hModel;
    spr->scale = ent->scale;
    spr->renderfx = ent->renderfx;
    spr->shaderTime = ent->shaderTime;
	AxisCopy(ent->axis, spr->axis);

	for (i = 0; i < 4; ++i) {
		spr->shaderRGBA[i] = ent->shaderRGBA[i];
	}

    ++r_numsprites;
}

/*
=====================
RE_AddDynamicLightToScene

=====================
*/
void RE_AddDynamicLightToScene( const vec3_t org, float intensity, float r, float g, float b, int type ) {
	dlight_t	*dl;

	if ( !tr.registered ) {
		return;
	}
	if ( r_numdlights >= MAX_DLIGHTS ) {
		return;
	}
	if ( intensity <= 0 ) {
		return;
	}
	dl = &backEndData[tr.smpFrame]->dlights[r_numdlights++];
	VectorCopy (org, dl->origin);
	dl->radius = intensity;
	dl->color[0] = r;
	dl->color[1] = g;
	dl->color[2] = b;
	dl->type = type;
}

/*
=====================
RE_AddLightToScene

=====================
*/
void RE_AddLightToScene( const vec3_t org, float intensity, float r, float g, float b, int type ) {
	RE_AddDynamicLightToScene( org, intensity, r, g, b, type );
}

void R_AddLightGridSurfacesToScene() {
	// FIXME: unimplemented
}

/*
=====================
RE_AddAdditiveLightToScene

=====================
*/
void RE_AddAdditiveLightToScene( const vec3_t org, float intensity, float r, float g, float b ) {
	RE_AddDynamicLightToScene( org, intensity, r, g, b, qtrue );
}

/*
@@@@@@@@@@@@@@@@@@@@@
RE_RenderScene

Draw a 3D view into a part of the window, then return
to 2D drawing.

Rendering a scene may require multiple views to be rendered
to handle mirrors,
@@@@@@@@@@@@@@@@@@@@@
*/
void RE_RenderScene( const refdef_t *fd ) {
	viewParms_t		parms;
	int				startTime;

	if ( !tr.registered ) {
		return;
	}
	GLimp_LogComment( "====== RE_RenderScene =====\n" );

	if ( r_norefresh->integer ) {
		return;
	}

	startTime = ri.Milliseconds();

	if (!tr.world && !( fd->rdflags & RDF_NOWORLDMODEL ) ) {
		ri.Error (ERR_DROP, "R_RenderScene: NULL worldmodel");
	}

	if (r_light_showgrid->integer) {
		R_AddLightGridSurfacesToScene();
	}

	R_VisDebug();
	TIKI_Reset_Caches();

	backEnd.in2D = qfalse;
	tr.refdef.x = fd->x;
	tr.refdef.y = fd->y;
	tr.refdef.width = fd->width;
	tr.refdef.height = fd->height;
	tr.refdef.fov_x = fd->fov_x;
	tr.refdef.fov_y = fd->fov_y;

	VectorCopy( fd->vieworg, tr.refdef.vieworg );
	VectorCopy( fd->viewaxis[0], tr.refdef.viewaxis[0] );
	VectorCopy( fd->viewaxis[1], tr.refdef.viewaxis[1] );
	VectorCopy( fd->viewaxis[2], tr.refdef.viewaxis[2] );

	tr.refdef.time = fd->time;
	tr.refdef.rdflags = fd->rdflags;

	// copy the areamask data over and note if it has changed, which
	// will force a reset of the visible leafs even if the view hasn't moved
	tr.refdef.areamaskModified = qfalse;
	if ( ! (tr.refdef.rdflags & RDF_NOWORLDMODEL) ) {
		int		areaDiff;
		int		i;

		// compare the area bits
		areaDiff = 0;
		for (i = 0 ; i < MAX_MAP_AREA_BYTES/4 ; i++) {
			areaDiff |= ((int *)tr.refdef.areamask)[i] ^ ((int *)fd->areamask)[i];
			((int *)tr.refdef.areamask)[i] = ((int *)fd->areamask)[i];
		}

		if ( areaDiff ) {
			// a door just opened or something
			tr.refdef.areamaskModified = qtrue;
		}
	}

	// copy the sky data
	tr.refdef.sky_alpha = fd->sky_alpha;
	tr.refdef.sky_portal = fd->sky_portal;

	VectorCopy(fd->sky_origin, tr.refdef.sky_origin);
	VectorCopy(fd->sky_axis[0], tr.refdef.sky_axis[0]);
	VectorCopy(fd->sky_axis[1], tr.refdef.sky_axis[1]);
	VectorCopy(fd->sky_axis[2], tr.refdef.sky_axis[2]);

	// derived info

	tr.refdef.floatTime = tr.refdef.time * 0.001f;

	tr.refdef.numDrawSurfs = r_firstSceneDrawSurf;
	tr.refdef.drawSurfs = backEndData[tr.smpFrame]->drawSurfs;

    tr.refdef.numSpriteSurfs = r_firstSceneSpriteSurf;
    tr.refdef.spriteSurfs = backEndData[tr.smpFrame]->spriteSurfs;

	tr.refdef.num_entities = r_numentities - r_firstSceneEntity;
	tr.refdef.entities = &backEndData[tr.smpFrame]->entities[r_firstSceneEntity];

	tr.refdef.num_sprites = r_numsprites - r_firstSceneSprite;
	tr.refdef.sprites = &backEndData[tr.smpFrame]->sprites[r_firstSceneSprite];

	tr.refdef.num_dlights = r_numdlights - r_firstSceneDlight;
	tr.refdef.dlights = &backEndData[tr.smpFrame]->dlights[r_firstSceneDlight];

	tr.refdef.numTerMarks = r_numtermarks - r_firstSceneTerMark;
	tr.refdef.terMarks = &backEndData[tr.smpFrame]->terMarks[r_firstSceneTerMark];

	tr.refdef.numPolys = r_numpolys - r_firstScenePoly;
	tr.refdef.polys = &backEndData[tr.smpFrame]->polys[r_firstScenePoly];

	backEndData[tr.smpFrame]->staticModelData = tr.refdef.staticModelData;

	// turn off dynamic lighting globally by clearing all the
	// dlights if it needs to be disabled or if vertex lighting is enabled
	if (r_vertexLight->integer == 1 ) {
		tr.refdef.num_dlights = 0;
	}

	// a single frame may have multiple scenes draw inside it --
	// a 3D game view, 3D status bar renderings, 3D menus, etc.
	// They need to be distinguished by the light flare code, because
	// the visibility state for a given surface may be different in
	// each scene / view.
	tr.frameSceneNum++;
	tr.sceneCount++;

	// setup view parms for the initial view
	//
	// set up viewport
	// The refdef takes 0-at-the-top y coordinates, so
	// convert to GL's 0-at-the-bottom space
	//
	tr.skyRendered = qfalse;
	tr.portalRendered = qfalse;
	Com_Memset( &parms, 0, sizeof( parms ) );
	parms.viewportX = tr.refdef.x;
	parms.viewportY = glConfig.vidHeight - ( tr.refdef.y + tr.refdef.height );
	parms.viewportWidth = tr.refdef.width;
	parms.viewportHeight = tr.refdef.height;
	parms.isPortal = qfalse;

	parms.fovX = tr.refdef.fov_x;
	parms.fovY = tr.refdef.fov_y;

	VectorCopy( fd->vieworg, parms.ori.origin );
	VectorCopy( fd->viewaxis[0], parms.ori.axis[0] );
	VectorCopy( fd->viewaxis[1], parms.ori.axis[1] );
	VectorCopy( fd->viewaxis[2], parms.ori.axis[2] );

	VectorCopy( fd->vieworg, parms.pvsOrigin );

	// copy the farplane data
	parms.farplane_distance = fd->farplane_distance;
	parms.farplane_color[0] = fd->farplane_color[0];
	parms.farplane_color[1] = fd->farplane_color[1];
	parms.farplane_color[2] = fd->farplane_color[2];
	parms.farplane_cull = fd->farplane_cull;

	R_ClearRealDlights();
	R_RenderView( &parms );

	// the next scene rendered in this frame will tack on after this one
	r_firstSceneDrawSurf = tr.refdef.numDrawSurfs;
	r_firstSceneSpriteSurf = tr.refdef.numSpriteSurfs;
	r_firstSceneEntity = r_numentities;
	r_firstSceneSprite = r_numsprites;
	r_firstSceneDlight = r_numdlights;
	r_firstSceneTerMark = r_numtermarks;
	r_firstScenePoly = r_numpolys;

	tr.frontEndMsec += ri.Milliseconds() - startTime;
}
