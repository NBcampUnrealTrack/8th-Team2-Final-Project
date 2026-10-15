# (선택) Grok 영상 — 그림책 컷에 미세한 움직임 넣기

**지금 구성에는 영상이 필요 없다.** 그림책 4장은 엔진 안에서 도트 한 칸 단위로 천천히 움직이고(팬), 5번째 컷은 실제 게임 화면이다.
영상 파일(MP4)은 색 정보를 절반 해상도로 저장해(4:2:0) 도트 경계가 번지므로 기본안에서 뺐다.

그래도 그림 속 정령의 빛이 실제로 떠다니고 천이 흔들리는 '살아 있는 그림책'을 원하면, 각 그림을 Grok 의 이미지→영상으로
6초 정도 움직이고 엔진에서는 PNG 프레임으로 받아 도트 격자에 다시 앉혀 쓴다. 그림(GPT)이 확정된 뒤에 할 일이다.

## 공통 조건 (모든 컷 앞에 붙임)

```
Animate this pixel-art storybook illustration with very subtle ambient motion only. The camera must stay completely still: no zoom, no pan, no parallax, no cuts. Keep every shape, colour and the pixel-art look exactly as in the image; do not add or remove objects, do not change faces or figures. 6 seconds, seamless loop if possible.
```

## 컷별 움직임

| 그림 | Grok 프롬프트 (공통 조건 뒤에) |
|---|---|
| P1 요람 | `The glowing spirit wisps drift slowly around the basket, bobbing gently; tiny light motes float upward; the wisps' glow pulses softly; the baby's tiny hand moves once. Nothing else moves.` |
| P2 아이 | `The spirit wisps loop playfully around the running child; the stream ripples; leaves sway lightly; dappled sunlight flickers. The child's pose stays the same with only a slight bounce.` |
| P3 책 | `The wisps hover over the open book with a gentle pulsing glow; one page lifts slightly and settles; dust motes drift in the warm light. The child stays still.` |
| P4 경계 | `Morning mist drifts slowly in the valley; chimney smoke rises from the distant village; the wisps on the roots sway; two wisps float a little forward past the threshold. The traveller's cloak moves slightly in the breeze.` |

## 받을 파일 / 연결

- MP4 원본 그대로 `ArtSource/Prologue_20261014/video/P1.mp4` … 에 넣어 주시면,
  제가 프레임을 704×396 도트 격자·48색으로 다시 앉혀 PNG 시퀀스로 만들고, 그림책 위젯이 그림 대신 이 시퀀스를 재생하게 연결한다(Img Media).
- 실제 효과는 1080p 기준 그림 한 칸 = 화면 3픽셀이라 아주 작은 움직임도 잘 보인다. 과한 움직임은 도트가 지글거려 보이니 '아주 약하게'가 핵심.
