/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */


#include "glExtra.h"
#include "VertexArray.h"
#include "Map/Ground.h"
#include "Sim/Weapons/Weapon.h"
#include "Sim/Weapons/WeaponDef.h"
#include "System/SpringMath.h"
#include "System/Threading/ThreadPool.h"


/**
 *  Draws a trigonometric circle in 'resolution' steps.
 */
static void defSurfaceCircle(const float3& center, float radius, unsigned int res)
{
	CVertexArray* va=GetVertexArray();
	va->Initialize();
	for (unsigned int i = 0; i < res; ++i) {
		const float radians = math::TWOPI * (float)i / (float)res;
		float3 pos;
		pos.x = center.x + (fastmath::sin(radians) * radius);
		pos.z = center.z + (fastmath::cos(radians) * radius);
		pos.y = CGround::GetHeightAboveWater(pos.x, pos.z, false) + 5.0f;
		va->AddVertex0(pos);
	}
	va->DrawArray0(GL_LINE_LOOP);
}

static void defSurfaceColoredCircle(const float3& center, float radius, const SColor& col, unsigned int res)
{
	CVertexArray* va = GetVertexArray();
	va->Initialize();
	for (unsigned int i = 0; i < res; ++i) {
		const float radians = math::TWOPI * (float)i / (float)res;
		float3 pos;
		pos.x = center.x + (fastmath::sin(radians) * radius);
		pos.z = center.z + (fastmath::cos(radians) * radius);
		pos.y = CGround::GetHeightAboveWater(pos.x, pos.z, false) + 5.0f;
		va->AddVertexC(pos, col);
	}
	va->DrawArrayC(GL_LINE_LOOP);
}

static void defSurfaceSquare(const float3& center, float xsize, float zsize)
{
	// FIXME: terrain contouring
	const float3 p0 = center + float3(-xsize, 0.0f, -zsize);
	const float3 p1 = center + float3( xsize, 0.0f, -zsize);
	const float3 p2 = center + float3( xsize, 0.0f,  zsize);
	const float3 p3 = center + float3(-xsize, 0.0f,  zsize);

	CVertexArray* va=GetVertexArray();
	va->Initialize();
		va->AddVertex0(p0.x, CGround::GetHeightAboveWater(p0.x, p0.z, false), p0.z);
		va->AddVertex0(p1.x, CGround::GetHeightAboveWater(p1.x, p1.z, false), p1.z);
		va->AddVertex0(p2.x, CGround::GetHeightAboveWater(p2.x, p2.z, false), p2.z);
		va->AddVertex0(p3.x, CGround::GetHeightAboveWater(p3.x, p3.z, false), p3.z);
	va->DrawArray0(GL_LINE_LOOP);
}


SurfaceCircleFunc glSurfaceCircle = defSurfaceCircle;
SurfaceColoredCircleFunc glSurfaceColoredCircle = defSurfaceColoredCircle;
SurfaceSquareFunc glSurfaceSquare = defSurfaceSquare;

void setSurfaceCircleFunc(SurfaceCircleFunc func)
{
	glSurfaceCircle = (func == nullptr)? defSurfaceCircle: func;
}

void setSurfaceColoredCircleFunc(SurfaceColoredCircleFunc func)
{
	glSurfaceColoredCircle = (func == nullptr) ? defSurfaceColoredCircle : func;
}

void setSurfaceSquareFunc(SurfaceSquareFunc func)
{
	glSurfaceSquare = (func == nullptr)? defSurfaceSquare: func;
}




static constexpr float (*weaponRangeFuncs[])(const CWeapon*, const WeaponDef*, float, float) = {
	CWeapon::GetStaticRange2D,
	CWeapon::GetLiveRange2D,
};

static void glBallisticCircle(const CWeapon* weapon, const WeaponDef* weaponDef, unsigned int resolution, const float3& center, const float3& params)
{
	constexpr int resDiv = 50;

	CVertexArray* va = GetVertexArray();
	va->Initialize();
	va->EnlargeArrays(resolution, 0, VA_SIZE_0);

	float3* vertices = va->GetTypedVertexArray<float3>(resolution);

	const float radius = params.x;
	const float slope = params.y;

	const float wdHeightMod = weaponDef->heightmod;
	const float wdProjGravity = mix(params.z, -weaponDef->myGravity, weaponDef->myGravity != 0.0f);

	for_mt(0, resolution, [&](const int i) {
		const float radians = math::TWOPI * (float)i / (float)resolution;

		const float sinR = fastmath::sin(radians);
		const float cosR = fastmath::cos(radians);

		float maxWeaponRange = radius;

		float3 pos;
		pos.x = center.x + (sinR * maxWeaponRange);
		pos.z = center.z + (cosR * maxWeaponRange);
		pos.y = CGround::GetHeightAboveWater(pos.x, pos.z, false);

		float posHeightDelta = (pos.y - center.y) * 0.5f;
		float posWeaponRange = weaponRangeFuncs[weapon != nullptr](weapon, weaponDef, posHeightDelta * wdHeightMod, wdProjGravity);
		float rangeIncrement = (maxWeaponRange -= (posHeightDelta * slope)) * 0.5f;
		float ydiff = 0.0f;

		// "binary search" for the maximum positional range per angle, accounting for terrain height
		for (int j = 0; j < resDiv && (std::fabs(posWeaponRange - maxWeaponRange) + ydiff) > (0.01f * maxWeaponRange); j++) {
			if (posWeaponRange > maxWeaponRange) {
				maxWeaponRange += rangeIncrement;
			} else {
				// overshot, reduce step-size
				maxWeaponRange -= rangeIncrement;
				rangeIncrement *= 0.5f;
			}

			pos.x = center.x + (sinR * maxWeaponRange);
			pos.z = center.z + (cosR * maxWeaponRange);

			const float newY = CGround::GetHeightAboveWater(pos.x, pos.z, false);
			ydiff = std::fabs(pos.y - newY);
			pos.y = newY;

			posHeightDelta = pos.y - center.y;
			posWeaponRange = weaponRangeFuncs[weapon != nullptr](weapon, weaponDef, posHeightDelta * wdHeightMod, wdProjGravity);
		}

		pos.x = center.x + (sinR * posWeaponRange);
		pos.z = center.z + (cosR * posWeaponRange);
		pos.y = CGround::GetHeightAboveWater(pos.x, pos.z, false) + 5.0f;

		vertices[i] = pos;
	});

	va->DrawArray0(GL_LINE_LOOP);
}


/*
 *  Draws a trigonometric circle in 'resolution' steps, with a slope modifier
 */
void glBallisticCircle(const CWeapon* weapon, unsigned int resolution, const float3& center, const float3& params)
{
	glBallisticCircle(weapon, weapon->weaponDef, resolution, center, params);
}

void glBallisticCircle(const WeaponDef* weaponDef, unsigned int resolution, const float3& center, const float3& params)
{
	glBallisticCircle(nullptr, weaponDef, resolution, center, params);
}


/******************************************************************************/

void glDrawVolume(DrawVolumeFunc drawFunc, const void* data)
{
	glDepthMask(GL_FALSE);
	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_DEPTH_CLAMP_NV);

	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);

		glEnable(GL_STENCIL_TEST);
		glStencilMask(0x1);
		glStencilFunc(GL_ALWAYS, 0, 0x1);
		glStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
		drawFunc(data); // draw

	glDisable(GL_DEPTH_TEST);

	glStencilFunc(GL_NOTEQUAL, 0, 0x1);
	glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO); // clear as we go

	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

	glEnable(GL_CULL_FACE);
	glCullFace(GL_FRONT);

	drawFunc(data);   // draw

	glDisable(GL_DEPTH_CLAMP_NV);
	glDisable(GL_STENCIL_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
}



/******************************************************************************/

void glWireCube(unsigned int* listID) {
	static constexpr float3 vertices[8] = {
		{ 0.5f,  0.5f,  0.5f},
		{ 0.5f, -0.5f,  0.5f},
		{-0.5f, -0.5f,  0.5f},
		{-0.5f,  0.5f,  0.5f},

		{ 0.5f,  0.5f, -0.5f},
		{ 0.5f, -0.5f, -0.5f},
		{-0.5f, -0.5f, -0.5f},
		{-0.5f,  0.5f, -0.5f},
	};

	if ((*listID) != 0) {
		glCallList(*listID);
		return;
	}

	glNewList(((*listID) = glGenLists(1)), GL_COMPILE);
	glPushAttrib(GL_POLYGON_BIT);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	glBegin(GL_QUADS);
		glVertex3f(vertices[0].x, vertices[0].y, vertices[0].z);
		glVertex3f(vertices[1].x, vertices[1].y, vertices[1].z);
		glVertex3f(vertices[2].x, vertices[2].y, vertices[2].z);
		glVertex3f(vertices[3].x, vertices[3].y, vertices[3].z);

		glVertex3f(vertices[4].x, vertices[4].y, vertices[4].z);
		glVertex3f(vertices[5].x, vertices[5].y, vertices[5].z);
		glVertex3f(vertices[6].x, vertices[6].y, vertices[6].z);
		glVertex3f(vertices[7].x, vertices[7].y, vertices[7].z);
	glEnd();
	glBegin(GL_QUAD_STRIP);
		glVertex3f(vertices[4].x, vertices[4].y, vertices[4].z);
		glVertex3f(vertices[0].x, vertices[0].y, vertices[0].z);

		glVertex3f(vertices[5].x, vertices[5].y, vertices[5].z);
		glVertex3f(vertices[1].x, vertices[1].y, vertices[1].z);

		glVertex3f(vertices[6].x, vertices[6].y, vertices[6].z);
		glVertex3f(vertices[2].x, vertices[2].y, vertices[2].z);

		glVertex3f(vertices[7].x, vertices[7].y, vertices[7].z);
		glVertex3f(vertices[3].x, vertices[3].y, vertices[3].z);

		glVertex3f(vertices[4].x, vertices[4].y, vertices[4].z);
		glVertex3f(vertices[0].x, vertices[0].y, vertices[0].z);
	glEnd();

	glPopAttrib();
	glEndList();
}

void glWireCylinder(unsigned int* listID, unsigned int numDivs, float zSize) {
	if ((*listID) != 0) {
		glCallList(*listID);
		return;
	}

	assert(numDivs > 2);
	assert(zSize > 0.0f);

	std::vector<float3> vertices(numDivs * 2);

	glNewList(((*listID) = glGenLists(1)), GL_COMPILE);
	glPushAttrib(GL_POLYGON_BIT);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	// front end-cap
	glBegin(GL_TRIANGLE_FAN);
		glVertex3f(0.0f, 0.0f, 0.0f);

		for (unsigned int n = 0; n <= numDivs; n++) {
			const unsigned int i = n % numDivs;

			vertices[i].x = std::cos(i * (math::TWOPI / numDivs));
			vertices[i].y = std::sin(i * (math::TWOPI / numDivs));
			vertices[i].z = 0.0f;

			glVertex3f(vertices[i].x, vertices[i].y, vertices[i].z);
		}
	glEnd();

	// back end-cap
	glBegin(GL_TRIANGLE_FAN);
		glVertex3f(0.0f, 0.0f, zSize);

		for (unsigned int n = 0; n <= numDivs; n++) {
			const unsigned int i = n % numDivs;

			vertices[i + numDivs].x = vertices[i].x;
			vertices[i + numDivs].y = vertices[i].y;
			vertices[i + numDivs].z = zSize;

			glVertex3f(vertices[i + numDivs].x, vertices[i + numDivs].y, vertices[i + numDivs].z);
		}
	glEnd();

	glBegin(GL_QUAD_STRIP);
		for (unsigned int n = 0; n < numDivs; n++) {
			glVertex3f(vertices[n          ].x, vertices[n          ].y, vertices[n          ].z);
			glVertex3f(vertices[n + numDivs].x, vertices[n + numDivs].y, vertices[n + numDivs].z);
		}

		glVertex3f(vertices[0          ].x, vertices[0          ].y, vertices[0          ].z);
		glVertex3f(vertices[0 + numDivs].x, vertices[0 + numDivs].y, vertices[0 + numDivs].z);
	glEnd();

	glPopAttrib();
	glEndList();
}

void glWireSphere(unsigned int* listID, unsigned int numRows, unsigned int numCols) {
	if ((*listID) != 0) {
		glCallList(*listID);
		return;
	}

	assert(numRows > 2);
	assert(numCols > 2);

	std::vector<float3> vertices((numRows + 1) * numCols);

	for (unsigned int row = 0; row <= numRows; row++) {
		for (unsigned int col = 0; col < numCols; col++) {
			const float a = (col * (math::TWOPI / numCols));
			const float b = (row * (math::PI    / numRows));

			float3& v = vertices[row * numCols + col];

			v.x = std::cos(a) * std::sin(b);
			v.y = std::sin(a) * std::sin(b);
			v.z = std::cos(b);
		}
	}

	glNewList(((*listID) = glGenLists(1)), GL_COMPILE);
	glPushAttrib(GL_POLYGON_BIT);
	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	const float3& v0 = vertices.front();
	const float3& v1 = vertices.back();

	// top slice
	glBegin(GL_TRIANGLE_FAN);
		glVertex3f(v0.x, v0.y, v0.z);

		for (unsigned int col = 0; col <= numCols; col++) {
			const unsigned int i = 1 * numCols + (col % numCols);
			const float3& v = vertices[i];

			glVertex3f(v.x, v.y, v.z);
		}
	glEnd();

	// bottom slice
	glBegin(GL_TRIANGLE_FAN);
		glVertex3f(v1.x, v1.y, v1.z);

		for (unsigned int col = 0; col <= numCols; col++) {
			const unsigned int i = ((numRows - 1) * numCols) + (col % numCols);
			const float3& v = vertices[i];

			glVertex3f(v.x, v.y, v.z);
		}
	glEnd();

	for (unsigned int row = 1; row < numRows - 1; row++) {
		glBegin(GL_QUAD_STRIP);

		for (unsigned int col = 0; col < numCols; col++) {
			const float3& v0 = vertices[(row + 0) * numCols + col];
			const float3& v1 = vertices[(row + 1) * numCols + col];

			glVertex3f(v0.x, v0.y, v0.z);
			glVertex3f(v1.x, v1.y, v1.z);
		}

		const float3& v0 = vertices[(row + 0) * numCols];
		const float3& v1 = vertices[(row + 1) * numCols];

		glVertex3f(v0.x, v0.y, v0.z);
		glVertex3f(v1.x, v1.y, v1.z);
		glEnd();
	}

	glPopAttrib();
	glEndList();
}
