/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */


#include "Game/Camera.h"
#include "Game/GlobalUnsynced.h"
#include "MuzzleFlame.h"
#include "Rendering/Env/Particles/ProjectileDrawer.h"
#include "Rendering/GL/RenderBuffers.h"
#include "Rendering/Textures/TextureAtlas.h"


CR_BIND_DERIVED(CMuzzleFlame, CProjectile, )

CR_REG_METADATA(CMuzzleFlame,(
	CR_MEMBER(size),
	CR_MEMBER(age),
	CR_MEMBER(numFlame),
	CR_MEMBER(numSmoke),
	CR_MEMBER(randSmokeDir)
))


CMuzzleFlame::CMuzzleFlame(const float3& pos, const float3& speed, const float3& dir, float size):
	CProjectile(pos, speed, nullptr, false, false, false),
	size(size),
	age(0)
{
	this->dir = dir;
	this->pos -= dir * size * 0.2f;
	checkCol = false;
	castShadow = true;
	numFlame = 1 + (int)(size * 3);
	numSmoke = 1 + (int)(size * 5);
//	randSmokeDir=new float3[numSmoke];
	randSmokeDir.resize(numSmoke);

	for (int a = 0; a < numSmoke; ++a) {
		randSmokeDir[a] = dir + guRNG.NextFloat() * 0.4f;
	}
}


void CMuzzleFlame::Update()
{
	age++;
	if (age > (4 + size * 30)) {
		deleteMe = true;
	}
	pos += speed;
}

void CMuzzleFlame::Draw()
{
	unsigned char col[4];
	float alpha = std::max(0.0f, 1 - (age / (4 + size * 30)));
	float modAge = fastmath::apxsqrt(static_cast<float>(age + 2));

	for (int a = 0; a < numSmoke; ++a) { //! CAUTION: loop count must match EnlargeArrays above
		const int tex = a % projectileDrawer->NumSmokeTextures();
		// float xmod = 0.125f + (float(int(tex % 6))) / 16.0f;
		// float ymod =                (int(tex / 6))  / 16.0f;

		float drawsize = modAge * 3;
		float3 interPos(pos+randSmokeDir[a]*(a+2)*modAge*0.4f);
		float fade = std::max(0.0f, std::min(1.0f, (1 - alpha) * (20 + a) * 0.1f));

		col[0] = (unsigned char) (180 * alpha * fade);
		col[1] = (unsigned char) (180 * alpha * fade);
		col[2] = (unsigned char) (180 * alpha * fade);
		col[3] = (unsigned char) (255 * alpha * fade);

		#define st projectileDrawer->GetSmokeTexture(tex)
		GetThreadRenderBuffer().AddQuadTriangles(
			{ interPos - camera->GetRight() * drawsize - camera->GetUp() * drawsize, st->xstart, st->ystart, col },
			{ interPos + camera->GetRight() * drawsize - camera->GetUp() * drawsize, st->xend,   st->ystart, col },
			{ interPos + camera->GetRight() * drawsize + camera->GetUp() * drawsize, st->xend,   st->yend,   col },
			{ interPos - camera->GetRight() * drawsize + camera->GetUp() * drawsize, st->xstart, st->yend,   col }
		);
		#undef st

		if (fade < 1.0f) {
			float ifade = 1.0f - fade;
			col[0] = (unsigned char) (ifade * 255);
			col[1] = (unsigned char) (ifade * 255);
			col[2] = (unsigned char) (ifade * 255);
			col[3] = (unsigned char) (1);

			#define mft projectileDrawer->muzzleflametex
			GetThreadRenderBuffer().AddQuadTriangles(
				{ interPos - camera->GetRight() * drawsize - camera->GetUp() * drawsize, mft->xstart, mft->ystart, col },
				{ interPos + camera->GetRight() * drawsize - camera->GetUp() * drawsize, mft->xend,   mft->ystart, col },
				{ interPos + camera->GetRight() * drawsize + camera->GetUp() * drawsize, mft->xend,   mft->yend,   col },
				{ interPos - camera->GetRight() * drawsize + camera->GetUp() * drawsize, mft->xstart, mft->yend,   col }
			);
			#undef mft
		}
	}
}

int CMuzzleFlame::GetProjectilesCount() const
{
	return numSmoke * 2;
}
