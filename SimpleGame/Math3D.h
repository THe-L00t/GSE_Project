#pragma once

// Minimal 3D math for the SimpleGame prototype.
// Header-only. Column-major matrices, matching OpenGL's memory layout.
// ASCII only: the project is built with CP949 sources, so non-ASCII comments corrupt.

#include <cmath>
#include <cstdlib>

const float kPi = 3.14159265358979f;

inline float Minf(float a, float b) { return a < b ? a : b; }
inline float Maxf(float a, float b) { return a > b ? a : b; }
inline float Clampf(float v, float a, float b) { return v < a ? a : (v > b ? b : v); }
inline float Saturatef(float v) { return Clampf(v, 0.0f, 1.0f); }
inline float Lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float DegToRad(float d) { return d * kPi / 180.0f; }
inline float RandUnit() { return (float)rand() / (float)RAND_MAX; }

// Smooth, frame-rate independent approach toward a target.
inline float Approach(float current, float target, float rate, float dt)
{
	float t = 1.0f - expf(-rate * dt);
	return current + (target - current) * t;
}

struct Vec3
{
	float x, y, z;

	Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
	Vec3(float a, float b, float c) : x(a), y(b), z(c) {}
};

inline Vec3 operator+(const Vec3& a, const Vec3& b) { return Vec3(a.x + b.x, a.y + b.y, a.z + b.z); }
inline Vec3 operator-(const Vec3& a, const Vec3& b) { return Vec3(a.x - b.x, a.y - b.y, a.z - b.z); }
inline Vec3 operator*(const Vec3& a, float s) { return Vec3(a.x * s, a.y * s, a.z * s); }
inline Vec3 operator*(const Vec3& a, const Vec3& b) { return Vec3(a.x * b.x, a.y * b.y, a.z * b.z); }
inline Vec3 operator-(const Vec3& a) { return Vec3(-a.x, -a.y, -a.z); }

inline float Dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3 Cross(const Vec3& a, const Vec3& b)
{
	return Vec3(a.y * b.z - a.z * b.y,
				a.z * b.x - a.x * b.z,
				a.x * b.y - a.y * b.x);
}

inline float Length(const Vec3& a) { return sqrtf(Dot(a, a)); }

inline Vec3 Normalize(const Vec3& a)
{
	float len = Length(a);
	if (len < 1e-6f) return Vec3(0.0f, 0.0f, 0.0f);
	return a * (1.0f / len);
}

inline Vec3 LerpV(const Vec3& a, const Vec3& b, float t)
{
	return Vec3(Lerpf(a.x, b.x, t), Lerpf(a.y, b.y, t), Lerpf(a.z, b.z, t));
}

// Horizontal (XZ plane) distance. The world is a flat quarter-view field,
// so most gameplay checks ignore height.
inline float DistXZ(const Vec3& a, const Vec3& b)
{
	float dx = a.x - b.x;
	float dz = a.z - b.z;
	return sqrtf(dx * dx + dz * dz);
}

struct Mat4
{
	float m[16];

	Mat4() { SetIdentity(); }

	void SetIdentity()
	{
		for (int i = 0; i < 16; ++i) m[i] = 0.0f;
		m[0] = m[5] = m[10] = m[15] = 1.0f;
	}
};

// result = a * b  (b is applied first)
inline Mat4 Mul(const Mat4& a, const Mat4& b)
{
	Mat4 r;
	for (int c = 0; c < 4; ++c)
	{
		for (int row = 0; row < 4; ++row)
		{
			float sum = 0.0f;
			for (int k = 0; k < 4; ++k)
				sum += a.m[k * 4 + row] * b.m[c * 4 + k];
			r.m[c * 4 + row] = sum;
		}
	}
	return r;
}

inline Mat4 MatTranslate(const Vec3& t)
{
	Mat4 r;
	r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z;
	return r;
}

inline Mat4 MatScale(const Vec3& s)
{
	Mat4 r;
	r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z;
	return r;
}

inline Mat4 MatRotateY(float rad)
{
	Mat4 r;
	float c = cosf(rad), s = sinf(rad);
	r.m[0] = c;  r.m[2] = -s;
	r.m[8] = s;  r.m[10] = c;
	return r;
}

inline Mat4 MatRotateX(float rad)
{
	Mat4 r;
	float c = cosf(rad), s = sinf(rad);
	r.m[5] = c;  r.m[6] = s;
	r.m[9] = -s; r.m[10] = c;
	return r;
}

inline Mat4 MatRotateZ(float rad)
{
	Mat4 r;
	float c = cosf(rad), s = sinf(rad);
	r.m[0] = c;  r.m[1] = s;
	r.m[4] = -s; r.m[5] = c;
	return r;
}

inline Mat4 MatOrtho(float l, float r_, float b, float t, float n, float f)
{
	Mat4 r;
	r.m[0] = 2.0f / (r_ - l);
	r.m[5] = 2.0f / (t - b);
	r.m[10] = -2.0f / (f - n);
	r.m[12] = -(r_ + l) / (r_ - l);
	r.m[13] = -(t + b) / (t - b);
	r.m[14] = -(f + n) / (f - n);
	r.m[15] = 1.0f;
	return r;
}

inline Mat4 MatLookAt(const Vec3& eye, const Vec3& center, const Vec3& up)
{
	Vec3 f = Normalize(center - eye);
	Vec3 s = Normalize(Cross(f, up));
	Vec3 u = Cross(s, f);

	Mat4 r;
	r.m[0] = s.x; r.m[1] = u.x; r.m[2] = -f.x; r.m[3] = 0.0f;
	r.m[4] = s.y; r.m[5] = u.y; r.m[6] = -f.y; r.m[7] = 0.0f;
	r.m[8] = s.z; r.m[9] = u.z; r.m[10] = -f.z; r.m[11] = 0.0f;
	r.m[12] = -Dot(s, eye);
	r.m[13] = -Dot(u, eye);
	r.m[14] = Dot(f, eye);
	r.m[15] = 1.0f;
	return r;
}

// Normal matrix = cofactor(upper 3x3) / det, i.e. transpose(inverse(M3)).
// Needed because boxes are drawn with non-uniform scale.
inline void MatNormal3x3(const Mat4& mat, float out[9])
{
	float a = mat.m[0], b = mat.m[4], c = mat.m[8];
	float d = mat.m[1], e = mat.m[5], f = mat.m[9];
	float g = mat.m[2], h = mat.m[6], i = mat.m[10];

	float c00 = e * i - f * h;
	float c01 = -(d * i - f * g);
	float c02 = d * h - e * g;
	float c10 = -(b * i - c * h);
	float c11 = a * i - c * g;
	float c12 = -(a * h - b * g);
	float c20 = b * f - c * e;
	float c21 = -(a * f - c * d);
	float c22 = a * e - b * d;

	float det = a * c00 + b * c01 + c * c02;
	if (fabsf(det) < 1e-8f) det = 1.0f;
	float inv = 1.0f / det;

	// column-major: out[col * 3 + row] = N[row][col]
	out[0] = c00 * inv; out[3] = c01 * inv; out[6] = c02 * inv;
	out[1] = c10 * inv; out[4] = c11 * inv; out[7] = c12 * inv;
	out[2] = c20 * inv; out[5] = c21 * inv; out[8] = c22 * inv;
}
