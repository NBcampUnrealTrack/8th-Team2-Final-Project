# PIE 도우미: 프롤로그 재생 시작
import unreal
unreal.SystemLibrary.execute_console_command(GAME_WORLD, 'nu.Prologue.Play', PC)
log('prologue play requested')
