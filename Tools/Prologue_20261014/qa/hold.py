# PIE 도우미: 건너뛰기 버튼을 누르고 있는 상태로 만든다(마우스 누름과 같은 경로, OnPressed). 1초 뒤 스킵된다.
import unreal
for button in unreal.ObjectIterator(unreal.Button):
    if button.get_name() == 'SkipButton' and button.get_world() == GAME_WORLD:
        button.on_pressed.broadcast()
        log('skip button pressed')
