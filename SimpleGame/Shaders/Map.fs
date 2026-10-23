#version 330

// Grandmother's map of Route 32, in pencil on old folded paper: the line she drew south from the village, the
// old town hatched around where the road begins with its streets left white, the bus stop circled in red, the
// reservoir coloured in with blue pencil, two ginkgo trees at the town, and at the south end, where her line
// gives out, one tree drawn over and over. Positions arrive in world units and are laid onto the paper here,
// north up. Paper coordinates are pixels from the top-left corner.

in vec2 v_UV;

uniform vec2  u_Size;        // the paper in pixels
uniform vec4  u_World;       // x0, z0, width, depth of the ground the paper covers
uniform vec4  u_Places[5];   // village, bus stop, reservoir, town gate, lone tree: world x, z, -, note read
uniform vec2  u_Road;        // world z where her line leaves the village, and where it gives out
uniform vec4  u_Reservoir;   // the water's half sizes x, z in world units, its turn, and the band it used to fill
uniform vec4  u_Player;      // world x, z, shown
uniform float u_Time;

layout(location = 0) out vec4 FragColor;

const vec3 kGraphite = vec3(0.26, 0.24, 0.23);
const vec3 kRed = vec3(0.76, 0.19, 0.15);
const vec3 kBlue = vec3(0.22, 0.42, 0.80);
const vec3 kGold = vec3(0.90, 0.66, 0.12);
const vec3 kHayul = vec3(0.86, 0.32, 0.14);

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

vec2 ToPaper(vec2 world)
{
	return (world - u_World.xy) / u_World.zw * u_Size;
}

// Must match PondRadius in Mesh.h.
float PondRadius(float a)
{
	return 1.0 + 0.10 * sin(2.0 * a + 0.8) + 0.07 * sin(3.0 * a + 2.1) + 0.04 * sin(5.0 * a + 4.0);
}

// Must match RoadCenter() in ChunkMap.h.
float RoadX(float z)
{
	return sin(z * 0.05) * 3.0;
}

float Segment(vec2 p, vec2 a, vec2 b)
{
	vec2 pa = p - a;
	vec2 ba = b - a;
	float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
	return length(pa - ba * h);
}

float BoxEdge(vec2 q, vec2 extent)
{
	vec2 d = abs(q) - extent;
	return abs(length(max(d, 0.0)) + min(max(d.x, d.y), 0.0));
}

// Lead on paper: how much of a stroke of this width covers a point at distance d, broken up by the grain.
float Lead(float d, float width, vec2 p)
{
	float line = 1.0 - smoothstep(width * 0.5, width * 0.5 + 1.0, d);
	return line * (0.62 + 0.38 * hash12(floor(p * 0.9)));
}

// Diagonal strokes of coloured pencil filling an area, every few pixels.
float Hatch(vec2 p, float spacing, float flip)
{
	float t = fract((p.x + flip * p.y) / spacing);
	return smoothstep(0.55, 0.95, abs(t * 2.0 - 1.0));
}

vec3 Mark(vec3 c, vec3 ink, float a)
{
	return mix(c, ink, clamp(a, 0.0, 1.0));
}

float House(vec2 q)
{
	float body = BoxEdge(q - vec2(0.0, 2.0), vec2(5.0, 3.5));
	float roof = min(Segment(q, vec2(-6.5, -1.5), vec2(0.0, -6.5)), Segment(q, vec2(0.0, -6.5), vec2(6.5, -1.5)));
	return min(body, roof);
}

void main()
{
	vec2 p = vec2(v_UV.x, 1.0 - v_UV.y) * u_Size;
	vec2 world = u_World.xy + p / u_Size * u_World.zw;

	// Paper: warm and fibrous, stained here and there, browned toward the edges, creased where it was folded in six.
	float fiber = valueNoise(p * vec2(0.6, 0.05)) * 0.5 + valueNoise(p * 0.25) * 0.5;
	float stain = smoothstep(0.58, 0.88, valueNoise(p * 0.012 + 3.7));
	vec2 toEdge = min(p, u_Size - p);
	float edge = min(toEdge.x, toEdge.y);
	vec3 c = vec3(0.91, 0.86, 0.73) * (0.92 + 0.10 * fiber);
	c *= 1.0 - 0.10 * stain;
	c *= mix(0.80, 1.0, smoothstep(0.0, 40.0, edge + valueNoise(p * 0.05) * 14.0));
	float crease = min(abs(p.x - u_Size.x * 0.5), min(abs(p.y - u_Size.y / 3.0), abs(p.y - u_Size.y * 2.0 / 3.0)));
	c *= 1.0 - 0.08 * (1.0 - smoothstep(0.0, 1.5, crease));
	c *= 1.0 + 0.04 * (smoothstep(1.0, 2.0, crease) - smoothstep(2.0, 5.0, crease));

	// The old town around where the road begins: a grid of blocks between the streets that cross the road at
	// each chunk border. Most are outlined and hatched, some only outlined, some left out, as she remembered it.
	float urban = 1.0 - smoothstep(30.0, 90.0, max(abs(world.x), abs(world.y)));
	float town = urban * smoothstep(-48.0, -38.0, world.y) * smoothstep(6.0, 8.0, abs(world.x - RoadX(world.y)));
	vec2 cell = vec2(world.x / 12.0, (world.y - 12.0) / 24.0);
	vec2 cellPx = vec2(12.0, 24.0) / u_World.zw * u_Size;
	vec2 fromBlock = abs(fract(cell) - 0.5) * cellPx - (cellPx * 0.5 - vec2(7.0, 6.0));
	float blockEdge = length(max(fromBlock, 0.0)) + min(max(fromBlock.x, fromBlock.y), 0.0);
	float kept = hash12(floor(cell) + 7.0);
	float present = step(0.22, kept) * town;
	float hatched = step(0.45, kept) * (1.0 - step(0.0, blockEdge));
	c = Mark(c, kGraphite, Hatch(p, 5.0, 1.0) * hatched * present * 0.28 * (0.7 + 0.3 * hash12(floor(p))));
	c = Mark(c, kGraphite, Lead(abs(blockEdge), 1.0, p + 3.0) * present * 0.45);

	// Her road: one line drawn south from the village, gone over twice, giving out at the lone tree with a few
	// hesitant dashes.
	float zStart = u_Road.x;
	float zEnd = u_Road.y;
	float rx = ToPaper(vec2(RoadX(world.y), world.y)).x;
	float drawn = smoothstep(zStart, zStart + 3.0, world.y) * (1.0 - smoothstep(zEnd, zEnd + 1.0, world.y));
	float dash = step(0.5, fract(world.y / 5.0)) * (1.0 - smoothstep(zEnd + 4.0, zEnd + 16.0, world.y)) * step(zEnd + 1.0, world.y);
	float wobbleA = (valueNoise(vec2(p.y * 0.04, 0.0)) - 0.5) * 3.0;
	float wobbleB = (valueNoise(vec2(p.y * 0.03, 5.0)) - 0.5) * 2.5;
	float road = Lead(abs(p.x - rx - wobbleA), 2.2, p) * max(drawn, dash * 0.7);
	road = max(road, Lead(abs(p.x - rx - 1.4 - wobbleB), 1.2, p + 17.0) * drawn * 0.6);
	c = Mark(c, kGraphite, road * 0.85);

	// The village: three small houses.
	vec2 village = ToPaper(u_Places[0].xy);
	float houses = 1e3;
	for (int i = 0; i < 3; ++i)
	{
		vec2 o = vec2(float(i - 1) * 15.0, i == 1 ? -7.0 : 2.0);
		houses = min(houses, House(p - village - o));
	}
	c = Mark(c, kGraphite, Lead(houses, 1.4, p) * 0.9);

	// The bus stop: a little shelter, and a red circle gone round twice.
	vec2 q = p - ToPaper(u_Places[1].xy);
	c = Mark(c, kGraphite, Lead(BoxEdge(q, vec2(4.0, 2.5)), 1.3, p) * 0.9);
	float angle = atan(q.y, q.x);
	float r1 = 13.0 + 1.2 * sin(angle * 2.0 + 0.7) + (valueNoise(vec2(angle * 2.0, 3.0)) - 0.5) * 1.5;
	float r2 = 14.4 + 1.0 * sin(angle * 3.0 + 1.9);
	float circle = max(Lead(abs(length(q) - r1), 1.6, p + 31.0), Lead(abs(length(q) - r2), 1.2, p + 47.0) * 0.55);
	c = Mark(c, kRed, circle * (0.75 + 0.15 * u_Places[1].w));

	// The reservoir, coloured in with blue pencil and its shore gone over harder. She drew it before the
	// drought, so it reaches as far as the water used to. Drawn to the paper's across-scale both ways, so the
	// stretched width does not flatten it.
	q = p - ToPaper(u_Places[2].xy);
	float across = u_Size.x / u_World.z;
	vec2 metres = q / across;
	float turnC = cos(u_Reservoir.z);
	float turnS = sin(u_Reservoir.z);
	vec2 turned = vec2(turnC * metres.x - turnS * metres.y, turnS * metres.x + turnC * metres.y);
	vec2 qn = turned / u_Reservoir.xy;
	float rho = max(length(qn), 1e-4);
	float edgeMetres = length(turned) / rho * (rho - PondRadius(atan(qn.y, qn.x + 1e-5))) - u_Reservoir.w;
	float shore = edgeMetres * across + (valueNoise(q * 0.12 + 9.0) - 0.5) * 3.0;
	float waterReach = (max(u_Reservoir.x, u_Reservoir.y) * 1.2 + u_Reservoir.w) * across;
	float fill = (1.0 - smoothstep(-1.0, 0.5, shore)) * (0.30 + 0.45 * Hatch(p, 4.0, -1.0));
	c = Mark(c, kBlue, fill * (0.75 + 0.25 * hash12(floor(p))));
	c = Mark(c, kBlue * 0.8, Lead(abs(shore), 1.4, p + 5.0) * 0.8);

	// The town: two ginkgo trees either side of the road, crowns coloured gold.
	vec2 gate = u_Places[3].xy;
	for (int side = 0; side < 2; ++side)
	{
		vec2 t = p - ToPaper(gate + vec2(side == 0 ? -6.5 : 6.5, 0.0));
		float crown = length(t - vec2(0.0, -9.0)) - 7.5 + (valueNoise(t * 0.4 + float(side) * 4.0) - 0.5) * 1.5;
		c = Mark(c, kGold, (1.0 - smoothstep(-0.5, 0.5, crown)) * (0.35 + 0.45 * Hatch(p, 3.5, 1.0)));
		float tree = min(abs(crown), Segment(t, vec2(0.0, -1.5), vec2(0.0, 5.0)));
		c = Mark(c, kGraphite, Lead(tree, 1.3, p + 23.0) * 0.85);
	}

	// The lone tree at the south end: no words, but the pencil has been over it again and again.
	vec2 lone = ToPaper(u_Places[4].xy);
	float strokes = 1e3;
	for (int k = 0; k < 3; ++k)
	{
		vec2 o = vec2(sin(float(k) * 2.1), cos(float(k) * 1.7)) * 1.5;
		vec2 t = p - lone - o;
		float crown = min(min(length(t - vec2(0.0, -13.0)) - 8.0, length(t - vec2(-6.0, -8.5)) - 6.0), length(t - vec2(6.0, -8.5)) - 6.0);
		strokes = min(strokes, min(abs(crown), Segment(t, vec2(0.0, -4.0), vec2(0.0, 6.0))));
	}
	c = Mark(c, kGraphite * 0.85, Lead(strokes, 1.3, p + 11.0) * (0.78 + 0.15 * u_Places[4].w));

	// The note being read now: rings of its own pencil keep going out from its place. The road's own note has
	// her whole line gone over again, a broad soft stroke widening and fading along it.
	float wave = fract(u_Time * 0.55);
	if (u_Places[0].z > 0.5)
	{
		float stroke = 1.0 - smoothstep(2.0 + wave * 7.0, 3.5 + wave * 7.0, abs(p.x - rx - wobbleA));
		c = Mark(c, kGraphite, stroke * drawn * (1.0 - wave) * 0.45);
	}
	vec3 inks[5] = vec3[5](kGraphite, kRed, kBlue, kGold, kGraphite);
	vec2 lifts[5] = vec2[5](vec2(0.0), vec2(0.0), vec2(0.0), vec2(0.0, -6.0), vec2(0.0, -8.0));
	float starts[5] = float[5](0.0, 19.0, waterReach + 6.0, 22.0, 24.0);
	for (int i = 1; i < 5; ++i)
	{
		if (u_Places[i].z < 0.5) continue;
		float from = length(p - ToPaper(u_Places[i].xy) - lifts[i]);
		float ring = 1.0 - smoothstep(0.8, 2.0, abs(from - (starts[i] + wave * 14.0)));
		c = Mark(c, inks[i], ring * (1.0 - wave) * 0.9);
	}

	// Hayul: a dot where she stands, a ring running out from it; off the paper, an arrow on its edge.
	if (u_Player.z > 0.5)
	{
		vec2 me = ToPaper(u_Player.xy);
		vec2 held = clamp(me, vec2(14.0), u_Size - 14.0);
		vec2 m = p - held;
		if (distance(me, held) < 0.5)
		{
			float spot = 1.0 - smoothstep(3.0, 4.2, length(m));
			float run = fract(u_Time * 0.7);
			float ring = (1.0 - smoothstep(0.6, 1.6, abs(length(m) - (5.0 + run * 10.0)))) * (1.0 - run);
			c = Mark(c, kHayul, max(spot, ring * 0.8));
		}
		else
		{
			vec2 dir = normalize(me - held);
			vec2 l = vec2(dot(m, dir), dot(m, vec2(-dir.y, dir.x)));
			float arrow = clamp(min(l.x + 4.0, (6.0 - l.x) - abs(l.y) * 1.1), 0.0, 1.0);
			c = Mark(c, kHayul, arrow);
		}
	}

	// Torn a little at the edges.
	float ragged = edge - (valueNoise(p * 0.08) * 3.0 + valueNoise(p * 0.5));
	FragColor = vec4(c, smoothstep(0.0, 1.5, ragged));
}
