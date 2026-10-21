# Art — 블렌더 리소스 스크립트

게임 모델은 전부 여기 파이썬 스크립트로 만든다. 스크립트가 원본이고, 다시 돌리면 같은 리소스가 나온다.
`.blend` 와 미리보기 이미지는 git 에 올리지 않는다(`Art/blend/`, `Art/previews/`).

## 빌드

```
"C:\Program Files\Blender Foundation\Blender 5.2\blender.exe" -b --factory-startup --python Art/build.py -- car
```

- 결과: `SimpleGame/Assets/Models/<이름>.gsm`(메시·뼈대·재질), `<이름>.gsa`(애니메이션, 리깅한 것만),
  `SimpleGame/Assets/Textures/<이름>_albedo.jpg · _normal.jpg · _mask.jpg`
- 미리보기: `Art/previews/<이름>.png` — 게임 카메라 각도(요 45°, 피치 30°, 직교)에서 앞·뒤를 찍는다.
- 지면 재질: `blender -b --factory-startup --python Art/ground.py`
- 전부: `blender -b --factory-startup --python Art/build_all.py`

## 컨셉: 「이끼가 덮어 가는 기억의 디오라마」

손으로 만들어 칠한 한국 시골 미니어처를 이끼와 포자가 천천히 덮어 가는 느낌. `docs/GameConcept.md` 「리소스 컨셉」 절.

1. **형태:** 현대물은 직선이 처지고 깨진 모양. 자연은 둥근 덩어리가 감싸고 흘러내린다. 포자·기억은 가장 작고 밝은 알갱이.
2. **비율:** 사람 머리·손·소품은 조금 크게, 건물은 조금 작게. 실루엣만 봐도 무엇인지 알게.
3. **색:** `gse/mat.py` 의 `PALETTE`(채도 낮은 녹·청록·회색·흙빛). 빛나는 색은 `spore_glow`(포자 청록)와 `lamp_glow`(등불 호박색) 둘만.
4. **재질:** 거칠기 높고 반짝임 적게. 모서리는 살짝 둥글린다(`bevel=`). 재질 셰이더가 오목한 곳에 때, 모서리에 드라이브러시 밝음을 자동으로 넣는다.
5. **자연화:** 현대물 표면에 `geo.moss_mask(obj)` 로 "이끼가 먼저 앉을 곳"을 칠한다. 게임이 청크 자연화 단계에 맞춰 이끼를 키운다.

## 좌표·단위

- 1 블렌더 단위 = 1 m = 게임 1 단위. **Z 위, 모델 앞은 -Y.** 내보낼 때 게임(Y 위, 앞 +Z)으로 바뀐다.
- 원점은 바닥 가운데(발밑). 캐릭터의 왼쪽(_L)은 +X, 오른쪽(_R)은 -X.
- 기본 줌(720p)에서 1 m ≈ 36 픽셀, 최대 확대 ≈ 90 픽셀. 잔무늬보다 실루엣·색 덩어리가 읽힌다.

## 스크립트 모양

`Art/assets/<게임 모델 이름>.py` 에 `build()` 하나. 부품을 만들고 `gse.pipeline` 으로 마무리해 결과 dict 를 돌려준다.

| 마무리 | 쓰는 곳 | 하는 일 |
|---|---|---|
| `pipeline.static(name, parts, size)` | 건물·차·소품 | 부품을 합쳐 그대로 최종 메시. UV → 베이크 |
| `pipeline.organic(name, parts, voxel, tris, size)` | 사람·짐승·바위·나무 둥치 | 부품을 복셀 리메시로 한 덩어리로 녹이고 줄인 뒤, 부품의 재질·범프·태그를 옮겨 굽는다 |
| `pipeline.mixed(name, hard, soft, ...)` | 딱딱한 몸체 + 덩어리 이끼 | 둘을 합쳐 한 텍스처 세트로 |

결과 dict 에 더할 수 있는 것: `unit=(x, y, z)`(게임 축, 단위 상자로 정규화 — 집·헛간·두둑·울타리 가로대처럼 게임이 늘려 쓰는 모델),
`armature`, `clips`, `preview_frames`, `preview_views`, `preview_height`, `specular`.

## 정점 그룹 태그

`geo.tag(obj, "이름", ...)` — 부품의 모든 정점에 붙인다. 합치거나 리메시해도 따라간다.

| 태그 | 게임에서 |
|---|---|
| 뼈 이름 (`hips`, `thigh_L` …) | 스키닝 가중치(부품마다 하나, 이음매는 `rig.smooth_groups` 로 부드럽게) |
| `sway` | 바람에 흔들림(높이에 비례) — 잎·풀 |
| `pulse` | 은은한 발광 맥박 — 이끼 덩어리·포자 |
| `breathe` | 숨쉬듯 위아래 늘었다 줄었다 |
| `hover`, `spin` | 바닥에 놓인 아이템의 둥실·회전 |
| `bob` | 걸음 출렁임(리깅 없는 모델) |
| `foliage` | 잎: 가을엔 단풍, 겨울엔 빛바램 |
| `moss` (가중치) | 이끼가 먼저 앉을 곳(0~1). `geo.moss_mask` |

## 재질

`mat.get("이름")` 프리셋: moss, moss_dark, leaf, leaf_dark, pine, ginkgo, bark, wood, wood_dark, wood_grey, planks,
plaster, plaster_grey, stone, stone_dark, concrete, slate, giwa, tin, rust, metal, paint_blue/green/red/cream, glass,
rubber, soil, onggi, paper, skin, hair_dark, hair_white, coat, scarf, trousers, boots, canvas, cardigan, apron, monpe,
cloth_grey, fur_boar, fur_deer, antler, bone, mite_shell, berry_red, berry_pale, spore_glow, lamp_glow.

새 재질은 자기 스크립트 안에서 `mat.make("고유한_이름", (r, g, b), pattern=..., rough=..., bump=...)`.
패턴: plain, wood, planks, rust, plaster, slate, giwa, stone, concrete, cloth, knit, hair, bark, leaf, moss, fur, onggi, paper, skin, shell.
범프맵은 패턴의 높이로 만들어지고 노멀맵에 구워진다.

## 텍스처 크기·폴리곤 (가볍게)

| 종류 | 텍스처 | 삼각형 |
|---|---|---|
| 캐릭터·짐승, 큰 건물, 차 | 512 | 3,000~6,000 |
| 나무·중간 소품 | 512 | 1,500~4,000 |
| 작은 소품·아이템·유물 | 256 | 300~1,500 |

JPG 세 장(바탕색·노멀·마스크)을 쓴다. 마스크: R 오목한 곳 그늘, G 거칠기, B 발광.

## 리깅·애니메이션

- 뼈대: `rig.armature(name, [(뼈, 머리, 꼬리, 부모), ...])`. 맨 위는 바닥의 `root`(변형 없음, 몸 전체 이동용).
- 부품을 뼈 이름으로 태그 → `pipeline.organic` → `rig.bind` → `rig.smooth_groups(low, 변형뼈들)`.
- 클립: `rig.clip(arm, "이름", [(프레임, {뼈: (rx, ry, rz)}, root_이동), ...], loop=True)` — 30 fps.
  회전은 **아마추어 축 기준 도(°)**: -X 회전 = 팔다리를 앞(-Y)으로, +X = 고개 숙이기·몸 앞으로 숙이기,
  왼팔 바깥으로 = -Y 회전, 오른팔 바깥으로 = +Y 회전, Z = 몸 비틀기. `rig.mirror(pose)` 는 좌우를 바꾼다.
- 참고 구현: `assets/player.py`.
