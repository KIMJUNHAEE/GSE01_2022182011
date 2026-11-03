# 눈 텍스처 생성 기록

모드: 내장 image_gen. Tripo 호출이나 크레딧은 사용하지 않았습니다. 모델 전체를 생성한 작업이 아니며 투명한 눈 그림 한 쌍을 생성해 Blender에서 얼굴 UV에 배치했습니다.

입력 1: 수정 전 모델 얼굴 미리보기. 입력 2: 사용자가 제공한 SD 여성 개척자 정면 참고 사진.

## 사용한 프롬프트 사양

Use case: precise-object-edit.
Asset type: production flat 2D eye-and-eyebrow texture sprite pair for an existing chibi female Stelle / female Trailblazer 3D model.
Input 1 is the existing face to repair. Input 2 is the desired style reference.
Extract and redraw ONLY two complete eyes and two thin eyebrows as clean, sharp anime art on a genuinely transparent background.
No face skin, cheeks, nose, mouth, hair, ears, clothing, lighting, scenery, shadow, text or watermark.
Two eyes in a natural mirrored pair, both open, with a calm, soft, neutral confident expression matching reference 2.
Round golden yellow/amber irises with deeper amber upper gradient, small dark amber oval pupils, radial detail, creamy sclera, dark chocolate upper lashes with a few points and a delicate lower line.
White catchlights share the same lighting direction. Thin gray-brown eyebrows.
Flat albedo artwork, not a render of eyeballs. Symmetric frontal orthographic view, no tilt or perspective, eyes at the same height.
Wide 2:1 canvas, left eye at 25% and right eye at 75%. Each complete eye occupies about 31% of canvas width and 48% of height, with clear margins and no clipped lashes.
Highest available resolution with clean antialiasing.

## 저장 파일

- `Textures/T_PLAYER_Stelle_Eyes_Clean.png`: 생성된 투명 원본, 1774×887.
- `Textures/T_PLAYER_Stelle_Face_Detail_2K.png`: Blender에서 피부 바탕·미소와 합성한 2048×2048 얼굴 맵.
- `Textures/T_PLAYER_Stelle_Clean_BaseColor_4K.png`: Blender에서 기존 UV로 베이크한 4096×4096 색상 맵.
- `Textures/T_PLAYER_Stelle_Clean_ORM_4K.png`: Blender에서 베이크한 표면 맵.

## 추가 수정

이번 추가 수정은 Blender의 재질·UV 할당과 텍스처 베이크로 진행했습니다. 이전에 생성한 눈 원본을 재사용하고 검정 잉크 색상을 적용했습니다. 새 이미지 생성 프롬프트나 외부 생성 호출은 없습니다.
