#include "stdafx.h"
#include "Asset.h"

#include <cstring>
#include <fstream>
#include <iostream>

namespace
{
	const uint32_t kFlagSkinned = 1;
	const uint32_t kFlagIndex32 = 2;

	struct Reader
	{
		const std::vector<char>& data;
		size_t at = 0;
		bool   ok = true;

		explicit Reader(const std::vector<char>& bytes) : data(bytes) {}

		void Bytes(void* dst, size_t n)
		{
			if (!ok || at + n > data.size())
			{
				ok = false;
				return;
			}
			memcpy(dst, &data[at], n);
			at += n;
		}

		template <typename T>
		T Get()
		{
			T v{};
			Bytes(&v, sizeof(T));
			return v;
		}

		std::string Name(size_t n)
		{
			char buf[64] = {};
			Bytes(buf, n < sizeof(buf) ? n : sizeof(buf));
			buf[sizeof(buf) - 1] = '\0';
			return std::string(buf);
		}
	};

	bool ReadAll(const std::string& path, std::vector<char>& out)
	{
		std::ifstream file(path.c_str(), std::ios::binary | std::ios::ate);
		if (!file.good()) return false;
		std::streamsize size = file.tellg();
		if (size <= 0) return false;
		out.resize((size_t)size);
		file.seekg(0);
		file.read(&out[0], size);
		return file.good();
	}

	float Snorm(int16_t v)
	{
		return Maxf((float)v / 32767.0f, -1.0f);
	}

	Quat Normalized(const Quat& q)
	{
		float len = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
		Quat r;
		if (len < 1e-8f) return r;
		float inv = 1.0f / len;
		r.x = q.x * inv;
		r.y = q.y * inv;
		r.z = q.z * inv;
		r.w = q.w * inv;
		return r;
	}

	// Close neighbours in a sampled clip, so a normalised lerp is as good as a slerp.
	Quat Nlerp(const Quat& a, Quat b, float t)
	{
		if (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w < 0.0f)
		{
			b.x = -b.x;
			b.y = -b.y;
			b.z = -b.z;
			b.w = -b.w;
		}
		Quat r;
		r.x = Lerpf(a.x, b.x, t);
		r.y = Lerpf(a.y, b.y, t);
		r.z = Lerpf(a.z, b.z, t);
		r.w = Lerpf(a.w, b.w, t);
		return Normalized(r);
	}

	Mat4 PoseMatrix(const BonePose& p)
	{
		const Quat& q = p.q;
		float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z;
		float xy = q.x * q.y, xz = q.x * q.z, yz = q.y * q.z;
		float wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;

		Mat4 m;
		m.m[0] = 1.0f - 2.0f * (yy + zz);
		m.m[1] = 2.0f * (xy + wz);
		m.m[2] = 2.0f * (xz - wy);
		m.m[4] = 2.0f * (xy - wz);
		m.m[5] = 1.0f - 2.0f * (xx + zz);
		m.m[6] = 2.0f * (yz + wx);
		m.m[8] = 2.0f * (xz + wy);
		m.m[9] = 2.0f * (yz - wx);
		m.m[10] = 1.0f - 2.0f * (xx + yy);
		m.m[12] = p.t.x;
		m.m[13] = p.t.y;
		m.m[14] = p.t.z;
		return m;
	}

	float WrapOrClamp(float t, float duration, bool loop)
	{
		if (duration <= 0.0f) return 0.0f;
		if (loop)
		{
			t = fmodf(t, duration);
			return t < 0.0f ? t + duration : t;
		}
		return Clampf(t, 0.0f, duration);
	}
}

int Skeleton::FindBone(const char* name) const
{
	for (size_t i = 0; i < names.size(); ++i)
	{
		if (names[i] == name) return (int)i;
	}
	return -1;
}

int Skeleton::FindClip(const char* name) const
{
	for (size_t i = 0; i < clips.size(); ++i)
	{
		if (clips[i].name == name) return (int)i;
	}
	return -1;
}

bool LoadAssetMesh(const std::string& path, AssetMesh& mesh)
{
	std::vector<char> data;
	if (!ReadAll(path, data)) return false;

	Reader r(data);
	char magic[4] = {};
	r.Bytes(magic, 4);
	if (!r.ok || memcmp(magic, "GSM1", 4) != 0)
	{
		std::cout << path << ": not a GSM1 model.\n";
		return false;
	}

	uint32_t vertexCount = r.Get<uint32_t>();
	uint32_t indexCount = r.Get<uint32_t>();
	uint32_t boneCount = r.Get<uint32_t>();
	uint32_t flags = r.Get<uint32_t>();
	float bounds[6];
	r.Bytes(bounds, sizeof(bounds));
	mesh.boundsMin = Vec3(bounds[0], bounds[1], bounds[2]);
	mesh.boundsMax = Vec3(bounds[3], bounds[4], bounds[5]);
	for (int i = 0; i < 3; ++i)
		mesh.textures[i] = r.Name(48);
	float material[4];
	r.Bytes(material, sizeof(material));
	mesh.specular = material[0];
	mesh.emissive = material[1];

	Skeleton& s = mesh.skeleton;
	s.names.clear();
	s.parents.clear();
	s.inverseBind.clear();
	if ((flags & kFlagSkinned) != 0)
	{
		for (uint32_t b = 0; b < boneCount && r.ok; ++b)
		{
			s.names.push_back(r.Name(32));
			int parent = r.Get<int32_t>();
			s.parents.push_back(parent < (int)b ? parent : -1);
			Mat4 inv;
			r.Bytes(inv.m, sizeof(inv.m));
			s.inverseBind.push_back(inv);
		}
	}

	mesh.vertices.resize(vertexCount);
	if (vertexCount > 0) r.Bytes(&mesh.vertices[0], (size_t)vertexCount * sizeof(AssetVertex));

	mesh.indices.resize(indexCount);
	for (uint32_t i = 0; i < indexCount && r.ok; ++i)
		mesh.indices[i] = (flags & kFlagIndex32) != 0 ? r.Get<uint32_t>() : (uint32_t)r.Get<uint16_t>();

	if (!r.ok)
	{
		std::cout << path << ": truncated model.\n";
		return false;
	}
	for (size_t i = 0; i < mesh.indices.size(); ++i)
	{
		if (mesh.indices[i] >= vertexCount)
		{
			std::cout << path << ": index out of range.\n";
			return false;
		}
	}
	return vertexCount > 0 && indexCount > 0;
}

bool LoadAssetClips(const std::string& path, Skeleton& skeleton)
{
	std::vector<char> data;
	if (!ReadAll(path, data)) return false;

	Reader r(data);
	char magic[4] = {};
	r.Bytes(magic, 4);
	if (!r.ok || memcmp(magic, "GSA1", 4) != 0) return false;

	uint32_t boneCount = r.Get<uint32_t>();
	uint32_t clipCount = r.Get<uint32_t>();
	if ((int)boneCount != skeleton.BoneCount())
	{
		std::cout << path << ": clips are for " << boneCount << " bones, the model has " << skeleton.BoneCount() << ".\n";
		return false;
	}

	skeleton.clips.clear();
	for (uint32_t c = 0; c < clipCount && r.ok; ++c)
	{
		AnimClip clip;
		clip.name = r.Name(32);
		clip.fps = r.Get<float>();
		clip.frameCount = (int)r.Get<uint32_t>();
		clip.loop = r.Get<uint32_t>() != 0;
		if (clip.fps <= 0.0f) clip.fps = 30.0f;

		clip.frames.resize((size_t)clip.frameCount * boneCount);
		for (size_t i = 0; i < clip.frames.size() && r.ok; ++i)
		{
			float t[3];
			int16_t q[4];
			r.Bytes(t, sizeof(t));
			r.Bytes(q, sizeof(q));
			BonePose& p = clip.frames[i];
			p.t = Vec3(t[0], t[1], t[2]);
			Quat raw;
			raw.x = Snorm(q[0]);
			raw.y = Snorm(q[1]);
			raw.z = Snorm(q[2]);
			raw.w = Snorm(q[3]);
			p.q = Normalized(raw);
		}
		skeleton.clips.push_back(clip);
	}
	return r.ok;
}

void Animator::Bind(const Skeleton* s)
{
	skeleton = (s != nullptr && s->BoneCount() > 0) ? s : nullptr;
	current = previous = -1;
	time = previousTime = 0.0f;
	fade = fadeLength = 0.0f;
	if (skeleton == nullptr) return;

	int n = skeleton->BoneCount();
	poseA.resize(n);
	poseB.resize(n);
	global.resize(n);
	skin.assign(n, Mat4());
	for (int i = 0; i < n; ++i)
		global[i] = Inverse(skeleton->inverseBind[i]);
}

void Animator::Play(const char* clip, float fadeTime, bool restart)
{
	if (skeleton == nullptr) return;
	int c = skeleton->FindClip(clip);
	if (c < 0 || (c == current && !restart)) return;

	if (current >= 0 && fadeTime > 0.0f)
	{
		previous = current;
		previousTime = time;
		fade = fadeLength = fadeTime;
	}
	else
	{
		previous = -1;
		fade = fadeLength = 0.0f;
	}
	current = c;
	time = 0.0f;
	Evaluate();
}

void Animator::Update(float dt)
{
	if (skeleton == nullptr || current < 0) return;

	const AnimClip& clip = skeleton->clips[current];
	time = WrapOrClamp(time + dt * speed, clip.Duration(), clip.loop);
	AdvanceFade(dt);
	Evaluate();
}

void Animator::AdvanceFade(float dt)
{
	if (previous < 0) return;
	const AnimClip& before = skeleton->clips[previous];
	previousTime = WrapOrClamp(previousTime + dt * speed, before.Duration(), before.loop);
	fade -= dt;
	if (fade <= 0.0f) previous = -1;
}

void Animator::SetNormalizedTime(float t, float dt)
{
	if (skeleton == nullptr || current < 0) return;
	time = Saturatef(t) * skeleton->clips[current].Duration();
	AdvanceFade(dt);
	Evaluate();
}

void Animator::SetTime(float seconds)
{
	if (skeleton == nullptr || current < 0) return;
	const AnimClip& clip = skeleton->clips[current];
	time = WrapOrClamp(seconds, clip.Duration(), clip.loop);
	Evaluate();
}

float Animator::NormalizedTime() const
{
	if (skeleton == nullptr || current < 0) return 1.0f;
	float d = skeleton->clips[current].Duration();
	return d > 0.0f ? time / d : 1.0f;
}

bool Animator::Finished() const
{
	if (skeleton == nullptr || current < 0) return true;
	const AnimClip& clip = skeleton->clips[current];
	return !clip.loop && time >= clip.Duration() - 1e-4f;
}

bool Animator::IsPlaying(const char* clip) const
{
	return skeleton != nullptr && current >= 0 && skeleton->clips[current].name == clip;
}

Mat4 Animator::BoneMatrix(int bone) const
{
	if (skeleton == nullptr || bone < 0 || bone >= (int)global.size()) return Mat4();
	return global[bone];
}

void Animator::Sample(int clip, float t, std::vector<BonePose>& out) const
{
	const AnimClip& c = skeleton->clips[clip];
	int n = skeleton->BoneCount();
	if (c.frameCount <= 0 || c.frames.size() < (size_t)c.frameCount * n) return;

	float f = t * c.fps;
	int f0 = (int)floorf(f);
	float k = f - (float)f0;
	if (f0 < 0)
	{
		f0 = 0;
		k = 0.0f;
	}
	if (f0 >= c.frameCount - 1)
	{
		f0 = c.frameCount - 1;
		k = 0.0f;
	}
	int f1 = f0 + 1 < c.frameCount ? f0 + 1 : f0;

	const BonePose* a = &c.frames[(size_t)f0 * n];
	const BonePose* b = &c.frames[(size_t)f1 * n];
	for (int i = 0; i < n; ++i)
	{
		out[i].t = LerpV(a[i].t, b[i].t, k);
		out[i].q = Nlerp(a[i].q, b[i].q, k);
	}
}

void Animator::Evaluate()
{
	if (skeleton == nullptr || current < 0) return;

	Sample(current, time, poseA);
	if (previous >= 0 && fadeLength > 0.0f)
	{
		Sample(previous, previousTime, poseB);
		float w = SmoothStep(0.0f, 1.0f, fade / fadeLength);
		for (size_t i = 0; i < poseA.size(); ++i)
		{
			poseA[i].t = LerpV(poseA[i].t, poseB[i].t, w);
			poseA[i].q = Nlerp(poseA[i].q, poseB[i].q, w);
		}
	}

	int n = skeleton->BoneCount();
	for (int i = 0; i < n; ++i)
	{
		Mat4 local = PoseMatrix(poseA[i]);
		int parent = skeleton->parents[i];
		global[i] = parent >= 0 ? Mul(global[parent], local) : local;
		skin[i] = Mul(global[i], skeleton->inverseBind[i]);
	}
}
