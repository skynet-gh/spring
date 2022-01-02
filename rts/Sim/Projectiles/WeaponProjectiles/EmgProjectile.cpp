/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */


#include "EmgProjectile.h"
#include "Game/Camera.h"
#include "Map/Ground.h"
#include "Rendering/GL/RenderBuffers.h"
#include "Rendering/Textures/TextureAtlas.h"
#include "Sim/Projectiles/ExplosionGenerator.h"
#include "Sim/Projectiles/ProjectileHandler.h"
#include "Sim/Weapons/WeaponDef.h"

CR_BIND_DERIVED(CEmgProjectile, CWeaponProjectile, )

CR_REG_METADATA(CEmgProjectile,(
	CR_SETFLAG(CF_Synced),
	CR_MEMBER(intensity),
	CR_MEMBER(color)
))


CEmgProjectile::CEmgProjectile(const ProjectileParams& params): CWeaponProjectile(params)
{
	projectileType = WEAPON_EMG_PROJECTILE;

	if (weaponDef != nullptr) {
		SetRadiusAndHeight(weaponDef->collisionSize, 0.0f);
		drawRadius = weaponDef->size;

		intensity = weaponDef->intensity;
		color = weaponDef->visuals.color;
	} else {
		intensity = 0.0f;
	}
}

void CEmgProjectile::Update()
{
	// disable collisions when ttl reaches 0 since the
	// projectile will travel far past its range while
	// fading out
	checkCol &= (ttl >= 0);
	deleteMe |= (intensity <= 0.0f);

	pos += (speed * (1 - luaMoveCtrl));

	if (ttl <= 0) {
		// fade out over the next 10 frames at most
		intensity -= 0.1f;
		intensity = std::max(intensity, 0.0f);
	} else {
		explGenHandler.GenExplosion(cegID, pos, speed, ttl, intensity, 0.0f, owner(), nullptr);
	}

	UpdateGroundBounce();
	UpdateInterception();

	--ttl;
}

void CEmgProjectile::Draw()
{
	if (!validTextures[0])
		return;

	const SColor col = SColor{
		(color.x * intensity),
		(color.y * intensity),
		(color.z * intensity),
		intensity
	};

	GetThreadRenderBuffer().AddQuadTriangles(
		{ drawPos - camera->GetRight() * drawRadius - camera->GetUp() * drawRadius, weaponDef->visuals.texture1->xstart, weaponDef->visuals.texture1->ystart, col },
		{ drawPos + camera->GetRight() * drawRadius - camera->GetUp() * drawRadius, weaponDef->visuals.texture1->xend,   weaponDef->visuals.texture1->ystart, col },
		{ drawPos + camera->GetRight() * drawRadius + camera->GetUp() * drawRadius, weaponDef->visuals.texture1->xend,   weaponDef->visuals.texture1->yend,   col },
		{ drawPos - camera->GetRight() * drawRadius + camera->GetUp() * drawRadius, weaponDef->visuals.texture1->xstart, weaponDef->visuals.texture1->yend,   col }
	);
}

int CEmgProjectile::ShieldRepulse(const float3& shieldPos, float shieldForce, float shieldMaxSpeed)
{
	if (luaMoveCtrl)
		return 0;

	const float3 rdir = (pos - shieldPos).Normalize();

	if (rdir.dot(speed) < shieldMaxSpeed) {
		SetVelocityAndSpeed(speed + (rdir * shieldForce));
		return 2;
	}

	return 0;
}

int CEmgProjectile::GetProjectilesCount() const
{
	return 1 * validTextures[0];
}
