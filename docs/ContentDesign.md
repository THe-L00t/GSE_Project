# 콘텐츠 설계 — 초반 튜토리얼 · 32번 국도 (v1)

> 작성일: 2026-09-23
> 근거: `GameConcept.md` 2026-09-23 결정(원문 기록), 기획서 C절 튜토리얼 7단계, 현재 구현(`Prototype.md` v0.4).
> 상태 표기: **[확정]** 사용자가 정함 / **[가결정]** Claude 설계, 사용자 확정 전. 작업은 이 값을 기준으로 진행한다.
> 화면 텍스트는 영어(확정). 이 문서의 대사는 게임에 들어가는 영어 원문과 뜻을 함께 적는다.

---

## 1. 시스템

### 1.1 성장 축 두 개 [확정 방향 / 수치 가결정]

| 축 | 담당 | 오르는 방법 | 효과 |
|---|---|---|---|
| **레벨 · 능력치** | 전투 | 적 처치, 유물 | 힘·체력·민첩·감각 |
| **자연 이해도** (Nature Insight) | 탐험 | 꿈 조각, 먹을 수 있는 열매 처음 구별, 등불 사슴 관찰, 물가 발견 | 포자 노출 상승 둔화 `1 / (1 + 0.15 × 이해도)`, 먹을 수 있는 열매 표시, (추후) 덩굴 벽·강 통과 |

- 포자 저항은 **자연 이해도만** 담당한다. 겹치던 능력치 Insight 는 **Senses(감각)** 로 바꾼다:
  1포인트(기본 3 초과분)당 경험치 +6%, 적 드롭 확률 +8%.

### 1.2 생존 자원 [확정 방향 / 수치 가결정]

| 자원 | 줄어드는 속도 (국도) | 채우는 방법 | 0 이 되면 |
|---|---|---|---|
| 물 | 100 → 0 에 6분 | 우물·저수지에서 E(가득), 물병 R(+35, 포자 노출 −0.45) | 포자 노출 상승 1.6배 |
| 음식 | 100 → 0 에 9분 | F 로 먹기: 열매 +15, 채소 +20, 통조림 +45 | 이동 0.85배, 공격력 0.8배 |
| 체온 | 밤에만 100 → 0 에 3분 20초, 낮·석등 곁에서 회복 | 석등 곁, 해 뜨기 | 체력 초당 −1 (긴 잠으로 이어질 수 있음) |

- **체온은 튜토리얼에서 뺀다 [확정].** 국도에서도 **국도에서 하루(4분)를 보낸 뒤부터** 켜진다 — "시간이 오래 지나면 영향" [확정 방향].
- 마을(튜토리얼)에서는 물·음식이 줄지 않는다. 행동(물 긷기·먹기)으로만 바뀐다. 실패 없음.

### 1.3 배낭 [가결정]

- 용량 **8칸**. 약초·물병·열매·채소·통조림은 1개당 1칸, 손전등 1칸.
- 마을에서는 제한 없이 모으고, **출발(7단계)에서 8칸만 골라 넣는다.** 나머지는 집에 두고 간다.
- 국도에서 배낭이 가득 차면 주울 수 없다(그 자리에 남는다). 무기·유물 기록은 칸을 쓰지 않는다.
- I: 배낭 보기.

### 1.4 유물 도감 (Relic Journal) [확정 방향 / 목록 가결정]

B: 도감 열기. 주우면 경험치 +12 와 함께 한 칸이 채워진다.

| # | 유물 | 얻는 곳 | 설명문 (게임 원문) |
|---|---|---|---|
| 1 | Hand torch | 마을 헛간 (고정) | Push the switch and the dark steps back. The battery will not come back. |
| 2 | Canned peaches | 국도 버스 정류장 (고정) | Sealed before the Bloom. Sweeter than anything that grows now. |
| 3 | Wristwatch | 무작위 | Still ticking. Nobody remembers what it was hurrying toward. |
| 4 | Pocket radio | 무작위 | Only static now, and under it, sometimes, something like breathing. |
| 5 | Bus ticket | 무작위 | Route 32, one adult. The bus never came back for the return. |
| 6 | Family photo | 무작위 | Four people squinting into the sun. The house behind them is a hill now. |
| 7 | Smartphone | 무작위 | A black mirror. People once carried the whole world in one of these. |
| 8 | School badge | 은행나무 읍내 입구 (고정) | Ginkgo Town Middle School. The ginkgo on it is the only thing still growing. |

- 무작위 유물(3~7)은 아직 못 얻은 것 중에서 고른다. 다 모으면 경험치만 준다.
- 국도 깊이 갈수록 유물이 드물어진다(3.4절 현대성).

### 1.5 손전등 [가결정]

- L: 켜고 끄기. 켜면 플레이어 앞을 비추는 점광원(반경 8).
- 배터리 100 → 0 에 켠 시간 2분 30초. **다시 채울 수 없다** — "편리하지만 유한한 유물"의 첫 경험.

### 1.6 적의 정체 [확정]

처음 마주치면 한 줄 설명이 뜬다.

| 적 | 정체 | 행동 변경 | 첫 조우 문구 |
|---|---|---|---|
| 포자 진드기 | 포자를 퍼뜨리는 해충. 싸워도 되는 대상 | 그대로 | Spore mite: a tick fat with spores. Where it walks, the air turns sweet and heavy. |
| 허물 | 포자가 움직이는 빈 옷·껍데기. **사람이 아니다** | 그대로 | Husk: an empty coat the spores have learned to wear. No one is inside. |
| 이끼 멧돼지 | 놀랐을 때만 돌진 | 감지 거리 10 → 3.5. 맞거나 놀라면 16 까지 쫓는다 | Moss boar: it only charges when startled. Walk wide, or stand your ground. |

### 1.7 조작 추가 [가결정]

| 키 | 동작 |
|---|---|
| I | 배낭 |
| B | 유물 도감 |
| L | 손전등 |
| F | 먹기 (열매 → 채소 → 통조림 순) |
| E | 대화·상호작용 (국도에서도: 저수지 물 채우기 등) |

---

## 2. 초반 튜토리얼: 물안개 마을 7단계 [확정: 7단계 그대로 / 세부 가결정]

하루 반. 1~5단계는 첫날(아침 → 밤), 6~7단계는 다음 날 아침.
**순임은 1~5단계 내내 깨어 있고 말을 건다.** 사투리 섞인 말투, 늘 손주가 밥을 먹었는지 걱정한다 [확정].

- 첫날 포자는 옅다(예쁘다고만 생각하는 단계). 밤이 되면 처음으로 빛나며 짙어진다. 둘째 날은 가득.
- 잠든 이웃 4명은 첫날부터 있다(지난밤 잠든 사람들). 곁에서 쉬면 꿈 조각 — 언제든 선택.
- 첫날에는 포자 노출이 오르지 않는다. 둘째 날부터 오른다(실패는 없다).

| 단계 | 장면 | 플레이어가 하는 것 | 가르치는 것 |
|---|---|---|---|
| 1 | 아침, 집 앞 | 순임과 대화 → 우물까지 걷는다 | 이동, 카메라, 대화(E) |
| 2 | 우물 | E 로 물을 긷는다 → 순임에게 가져간다 | 상호작용, **물·음식 게이지 등장** |
| 3 | 텃밭과 뒷산 | 텃밭 채소 3개, 뒷산 붉은 열매 3개. 흰 열매는 못 먹는다 → 돌아와 F 로 먹는다 | 채집, 배낭(I), 먹기(F), **자연 이해도 첫 획득** |
| 4 | 헛간 | 헛간에서 손전등 발견 → L 로 켜 보고 가져간다 | **유물**, 도감(B), 소모 게이지 |
| 5 | 저녁과 밤 | 해가 진다(시간 빨라짐). 집으로 → 저수지가에 등불 사슴 → 지켜본다 → 문에서 잔다 | 시간 흐름, 관찰, 분위기 연출(색온도·빛나는 포자·반딧불이) |
| 6 | 다음 날 아침 | 순임이 깨어나지 않는다. 손끝의 이끼. 편지와 지도를 읽는다. (선택) 솥의 밥을 먹는다 | 스토리 발단, 문서 읽기 |
| 7 | 출발 | 배낭에 8칸만 골라 넣는다 → 남쪽 길로 → 경계에서 "ROUTE 32" 가 크게 뜨고 안개가 걷힌다 | 배낭 한도, 지역 전환 |

### 2.1 배치 (마을, 1유닛 ≈ 1m)

| 장소 | 위치 | 비고 |
|---|---|---|
| 순임의 집 문 | (−2, −6.9) | 1단계 시작, 5단계 잠자리, 6·7단계 상호작용 |
| 우물 | (2.5, −2) | 기존 |
| 텃밭 | 울타리 안 x −5.5 ~ −2, z −4.5 ~ 0.5 | 두둑 3줄, 채소 6개 |
| 뒷산 | z −19 ~ −25 | 붉은 열매 덤불 3, 흰 열매 덤불 2, 소나무·바위 |
| 헛간 | (13.5, −13), 문은 남쪽 | 새 모델 |
| 등불 사슴 길 | 저수지 남쪽 물가 x −24 → −6, z −3.5 | 밤에만 |

### 2.2 대사 (게임 원문 / 뜻)

**1단계 — 시작**
- G: "Up already, Yul? Did ye eat? ...Course ye didn't. Skin an' bones, this one." (벌써 일어났냐? 밥은? …안 먹었지. 뼈만 남았네.)
- G: "Fetch us some water from the well first, there's a dear. Then we'll see about feedin' ye." (우물에서 물 좀 떠 오너라. 그다음에 밥 먹이자.)

**2단계 — 물을 가져왔을 때**
- G: "Good child. Drink some yerself, go on. Ye look peaky." (착하다. 너도 마셔라. 얼굴이 핼쑥하다.)
- G: "Now then. Pull a few radishes from the patch, an' pick berries up the back hill." (텃밭에서 무 좀 뽑고, 뒷산에서 열매 따 오너라.)
- G: "Red'uns only, mind! Them pale ones'll twist yer belly." (빨간 것만! 허연 건 배 뒤틀린다.)

**3단계**
- 흰 열매 E: "Grandmother said the pale ones twist your belly. You leave them."
- 첫 붉은 열매: "You know which berries are safe now.  Nature Insight +1"
- 돌아왔을 때 G: "That's a good haul. Now eat a bit, I'm watchin' ye." (많이 따 왔네. 이제 좀 먹어라, 보고 있다.) → F 로 먹을 때까지 기다린다.
- 먹은 뒤 G: "There. Colour's comin' back already." (봐라, 벌써 혈색이 돈다.)
- G: "One more thing, love. There's an old torch in the shed, from before the Bloom. Fetch it, would ye? Nights are long now." (헛간에 개화 전 손전등이 있다. 가져오너라. 요즘 밤이 길다.)

**4단계**
- 헛간 E: "Under a tarp: a hand torch from the old world.  New entry in your Relic Journal (B)"
- 돌아왔을 때 G: "Ah, that's the one. Yer grandad's. Mind the battery, there's no more where that came from." (그래, 그거다. 너희 할아버지 거다. 배터리 아껴라, 더는 없다.)
- G: "Sun's goin' down already. Stay close to home tonight, an' come in 'fore it's dark." (벌써 해가 진다. 오늘 밤은 집 가까이 있다가 어두워지기 전에 들어와라.)

**5단계**
- 문 가까이, 밤 G: "Hush... look there, by the water. A lantern deer. Don't ye go followin' it tonight." (쉿… 저기 물가 봐라. 등불 사슴이다. 오늘 밤엔 따라가지 마라.)
- 사슴 관찰 E: "The lights on its antlers pulse like slow breathing.  Nature Insight +1"
- 잠자리 G: "Did ye eat enough? ...Sleep, then. Them lights'll be gone by mornin'." (배는 불렀나? …자거라. 저 불빛들 아침이면 없어질 거다.)

**6단계**
- 순임 E: "She is breathing. Moss has started at her fingertips. A letter and a map lie beside her."
- 편지(기존) 끝에 추신: "P.S. There's rice in the pot. Eat before ye go, mind." (추신. 솥에 밥 있다. 먹고 가거라.)
- 문 E(밥): "The rice is still warm. You eat all of it."

**7단계**
- 문 E: 배낭 창. W/S 선택, A/D 빼기·넣기, E 확정.
- 경계: 화면이 옅어지며 "ROUTE 32" 와 함께 안개가 걷힌다.

### 2.3 첫날 시간 흐름 [가결정]

- 1~4단계는 아침~오후. 시간은 흐르되 4단계 끝까지 해가 지지 않게 오후 0.62 에서 멈춘다.
- 4단계 대사가 끝나면 시간이 12배로 흘러 황혼(0.78)까지 간다. 사슴은 0.80 이후.
- 잠들면 화면이 어두워지고 다음 날 0.27(새벽 직후)로.

---

## 3. 32번 국도 — 재설계 [확정: 끝·무한 생성 / 목적·구간 가결정]

### 3.1 이 레벨의 목적

**"첫 바깥."** 마을에서 한 가지씩 배운 것을, 이후 게임 전체가 쓰는 한 바퀴의 고리로 묶는다.

1. **싸워서 자란다** — 레벨·능력치 (기존 교육 1~5단계)
2. **몸을 지킨다** — 물·음식이 처음으로 줄어든다. 하루가 지나면 밤이 추워진다
3. **자연이 길을 알려준다** — 등불 사슴을 따라 숨은 저수지를 찾는다 (기획서 B-3 의 첫 경험)
4. **옛 세계를 짊어진다** — 버스 정류장의 통조림, 읍내 입구의 교표. 편리하지만 유한하다

레벨의 끝은 **은행나무 읍내 입구**(남쪽 6청크, 약 144m). 도착하면 프로토타입의 한 장이 닫히고,
길은 계속 자라며 걸을수록 현대의 흔적이 무뎌진다.

### 3.2 구간

| 청크 z | 구간 | 목표 (안내) | 배우는 것 |
|---|---|---|---|
| 0~1 | 석등 | 파이프 → 진드기 3 → 능력치 → 멧돼지 → 능력치 (기존) | 전투, 레벨업, 능력치 |
| 2 | 버스 정류장 | "Something is sheltered at the bus stop down the road." 통조림(유물 2) 줍기 → F 로 먹을 수 있다 | 국도의 유물, 음식 |
| 3~4 | 밤길 | "Your water is running low. Grandmother said the lantern deer know where water is." 밤이면 사슴이 나타나 동쪽 저수지로 이끈다(플레이어가 멀어지면 기다린다). 낮이면 직접 찾아도 된다. 물가 E 로 물 가득 + 물병 채우기, 자연 이해도 +1 | 관찰, 물 보급 |
| 5~6 | 은행나무 입구 | "Reach Ginkgo Town, further south." z 138 을 넘으면 도착. 입구에 노란 은행나무 두 그루, 무너진 학교 담, 교표(유물 8) | 레벨 완료 |
| 7~ | 자유 탐험 | "The road goes on. Walk as far as you like." | — |

- 저수지: 청크 (1, 4), 중심 (20, 96), 12 × 9. 도로에서 동쪽으로 벗어나 있어 길만 따라가면 안 보인다.
- 체온: 국도에서 하루(240초)를 보내면 "The nights are getting colder. Stay near light after dark." 와 함께 켜진다.

### 3.3 무한 생성과 현대성 [확정 방향 / 수치 가결정]

청크마다 **현대성 m** = `clamp(1 − (링 − 3) / 9, 0, 1)` (링 3 까지 1, 링 12 에서 0).

- 현대 소품(폐허 벽·버려진 차·전봇대) 가중치 × m, 자연 소품(나무·소나무·덤불·바위) × (1 + 0.5 × (1 − m)).
- 도로 칠: 지면 셰이더의 도로가 m 에 비례해 흐려진다. m = 0 이면 길이 사라진다.
- 유물이 나올 확률 × m.
- 자연화 단계(1~3)는 기존대로 링에 따라 오른다.

---

## 4. 구현 범위 (이번 작업)

1. 시스템: 생존 자원·자연 이해도·감각 능력치·배낭·도감·손전등·적 정체·멧돼지 행동
2. 모델: 순임(서 있는), 헛간, 텃밭 두둑, 채소, 붉은·흰 열매 덤불, 등불 사슴, 통조림, 버스 정류장, 은행나무, 전봇대
3. 마을 튜토리얼 7단계, 대사 창, 배낭 창, 지역 전환
4. 국도 재설계: 랜드마크 3곳, 사슴 안내, 저수지, 읍내 도착, 현대성, 체온
