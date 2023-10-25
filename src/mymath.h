#pragma once
#include <stdint.h>

typedef struct Vec2D
{
	float X;
	float Y;
} Vec2D;

typedef struct Vec3D
{
	float X;
	float Y;
	float Z;
} Vec3D;

typedef struct Vec4D
{
	float X;
	float Y;
	float Z;
	float W;
} Vec4D;

typedef struct Mat3X3
{
	union
	{
		struct
		{
			float A00, A01, A02;
			float A10, A11, A12;
			float A20, A21, A22;
		};
		Vec3D V[3];
		float A[3][3];
	};
} Mat3X3;

typedef struct Mat4X4
{
	union
	{
		struct
		{
			float A00, A01, A02, A03;
			float A10, A11, A12, A13;
			float A20, A21, A22, A23;
			float A30, A31, A32, A33;
		};
		Vec4D V[4];
		float A[4][4];
	};
} Mat4X4;

// *** 2D vector math ***
Vec2D MathVec2DZero(void);

void MathVec2DModulateByVec2D(const Vec2D* vec1, const Vec2D* vec2, Vec2D* out);

void MathVec2DModulateByScalar(const Vec2D* vec1, float s, Vec2D* out);

void MathVec2DAddition(const Vec2D* vec1, const Vec2D* vec2, Vec2D* out);

void MathVec2DSubtraction(const Vec2D* vec1, const Vec2D* vec2, Vec2D* out);

float MathVec2DDot(const Vec2D* vec1, const Vec2D* vec2);

void MathVec2DProj(const Vec2D* vec1, const Vec2D* vec2, Vec2D* out);

void MathVec2DPerp(const Vec2D* vec1, const Vec2D* vec2, Vec2D* out);

void MathVec2DNormalize(Vec2D* vec1);

// *** 3D vector math ***
Vec3D MathVec3DZero(void);

Vec3D MathVec3DFromXYZ(float x, float y, float z);

void MathVec3DModulateByVec3D(const Vec3D* vec1, const Vec3D* vec2, Vec3D* out);

Vec3D MathVec3DModulateByScalar(const Vec3D* vec1, float s);

Vec3D MathVec3DAddition(const Vec3D* vec1, const Vec3D* vec2);

Vec3D MathVec3DSubtraction(const Vec3D* vec1, const Vec3D* vec2);

float MathVec3DDot(const Vec3D* vec1, const Vec3D* vec2);

void MathVec3DProj(const Vec3D* vec1, const Vec3D* vec2, Vec3D* out);

void MathVec3DPerp(const Vec3D* vec1, const Vec3D* vec2, Vec3D* out);

Vec3D MathVec3DCross(const Vec3D* vec1, const Vec3D* vec2);

void MathVec3DNormalize(Vec3D* vec1);

void MathVec3DNegate(Vec3D* vec);

void MathVec3DPrint(const Vec3D* vec);

// *** 4D vector math ***
Vec4D MathVec4DZero(void);

Vec4D MathVec4DFromXYZW(float x, float y, float z, float w);

void MathVec4DModulateByVec4D(const Vec4D* vec1, const Vec4D* vec2, Vec4D* out);

void MathVec4DModulateByScalar(const Vec4D* vec1, float s, Vec4D* out);

Vec4D MathVec4DAddition(const Vec4D* vec1, const Vec4D* vec2);

void MathVec4DSubtraction(const Vec4D* vec1, const Vec4D* vec2, Vec4D* out);

float MathVec4DDot(const Vec4D* vec1, const Vec4D* vec2);

void MathVec4DNormalize(Vec4D* vec1);

void MathVec4DPrint(const Vec4D* vec);

// *** 3X3 matrix math ***
Mat3X3 MathMat3X3Identity(void);

Mat3X3 MathMat3X3Addition(const Mat3X3* mat1, const Mat3X3* mat2);

void MathMat3X3ModulateByScalar(Mat3X3* mat, float s);

Vec3D MathMat3X3MultByVec3D(const Mat3X3* mat, const Vec3D* vec);

Mat3X3 MathMat3X3MultByMat3X3(const Mat3X3* mat1, const Mat3X3* mat2);

void MathMat3X3Transpose(Mat3X3* mat);

void MathMat3X3Copy(const Mat3X3* from, Mat3X3* to);

// *** 4X4 matrix math ***
Mat4X4 MathMat4X4Identity(void);

void MathMat4X4Copy(const Mat4X4* from, Mat4X4* to);

void MathMat4X4ModulateByScalar(Mat4X4* mat, float s);

Mat4X4 MathMat4X4Addition(const Mat4X4* mat1, const Mat4X4* mat2);

Vec4D MathMat4X4MultVec4DByMat4X4(const Vec4D* vec, const Mat4X4* mat);

Mat4X4 MathMat4X4MultMat4X4ByMat4X4(const Mat4X4* mat1, const Mat4X4* mat2);

void MathMat4X4Transpose(Mat4X4* mat);

Mat4X4 MathMat4X4ScaleFromVec3D(const Vec3D* scale);

Mat4X4 MathMat4X4TranslateFromVec3D(const Vec3D* offset);

Mat4X4 MathMat4X4RotateFromVec3D(const Vec3D* angles);

float MathMat4X4Determinant(const Mat4X4* mat);

void MathMat4X4Normalize(Mat4X4* mat);

Mat4X4 MathMat4X4Orthographic(float viewWidth,
                              float viewHeight,
                              float zNear,
                              float zFar);

Mat4X4 MathMat4X4ViewAt(const Vec3D* eyePos, const Vec3D* focusPos, const Vec3D* upDirect);

Mat4X4 MathMat4X4RotateZ(float angle);

Mat4X4 MathMat4X4RotateX(float angle);

Mat4X4 MathMat4X4RotateY(float angle);

Mat4X4 MathMat4X4PerspectiveFov(float fovAngleY, float aspectRatio, float nearZ, float farZ);

// *** misc math helpers ***
float MathClamp(float min, float max, float v);

float MathToRadians(float degrees);

float MathToDegrees(float radians);

unsigned int MathNearlyEqual(const float lhs, const float rhs);

int32_t MathIsNaN(float n);

float MathRandom(float min, float max);

#ifdef MATH_TEST
void MathTest(void);
#endif
