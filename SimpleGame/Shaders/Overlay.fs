#version 330

// Mode 0: flat rect (fades).
// Mode 1: full-screen atmosphere pass (vignette + spore haze).
// Mode 2: progress bar: a worn groove, its fill running in it like water with a light at its head.
// Mode 3 and up: a styled panel; the style is the mode less three (UiStyle in Renderer.h).
//
// The panels follow the look of the world: what people made is a straight-edged slate whose frame has
// sagged and broken in places, and moss grows in over it from a corner in round clumps, spores glinting
// in it. Only the spore teal and the lantern amber ever glow. Her letter is old paper, like her map.

#include "Env.glsl"

in vec2 v_UV;

uniform vec4  u_Rect;
uniform int   u_Mode;
uniform vec4  u_Color;
uniform vec3  u_Color2;
uniform float u_Fill;
uniform float u_Vignette;
uniform float u_Haze;
uniform vec3  u_HazeColor;
uniform float u_Time;

layout(location = 0) out vec4 FragColor;

const int STYLE_MIST = 0;      // a HUD panel
const int STYLE_WINDOW = 1;    // a window over the paused game
const int STYLE_DIALOG = 2;
const int STYLE_LANTERN = 3;   // the speaker's lamp at the dialog's edge
const int STYLE_BAND = 4;      // a band of shade under a line of words
const int STYLE_TAG = 5;       // a key prompt
const int STYLE_SELECT = 6;    // the chosen row in a window
const int STYLE_SHADOW = 7;
const int STYLE_PAPER = 8;     // her letter
const int STYLE_RULE = 9;      // the stroke along a window's top

const vec3 kSpore = vec3(0.55, 0.95, 0.80);
const vec3 kLantern = vec3(0.98, 0.74, 0.42);
const vec3 kFrame = vec3(0.58, 0.66, 0.59);
const vec3 kMossDark = vec3(0.09, 0.15, 0.09);
const vec3 kMossLight = vec3(0.38, 0.49, 0.26);

float hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

float valueNoise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float a = hash12(i);
	float b = hash12(i + vec2(1.0, 0.0));
	float c = hash12(i + vec2(0.0, 1.0));
	float d = hash12(i + vec2(1.0, 1.0));
	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p)
{
	return valueNoise(p) * 0.55 + valueNoise(p * 2.07 + 5.3) * 0.30 + valueNoise(p * 4.13 + 9.1) * 0.15;
}

// The quad's size in pixels, from how fast its UV runs across a pixel.
vec2 QuadSize()
{
	return 1.0 / max(abs(vec2(dFdx(v_UV.x), dFdy(v_UV.y))), vec2(1e-5));
}

// Distance in pixels inside a rounded rectangle: positive inside, negative outside.
float Inside(vec2 p, vec2 size, float r)
{
	vec2 q = abs(p - size * 0.5) - (size * 0.5 - r);
	return -(length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r);
}

// How far round the border a point is, so marks along it run on from one side to the next.
float Along(vec2 p, vec2 size)
{
	float left = p.x;
	float right = size.x - p.x;
	float up = p.y;
	float down = size.y - p.y;
	float m = min(min(left, right), min(up, down));
	if (m == up) return p.x;
	if (m == right) return size.x + p.y;
	if (m == down) return size.x + size.y + right;
	return 2.0 * size.x + size.y + down;
}

// Lays a colour over what is there, covering it by k.
vec4 Over(vec4 under, vec3 color, float k)
{
	k = clamp(k, 0.0, 1.0);
	float a = k + under.a * (1.0 - k);
	if (a < 1e-4) return vec4(color, 0.0);
	return vec4((color * k + under.rgb * under.a * (1.0 - k)) / a, a);
}

// The corner of the screen a panel is nearest: 0 top-left, 1 top-right, 2 bottom-right, 3 bottom-left.
int ScreenCorner()
{
	vec2 c = u_Rect.xy + u_Rect.zw * 0.5;
	if (c.y >= 0.0) return c.x < 0.0 ? 0 : 1;
	return c.x < 0.0 ? 3 : 2;
}

// A frame line set in from the edge that has sagged a little and broken off here and there; at the corners it
// holds, a little brighter, like the brackets of an old frame.
vec4 WornFrame(vec4 c, vec2 p, vec2 size, float inset, float strength, float seed)
{
	float along = Along(p, size);
	float sag = (valueNoise(vec2(along * 0.011, seed + 4.0)) - 0.5) * 1.6;
	float d = Inside(p, size, inset + 3.0) - inset - sag;
	float line = 1.0 - smoothstep(0.35, 1.05, abs(d));
	float kept = smoothstep(0.24, 0.34, valueNoise(vec2(along * 0.028, seed + 11.0)));
	float fromCorner = max(min(p.x, size.x - p.x), min(p.y, size.y - p.y));
	float bracket = 1.0 - smoothstep(inset + 10.0, inset + 12.0, fromCorner);
	return Over(c, kFrame, line * strength * (max(kept, bracket) + bracket * 0.5));
}

// One clump of moss, rounded off at its radius.
float Blob(vec2 q, vec2 at, float r)
{
	vec2 d = (q - at) / r;
	return max(0.0, 1.0 - dot(d, d));
}

// The clumps of moss at a corner: a big one on the corner and smaller ones trailing off along both edges.
// q runs inward from that corner along both edges.
float MossField(vec2 p, vec2 size, int corner, float scale, float seed)
{
	vec2 q = p;
	if (corner == 1 || corner == 2) q.x = size.x - p.x;
	if (corner == 2 || corner == 3) q.y = size.y - p.y;

	float f = Blob(q, vec2(scale * 0.10), scale);
	for (int i = 1; i < 6; ++i)
	{
		float fi = float(i);
		float h1 = hash12(vec2(fi, seed));
		float h2 = hash12(vec2(fi, seed + 3.3));
		float r = scale * (0.75 - 0.10 * fi) * (0.70 + 0.50 * h1) * exp(-0.18 * fi);
		f += Blob(q, vec2(scale * (0.55 + 0.62 * fi + 0.30 * h2), scale * (0.12 * h1 - 0.08)), r);
		if (i < 4) f += Blob(q, vec2(scale * (0.12 * h2 - 0.08), scale * (0.55 + 0.62 * fi + 0.30 * h1)), r * 0.85);
	}
	return f;
}

// Moss grown in over a corner: round clumps lit from the upper left, fuzzy at their rims, a little shade
// cast on the panel round them, and spores glinting on them now and then.
vec4 Moss(vec4 c, vec2 p, vec2 size, int corner, float scale, float seed)
{
	float f = MossField(p, size, corner, scale, seed);
	float fuzz = (fbm(p * 0.9 + seed) - 0.5) * 0.34 + (hash12(floor(p) + seed) - 0.5) * 0.06;
	float m = smoothstep(0.17, 0.24, f + fuzz);

	c = Over(c, vec3(0.0), (1.0 - m) * smoothstep(0.02, 0.17, f + fuzz) * 0.40);

	// Lit where the clump's surface turns toward the upper left, gently: a cushion, not a bead.
	float gx = MossField(p + vec2(1.0, 0.0), size, corner, scale, seed) - f;
	float gy = MossField(p + vec2(0.0, 1.0), size, corner, scale, seed) - f;
	float lit = clamp(0.50 + (0.6 * gx + 0.8 * gy) * scale * 0.20, 0.0, 1.0);
	// Grainy all over, as moss is: fine tufts a pixel or two across.
	float grain = valueNoise(p * 0.95 + seed) * 0.6 + hash12(floor(p * 0.8) + seed) * 0.4;
	vec3 moss = mix(kMossDark, kMossLight, lit * 0.75 + grain * 0.35) * (0.80 + 0.30 * grain);
	moss += vec3(0.10, 0.12, 0.04) * (1.0 - smoothstep(0.24, 0.34, f + fuzz)) * lit;
	c = Over(c, moss, m * 0.97);

	vec2 cell = floor(p / 7.0);
	float h = hash12(cell + seed * 1.7);
	vec2 at = (cell + 0.25 + 0.5 * vec2(hash12(cell + 3.1), hash12(cell + 5.7))) * 7.0;
	float r = length(p - at);
	float twinkle = max(0.0, sin(u_Time * (0.6 + h * 1.3) + h * 40.0)) * step(0.72, h) * smoothstep(0.3, 0.6, f);
	c = Over(c, kSpore, exp(-r / 2.2) * 0.35 * twinkle);
	return Over(c, kSpore, (1.0 - smoothstep(0.3, 1.3, r)) * twinkle);
}

// A slate of mist: dark, a little lighter where the light falls on its top, edges not quite straight.
vec4 MistPanel(vec2 p, vec2 size, vec3 color, float alpha, float seed, float soft)
{
	float along = Along(p, size);
	float wobble = (valueNoise(vec2(along * 0.035, seed)) - 0.5) * 2.4;
	float d = Inside(p, size, 5.0) + wobble;
	float body = smoothstep(0.0, soft, d);

	float grain = 0.92 + 0.14 * valueNoise(p * vec2(0.08, 0.6) + seed);
	float fall = 1.0 - p.y / size.y;
	vec3 col = (color + vec3(0.030, 0.042, 0.038) * fall) * grain;
	return vec4(col, alpha * body);
}

vec4 Styled(int style)
{
	vec2 size = QuadSize();
	vec2 p = vec2(v_UV.x, 1.0 - v_UV.y) * size;
	float seed = hash12(floor(size * 0.5)) * 37.0;
	vec3 color = u_Color.rgb;
	float alpha = u_Color.a;

	if (style == STYLE_MIST)
	{
		vec4 c = MistPanel(p, size, color, min(alpha * 1.35, 0.82), seed, 6.0);
		c = WornFrame(c, p, size, 4.5, 0.36, seed);
		return Moss(c, p, size, ScreenCorner(), clamp(min(size.x, size.y) * 0.34, 10.0, 22.0), seed);
	}

	if (style == STYLE_WINDOW || style == STYLE_DIALOG)
	{
		vec4 c = MistPanel(p, size, color, alpha, seed, 3.5);
		// Darker toward the edges, as if the light were held in the middle.
		float d = Inside(p, size, 5.0);
		c.rgb *= 0.80 + 0.22 * smoothstep(0.0, 46.0, d);
		if (style == STYLE_DIALOG)
		{
			// The speaker's lamp warms the side it stands on.
			float flicker = 0.88 + 0.12 * valueNoise(vec2(u_Time * 2.3, 0.0));
			c.rgb += kLantern * 0.11 * exp(-p.x / 80.0) * flicker;
		}
		c = WornFrame(c, p, size, 6.0, 0.42, seed);
		c = WornFrame(c, p, size, 10.0, 0.13, seed + 19.0);
		if (style == STYLE_DIALOG) return Moss(c, p, size, 2, 20.0, seed);
		c = Moss(c, p, size, 0, 26.0, seed);
		return Moss(c, p, size, 2, 20.0, seed + 23.0);
	}

	if (style == STYLE_LANTERN)
	{
		// Brightest beside the speaker's name, its light running down the edge and fading.
		float flicker = 0.85 + 0.25 * valueNoise(vec2(p.y * 0.04 - u_Time * 1.2, u_Time * 0.7));
		float fall = smoothstep(4.0, 16.0, p.y) * mix(0.25, 1.0, exp(-max(p.y - 24.0, 0.0) / 34.0));
		float core = 1.0 - smoothstep(0.2, 0.5, abs(v_UV.x - 0.5));
		return vec4(mix(color, kLantern * 1.2, 0.6) * flicker, alpha * fall * (0.55 + 0.45 * core));
	}

	if (style == STYLE_BAND)
	{
		float reach = min(56.0, size.x * 0.25);
		float ends = smoothstep(0.0, reach, p.x) * smoothstep(0.0, reach, size.x - p.x);
		float rim = min(size.y * 0.35, 14.0);
		float vert = smoothstep(0.0, rim, p.y) * smoothstep(0.0, rim, size.y - p.y);
		float wisp = 0.86 + 0.28 * (valueNoise(vec2(p.x * 0.018 - u_Time * 0.12, p.y * 0.06 + seed)) - 0.5);
		return vec4(color, min(alpha * 1.3, 0.8) * ends * mix(0.55, 1.0, vert) * wisp);
	}

	if (style == STYLE_TAG)
	{
		float d = Inside(p, size, size.y * 0.5);
		vec4 c = vec4(color + kSpore * 0.05 * (1.0 - p.y / size.y), min(alpha * 1.5, 0.85) * smoothstep(0.0, 1.5, d));
		float breath = 0.70 + 0.30 * sin(u_Time * 2.2);
		c = Over(c, kSpore, (1.0 - smoothstep(0.3, 1.0, abs(d - 2.5))) * 0.50 * breath);
		return Over(c, kSpore, exp(-max(d - 3.0, 0.0) / 5.0) * step(3.0, d) * 0.10 * breath);
	}

	if (style == STYLE_SELECT)
	{
		float ends = smoothstep(0.0, 26.0, p.x) * smoothstep(0.0, 90.0, size.x - p.x);
		float vert = smoothstep(0.0, 5.0, p.y) * smoothstep(0.0, 5.0, size.y - p.y);
		vec4 c = vec4(color, alpha * ends * vert);
		float under = (1.0 - smoothstep(0.4, 1.2, abs(p.y - (size.y - 1.5)))) * smoothstep(size.x, 0.0, p.x);
		c = Over(c, kSpore, under * 0.40);
		float glint = 1.0 - smoothstep(0.6, 2.2, length(p - vec2(5.0, size.y * 0.5)));
		return Over(c, kSpore, glint * (0.65 + 0.35 * sin(u_Time * 3.0)));
	}

	if (style == STYLE_SHADOW)
	{
		float d = Inside(p, size, 10.0);
		return vec4(color, alpha * smoothstep(-2.0, 16.0, d));
	}

	if (style == STYLE_PAPER)
	{
		// The same paper as her map (Map.fs): warm and fibrous, stained, browned toward the edges, folded once,
		// torn a little at the edges. Moss has crept onto a corner where it lay beside her.
		vec2 toEdge = min(p, size - p);
		float edge = min(toEdge.x, toEdge.y);
		float fiber = valueNoise(p * vec2(0.6, 0.05)) * 0.5 + valueNoise(p * 0.25) * 0.5;
		float stain = smoothstep(0.58, 0.88, valueNoise(p * 0.012 + 3.7));
		vec3 paper = vec3(0.91, 0.86, 0.73) * (0.92 + 0.10 * fiber);
		paper *= 1.0 - 0.10 * stain;
		paper *= mix(0.80, 1.0, smoothstep(0.0, 40.0, edge + valueNoise(p * 0.05) * 14.0));
		float crease = abs(p.y - size.y * 0.5);
		paper *= 1.0 - 0.08 * (1.0 - smoothstep(0.0, 1.5, crease));
		paper *= 1.0 + 0.04 * (smoothstep(1.0, 2.0, crease) - smoothstep(2.0, 5.0, crease));

		float ragged = edge - 4.0 - (valueNoise(p * 0.08) * 3.0 + valueNoise(p * 0.5));
		float sheet = smoothstep(0.0, 1.5, ragged);
		vec4 c = vec4(paper, min(alpha * 1.06, 1.0) * sheet);
		// Lying on the table, a soft shade round it.
		c = Over(c, vec3(0.0), (1.0 - sheet) * smoothstep(-5.0, 0.0, ragged) * 0.35 * alpha);
		return Moss(c, p, size, 2, 22.0, seed);
	}

	// STYLE_RULE: a dry brush stroke, thinning out at both ends.
	float taper = smoothstep(0.06, 0.30, v_UV.x) * smoothstep(1.0, 0.70, v_UV.x);
	float dry = 0.50 + 0.50 * valueNoise(vec2(p.x * 0.07, p.y * 1.4 + seed));
	return vec4(color * 1.1, alpha * taper * dry);
}

void main()
{
	if (u_Mode == 0)
	{
		FragColor = u_Color;
		return;
	}

	if (u_Mode >= 3)
	{
		FragColor = Styled(u_Mode - 3);
		return;
	}

	if (u_Mode == 2)
	{
		vec2 size = QuadSize();
		vec2 p = vec2(v_UV.x, 1.0 - v_UV.y) * size;
		float d = Inside(p, size, size.y * 0.5);
		float fill = clamp(u_Fill, 0.0, 1.0);

		// The groove: sunk in, its lower lip catching a little light.
		vec3 groove = vec3(0.028, 0.042, 0.040) + vec3(0.022) * smoothstep(size.y * 0.3, size.y, p.y);
		vec4 c = vec4(groove, 0.60 * smoothstep(0.0, 1.0, d) * u_Color.a);
		c = Over(c, kFrame, (1.0 - smoothstep(0.3, 1.0, abs(d - 0.8))) * step(size.y * 0.55, p.y) * 0.30);

		// The fill runs in it like water, lighter on top, a slow sheen drifting along and a light at its head.
		float head = fill * size.x;
		float inFill = (1.0 - smoothstep(head - 1.2, head + 0.4, p.x)) * smoothstep(0.5, 1.5, d) * step(0.001, fill);
		vec3 base = mix(u_Color.rgb, u_Color2, fill);
		float sheen = valueNoise(vec2(p.x * 0.06 - u_Time * 0.45, p.y * 0.35));
		vec3 fc = base * (0.78 + 0.32 * (1.0 - p.y / size.y)) * (0.90 + 0.20 * sheen);
		c = Over(c, fc, inFill * 0.92 * u_Color.a);

		// Quarter marks on the lip of a long groove, like an old gauge.
		if (size.x > 100.0)
		{
			float tick = 0.0;
			for (int i = 1; i < 4; ++i)
				tick = max(tick, 1.0 - smoothstep(0.3, 0.9, abs(p.x - size.x * 0.25 * float(i))));
			c = Over(c, kFrame, tick * step(size.y * 0.5, p.y) * 0.35);
		}

		// A bead of light at the head of a bar that is neither empty nor full.
		float r = size.y * 0.5;
		float toHead = length(p - vec2(clamp(head, r, size.x - r), r));
		float lit = step(0.001, fill) * step(fill, 0.999) * u_Color.a;
		c = Over(c, base * 1.25 + vec3(0.18), exp(-toHead / max(r * 0.8, 1.5)) * 0.45 * lit);
		FragColor = Over(c, vec3(1.0, 0.98, 0.90), (1.0 - smoothstep(r * 0.25, r * 0.7, toHead)) * 0.55 * lit);
		return;
	}

	vec2 uv = v_UV;
	float d = length((uv - vec2(0.5)) * vec2(1.15, 1.0));

	// Vignette closes in as spore exposure rises.
	float vig = smoothstep(0.34, 0.80, d) * u_Vignette;

	// Haze breathes, bleeds inward from the edges, and borrows the time-of-day fog tint.
	float breathe = 0.72 + 0.28 * sin(u_Time * 0.55);
	float edgeMask = smoothstep(0.10, 0.72, d);
	float haze = u_Haze * breathe * (0.30 + 0.70 * edgeMask);
	vec3 hazeColor = mix(u_HazeColor, EnvAt(u_TimeOfDay).fogColor, 0.35);

	vec3 color = mix(hazeColor, vec3(0.02, 0.03, 0.05), vig / max(vig + haze, 0.0001));
	float alpha = clamp(vig * 0.85 + haze, 0.0, 0.92);

	FragColor = vec4(color, alpha);
}
