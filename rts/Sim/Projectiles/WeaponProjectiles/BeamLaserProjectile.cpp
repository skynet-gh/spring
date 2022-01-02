/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */


#include "BeamLaserProjectile.h"
#include "Game/Camera.h"
#include "Rendering/GL/RenderBuffers.h"
#include "Rendering/Textures/TextureAtlas.h"
#include "Sim/Projectiles/ExplosionGenerator.h"
#include "Sim/Projectiles/ProjectileHandler.h"
#include "Sim/Weapons/WeaponDef.h"
#include <cstring> //memset

CR_BIND_DERIVED(CBeamLaserProjectile, CWeaponProjectile, )

CR_REG_METADATA(CBeamLaserProjectile,(
	CR_SETFLAG(CF_Synced),
	CR_MEMBER(coreColStart),
	CR_MEMBER(coreColEnd),
	CR_MEMBER(edgeColStart),
	CR_MEMBER(edgeColEnd),
	CR_MEMBER(thickness),
	CR_MEMBER(corethickness),
	CR_MEMBER(flaresize),
	CR_MEMBER(decay),
	CR_MEMBER(midtexx)
))


CBeamLaserProjectile::CBeamLaserProjectile(const ProjectileParams& params): CWeaponProjectile(params)
	, thickness(0.0f)
	, corethickness(0.0f)
	, flaresize(0.0f)
	, decay(0.0f)
	, midtexx(0.0f)
{
	projectileType = WEAPON_BEAMLASER_PROJECTILE;

	if (weaponDef != nullptr) {
		assert(weaponDef->IsHitScanWeapon());

		thickness = weaponDef->visuals.thickness;
		corethickness = weaponDef->visuals.corethickness;
		flaresize = weaponDef->visuals.laserflaresize;
		decay = weaponDef->visuals.beamdecay;

		midtexx =
			(weaponDef->visuals.texture2->xstart +
			(weaponDef->visuals.texture2->xend - weaponDef->visuals.texture2->xstart) * 0.5f);

		coreColStart = SColor{
			(weaponDef->visuals.color2.x * params.startAlpha),
			(weaponDef->visuals.color2.y * params.startAlpha),
			(weaponDef->visuals.color2.z * params.startAlpha),
			1u
		};

		coreColEnd = SColor{
			(weaponDef->visuals.color2.x * params.endAlpha),
			(weaponDef->visuals.color2.y * params.endAlpha),
			(weaponDef->visuals.color2.z * params.endAlpha),
			1u
		};

		edgeColStart = SColor{
			(weaponDef->visuals.color.x * params.startAlpha),
			(weaponDef->visuals.color.y * params.startAlpha),
			(weaponDef->visuals.color.z * params.startAlpha),
			1u
		};

		edgeColEnd = SColor{
			(weaponDef->visuals.color.x * params.endAlpha),
			(weaponDef->visuals.color.y * params.endAlpha),
			(weaponDef->visuals.color.z * params.endAlpha),
			1u
		};

	} else {
		coreColStart = SColor::Zero;
		coreColEnd   = SColor::Zero;
		edgeColStart = SColor::Zero;
		edgeColEnd   = SColor::Zero;
	}
}



void CBeamLaserProjectile::Update()
{
	if ((--ttl) <= 0) {
		deleteMe = true;
	} else {
		for (int i = 0; i < 3; i++) {
			coreColStart[i] *= decay;
			coreColEnd[i]   *= decay;
			edgeColStart[i] *= decay;
			edgeColEnd[i]   *= decay;
		}

		explGenHandler.GenExplosion(cegID, startPos + ((targetPos - startPos) / ttl), (targetPos - startPos), 0.0f, flaresize, 0.0f, owner(), nullptr);
	}

	UpdateInterception();
}

void CBeamLaserProjectile::Draw()
{
	if (!validTextures[0])
		return;

	const float3 midPos = (targetPos + startPos) * 0.5f;
	const float3 cameraDir = (midPos - camera->GetPos()).SafeANormalize();
	// beam's coor-system; degenerate if targetPos == startPos
	const float3 zdir = (targetPos - startPos).SafeANormalize();
	const float3 xdir = (cameraDir.cross(zdir)).SafeANormalize();
	const float3 ydir = (cameraDir.cross(xdir));

	const float beamEdgeSize = thickness;
	const float beamCoreSize = beamEdgeSize * corethickness;
	const float flareEdgeSize = thickness * flaresize;
	const float flareCoreSize = flareEdgeSize * corethickness;

	const float3& pos1 = startPos;
	const float3& pos2 = targetPos;

	#define WT1 weaponDef->visuals.texture1
	#define WT2 weaponDef->visuals.texture2
	#define WT3 weaponDef->visuals.texture3

	if ((midPos - camera->GetPos()).SqLength() < (1000.0f * 1000.0f)) {
		if (validTextures[2]) {
			GetThreadRenderBuffer().AddQuadTriangles(
				{ pos1 - xdir * beamEdgeSize,                       midtexx,   WT2->ystart, edgeColStart },
				{ pos1 + xdir * beamEdgeSize,                       midtexx,   WT2->yend,   edgeColStart },
				{ pos1 + xdir * beamEdgeSize - ydir * beamEdgeSize, WT2->xend, WT2->yend,   edgeColStart },
				{ pos1 - xdir * beamEdgeSize - ydir * beamEdgeSize, WT2->xend, WT2->ystart, edgeColStart }
			);
			GetThreadRenderBuffer().AddQuadTriangles(
				{ pos1 - xdir * beamCoreSize,                       midtexx,   WT2->ystart, coreColStart },
				{ pos1 + xdir * beamCoreSize,                       midtexx,   WT2->yend,   coreColStart },
				{ pos1 + xdir * beamCoreSize - ydir * beamCoreSize, WT2->xend, WT2->yend,   coreColStart },
				{ pos1 - xdir * beamCoreSize - ydir * beamCoreSize, WT2->xend, WT2->ystart, coreColStart }
			);

		}
		if (validTextures[1]) {
			GetThreadRenderBuffer().AddQuadTriangles(
				{ pos1 - xdir * beamEdgeSize,                       WT1->xstart, WT1->ystart, edgeColStart },
				{ pos1 + xdir * beamEdgeSize,                       WT1->xstart, WT1->yend,   edgeColStart },
				{ pos2 + xdir * beamEdgeSize,                       WT1->xend,   WT1->yend,   edgeColEnd },
				{ pos2 - xdir * beamEdgeSize,                       WT1->xend,   WT1->ystart, edgeColEnd }
			);

			GetThreadRenderBuffer().AddQuadTriangles(
				{ pos1 - xdir * beamCoreSize,                       WT1->xstart, WT1->ystart, coreColStart },
				{ pos1 + xdir * beamCoreSize,                       WT1->xstart, WT1->yend,   coreColStart },
				{ pos2 + xdir * beamCoreSize,                       WT1->xend,   WT1->yend,   coreColEnd },
				{ pos2 - xdir * beamCoreSize,                       WT1->xend,   WT1->ystart, coreColEnd }
			);
		}
		if (validTextures[2]) {
			GetThreadRenderBuffer().AddQuadTriangles(
				{ pos2 - xdir * beamEdgeSize,                       midtexx,   WT2->ystart, edgeColStart },
				{ pos2 + xdir * beamEdgeSize,                       midtexx,   WT2->yend,   edgeColStart },
				{ pos2 + xdir * beamEdgeSize + ydir * beamEdgeSize, WT2->xend, WT2->yend,   edgeColStart },
				{ pos2 - xdir * beamEdgeSize + ydir * beamEdgeSize, WT2->xend, WT2->ystart, edgeColStart }
			);

			GetThreadRenderBuffer().AddQuadTriangles(
				{ pos2 - xdir * beamCoreSize,                       midtexx,   WT2->ystart, coreColStart },
				{ pos2 + xdir * beamCoreSize,                       midtexx,   WT2->yend,   coreColStart },
				{ pos2 + xdir * beamCoreSize + ydir * beamCoreSize, WT2->xend, WT2->yend,   coreColStart },
				{ pos2 - xdir * beamCoreSize + ydir * beamCoreSize, WT2->xend, WT2->ystart, coreColStart }
			);
		}
	} else {
		if (validTextures[1]) {
			GetThreadRenderBuffer().AddQuadTriangles(
				{ pos1 - xdir * beamEdgeSize,                       WT1->xstart, WT1->ystart, edgeColStart },
				{ pos1 + xdir * beamEdgeSize,                       WT1->xstart, WT1->yend,   edgeColStart },
				{ pos2 + xdir * beamEdgeSize,                       WT1->xend,   WT1->yend,   edgeColEnd },
				{ pos2 - xdir * beamEdgeSize,                       WT1->xend,   WT1->ystart, edgeColEnd }
			);

			GetThreadRenderBuffer().AddQuadTriangles(
				{ pos1 - xdir * beamCoreSize,                       WT1->xstart, WT1->ystart, coreColStart },
				{ pos1 + xdir * beamCoreSize,                       WT1->xstart, WT1->yend,   coreColStart },
				{ pos2 + xdir * beamCoreSize,                       WT1->xend,   WT1->yend,   coreColEnd },
				{ pos2 - xdir * beamCoreSize,                       WT1->xend,   WT1->ystart, coreColEnd }
			);
		}
	}

	// draw flare
	if (validTextures[3]) {
		GetThreadRenderBuffer().AddQuadTriangles(
			{ pos1 - camera->GetRight() * flareEdgeSize - camera->GetUp() * flareEdgeSize, WT3->xstart, WT3->ystart, edgeColStart },
			{ pos1 + camera->GetRight() * flareEdgeSize - camera->GetUp() * flareEdgeSize, WT3->xend,   WT3->ystart, edgeColStart },
			{ pos1 + camera->GetRight() * flareEdgeSize + camera->GetUp() * flareEdgeSize, WT3->xend,   WT3->yend,   edgeColStart },
			{ pos1 - camera->GetRight() * flareEdgeSize + camera->GetUp() * flareEdgeSize, WT3->xstart, WT3->yend,   edgeColStart }
		);

		GetThreadRenderBuffer().AddQuadTriangles(
			{ pos1 - camera->GetRight() * flareCoreSize - camera->GetUp() * flareCoreSize, WT3->xstart, WT3->ystart, coreColStart },
			{ pos1 + camera->GetRight() * flareCoreSize - camera->GetUp() * flareCoreSize, WT3->xend,   WT3->ystart, coreColStart },
			{ pos1 + camera->GetRight() * flareCoreSize + camera->GetUp() * flareCoreSize, WT3->xend,   WT3->yend,   coreColStart },
			{ pos1 - camera->GetRight() * flareCoreSize + camera->GetUp() * flareCoreSize, WT3->xstart, WT3->yend,   coreColStart }
		);
	}

	#undef WT3
	#undef WT2
	#undef WT1
}

void CBeamLaserProjectile::DrawOnMinimap()
{
	const SColor color = { edgeColStart[0], edgeColStart[1], edgeColStart[2], 255u };

	rbMM.AddVertex({ startPos , color });
	rbMM.AddVertex({ targetPos, color });
}

int CBeamLaserProjectile::GetProjectilesCount() const
{
	return
		2 * validTextures[1] +
		4 * validTextures[2] +
		2 * validTextures[3];
}
